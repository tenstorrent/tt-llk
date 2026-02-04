# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import math

import torch
from helpers.format_config import DataFormat, is_dest_acc_needed
from helpers.golden_generators import ReduceGolden, get_golden_generator
from helpers.llk_params import (
    DestAccumulation,
    DestSync,
    MathFidelity,
    MathOperation,
    ReduceDimension,
    ReducePool,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    IN_FACE_DIMS,
    IN_TILE_DIMS,
    INPUT_TILE_CNT,
    MATH_FIDELITY,
    MATH_OP,
    MAX_TILES_IN_DEST,
    NUM_FACES,
    NUM_FACES_C_DIM,
    NUM_FACES_R_DIM,
    OUTPUT_TILE_CNT,
    REDUCE_TO_ONE,
)
from helpers.tile_shape import construct_tile_shape
from helpers.utils import passed_test

# Helper dictionary to map reduce dimensions to math operations
mathop_mapping = {
    ReduceDimension.Row: MathOperation.ReduceRow,
    ReduceDimension.Column: MathOperation.ReduceColumn,
    ReduceDimension.Scalar: MathOperation.ReduceScalar,
}


@parametrize(
    input_dimensions=[[512, 32]],
    tile_dimensions=[[1, 32], [2, 32], [4, 32], [8, 32], [16, 32], [32, 32]],
    formats=input_output_formats(
        [
            DataFormat.Float32,
            DataFormat.Float16_b,
            DataFormat.Bfp8_b,
        ]
    ),
    is_reduce_to_one=[False, True],
    reduce_dim=[ReduceDimension.Row, ReduceDimension.Column, ReduceDimension.Scalar],
    pool_type=[ReducePool.Max, ReducePool.Average, ReducePool.Sum],
    math_fidelity=[
        # MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
)
def test_reduce(
    input_dimensions,
    formats,
    reduce_dim,
    pool_type,
    is_reduce_to_one,
    math_fidelity,
    workers_tensix_coordinates,
    tile_dimensions,
):

    tile_shape = construct_tile_shape(tile_dimensions)

    if is_reduce_to_one:
        # Accumulating large tensors into a single value can lead to significant numerical errors
        # especially in lower precision formats. Individual values can fall between their rtol, atol limits
        # but their accumulated sum may exceed these limits.
        # To mitigate this, the test is using smaller tensors for accumulation when reducing to one value.
        input_dimensions = [32, 32]

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=[32, 32],
        face_r_dim=tile_shape.face_r_dim,
        num_faces=tile_shape.total_num_faces(),
        tile_dimensions=tile_dimensions,
    )

    if pool_type in [
        ReducePool.Max,
        ReducePool.Sum,
    ]:  # result in srcA should be divided by 1
        src_B = torch.full((1024,), 1)
    else:
        # reduce average divides by length of elements in array we reduce
        if reduce_dim == ReduceDimension.Row:
            src_B = torch.full((1024,), 1 / tile_dimensions[1])
        elif reduce_dim == ReduceDimension.Column:
            src_B = torch.full((1024,), 1 / tile_dimensions[0])
        else:  # Scalar
            src_B = torch.full(
                (1024,), 1 / math.sqrt(tile_dimensions[0] * tile_dimensions[1])
            )

    generate_golden = get_golden_generator(ReduceGolden)
    golden_tensor = generate_golden(
        src_A,
        reduce_dim,
        pool_type,
        formats.output_format,
        tile_cnt_A,
        reduce_to_one=is_reduce_to_one,
        tile_shape=tile_shape,
    )

    dest_acc = (
        DestAccumulation.Yes
        if (formats.input_format.is_32_bit() or is_dest_acc_needed(formats))
        else DestAccumulation.No
    )

    output_tile_count = 1 if is_reduce_to_one else tile_cnt_A

    DEST_SYNC_TILE_LIMITS = {
        DestSync.Half: 8,
        DestSync.Full: 16,
    }

    capacity_divisor = 2 if dest_acc == DestAccumulation.Yes else 1
    max_tiles_in_dest = DEST_SYNC_TILE_LIMITS[DestSync.Half] // capacity_divisor
    configuration = TestConfig(
        "sources/reduce_test.cpp",
        formats,
        templates=[
            MATH_OP(mathop=mathop_mapping[reduce_dim], pool_type=pool_type),
            MATH_FIDELITY(math_fidelity),
        ],
        runtimes=[
            IN_FACE_DIMS(tile_shape.face_r_dim, tile_shape.face_c_dim),
            IN_TILE_DIMS(tile_dimensions[0], tile_dimensions[1]),
            INPUT_TILE_CNT(tile_cnt_A),
            OUTPUT_TILE_CNT(output_tile_count),
            MAX_TILES_IN_DEST(max_tiles_in_dest),
            REDUCE_TO_ONE(is_reduce_to_one),
            NUM_FACES(num_faces_A=tile_shape.total_num_faces(), num_faces_B=4),
            NUM_FACES_R_DIM(tile_shape.num_faces_r_dim),
            NUM_FACES_C_DIM(tile_shape.num_faces_c_dim),
        ],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=output_tile_count,
            num_faces=tile_shape.total_num_faces(),
            face_r_dim=tile_shape.face_r_dim,
            tile_dimensions=tile_dimensions,
        ),
        dest_acc=dest_acc,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golder tensor are not of the same length"

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    if is_reduce_to_one and formats.output_format == DataFormat.Bfp8_b:
        # For BFP8, allow for larger tolerance due to reduced precision
        assert passed_test(
            golden_tensor,
            res_tensor,
            formats.output_format,
            L1_to_L1_iterations=tile_cnt_A,
        ), "Assert against golden failed"
    else:
        assert passed_test(
            golden_tensor, res_tensor, formats.output_format
        ), "Assert against golden failed"
