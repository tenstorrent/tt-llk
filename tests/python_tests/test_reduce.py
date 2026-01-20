# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
from helpers.format_config import DataFormat
from helpers.golden_generators import ReduceGolden, get_golden_generator
from helpers.llk_params import (
    DestAccumulation,
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
    INPUT_TILE_COUNT,
    MATH_OP,
    NUM_FACES,
    OUTPUT_TILE_COUNT,
    TEST_FACE_DIMS,
)
from helpers.utils import passed_test

# Helper dictionary to map reduce dimensions to math operations
mathop_mapping = {
    ReduceDimension.Row: MathOperation.ReduceRow,
    ReduceDimension.Column: MathOperation.ReduceColumn,
    ReduceDimension.Scalar: MathOperation.ReduceScalar,
}


@parametrize(
    input_dimensions=[[32, 32], [32, 64]],
    formats=input_output_formats(
        [
            DataFormat.Float32,
            # DataFormat.Float16,
            # DataFormat.Float32,
            # DataFormat.Int32
        ]
    ),
    dest_acc=[DestAccumulation.Yes],
    reduce_dim=[ReduceDimension.Column],
    pool_type=[ReducePool.Max],
)
def test_reduce(
    input_dimensions,
    formats,
    dest_acc,
    reduce_dim,
    pool_type,
    workers_tensix_coordinates,
):

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=[32, 32],
        sequential_A=True,
    )
    if pool_type in [
        ReducePool.Max,
        ReducePool.Sum,
    ]:  # result in srcA should be divided by 1
        src_B = torch.full((1024,), 1)
    else:
        # reduce average divides by length of elements in array we reduce
        if reduce_dim == ReduceDimension.Row:
            src_B = torch.full((1024,), 1 / input_dimensions[1])
        elif reduce_dim == ReduceDimension.Column:
            src_B = torch.full((1024,), 1 / input_dimensions[0])
        else:
            src_B = torch.full((1024,), 1 / (input_dimensions[1] * input_dimensions[0]))

    generate_golden = get_golden_generator(ReduceGolden)
    golden_tensor = generate_golden(
        src_A, reduce_dim, pool_type, formats.output_format, tile_cnt_A
    )

    configuration = TestConfig(
        "sources/reduce_test.cpp",
        formats,
        templates=[MATH_OP(mathop=mathop_mapping[reduce_dim], pool_type=pool_type)],
        runtimes=[
            TEST_FACE_DIMS(),
            INPUT_TILE_COUNT(tile_cnt_A),
            OUTPUT_TILE_COUNT(tile_cnt_A),
            NUM_FACES(4),
        ],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
        ),
        dest_acc=dest_acc,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golder tensor are not of the same length"

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format, print_erros=True
    ), "Assert against golden failed"
