# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
from helpers.format_config import DataFormat
from helpers.param_config import input_output_formats
from helpers.stimuli_config import StimuliConfig
from helpers.stream import Stream
from helpers.test_config import TestConfig, TestMode

# Must match the C++ constants in stream_d2d_test.cpp
STREAM_ADDRESS = 0x70000
STREAM_DEPTH = 8


def test_stream(workers_tensix_coordinates):
    formats = input_output_formats([DataFormat.Float16_b])[0]

    # Create minimal empty stimuli (not used by test, but required by infrastructure)
    empty_tensor = torch.zeros(1024)

    configuration = TestConfig(
        "sources/stream_d2d_test.cpp",
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

    # Initialize stream indices to zero before starting kernels
    if TestConfig.MODE != TestMode.PRODUCE:
        Stream(STREAM_ADDRESS, STREAM_DEPTH, workers_tensix_coordinates).init()

    configuration.run(workers_tensix_coordinates)
