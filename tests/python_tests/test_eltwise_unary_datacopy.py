import pytest
import torch
import os
from helpers import *

def generate_golden(operand1,format):
    return operand1

param_combinations = [
    (format, dest_acc, testname)
    for format in ["Float32","Bfp8_b", "Float16_b", "Float16", "Int32"]
    for dest_acc in ["", "DEST_ACC"]
    for testname in ["eltwise_unary_datacopy_test"]
]

param_ids = [
    f" format={comb[0]} | dest_acc={comb[1]} "
    for comb in param_combinations
]

@pytest.mark.parametrize(
    "format, dest_acc, testname",
    param_combinations,
    ids=param_ids
)


def test_unary_datacopy(format, testname, dest_acc):

    src_A,src_B = generate_stimuli(format)
    srcB = torch.full((1024,), 0)
    golden = generate_golden(src_A,format)
    write_stimuli_to_l1(src_A, src_B, format)

    if( format in ["Float32", "Int32"] and dest_acc!="DEST_ACC"):
        pytest.skip("SKipping test for 32 bit wide data without 32 bit accumulation in Dest")

    test_config = {
        "input_format": format,
        "output_format": format,
        "testname": testname,
        "dest_acc": dest_acc,
    }


    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd} >/dev/null")
    run_elf_files(testname)

    res_from_L1 = collect_results(format)

    run_shell_command("cd .. && make clean")

    assert len(res_from_L1) == len(golden)
    assert read_mailboxes() == True

    if(format in format_dict):
        atol = 0.05
        rtol = 0.1
    else:
        atol = 0.2
        rtol = 0.1

    golden_tensor = torch.tensor(golden, dtype=format_dict[format] if format in ["Float16", "Float16_b", "Float32", "Int32"] else torch.bfloat16)
    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[format] if format in ["Float16", "Float16_b", "Float32", "Int32"] else torch.bfloat16)

    for i in range(len(golden)):
        assert torch.isclose(golden_tensor[i],res_tensor[i], rtol = rtol, atol = atol), f"Failed at index {i} with values {golden[i]} and {res_from_L1[i]}"
    
    _ , pcc = compare_pcc(golden_tensor, res_tensor, pcc=0.99) 
    assert pcc > 0.99