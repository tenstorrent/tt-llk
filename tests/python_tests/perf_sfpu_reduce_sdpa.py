# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
from helpers.format_config import DataFormat
from helpers.llk_params import (
    DestAccumulation,
    MathOperation,
    ReducePool,
)
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.perf import (
    PerfRunType,
    perf_benchmark,
    update_report,
)
from helpers.stimuli_generator import calculate_tile_and_face_counts

FACE_R_DIM = 16
NUM_FACES = 4


@pytest.mark.perf
@parametrize(
    test_name="sfpu_reduce_sdpa_perf",
    formats=input_output_formats(
        [DataFormat.Float16_b],  # Only Float16_b is supported for SDPA reduce
        same=True,
    ),
    dest_acc=[DestAccumulation.No],
    mathop=[MathOperation.ReduceColumn],
    reduce_pool=[ReducePool.Max],  # Only MAX is supported for SDPA reduce
    loop_factor=[
        1,
        2,
        4,
        8,
    ],
    face_r_dim=[FACE_R_DIM],
    num_faces=[NUM_FACES],
    input_dimensions=[
        [32, 32],  # tile_cnt = 1
        [128, 64],  # tile_cnt = 8
    ],
    run_types=[
        [PerfRunType.L1_TO_L1],
        [PerfRunType.UNPACK_ISOLATE],
        [PerfRunType.MATH_ISOLATE],
        [PerfRunType.PACK_ISOLATE],
        [PerfRunType.L1_CONGESTION],
    ],
)
def test_perf_sfpu_reduce_sdpa(
    perf_report,
    test_name,
    formats,
    dest_acc,
    mathop,
    reduce_pool,
    loop_factor,
    face_r_dim,
    num_faces,
    input_dimensions,
    run_types,
):
    """
    Performance test for SFPU reduce SDPA operation.

    This test specifically measures the performance of the SFPU reduce operation
    used in SDPA (Scaled Dot-Product Attention) implementations. It focuses on
    measuring cycles spent in the SFPU calculations, not including memory operations.

    The test uses a 128x32 input dimension (4 tiles) and performs column-wise
    max reduction, which is the typical operation in SDPA softmax computation.
    """

    unpack_to_dest = (
        formats.input_format.is_32_bit()
        and dest_acc
        == DestAccumulation.Yes  # If dest_acc is off, we unpack Float32 into 16-bit format in src registers (later copied over in dest reg for SFPU op)
    )

    tile_count, faces_to_generate = calculate_tile_and_face_counts(
        input_dimensions, face_r_dim, num_faces
    )
    test_config = {
        "testname": test_name,
        "tile_cnt": tile_count,
        "formats": formats,
        "dest_acc": dest_acc,
        "pool_type": reduce_pool,
        "mathop": mathop,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "unpack_to_dest": unpack_to_dest,
        "num_faces": faces_to_generate,
        "face_r_dim": face_r_dim,
        "tile_cnt": tile_count,
        "loop_factor": loop_factor,
    }

    # Run performance benchmarks focusing on MATH_ISOLATE to measure SFPU cycles
    # MATH_ISOLATE measures only the math operation cycles, excluding unpack/pack
    # This specifically measures the _calculate_reduce_sdpa_ function cycles
    results = perf_benchmark(test_config, run_types)

    update_report(perf_report, test_config, results)
