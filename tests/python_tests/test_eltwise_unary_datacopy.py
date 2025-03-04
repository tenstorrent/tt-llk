# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers import *

def generate_golden(operand1,format):
    return operand1
formats = ["Float32","Bfp8_b", "Float16_b", "Float16", "Int32"]
param_combinations = [
    (unpack_src, unpack_dst, math, pack_src, pack_dst, dest_acc, testname)
    for unpack_src in formats
    for unpack_dst in formats
    for math in formats
    for pack_src in formats
    for pack_dst in formats
    for dest_acc in ["", "DEST_ACC"]
    for testname in ["eltwise_unary_datacopy_test"]
]

param_ids = [
    f" unpack_src={comb[0]} | unpack_dst={comb[1]} | math = {comb[2]} | pack_src = {comb[3]} | pack_dst = {comb[4]} | dest_acc={comb[5]} "
    for comb in param_combinations
]

@pytest.mark.parametrize(
    "unpack_src, unpack_dst, math, pack_src, pack_dst, dest_acc, testname",
    param_combinations,
    ids=param_ids
)


def test_unary_datacopy(unpack_src, unpack_dst, math, pack_src, pack_dst, testname, dest_acc):
    if not (unpack_src == unpack_dst and unpack_dst == math and math == pack_src and pack_src == pack_dst):
        pytest.skip(reason = "This test is only for uniform format")
    if (unpack_src == "Float16" and dest_acc == "DEST_ACC"):
        pytest.skip(reason = "This combination is not fully implemented in testing")

    if(unpack_src in ["Float32", "Int32"] and dest_acc!="DEST_ACC"):
        pytest.skip(reason = "Skipping test for 32 bit wide data without 32 bit accumulation in Dest")

    src_A,src_B = generate_stimuli(unpack_src)
    srcB = torch.full((1024,), 0)
    golden = generate_golden(src_A,pack_dst)
    write_stimuli_to_l1(src_A, src_B, unpack_src)

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

    res_from_L1 = collect_results(unpack_src,pack_dst)

    run_shell_command("cd .. && make clean")

    assert len(res_from_L1) == len(golden)
    assert_tensix_operations_finished()

    if(format in format_dict):
        atol = 0.05
        rtol = 0.1
    else:
        atol = 0.2
        rtol = 0.1

    golden_tensor = torch.tensor(golden, dtype=format_dict[pack_dst] if pack_dst in ["Float16", "Float16_b", "Float32", "Int32"] else torch.bfloat16)
    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[pack_dst] if pack_dst in ["Float16", "Float16_b", "Float32", "Int32"] else torch.bfloat16)

    for i in range(len(golden)):
        assert torch.isclose(golden_tensor[i],res_tensor[i], rtol = rtol, atol = atol), f"Failed at index {i} with values {golden[i]} and {res_from_L1[i]}"
    
    _ , pcc = compare_pcc(golden_tensor, res_tensor, pcc=0.99) 
    assert pcc > 0.99
