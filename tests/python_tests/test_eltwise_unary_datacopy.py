# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import subprocess
import pytest
import torch
from helpers import *


def generate_golden(operand1, format):
    return operand1


all_format_combos = generate_format_combinations(["Float32", "Bfp8_b", "Float16_b", "Float16", "Int32"], True)
dest_acc = ["", "DEST_ACC"]
testname = ["eltwise_unary_datacopy_test"]
all_params = generate_params(testname, all_format_combos, dest_acc)
param_ids = generate_param_ids(all_params)

@pytest.mark.parametrize(
    "testname, formats, dest_acc",
    all_params,
    ids=param_ids
)


def test_unary_datacopy(testname, formats, dest_acc):
        
    if (formats.unpack_src == "Float16" and dest_acc == "DEST_ACC"):
        pytest.skip(reason = "This combination is not fully implemented in testing")
    if(formats.unpack_src in ["Float32", "Int32"] and dest_acc!="DEST_ACC"):
        pytest.skip(reason = "Skipping test for 32 bit wide data without 32 bit accumulation in Dest")

    src_A,src_B = generate_stimuli(formats.unpack_src)
    srcB = torch.full((1024,), 0)
    golden = generate_golden(src_A,formats.pack_dst)
    write_stimuli_to_l1(src_A, src_B, formats.unpack_src)

    test_config = {
        "formats": formats,
        "testname": testname,
        "dest_acc": dest_acc,
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")
    run_elf_files(testname)

    # JUST PASS formats
    res_from_L1 = collect_results(formats) # Bug patchup in (unpack.py): passing formats struct to check unpack_src with pack_dst and distinguish when input and output formats have different exponent widths then reading from L1 changes

    run_shell_command("cd .. && make clean")

    assert len(res_from_L1) == len(golden)
    assert_tensix_operations_finished()

    if formats.pack_dst in format_dict:
        atol = 0.05
        rtol = 0.1
    else:
        atol = 0.2
        rtol = 0.1

    golden_tensor = torch.tensor(
        golden,
        dtype=(
            format_dict[formats.pack_dst]
            if formats.pack_dst in ["Float16", "Float16_b", "Float32", "Int32"]
            else torch.bfloat16
        ),
    )
    res_tensor = torch.tensor(
        res_from_L1,
        dtype=(
            format_dict[formats.pack_dst]
            if formats.pack_dst in ["Float16", "Float16_b", "Float32", "Int32"]
            else torch.bfloat16
        ),
    )

    for i in range(len(golden)):
        assert torch.isclose(
            golden_tensor[i], res_tensor[i], rtol=rtol, atol=atol
        ), f"Failed at index {i} with values {golden[i]} and {res_from_L1[i]}"

    _, pcc = compare_pcc(golden_tensor, res_tensor, pcc=0.99)
    assert pcc > 0.99
