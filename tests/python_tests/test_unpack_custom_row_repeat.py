# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import (
    DestAccumulation,
    MathFidelity,
    MathOperation,
    format_dict,
)
from helpers.format_config import DataFormat
from helpers.golden_generators import EltwiseBinaryGolden, get_golden_generator
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


@parametrize(
    test_name="unpack_custom_row_repeat",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    mathop=[MathOperation.Elwsub],
    dest_acc=[DestAccumulation.No],
    math_fidelity=[
        MathFidelity.LoFi,
    ],
    input_dimensions=[[32, 64]],
)
def test_multiple_tiles(
    test_name, formats, mathop, dest_acc, math_fidelity, input_dimensions
):

    if mathop != MathOperation.Elwmul and math_fidelity != MathFidelity.LoFi:
        pytest.skip("Fidelity does not affect Elwadd and Elwsub operations")

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    # Generating easy to debug test case
    # ******************************************************************************

    ones = torch.tensor([1] * 16)
    twos = torch.tensor([2] * 16)
    threes = torch.tensor([3] * 16)
    fours = torch.tensor([4] * 16)
    padding = torch.tensor([0] * (256 - 16))

    src_A = torch.cat((ones, padding, twos, padding, threes, padding, fours, padding))
    src_A = src_A.repeat(input_dimensions[0] * input_dimensions[1] // 1024)

    src_B = torch.ones(input_dimensions[0] * input_dimensions[1]) * (5)

    print("SRC A")
    print("*" * 100)

    print(src_A.view(input_dimensions[0] * input_dimensions[1] // 16, 16))

    print("SRC B")
    print("*" * 100)

    print(src_B.view(input_dimensions[0] * input_dimensions[1] // 16, 16))

    # ******************************************************************************

    generate_golden = get_golden_generator(EltwiseBinaryGolden)
    golden_tensor = generate_golden(
        mathop, src_A, src_B, formats.output_format, math_fidelity
    )

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

    print("\nRESULT")
    print(res_tensor.view(input_dimensions[0] * input_dimensions[1] // 16, 16))

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
