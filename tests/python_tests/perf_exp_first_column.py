# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import pytest
from helpers.format_config import DataFormat
from helpers.llk_params import (
    ApproximationMode,
    DestAccumulation,
    PerfRunType,
    Transpose,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.perf import PerfConfig
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import calculate_tile_and_face_counts
from helpers.test_variant_parameters import (
    INPUT_DIMENSIONS,
    LOOP_FACTOR,
    NUM_FACES,
    SDPA_EXP_APPROX_MODE,
    TILE_COUNT,
    UNPACK_TRANS_FACES,
    UNPACK_TRANS_WITHIN_FACE,
)


@pytest.mark.perf
@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float32,
            DataFormat.Float16,
            DataFormat.Float16_b,
            DataFormat.Bfp8_b,
        ]
    ),
    sdpa_exp_approx_mode=[
        ApproximationMode.Yes,
        ApproximationMode.No,
    ],
    dest_acc=[
        DestAccumulation.Yes,
        DestAccumulation.No,
    ],
    loop_factor=[
        16,
    ],  # Number of iterations to run the test in order to minimize profiler overhead in measurement
    input_dimensions=[
        [128, 64],  # tile_cnt: 8
        [256, 64],  # tile_cnt: 16
    ],  # Specifying different input sizes to cover different tile counts
)
def test_perf_exp_first_column(
    perf_report,
    formats,
    sdpa_exp_approx_mode,
    dest_acc,
    loop_factor,
    input_dimensions,
    workers_tensix_coordinates,
):
    """
    Performance test for exp_tile_first_column operation used in SDPA.

    This test measures the performance of computing exponential on only the first 8 columns
    of each face in a tile, which is used in the SDPA attention mechanism.

    The SDPA_EXP_APPROX_MODE parameter controls whether to use:
    - True: Piecewise approximation (_calculate_exponential_piecewise_)
    - False: Polynomial approximation (polynomial degree 2 for bf16, 4 for fp32)
    """

    # Calculate tile count from input dimensions
    tile_count_A, tile_count_B, faces_to_generate = calculate_tile_and_face_counts(
        input_dimensions, input_dimensions, face_r_dim=16, num_faces=4
    )

    # If dest_acc is on, we unpack Float32 into 16-bit format in src registers
    # (later copied over in dest reg for SFPU op)
    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.No
    )

    configuration = PerfConfig(
        "sources/exp_first_column_perf.cpp",
        formats,
        run_types=[
            PerfRunType.L1_TO_L1,
            PerfRunType.UNPACK_ISOLATE,
            PerfRunType.MATH_ISOLATE,
            PerfRunType.PACK_ISOLATE,
            PerfRunType.L1_CONGESTION,
        ],
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
            SDPA_EXP_APPROX_MODE(sdpa_exp_approx_mode),
        ],
        runtimes=[
            TILE_COUNT(tile_count_A),
            LOOP_FACTOR(loop_factor),
            NUM_FACES(num_faces=faces_to_generate),
            UNPACK_TRANS_FACES(Transpose.No),
            UNPACK_TRANS_WITHIN_FACE(Transpose.No),
        ],
        variant_stimuli=StimuliConfig(
            None,
            formats.input_format,
            None,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_count_A,
            tile_count_B=tile_count_B,
            tile_count_res=tile_count_A,
        ),
        unpack_to_dest=unpack_to_dest,
        dest_acc=dest_acc,
    )

    configuration.run(perf_report, location=workers_tensix_coordinates)
