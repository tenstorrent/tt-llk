# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import List

import pytest
from helpers.format_config import DataFormat, FormatConfig, is_dest_acc_needed
from helpers.llk_params import DestAccumulation, MathFidelity
from helpers.matmul_sweep import (
    generate_matmul_dimension_combinations,
    generate_tile_dims,
)
from helpers.param_config import parametrize
from helpers.perf import PerfRunType, perf_benchmark, update_report

# Important K dimensions to test
KT_DIMS = [8]


def generate_matmul_dimension_combinations_simple(max_tiles: int, kt_dims):
    """
    Generate simplified matmul dimension pairs with fixed rt_dim=1 (mt_dim=1).
    Sweeps over kt_dim and all possible ct_dim (nt_dim) values.

    Args:
        max_tiles: Maximum size of result matrix in tiles (M×N tiles)
        kt_dims: K dimension sizes to test (in tiles)

    Returns:
        List[(A_dims, B_dims)] where A_dims=[32, K], B_dims=[K, N]
        with M fixed at 32 (1 tile) and N varying from 32 to max_tiles*32
    """
    TILE_DIM = 32  # Standard tile dimension for row and column
    mt_dim = 1  # Fixed row dimension

    return [
        ([mt_dim * TILE_DIM, kt_dim * TILE_DIM], [kt_dim * TILE_DIM, nt_dim * TILE_DIM])
        for kt_dim in kt_dims
        for nt_dim in range(1, max_tiles + 1)
    ]


def matmul_combos(
    formats: List[FormatConfig],
    dest_acc: List[DestAccumulation],
):
    def _dest_bank_max_tiles(format: FormatConfig, dest_acc: DestAccumulation):
        if is_dest_acc_needed(format) or dest_acc == DestAccumulation.Yes:
            return 4
        return 8

    unique_max_tiles = set(
        _dest_bank_max_tiles(fmt, acc) for fmt in formats for acc in dest_acc
    )
    dimensions = {
        max_tiles: generate_matmul_dimension_combinations(max_tiles, kt_dims=KT_DIMS)
        for max_tiles in unique_max_tiles
    }

    return [
        (format, accumulation, dims)
        for format in formats
        for accumulation in dest_acc
        for dims in dimensions[_dest_bank_max_tiles(format, accumulation)]
    ]


@pytest.mark.perf
@parametrize(
    test_name="matmul_perf",
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    kt_dim=KT_DIMS,
    dimension_combo=lambda dest_acc, kt_dim: generate_matmul_dimension_combinations_simple(
        max_tiles=4 if dest_acc == DestAccumulation.Yes else 8, kt_dims=[kt_dim]
    ),
)
def test_perf_matmul_simple(perf_report, test_name, dest_acc, kt_dim, dimension_combo):
    """
    Simplified matmul performance test with fixed format (Float16_b) and LoFi fidelity.
    Fixed rt_dim=1, sweeps over dest_acc (No/Yes), kt_dim, and all possible ct_dim values.
    Tests different formats for unpack A (Float16_b) and unpack B (Bfp8_b).
    """
    # Fixed parameters with different formats for A and B
    formats = FormatConfig(
        unpack_A_src=DataFormat.Float16_b,
        unpack_A_dst=DataFormat.Float16_b,
        unpack_B_src=DataFormat.Bfp4_b,
        unpack_B_dst=DataFormat.Bfp8_b,
        pack_src=DataFormat.Float16_b,
        pack_dst=DataFormat.Float16_b,
        math=DataFormat.Float16_b,
        same_src_format=False,
    )
    math_fidelity = MathFidelity.LoFi

    matrix_a, matrix_b = dimension_combo

    run_types = [
        PerfRunType.L1_TO_L1,
        PerfRunType.UNPACK_ISOLATE,
        PerfRunType.MATH_ISOLATE,
        PerfRunType.PACK_ISOLATE,
        PerfRunType.L1_CONGESTION,
    ]

    # Calculate all matmul dimensions using helper function
    dims = generate_tile_dims((matrix_a, matrix_b))

    test_config = {
        "formats": formats,
        "testname": test_name,
        "loop_factor": 16,
        "tile_cnt": dims.rt_dim * dims.ct_dim * dims.kt_dim,
        "input_A_dimensions": matrix_a,
        "input_B_dimensions": matrix_b,
        "output_dimensions": dims.output_dimensions,
        "rt_dim": dims.rt_dim,
        "ct_dim": dims.ct_dim,
        "kt_dim": dims.kt_dim,
        "dest_acc": dest_acc,
        "math_fidelity": math_fidelity,
    }

    results = perf_benchmark(test_config, run_types)
    update_report(perf_report, test_config, results)


# @pytest.mark.perf
# @parametrize(
#     test_name="matmul_perf",
#     combos=matmul_combos(
#         formats=input_output_formats(
#             [
#                 DataFormat.Float16_b,
#                 DataFormat.Float16,
#                 DataFormat.Float32,
#                 DataFormat.Bfp8_b,
#             ]
#         ),
#         dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
#     ),
#     math_fidelity=[
#         MathFidelity.LoFi,
#         MathFidelity.HiFi2,
#         MathFidelity.HiFi3,
#         MathFidelity.HiFi4,
#     ],
# )
# def test_perf_matmul(perf_report, test_name, combos, math_fidelity):

#     formats, dest_acc, (matrix_a, matrix_b) = combos

#     print (formats)
#     print (type(formats))

#     if is_dest_acc_needed(formats) and dest_acc == DestAccumulation.No:
#         pytest.skip("Dest accumulation must be enabled for this format")

#     run_types = [
#         PerfRunType.L1_TO_L1,
#         PerfRunType.UNPACK_ISOLATE,
#         PerfRunType.MATH_ISOLATE,
#         PerfRunType.PACK_ISOLATE,
#         PerfRunType.L1_CONGESTION,
#     ]

#     # Calculate all matmul dimensions using helper function
#     dims = generate_tile_dims((matrix_a, matrix_b))

#     test_config = {
#         "formats": formats,
#         "testname": test_name,
#         "loop_factor": 16,
#         "tile_cnt": dims.rt_dim * dims.ct_dim * dims.kt_dim,
#         "input_A_dimensions": matrix_a,
#         "input_B_dimensions": matrix_b,
#         "output_dimensions": dims.output_dimensions,
#         "rt_dim": dims.rt_dim,
#         "ct_dim": dims.ct_dim,
#         "kt_dim": dims.kt_dim,
#         "dest_acc": dest_acc,
#         "math_fidelity": math_fidelity,
#     }

#     results = perf_benchmark(test_config, run_types)
#     update_report(perf_report, test_config, results)
