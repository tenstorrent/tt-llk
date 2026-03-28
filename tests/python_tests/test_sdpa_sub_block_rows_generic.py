# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.device import read_from_device
from helpers.format_config import DataFormat
from helpers.golden_generators import BroadcastGolden, get_golden_generator
from helpers.llk_params import BroadcastType, MathFidelity, format_dict
from helpers.param_config import input_output_formats
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig, TestMode
from helpers.test_variant_parameters import MATH_FIDELITY
from helpers.tilize_untilize import tilize_block, untilize_block
from helpers.unpack import unpack_res_tiles
from helpers.utils import passed_test

FORMATS = input_output_formats([DataFormat.Float16_b])[0]

SUB_A_ADDR = 0x26000
SUB_B_ADDR = 0x2A000
SUB_OUT_ADDR = 0x2C000
SUB_A_DIMS = [64, 128]
SUB_B_DIMS = [64, 32]
SUB_OUT_TILES = 8
TILE_SIZE_BYTES = 2048


def test_sdpa_sub_block_rows_generic(workers_tensix_coordinates):
    sub_a, sub_tile_cnt_a, sub_b, sub_tile_cnt_b = generate_stimuli(
        stimuli_format_A=FORMATS.input_format,
        input_dimensions_A=SUB_A_DIMS,
        stimuli_format_B=FORMATS.input_format,
        input_dimensions_B=SUB_B_DIMS,
        sfpu=False,
    )

    tilized_sub_a = tilize_block(
        sub_a, dimensions=SUB_A_DIMS, stimuli_format=FORMATS.input_format
    )
    tilized_sub_b = tilize_block(
        sub_b, dimensions=SUB_B_DIMS, stimuli_format=FORMATS.input_format
    )

    sub_a_tensor = torch.tensor(sub_a, dtype=format_dict[FORMATS.input_format]).view(
        SUB_A_DIMS[0], SUB_A_DIMS[1]
    )
    broadcast_golden = get_golden_generator(BroadcastGolden)
    tilized_sub_b_broadcast = broadcast_golden(
        BroadcastType.Column,
        tilized_sub_b.flatten(),
        FORMATS.input_format,
        num_faces=4,
        tile_cnt=sub_tile_cnt_b,
        face_r_dim=16,
    )
    sub_b_broadcast = (
        untilize_block(tilized_sub_b_broadcast, FORMATS.input_format, SUB_B_DIMS)
        .view(SUB_B_DIMS[0], SUB_B_DIMS[1])
        .repeat(1, SUB_A_DIMS[1] // SUB_B_DIMS[1])
    )
    sub_golden_matrix = sub_a_tensor - sub_b_broadcast
    sub_golden = tilize_block(
        sub_golden_matrix.flatten(),
        dimensions=SUB_A_DIMS,
        stimuli_format=FORMATS.output_format,
    ).flatten()

    configuration = TestConfig(
        "sources/sdpa_sub_block_rows_generic_test.cpp",
        FORMATS,
        templates=[MATH_FIDELITY(MathFidelity.LoFi)],
        runtimes=[],
        variant_stimuli=None,
    )

    configuration.generate_variant_hash()
    if TestConfig.MODE in [TestMode.PRODUCE, TestMode.DEFAULT]:
        configuration.build_elfs()

    if TestConfig.MODE == TestMode.PRODUCE:
        pytest.skip(TestConfig.SKIP_JUST_FOR_COMPILE_MARKER)

    input_packer = StimuliConfig.get_packer(FORMATS.input_format)
    StimuliConfig.write_matrix(
        tilized_sub_a.flatten(),
        sub_tile_cnt_a,
        input_packer,
        SUB_A_ADDR,
        TILE_SIZE_BYTES,
        4,
        16,
        workers_tensix_coordinates,
    )
    StimuliConfig.write_matrix(
        tilized_sub_b.flatten(),
        sub_tile_cnt_b,
        input_packer,
        SUB_B_ADDR,
        TILE_SIZE_BYTES,
        4,
        16,
        workers_tensix_coordinates,
    )

    configuration.run_elf_files(workers_tensix_coordinates)
    configuration.wait_for_tensix_operations_finished(workers_tensix_coordinates)

    result = read_from_device(
        workers_tensix_coordinates,
        SUB_OUT_ADDR,
        num_bytes=SUB_OUT_TILES * TILE_SIZE_BYTES,
    )
    unpacked = unpack_res_tiles(
        result, FORMATS.output_format, SUB_OUT_TILES, False, 4, 16
    )
    sub_tensor = torch.tensor(unpacked, dtype=format_dict[FORMATS.output_format])

    assert passed_test(
        sub_golden, sub_tensor, FORMATS.output_format, print_errors=False
    )
