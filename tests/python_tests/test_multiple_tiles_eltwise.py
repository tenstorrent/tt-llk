# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
from helpers.llk_params import (
    DestAccumulation,
    MathFidelity,
    MathOperation,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.test_config import run_test
from helpers.utils import passed_test


@parametrize(
    test_name="multiple_tiles_eltwise_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    mathop=[MathOperation.Elwadd],
    dest_acc=[DestAccumulation.No],
    math_fidelity=[
        MathFidelity.LoFi,
    ],
    input_dimensions=[[32, 32]],
)
def test_multiple_tiles(
    test_name, formats, mathop, dest_acc, math_fidelity, input_dimensions
):

    if mathop != MathOperation.Elwmul and math_fidelity != MathFidelity.LoFi:
        pytest.skip("Fidelity does not affect Elwadd and Elwsub operations")

    # src_A, src_B, tile_cnt = generate_stimuli(
    #     formats.input_format, formats.input_format, input_dimensions=input_dimensions
    # )

    tile_cnt = input_dimensions[0] * input_dimensions[1] // 1024

    src_A = torch.ones(input_dimensions[0] * input_dimensions[1])
    # INSERT_YOUR_CODE
    src_B = torch.full((input_dimensions[0] * input_dimensions[1],), 2)
    # Each tile is 32x32 = 1024 elements.
    # Set last row (row 31, indices 31*32:32*32) of each tile to 3.

    elements_per_tile = 32 * 32
    last_row_start = 31 * 32
    for t in range(tile_cnt):
        start_idx = t * elements_per_tile + last_row_start
        end_idx = t * elements_per_tile + last_row_start + 32
        src_B[start_idx:end_idx] = 3

    print("src_A:")
    print(src_A.view(input_dimensions[0], input_dimensions[1]))
    print("src_B:")
    print(src_B.view(input_dimensions[0], input_dimensions[1]))

    print("\n" * 5)

    # For golden, construct src_B_bcasted: for each tile, grab its last row (last 32 elements), replicate 32 times → 32x32 tile
    src_B_bcasted = src_B.clone()
    for t in range(tile_cnt):
        elements_per_tile = 32 * 32
        offset = t * elements_per_tile
        # get last row of this tile
        last_row = src_B[offset + 31 * 32 : offset + 32 * 32]
        # replicate last row 32 times
        new_tile = last_row.repeat(32)
        src_B_bcasted[offset : offset + elements_per_tile] = new_tile

    print("src_B_bcasted:")
    print(src_B_bcasted.view(input_dimensions[0], input_dimensions[1]))
    print("\n" * 5)

    # generate_golden = get_golden_generator(EltwiseBinaryGolden)
    # golden_tensor = generate_golden(
    #     mathop, src_A, src_B_bcasted, formats.output_format, math_fidelity
    # )

    golden_tensor = src_A + src_B_bcasted

    print("golden_tensor:")
    print(golden_tensor.view(input_dimensions[0], input_dimensions[1]))
    print("\n" * 5)

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "mathop": mathop,
        "math_fidelity": math_fidelity,
        "tile_cnt": tile_cnt,
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

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    print("res_tensor:")
    print(res_tensor.view(input_dimensions[0], input_dimensions[1]))

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
