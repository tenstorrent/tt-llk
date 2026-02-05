# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import List

import pytest
from helpers.format_config import DataFormat, FormatConfig, is_dest_acc_needed
from helpers.llk_params import DestAccumulation, MathFidelity, PerfRunType, Transpose
from helpers.matmul_sweep import (
    generate_tile_dims,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.perf import PerfConfig
from helpers.stimuli_config import StimuliConfig
from helpers.test_variant_parameters import (
    CRK_TILE_DIMM,
    DEST_SYNC,
    INPUT_DIMENSIONS,
    LOOP_FACTOR,
    MATH_FIDELITY,
    NUM_FACES,
    THROTTLE_LEVEL,
    TILE_COUNT,
    UNPACK_TRANS_FACES,
)


def generate_format_aware_matmul_combinations(
    formats_list: List[FormatConfig],
    dest_acc_modes: List[DestAccumulation],
):
    """
    Generate matmul dimension combinations for multiple tiles.
    Matches the configuration from test_matmul.py exactly.

    Returns: List of (format, dest_acc, dimensions) tuples
    """
    combinations = []

    for fmt in formats_list:
        for dest_acc in dest_acc_modes:
            # Format: ([A_rows, A_cols], [B_rows, B_cols]) for matmul A @ B
            dimensions_list = [
                ([64, 128], [128, 128]),  # A: 64x128, B: 128x128 -> Output: 64x128
            ]
            combinations.extend([(fmt, dest_acc, dims) for dims in dimensions_list])

    return combinations


# Generate format-aware combinations - matches test_matmul.py
MATMUL_FORMATS = input_output_formats(
    [
        DataFormat.Float16_b,
    ]
)
DEST_ACC_MODES = [DestAccumulation.No]
ALL_MATMUL_COMBINATIONS = generate_format_aware_matmul_combinations(
    MATMUL_FORMATS, DEST_ACC_MODES
)


@pytest.mark.perf
@parametrize(
    math_fidelity=[
        MathFidelity.HiFi2,
    ],
    format_dest_acc_and_dims=ALL_MATMUL_COMBINATIONS,
)
def test_perf_matmul(
    perf_report, math_fidelity, format_dest_acc_and_dims, workers_tensix_coordinates
):

    formats = format_dest_acc_and_dims[0]
    dest_acc = format_dest_acc_and_dims[1]
    # dims should be a tuple of (A_dimensions, B_dimensions)
    # Each should be a list [rows, cols]
    input_A_dimensions, input_B_dimensions = format_dest_acc_and_dims[2]

    if is_dest_acc_needed(formats) and dest_acc == DestAccumulation.No:
        pytest.skip("Dest accumulation must be enabled for this format")

    run_types = [
        # PerfRunType.L1_TO_L1,
        # PerfRunType.UNPACK_ISOLATE,
        PerfRunType.MATH_ISOLATE,
        # PerfRunType.PACK_ISOLATE,
        # PerfRunType.L1_CONGESTION,
    ]

    # Calculate all matmul dimensions using helper function
    matmul_dims = generate_tile_dims((input_A_dimensions, input_B_dimensions))

    configuration = PerfConfig(
        "sources/matmul_perf.cpp",
        formats,
        run_types,
        templates=[
            INPUT_DIMENSIONS(input_A_dimensions, input_B_dimensions),
            MATH_FIDELITY(math_fidelity),
            DEST_SYNC(),
            THROTTLE_LEVEL(),
        ],
        runtimes=[
            UNPACK_TRANS_FACES(Transpose.No),
            NUM_FACES(),
            LOOP_FACTOR(16),
            TILE_COUNT(matmul_dims.output_tile_cnt),
            CRK_TILE_DIMM(matmul_dims.ct_dim, matmul_dims.rt_dim, matmul_dims.kt_dim),
        ],
        variant_stimuli=StimuliConfig(
            None,
            formats.input_format,
            None,
            formats.input_format,
            formats.output_format,
            tile_count_A=matmul_dims.tile_cnt_A,
            tile_count_B=matmul_dims.tile_cnt_B,
            tile_count_res=matmul_dims.output_tile_cnt,
        ),
        dest_acc=dest_acc,
    )

    configuration.run(perf_report, location=workers_tensix_coordinates)
