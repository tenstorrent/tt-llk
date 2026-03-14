# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.device import wait_for_tensix_operations_finished
from helpers.format_config import DataFormat
from helpers.param_config import input_output_formats
from helpers.stimuli_config import StimuliConfig
from helpers.stream import Stream
from helpers.test_config import TestConfig, TestMode

# Must match the C++ constants in stream_d2h_test.cpp
STREAM_ADDRESS = 0x70000
STREAM_DEPTH = 8
PACKET_SIZES = [1, 3, 7, 16, 32, 64, 100, 128, 200, 300, 50, 25, 13]
DATA_SEED = 0xDEADBEEF


class Prng:
    """xorshift32 — identical to the C++ prng_t in the device test."""

    def __init__(self, seed=0xDEADBEEF):
        self.state = seed if seed else 1

    def next(self):
        self.state ^= (self.state << 13) & 0xFFFFFFFF
        self.state ^= self.state >> 17
        self.state ^= (self.state << 5) & 0xFFFFFFFF
        self.state &= 0xFFFFFFFF
        return self.state

    def byte(self):
        return self.next() & 0xFF


def test_stream_d2h(workers_tensix_coordinates):
    location = workers_tensix_coordinates
    formats = input_output_formats([DataFormat.Float16_b])[0]

    empty_tensor = torch.zeros(1024)
    config = TestConfig(
        "sources/stream_d2h_test.cpp",
        formats,
        variant_stimuli=StimuliConfig(
            empty_tensor,
            formats.input_format,
            empty_tensor,
            formats.input_format,
            formats.output_format,
            tile_count_A=1,
            tile_count_B=1,
            tile_count_res=1,
        ),
    )

    config.generate_variant_hash()

    if TestConfig.MODE in [TestMode.PRODUCE, TestMode.DEFAULT]:
        config.build_elfs()
    if TestConfig.MODE == TestMode.PRODUCE:
        pytest.skip(TestConfig.SKIP_JUST_FOR_COMPILE_MARKER)

    config.write_runtimes_to_L1(location)
    config.variant_stimuli.write(location)

    # Initialize stream indices to zero before starting kernels
    stream = Stream(STREAM_ADDRESS, STREAM_DEPTH, location)
    stream.init()

    elfs = config.run_elf_files(location)

    # Device → host: pop packets while MATH kernel pushes
    assert stream._read_idx == 0, f"read_idx not zero at start: {stream._read_idx}"
    rng = Prng(DATA_SEED)
    for packet_size in PACKET_SIZES:
        expected = bytes(rng.byte() for _ in range(packet_size))
        actual = stream.pop(packet_size)
        assert actual == expected, (
            f"Stream D2H mismatch at packet size {packet_size}: "
            f"expected {expected.hex()} got {actual.hex()}"
        )

    wait_for_tensix_operations_finished(elfs, location)
