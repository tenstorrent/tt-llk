# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers import *


def generate_golden(operand1, operand2, data_format, math_fidelity):

    operand1_f0 = operand1[:256].view(16, 16)
    operand1_f1 = operand1[256:512].view(16, 16)
    operand1_f2 = operand1[512:768].view(16, 16)
    operand1_f3 = operand1[768:].view(16, 16)

    operand2_f0 = operand2[:256].view(16, 16)
    operand2_f1 = operand2[256:512].view(16, 16)
    operand2_f2 = operand2[512:768].view(16, 16)
    operand2_f3 = operand2[768:].view(16, 16)

    operand1_matrix = torch.cat(
        [
            torch.cat([operand1_f0, operand1_f1], dim=1),
            torch.cat([operand1_f2, operand1_f3], dim=1),
        ],
        dim=0,
    ).reshape(32, 32)

    operand2_matrix = torch.cat(
        [
            torch.cat([operand2_f0, operand2_f1], dim=1),
            torch.cat([operand2_f2, operand2_f3], dim=1),
        ],
        dim=0,
    ).reshape(32, 32)

    print(operand1_matrix)
    print(operand2_matrix)

    result_matrix = torch.matmul(operand1_matrix, operand2_matrix).view(32, 32)

    print(result_matrix)

    return result_matrix.view(1024)


param_combinations = [
    (format, dest_acc, testname, math_fidelity)
    for format in ["Float16_b"]  # ,"Float16"]
    for dest_acc in ["DEST_ACC"]  # , "DEST_ACC"]
    for testname in ["matmul_test"]
    for math_fidelity in [4]  # [3, 4]
]

param_ids = [
    f" format={comb[0]} | dest_acc={comb[1]} | math_fidelity={comb[3]}"
    for comb in param_combinations
]


@pytest.mark.parametrize(
    "format, dest_acc, testname, math_fidelity", param_combinations, ids=param_ids
)
def test_matmul(format, testname, dest_acc, math_fidelity):

    # src_A, src_B = generate_stimuli(format,tile_cnt=1,sfpu=False,const_face=True,const_value_A=3,const_value_B=2)
    src_A, src_B = generate_stimuli(
        format,
        tile_cnt=1,
        sfpu=False,
        const_face=True,
        const_value_A=[1, 2, 3, 4],
        const_value_B=[4, 3, 2, 1],
    )

    # src_A = torch.tensor(
    #     [1.0] * 256 + [2.0] * 256 + [3] * 256 + [4] * 256,
    #     dtype=torch.bfloat16,
    # )
    # src_B = torch.tensor(
    #     [1] * 256 + [2.0] * 256 + [3] * 256 + [4] * 256,
    #     dtype=torch.bfloat16,
    # )

    golden_tensor = generate_golden(src_A, src_B, format, math_fidelity)
    golden_tensor = tilize(golden_tensor, format)

    write_stimuli_to_l1(src_A, src_B, format)

    test_config = {
        "input_format": format,
        "output_format": format,
        "testname": testname,
        "dest_acc": dest_acc,
        "math_fidelity": math_fidelity,
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")

    run_elf_files(testname)

    res_from_L1 = collect_results(format)
    run_shell_command("cd .. && make clean")

    assert len(res_from_L1) == len(golden_tensor)
    assert_tensix_operations_finished()

    res_tensor = torch.tensor(
        res_from_L1,
        dtype=(
            format_dict[format]
            if format in ["Float16", "Float16_b"]
            else torch.bfloat16
        ),
    )

    print("GOLDEN")
    print_faces(golden_tensor)
    print("RES")
    print_faces(res_tensor)

    if format == "Float16_b" or format == "Float16":
        atol = 0.1
        rtol = 0.05
    elif format == "Bfp8_b":
        atol = 0.1
        rtol = 0.2

    for i in range(len(golden_tensor)):
        assert torch.isclose(
            golden_tensor[i], res_tensor[i], rtol=rtol, atol=atol
        ), f"Failed at index {i} with values {golden_tensor[i]} and {res_from_L1[i]}"

    _, pcc = compare_pcc(golden_tensor, res_tensor, pcc=0.99)
    assert pcc > 0.98
