# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import DestAccumulation, MathFidelity, format_dict
from helpers.format_config import DataFormat
from helpers.math_fidelity import apply_fidelity
from helpers.param_config import (
    clean_params,
    generate_param_ids,
    generate_params,
    input_output_formats,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import generate_make_command
from helpers.tilize_untilize import tilize
from helpers.utils import passed_test, run_shell_command


def generate_golden(operand1, operand2, data_format, math_fidelity_phases):

    result_matrix = torch.zeros((32, 32))

    for i in range(math_fidelity_phases):

        print("GENERATING GOLDEN TENSOR FOR PHASE: ", i)
        operand1, operand2 = apply_fidelity(operand1, operand2, data_format, i)

        operand1_matrix = operand1.view(32, 32)
        operand2_matrix = operand2.view(32, 32)

        # accumulate results in a 32x32 matrix
        result_matrix += torch.matmul(operand1_matrix, operand2_matrix)

    return result_matrix.view(1024)


# SUPPORTED FORMATS FOR TEST
supported_formats = [
    DataFormat.Float16_b,
    DataFormat.Float16,
    DataFormat.Bfp8_b,
    DataFormat.Float32,
]
#   INPUT-OUTPUT FORMAT SWEEP
#   input_output_formats(supported_formats)

#   FULL FORMAT SWEEP
#   format_combination_sweep(formats=supported_formats, all_same=False, same_src_reg_format=True)

#   SPECIFIC FORMAT COMBINATION
#   generate_combination(
#       [(DataFormat.Float16_b,  # index 0 is for unpack_A_src
#         DataFormat.Float16_b,  # index 1 is for unpack_A_dst
#         DataFormat.Float16_b,  # index 2 is for pack_src (if src registers have same formats)
#         DataFormat.Bfp8_b,  # index 3 is for pack_dst
#         DataFormat.Float16_b,  # index 4 is for math format)])

#   SPECIFIC INPUT-OUTPUT COMBINATION
#   [InputOutputFormat(DataFormat.Float16, DataFormat.Float32)]

test_formats = input_output_formats(supported_formats)
all_params = generate_params(
    ["matmul_test"],
    test_formats,
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
)
param_ids = generate_param_ids(all_params)


@pytest.mark.parametrize(
    "testname, formats, dest_acc, math_fidelity",
    clean_params(all_params),
    ids=param_ids,
)
def test_matmul(testname, formats, dest_acc, math_fidelity):
    torch_format = format_dict[formats.output_format]

    input_dimensions = [64, 64]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    # src_A, src_B = generate_stimuli(formats.input_format, formats.input_format,const_face=True, const_value_A=1.54, const_value_B=1.67)
    src_A, src_B = generate_stimuli(formats.input_format, formats.input_format)

    if math_fidelity == MathFidelity.LoFi:
        math_fidelity_phases = 1
    elif math_fidelity == MathFidelity.HiFi2:
        math_fidelity_phases = 2
    elif math_fidelity == MathFidelity.HiFi3:
        math_fidelity_phases = 3
    elif math_fidelity == MathFidelity.HiFi4:
        math_fidelity_phases = 4

    print("GENERATING GOLDEN TENSOR")

    golden_tensor = generate_golden(
        src_A, src_B, formats.input_format, math_fidelity_phases
    )
    golden_tensor = tilize(golden_tensor, torch_format).to(torch_format)

    res_address = write_stimuli_to_l1(
        tilized_A.flatten(),
        tilized_B.flatten(),
        formats.input_format,
        formats.input_format,
        tile_count=tile_cnt,
    )

    test_config = {
        "formats": formats,
        "testname": testname,
        "dest_acc": dest_acc,
        "math_fidelity": math_fidelity,
        "tile_cnt": tile_cnt,
        "input_dimensions": input_dimensions,
    }

    run_test(test_config)

    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    print(f"Golden tensor: {golden_tensor.view(32,32)}")
    print("\n\n")
    print(f"Result tensor: {res_tensor.view(32,32)}")

    assert 1 == 2
    assert passed_test(golden_tensor, res_tensor, formats.output_format)
