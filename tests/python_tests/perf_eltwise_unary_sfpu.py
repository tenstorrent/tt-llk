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
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.llk_params import (
    ApproximationMode,
    DestAccumulation,
    MathOperation,
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
            DataFormat.Float16,
            DataFormat.Float16_b,
            DataFormat.Bfp8_b,
        ]
    ),
    approx_mode=[
        ApproximationMode.Yes,
        ApproximationMode.No,
    ],
    mathop=[
        MathOperation.Abs,
        MathOperation.Atanh,
        MathOperation.Asinh,
        MathOperation.Acosh,
        MathOperation.Cos,
        MathOperation.Log,
        MathOperation.Reciprocal,
        MathOperation.Sin,
        MathOperation.Sqrt,
        MathOperation.Rsqrt,
        MathOperation.Square,
        MathOperation.Celu,
        MathOperation.Silu,
        MathOperation.Gelu,
        MathOperation.Neg,
        MathOperation.Fill,
        MathOperation.Elu,
        MathOperation.Exp,
        MathOperation.Exp2,
        MathOperation.Hardsigmoid,
        MathOperation.Threshold,
        MathOperation.ReluMax,
        MathOperation.ReluMin,
    ],
    dest_acc=[
        DestAccumulation.No,
        # DestAccumulation.Yes TODO: fix this case
    ],
    loop_factor=[
        # 1,
        # 2,
        # 4,
        # 8,
        16,
        # 32,
        # 64,
        # 128,
    ],  # Number of iterations to run the test in order to minimize measurement noise
    face_r_dim=[FACE_R_DIM],
    num_faces=[NUM_FACES],
    input_dimensions=[
        # [32, 32],  # tile_cnt: 1
        # [64, 32],  # tile_cnt: 2
        # [64, 64],  # tile_cnt: 4
        [128, 64],  # tile_cnt: 8
        # [128, 128],  # tile_cnt: 16
        # [256, 256],  # tile_cnt: 64
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
    face_r_dim,
    num_faces,
    input_dimensions,
    run_types,
):
    arch = get_chip_architecture()

    if dest_acc == DestAccumulation.No and arch == ChipArchitecture.BLACKHOLE:
        if formats.input_format == DataFormat.Float16 or formats == InputOutputFormat(
            DataFormat.Float32, DataFormat.Float16
        ):
            pytest.skip(reason="This combination is not supported on BH architecture")

    if (
        approx_mode == ApproximationMode.Yes
        and mathop in [MathOperation.Exp, MathOperation.Exp2, MathOperation.Elu]
        and (
            formats.input_format == DataFormat.Bfp8_b
            or formats.output_format == DataFormat.Bfp8_b
        )
    ):
        pytest.skip(
            reason="Exp-related operations are not supported for bf8_b format in approximation mode."
        )

    # If dest_acc is off, we unpack Float32 into 16-bit format in src registers
    # (later copied over in dest reg for SFPU op)
    #
    # TODO: Fix and enable unpack to dest
    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
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
    }

    results = perf_benchmark(test_config, run_types)
    update_report(perf_report, test_config, results)
