# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
from helpers.format_config import DataFormat
from helpers.llk_params import (
    ApproximationMode,
    DestAccumulation,
    MathFidelity,
    MathOperation,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.perf import SFPU_ALL_RUN_TYPES, perf_benchmark, update_report
from helpers.stimuli_generator import calculate_tile_and_face_counts

FACE_R_DIM = 16
NUM_FACES = 4


@pytest.mark.perf
@parametrize(
    test_name="matmul_and_unary_sfpu_perf",
    formats=input_output_formats(
        [
            DataFormat.Float16,
            # DataFormat.Float16_b,
            # DataFormat.Float32,
            # DataFormat.Bfp8_b,
        ],
        same=True,
    ),
    mathop=[
        MathOperation.Abs,
        # MathOperation.Celu,
        # MathOperation.Cos,
        # MathOperation.Gelu,
        # MathOperation.Hardsigmoid,
        # MathOperation.Log,
        # MathOperation.Reciprocal,
        # MathOperation.Silu,
        # MathOperation.Sin,
        # MathOperation.Sqrt,
        # MathOperation.Square,
    ],
    approx_mode=[
        ApproximationMode.No,
        # ApproximationMode.Yes
    ],
    dest_acc=[
        DestAccumulation.No
        # DestAccumulation.Yes,
    ],
    math_fidelity=[
        MathFidelity.LoFi,
        # MathFidelity.HiFi2, TODO: FIND OUT WHY
        # MathFidelity.HiFi3,
        # MathFidelity.HiFi4,
    ],
    loop_factor=[1],
    face_r_dim=[FACE_R_DIM],
    num_faces=[NUM_FACES],
    input_dimensions=[
        [32, 32],  # tile_cnt: 1
        # [64, 64],   # tile_cnt: 4
        # [128, 128], # tile_cnt: 16
        # [160, 160], # tile_cnt: 25
        # [256, 256], # tile_cnt: 64
    ],  # Specifying different input sizes to cover different tile counts
    run_types=[SFPU_ALL_RUN_TYPES],
)
def test_perf_matmul_and_unary_sfpu(
    perf_report,
    test_name,
    formats,
    mathop,
    approx_mode,
    dest_acc,
    math_fidelity,
    loop_factor,
    face_r_dim,
    num_faces,
    input_dimensions,
    run_types,
):
    """
    Performance test for combined matmul and unary SFPU operations.

    This test performs:
    1. Matrix multiplication (1 tile x 1 tile)
    2. Followed by a unary SFPU operation on the result

    This pattern is common in neural network operations where matmul
    is followed by activation functions.
    """

    # TODO: Fix and enable unpack to dest
    unpack_to_dest = (
        formats.input_format.is_32_bit()
        and dest_acc
        == DestAccumulation.Yes  # If dest_acc is off, we unpack Float32 into 16-bit format in src registers (later copied over in dest reg for SFPU op)
    )

    tile_cnt, faces_to_generate = calculate_tile_and_face_counts(
        input_dimensions, face_r_dim, num_faces
    )

    test_config = {
        "testname": test_name,
        "mathop": mathop,
        "formats": formats,
        "approx_mode": approx_mode,
        "dest_acc": dest_acc,
        "math_fidelity": math_fidelity,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "unpack_to_dest": unpack_to_dest,
        "num_faces": faces_to_generate,
        "face_r_dim": face_r_dim,
        "tile_cnt": tile_cnt,
        "loop_factor": loop_factor,
        "L1_to_L1_iterations": 2,  # This is a fused test does two runs of L1-L1, result tensor from first run (matmul) is used as input for second run (sfpu operation)
    }

    results = perf_benchmark(test_config, run_types)
    update_report(perf_report, test_config, results)
