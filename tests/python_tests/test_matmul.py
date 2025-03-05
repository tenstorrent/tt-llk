# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers import *
from helpers.check_hw import *

def generate_golden(operand1, operand2, data_format, math_fidelity):

    if data_format == "Float16_b":
        if math_fidelity in [0, 2]:  # LoFi or HiFi2
            for element in operand2:
                element = element.to(torch.int32)
                element &= 0xFFFE
        if math_fidelity == 0:  # LoFi
            for element in operand1:
                element = element.to(torch.int32)
                element &= 0xFFF8

    return torch.matmul(tilize(operand1).view(32, 32), tilize(operand2).view(32, 32)).view(-1)

formats = ["Float16_b"]#,"Float16"]
param_combinations = [
    (unpack_src, unpack_dst, math, pack_src, pack_dst, dest_acc, testname, math_fidelity)
    for unpack_src in formats
    for unpack_dst in formats
    for math in formats
    for pack_src in formats
    for pack_dst in formats
    for dest_acc in ["", "DEST_ACC"]
    for testname in ["matmul_test"]
    for math_fidelity in [3,4]
]

param_ids = [
    f" format={comb[0]} | dest_acc={comb[1]} | math_fidelity={comb[3]}"
    for comb in param_combinations
]

@pytest.mark.parametrize(
    "unpack_src, unpack_dst, math, pack_src, pack_dst, dest_acc, testname, math_fidelity",
    param_combinations,
    ids=param_ids
)

def test_matmul(unpack_src, unpack_dst, math, pack_src, pack_dst, testname, dest_acc, math_fidelity, test_results):

    hw = hw_support(unpack_src, unpack_dst, pack_src, pack_dst, True if dest_acc == "DEST_ACC" else False)
    test_results.append([
        "FAIL",  # Result
        hw, # is format combination supported by HW
        unpack_src,  # Input Format
        pack_dst,  # Output Format
        0,  # PCC placeholder until calculated
        "deadbeef",  # Placeholder for error (replace if failure)
        unpack_src,  # Unpack Src
        unpack_dst,  # Unpack Dst
        math,  # FPU (use output format for FPU)
        pack_src,  # Pack Src
        pack_dst,  # Pack Dst
        "No mathop",  # Math Operation
        "ON" if dest_acc != "" else "OFF" # Destination Accumulation
    ])

    src_A = torch.tensor([torch.rand(1,dtype=format_dict[unpack_src]).item()]*256 + [torch.rand(1,dtype=format_dict[unpack_src]).item()]*256 + [torch.rand(1,dtype=format_dict[unpack_src]).item()]*256 + [torch.rand(1,dtype=format_dict[unpack_src]).item()]*256, dtype=torch.bfloat16)
    src_B = torch.tensor([torch.rand(1,dtype=format_dict[unpack_src]).item()]*256 + [torch.rand(1,dtype=format_dict[unpack_src]).item()]*256 + [torch.rand(1,dtype=format_dict[unpack_src]).item()]*256 + [torch.rand(1,dtype=format_dict[unpack_src]).item()]*256, dtype=torch.bfloat16)

    golden_tensor = generate_golden(src_A, src_B, pack_dst,math_fidelity)

    write_stimuli_to_l1(tilize(src_A), tilize(src_B), unpack_src)

    test_config = {
        "unpack_src": unpack_src,
        "unpack_dst": unpack_dst,
        "math": math,
        "pack_src": pack_src,
        "pack_dst": pack_dst,
        "testname": testname,
        "dest_acc": dest_acc,
        "math_fidelity" : math_fidelity
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")

    run_elf_files(testname)

    res_from_L1 = collect_results(unpack_src,pack_dst)
    run_shell_command("cd .. && make clean")

    assert len(res_from_L1) == len(golden_tensor)
    assert_tensix_operations_finished()

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[pack_dst] if pack_dst in ["Float16", "Float16_b"] else torch.bfloat16)

    if(pack_dst == "Float16_b" or pack_dst == "Float16"):
        atol = 0.1
        rtol = 0.05
    elif(pack_dst == "Bfp8_b"):
        atol = 0.1
        rtol = 0.2

    for i in range(len(golden_tensor)):
        test_results[-1][5] = (golden_tensor[i].item(), res_tensor[i].item())
        assert torch.isclose(golden_tensor[i],res_tensor[i], rtol = rtol, atol = atol), f"Failed at index {i} with values {golden_tensor[i]} and {res_from_L1[i]}"

    _ , pcc = compare_pcc(golden_tensor, res_tensor, pcc=0.99) 
    assert pcc > 0.98
    test_results[-1][4] = pcc
    test_results[-1][0] = "PASS"
