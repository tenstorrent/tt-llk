# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
import math
from helpers import *


def generate_golden(operation, operand1, data_format):
    tensor1_float = (
        operand1.clone()
        .detach()
        .to(format_dict[data_format] if data_format != "Bfp8_b" else torch.bfloat16)
    )
    ops = {
        "sqrt": lambda x: math.sqrt(x),
        "square": lambda x: x * x,
        "log": lambda x: math.log(x) if x != 0 else float("nan"),
    }
    if operation not in ops:
        raise ValueError("Unsupported operation!")
    return [ops[operation](num) for num in tensor1_float.tolist()][:256]

formats = ["Float16_b", "Float16", "Float32"]
param_combinations = [
    (mathop, unpack_src, unpack_dst, math, pack_src, pack_dst, dest_acc, testname, approx_mode)
    for mathop in ["sqrt", "log", "square"]
    for unpack_src in formats
    for unpack_dst in formats
    for math in formats
    for pack_src in formats
    for pack_dst in formats
    for dest_acc in ["DEST_ACC", ""]
    for testname in ["eltwise_unary_sfpu_test"]
    for approx_mode in ["false", "true"]
]

param_ids = [
    f"mathop={comb[0]} | unpack_src={comb[1]} | unpack_dst={comb[2]} | math={comb[3]} | pack_src={comb[4]} | pack_dst={comb[5]} | dest_acc={comb[6]} | approx_mode={comb[8]}"
    for comb in param_combinations
]


@pytest.mark.parametrize(
    "mathop, unpack_src, unpack_dst, math, pack_src, pack_dst, dest_acc, testname, approx_mode", param_combinations, ids=param_ids
)
def test_eltwise_unary_sfpu(unpack_src, unpack_dst, math, pack_src, pack_dst, mathop, testname, dest_acc, approx_mode):#
    if not (unpack_src == unpack_dst and unpack_dst == math and math == pack_src and pack_src == pack_dst):
        pytest.skip(reason="This test is only for uniform format")
    
    if unpack_src in ["Float32", "Int32"] and dest_acc != "DEST_ACC":
        pytest.skip(
            reason="Skipping test for 32 bit wide data without 32 bit accumulation in Dest"
        )

    if unpack_src == "Float16" and dest_acc == "":
        pytest.skip(reason="This combination is not fully implemented in testing")

    src_A, src_B = generate_stimuli(unpack_src, sfpu=True)
    golden = generate_golden(mathop, src_A, pack_dst)
    write_stimuli_to_l1(src_A, src_B, unpack_src)

    test_config = {
        "unpack_src": unpack_src,
        "unpack_dst": unpack_dst,
        "math": math,
        "pack_src": pack_src,
        "pack_dst": pack_dst,
        "testname": testname,
        "dest_acc": dest_acc,
        "mathop": mathop,
        "approx_mode": approx_mode,
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")
    run_elf_files(testname)

    res_from_L1 = collect_results(unpack_src, pack_dst, sfpu=True) # Bug patchup in (unpack.py): Added unpack_src argument to distinguish when input and output formats have different exponent widths, reading from L1 changes

    run_shell_command("cd .. && make clean")

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

    if pack_dst in ["Float16_b", "Float16", "Float32"]:
        atol = 0.05
        rtol = 0.1
    elif pack_dst == "Bfp8_b":
        atol = 0.05
        rtol = 0.1

    for i in range(len(golden)):
        assert torch.isclose(
            golden_tensor[i], res_tensor[i], rtol=rtol, atol=atol
        ), f"Failed at index {i} with values {golden[i]} and {res_from_L1[i]}"

    _, pcc = compare_pcc(golden_tensor, res_tensor, pcc=0.99)
    assert pcc > 0.99
