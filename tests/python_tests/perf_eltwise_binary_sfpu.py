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
from helpers.format_config import DataFormat
from helpers.llk_params import (
    ApproximationMode,
    BlockMode,
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
    test_name="eltwise_binary_sfpu_perf",
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
    block_mode=[
        BlockMode.Yes,
        BlockMode.No,
    ],
    mathop=[
        MathOperation.SfpuElwadd,
        MathOperation.SfpuElwsub,
        MathOperation.SfpuElwmul,
        MathOperation.SfpuElwdiv,
        MathOperation.SfpuElwrsub,
        MathOperation.SfpuElwpow,
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
    ],
    face_r_dim=[FACE_R_DIM],
    num_faces=[NUM_FACES],
    input_dimensions=[
        [128, 64],  # tile_cnt: 8
    ],  # Specifying different input sizes to cover different tile counts
    run_types=[ALL_RUN_TYPES],
)
def test_perf_eltwise_binary_sfpu_float(
    perf_report,
    test_name,
    formats,
    mathop,
    approx_mode,
    block_mode,
    dest_acc,
    loop_factor,
    iterations,
    face_r_dim,
    num_faces,
    input_dimensions,
    run_types,
):
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
        "block_mode": block_mode,
        "unpack_to_dest": unpack_to_dest,
        "num_faces": faces_to_generate,
        "face_r_dim": face_r_dim,
        "tile_cnt": tile_cnt,
        "loop_factor": loop_factor,
        "iterations": iterations,
    }

    results = perf_benchmark(test_config, run_types)
    update_report(perf_report, test_config, results)


@pytest.mark.perf
@parametrize(
    test_name="eltwise_binary_sfpu_perf",
    formats=input_output_formats(
        [
            DataFormat.Int32,
        ]
    ),
    approx_mode=[
        ApproximationMode.Yes,
        ApproximationMode.No,
    ],
    block_mode=[
        BlockMode.Yes,
        BlockMode.No,
    ],
    mathop=[
        MathOperation.SfpuElwRightShift,
        MathOperation.SfpuElwLeftShift,
        MathOperation.SfpuElwLogicalRightShift,
    ],
    dest_acc=[
        DestAccumulation.Yes,
        DestAccumulation.No,
    ],
    loop_factor=[
        16,
    ],
    iterations=[
        8,
        32,
    ],
    face_r_dim=[FACE_R_DIM],
    num_faces=[NUM_FACES],
    input_dimensions=[
        [128, 64],  # tile_cnt: 8
    ],
    run_types=[ALL_RUN_TYPES],
)
def test_perf_eltwise_binary_sfpu_int(
    perf_report,
    test_name,
    formats,
    mathop,
    approx_mode,
    block_mode,
    dest_acc,
    loop_factor,
    iterations,
    face_r_dim,
    num_faces,
    input_dimensions,
    run_types,
):
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
        "block_mode": block_mode,
        "unpack_to_dest": unpack_to_dest,
        "num_faces": faces_to_generate,
        "face_r_dim": face_r_dim,
        "tile_cnt": tile_cnt,
        "loop_factor": loop_factor,
        "iterations": iterations,
    }

    results = perf_benchmark(test_config, run_types)
    update_report(perf_report, test_config, results)


@pytest.mark.perf
@parametrize(
    test_name="eltwise_binary_sfpu_perf",
    formats=input_output_formats(
        [
            DataFormat.Float32,
            DataFormat.Int32,
            DataFormat.UInt32,
        ],
        same=True,
    ),
    approx_mode=[
        ApproximationMode.Yes,
        ApproximationMode.No,
    ],
    block_mode=[
        BlockMode.Yes,
        BlockMode.No,
    ],
    mathop=[
        MathOperation.SfpuAddTopRow,
    ],
    dest_acc=[
        DestAccumulation.Yes,
        DestAccumulation.No,
    ],
    loop_factor=[
        16,
    ],
    iterations=[
        8,
        32,
    ],
    face_r_dim=[FACE_R_DIM],
    num_faces=[NUM_FACES],
    input_dimensions=[
        [128, 64],  # tile_cnt: 8
    ],
    run_types=[ALL_RUN_TYPES],
)
def test_perf_eltwise_binary_sfpu_add_top_row(
    perf_report,
    test_name,
    formats,
    mathop,
    approx_mode,
    block_mode,
    dest_acc,
    loop_factor,
    iterations,
    face_r_dim,
    num_faces,
    input_dimensions,
    run_types,
):
    chip_arch = get_chip_architecture()

    # Skip DestAccumulation.No on Blackhole for SfpuAddTopRow
    if chip_arch == ChipArchitecture.BLACKHOLE and dest_acc == DestAccumulation.No:
        pytest.skip(
            "DestAccumulation.No is not supported for SfpuAddTopRow on Blackhole"
        )

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
        "block_mode": block_mode,
        "unpack_to_dest": unpack_to_dest,
        "num_faces": faces_to_generate,
        "face_r_dim": face_r_dim,
        "tile_cnt": tile_cnt,
        "loop_factor": loop_factor,
        "iterations": iterations,
    }

    results = perf_benchmark(test_config, run_types)
    update_report(perf_report, test_config, results)
