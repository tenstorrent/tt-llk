# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
All commented out formats, mathop options, loop factors, and input dimensions
can be uncommented to expand the test coverage as needed.

Uncommenting all options may lead to a large number of test cases being generated,
which could increase the total test execution time significantly.

Therefore, it's recommended to selectively enable options based on specific testing requirements.
"""

import pytest
from helpers.format_config import DataFormat
from helpers.llk_params import (
    ApproximationMode,
    DestAccumulation,
    FastMode,
    MathOperation,
    StableSort,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.perf import ALL_RUN_TYPES, perf_benchmark, update_report
from helpers.stimuli_generator import calculate_tile_and_face_counts

FACE_R_DIM = 16
NUM_FACES = 4


@pytest.mark.perf
@parametrize(
    test_name="eltwise_unary_sfpu_perf",
    formats=input_output_formats(
        [
            DataFormat.Float32,
            # DataFormat.Float16,
            # DataFormat.Float16_b,
            # DataFormat.Bfp8_b,
        ]
    ),
    approx_mode=[
        ApproximationMode.Yes,
        ApproximationMode.No,
    ],
    mathop=[
        # MathOperation.Abs,
        # MathOperation.Atanh,
        # MathOperation.Asinh,
        # MathOperation.Acosh,
        # MathOperation.Cos,
        # MathOperation.Log,
        MathOperation.Reciprocal,
        # MathOperation.Sin,
        MathOperation.Sqrt,
        # MathOperation.Rsqrt,
        # MathOperation.Square,
        # MathOperation.Celu,
        MathOperation.Silu,
        MathOperation.Gelu,
        # MathOperation.Neg,
        # MathOperation.Fill,
        # MathOperation.Elu,
        MathOperation.Exp,
        # MathOperation.Exp2,
        # MathOperation.Hardsigmoid,
        # MathOperation.Threshold,
        # MathOperation.ReluMax,
        # MathOperation.ReluMin,
        MathOperation.TopKLocalSort,
        MathOperation.TopKMerge,
        MathOperation.TopKRebuild,
    ],
    dest_acc=[
        DestAccumulation.Yes,
        DestAccumulation.No,
    ],
    loop_factor=[
        16,
    ],  # Number of iterations to run the test in order to minimize profiler overhead in measurement
    iterations=[
        8,
        32,
    ],  # Number of SFPU iterations
    fast_mode=[
        FastMode.Yes,
        FastMode.No,
    ],
    stable_sort=[
        StableSort.Yes,
        StableSort.No,
    ],
    face_r_dim=[FACE_R_DIM],
    num_faces=[NUM_FACES],
    input_dimensions=[
        [128, 64],  # tile_cnt: 8
    ],  # Specifying different input sizes to cover different tile counts
    run_types=[ALL_RUN_TYPES],
)
def test_perf_eltwise_unary_sfpu(
    perf_report,
    test_name,
    formats,
    mathop,
    approx_mode,
    dest_acc,
    loop_factor,
    iterations,
    fast_mode,
    stable_sort,
    face_r_dim,
    num_faces,
    input_dimensions,
    run_types,
):
    # If dest_acc is on, we unpack Float32 into 16-bit format in src registers
    # (later copied over in dest reg for SFPU op)
    #
    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.No
    )

    tile_cnt, faces_to_generate = calculate_tile_and_face_counts(
        input_dimensions, face_r_dim, num_faces
    )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "mathop": mathop,
        "approx_mode": approx_mode,
        "unpack_to_dest": unpack_to_dest,
        "num_faces": faces_to_generate,
        "face_r_dim": face_r_dim,
        "tile_cnt": tile_cnt,
        "loop_factor": loop_factor,
        "iterations": iterations,
        "fast_mode": fast_mode,
        "stable_sort": stable_sort,
    }

    results = perf_benchmark(test_config, run_types)
    update_report(perf_report, test_config, results)
