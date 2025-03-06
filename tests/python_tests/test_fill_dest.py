# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers import *
from helpers.check_hw import *

def generate_golden(operations, operand1, operand2, data_format):
    tensor1_float = (
        operand1.clone()
        .detach()
        .to(format_dict.get(data_format, format_dict["Float16_b"]))
    )
    tensor2_float = (
        operand2.clone()
        .detach()
        .to(format_dict.get(data_format, format_dict["Float16_b"]))
    )

    res = []

    # to se why this encoding look at llk_defs.h -> enum EltwiseBinaryType

    for op in operations:
        if op == 0:
            res_tmp = tensor1_float * tensor2_float
        elif op == 1:
            res_tmp = tensor1_float / tensor2_float
        elif op == 2:
            res_tmp = tensor1_float + tensor2_float
        elif op == 3:
            res_tmp = tensor1_float - tensor2_float
        else:
            raise ValueError("Unsupported operation!")

        res.append(res_tmp.tolist())

    return flatten_list(res)

formats = ["Float16_b", "Float16", "Bfp8_b"]
param_combinations = [
    (unpack_src, unpack_dst, math, pack_src, pack_dst, dest_acc, testname)
    for unpack_src in formats
    for unpack_dst in formats
    for math in formats
    for pack_src in formats
    for pack_dst in formats
    for dest_acc in ["", "DEST_ACC"]
    for testname in ["fill_dest_test"]
]

param_ids = [
    f" unpack_src={comb[0]} | unpack_dst={comb[1]} | math={comb[2]} | pack_src={comb[3]} | pack_dst={comb[4]} | dest_acc={comb[5]}"
    for comb in param_combinations
]

@pytest.mark.parametrize(
    "unpack_src, unpack_dst, math, pack_src, pack_dst, dest_acc, testname",
    param_combinations,
    ids=param_ids
)

def test_fill_dest(unpack_src, unpack_dst, math, pack_src, pack_dst, testname, dest_acc, test_results):
    
    if (unpack_src != unpack_dst or unpack_dst != math or math != pack_src or pack_src != pack_dst):
        pytest.skip(reason = "This test is only for uniform format")

    if (unpack_src == "Float16" and dest_acc == "DEST_ACC"):
        pytest.skip(reason = "This combination is not fully implemented in testing")

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
    
    pack_start_address = 0x1C000
    pack_addresses = [pack_start_address + 0x1000 * i for i in range(16)]

    src_A, src_B = generate_stimuli(unpack_src)
    golden = generate_golden([2]*16,src_A,src_B,pack_dst)
    write_stimuli_to_l1(src_A,src_B,unpack_src)

    test_config = {
        "unpack_src": unpack_src,
        "unpack_dst": unpack_dst,
        "math": math,
        "pack_src": pack_src,
        "pack_dst": pack_dst,
        "testname": testname,
        "dest_acc": dest_acc,
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")

    run_elf_files(testname)

    run_shell_command("cd .. && make clean")

    res_from_L1 = []

    for address in pack_addresses:
        res_from_L1.append(collect_results(unpack_src, pack_dst,address)) # Bug patchup in (unpack.py): Added unpack_src argument to distinguish when input and output formats have different exponent widths, reading from L1 changes
     
    res_from_L1 = flatten_list(res_from_L1)

    assert len(res_from_L1) == len(golden)
    assert_tensix_operations_finished()

    golden_tensor = torch.tensor(
        golden,
        dtype=(
            format_dict[pack_dst]
            if pack_dst in ["Float16", "Float16_b"]
            else torch.bfloat16
        ),
    )
    res_tensor = torch.tensor(
        res_from_L1,
        dtype=(
            format_dict[pack_dst]
            if pack_dst in ["Float16", "Float16_b"]
            else torch.bfloat16
        ),
    )

    if pack_dst == "Float16_b" or pack_dst == "Float16":
        atol = 0.05
        rtol = 0.1
    elif pack_dst == "Bfp8_b":
        atol = 0.1
        rtol = 0.2

    for i in range(len(golden)):
        test_results[-1][5] = (golden_tensor[i].item(), res_tensor[i].item())
        assert torch.isclose(
            golden_tensor[i], res_tensor[i], rtol=rtol, atol=atol
        ), f"Failed at index {i} with values {golden[i]} and {res_from_L1[i]}"

    _, pcc = compare_pcc(golden_tensor, res_tensor, pcc=0.99)
    assert pcc > 0.99
    test_results[-1][4] = pcc
    test_results[-1][0] = "PASS"
