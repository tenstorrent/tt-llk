# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    ReduceGapoolGolden,
    ReduceGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    DestAccumulation,
    ImpliedMathFormat,
    MathFidelity,
    MathOperation,
    ReduceDimension,
    ReducePool,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.tilize_untilize import untilize_block
from helpers.utils import passed_test

# Helper dictionary to map reduce dimensions to math operations
mathop_mapping = {
    ReduceDimension.Row: MathOperation.ReduceRow,
    ReduceDimension.Column: MathOperation.ReduceColumn,
    ReduceDimension.Scalar: MathOperation.ReduceScalar,
}

MATH_FIDELITY_MODES = [
    MathFidelity.LoFi,
    MathFidelity.HiFi2,
    MathFidelity.HiFi3,
    MathFidelity.HiFi4,
]
POOL_TYPES = [ReducePool.Max, ReducePool.Sum, ReducePool.Average]


def generate_pool_type_and_math_fidelity_combinations():
    combinations = []
    for pool_type in POOL_TYPES:
        for math_fidelity in MATH_FIDELITY_MODES:
            # Math fidelity iterations work only for Sum and Average pool types
            # Max pool type does eltwise max, Sum and Average pool types do matmul
            if pool_type == ReducePool.Max and math_fidelity != MathFidelity.LoFi:
                continue
            combinations.append((pool_type, math_fidelity))
    return combinations


@pytest.mark.quasar
@parametrize(
    test_name="reduce_quasar_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float16,
        ],
    ),
    dest_acc=[DestAccumulation.No],
    reduce_dim=[ReduceDimension.Row, ReduceDimension.Column, ReduceDimension.Scalar],
    pool_type_and_math_fidelity=generate_pool_type_and_math_fidelity_combinations(),
)
def test_reduce_quasar(
    test_name, formats, dest_acc, reduce_dim, pool_type_and_math_fidelity
):

    pool_type, math_fidelity = pool_type_and_math_fidelity

    input_dimensions = [32, 32]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    src_A = torch.repeat_interleave(
        torch.arange(1, tile_cnt * 4 + 1, dtype=format_dict[formats.input_format]), 256
    )

    if pool_type in [
        ReducePool.Max,
        ReducePool.Sum,
    ]:  # result in srcA should be multiplied by 1
        src_B = torch.full((1024 * tile_cnt,), 1)
    else:
        # reduce average divides by length of elements in array we reduce
        src_B = torch.full((1024 * tile_cnt,), 1 / 32)

    if pool_type == ReducePool.Max:
        generate_golden = get_golden_generator(ReduceGolden)
        golden_tensor = generate_golden(
            src_A, reduce_dim, pool_type, formats.output_format
        )
    else:
        generate_golden = get_golden_generator(ReduceGapoolGolden)
        golden_tensor = generate_golden(
            src_A, src_B, formats.output_format, reduce_dim, math_fidelity, tile_cnt
        )

    mathop = mathop_mapping[reduce_dim]

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "reduce_dim": reduce_dim,
        "pool_type": pool_type,
        "mathop": mathop,
        "math_fidelity": math_fidelity,
        "implied_math_format": ImpliedMathFormat.Yes,
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt,
        tile_count_B=tile_cnt,
    )

    run_test(test_config)

    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])
    res_tensor = untilize_block(
        res_tensor, formats.output_format, input_dimensions
    ).flatten()

    print(f"res_tensor: {res_tensor}")
    print(f"golden_tensor: {golden_tensor}")

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
