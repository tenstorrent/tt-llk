# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Performance test for SFPU eltwise max operation.

Sweeps the same formats and input dimensions as test_sfpu_eltwise_max.py.
"""

import pytest
import torch
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.format_config import DataFormat
from helpers.llk_params import (
    DestAccumulation,
    PerfRunType,
    Transpose,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.profiler import ProfilerConfig
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_variant_parameters import (
    INPUT_DIMENSIONS,
    LOOP_FACTOR,
    NUM_FACES,
    TILE_COUNT,
    UNPACK_TRANS_FACES,
    UNPACK_TRANS_WITHING_FACE,
)


@pytest.mark.perf
@parametrize(
    formats=input_output_formats(
        [
            # DataFormat.Float32,
            # DataFormat.Float16,
            DataFormat.Float16_b,
            # DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=[
        DestAccumulation.No,
        # DestAccumulation.Yes,
    ],
    loop_factor=[
        16,
    ],  # Number of iterations to run the test in order to minimize profiler overhead in measurement
    input_dimensions=[
        [32, 32],
        # [32, 64],
        # [64, 32],
        # [64, 64],
    ],  # Same dimensions as test_sfpu_eltwise_max.py
)
def test_perf_sfpu_eltwise_max(
    perf_report,
    formats,
    dest_acc,
    loop_factor,
    input_dimensions,
    workers_tensix_coordinates,
):
    chip_arch = get_chip_architecture()

    # Skip Float16 without dest_acc on Blackhole (same as test_sfpu_eltwise_max.py)
    if (
        chip_arch == ChipArchitecture.BLACKHOLE
        and formats.input_format == DataFormat.Float16
        and dest_acc == DestAccumulation.No
    ):
        pytest.skip(
            "Float16_a isn't supported for SFPU on Blackhole without being converted to 32-bit intermediate format in dest register"
        )

    # Force dest_acc for certain formats (same as test_sfpu_eltwise_max.py)
    if formats.input_format in [DataFormat.Float16, DataFormat.Float32]:
        dest_acc = DestAccumulation.Yes

    # Generate stimuli - src_A and src_B each have input_dimensions
    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    # Combine A and B tensors (A tiles followed by B tiles in buffer_A)
    src_A_combined = torch.cat([src_A, src_B], dim=0)

    # Output tile count = input tile count (one output per A,B pair)
    output_tile_cnt = tile_cnt_A

    # Tile counts: srcA has 2x tiles (A tiles + B tiles), srcB not used
    combined_tile_cnt = tile_cnt_A * 2

    # Dimensions for combined srcA (doubled in first dimension)
    srcA_dimensions = [input_dimensions[0] * 2, input_dimensions[1]]

    # Determine unpack_to_dest based on format
    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.No
    )

    configuration = ProfilerConfig(
        "sources/eltwise_max_sfpu_perf.cpp",
        formats,
        run_types=[
            PerfRunType.L1_TO_L1,
            PerfRunType.UNPACK_ISOLATE,
            PerfRunType.MATH_ISOLATE,
            PerfRunType.PACK_ISOLATE,
            PerfRunType.L1_CONGESTION,
        ],
        templates=[
            INPUT_DIMENSIONS(srcA_dimensions, input_dimensions),
        ],
        runtimes=[
            TILE_COUNT(combined_tile_cnt),
            LOOP_FACTOR(loop_factor),
            NUM_FACES(num_faces=4),
            UNPACK_TRANS_FACES(Transpose.No),
            UNPACK_TRANS_WITHING_FACE(Transpose.No),
        ],
        variant_stimuli=StimuliConfig(
            src_A_combined,
            formats.input_format,
            None,  # srcB not used, tiles are combined in srcA
            formats.input_format,
            formats.output_format,
            tile_count_A=combined_tile_cnt,
            tile_count_B=0,
            tile_count_res=combined_tile_cnt,  # Reserve space for all tiles, though only num_pairs are written
        ),
        unpack_to_dest=unpack_to_dest,
        dest_acc=dest_acc,
    )

    configuration.run(perf_report, location=workers_tensix_coordinates)
