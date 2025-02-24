import pytest
import torch
import os
from helpers import *

def generate_golden(operand1, operand2, data_format, math_fidelity):

    # if data_format == "Float16_b":
    #     if math_fidelity == 0:  # LoFi
    #         operand1 = operand1.to(torch.int32)  # Convert to int32
    #         operand1 &= 0xFFF8
    #         operand2 = operand2.to(torch.int32)  # Convert to int32
    #         operand2 &= 0xFFFE
    #     elif math_fidelity == 2:  # HiFi2
    #         operand2 = operand2.to(torch.int32)  # Convert to int32
    #         operand2 &= 0xFFFE
    #     elif math_fidelity == 3:  # HiFi3
    #         pass
    #     elif math_fidelity == 4:  # HiFi4
    #         pass

    A_tilized = tilize(operand1, data_format)
    B_tilized = tilize(operand2, data_format)

    # print(A_tilized.view(32,32))
    # print(B_tilized.view(32,32))

    result = torch.matmul(A_tilized.view(32, 32), B_tilized.view(32, 32))
    result = untilize(result, data_format)

    return result.view(-1)

param_combinations = [
    (format, dest_acc, testname, math_fidelity)
    for format in ["Float16_b"]#,"Float16"]
    for dest_acc in [""]#, "DEST_ACC"]
    for testname in ["matmul_test"]
    for math_fidelity in [4]#,2,3,4]
]

param_ids = [
    f" format={comb[0]} | dest_acc={comb[1]} | math_fidelity={comb[3]}"
    for comb in param_combinations
]

@pytest.mark.parametrize(
    "format, dest_acc, testname, math_fidelity",
    param_combinations,
    ids=param_ids
)

def test_matmul(format, testname, dest_acc, math_fidelity):

    #src_A, src_B = generate_stimuli(format,tile_cnt=1,sfpu=False,const_face=True,const_value_A=3,const_value_B=2)  
    #src_A, src_B = generate_stimuli(format)

    src_A = torch.tensor([1]*256 + [2]*256 + [3]*256 + [4]*256, dtype=torch.bfloat16)
    src_B = torch.tensor([1]*256 + [2]*256 + [3]*256 + [4]*256, dtype=torch.bfloat16)

    # Reshape both src_A and src_B to (32, 32)
    src_A_reshaped = src_A.view(32, 32)
    src_B_reshaped = src_B.view(32, 32)

    # Perform matrix multiplication
    result = torch.matmul(src_A_reshaped, src_B_reshaped)

    print(result)

    golden_tensor = generate_golden(src_A, src_B, format,math_fidelity)

    write_stimuli_to_l1(src_A, src_B, format)

    test_config = {
        "input_format": format,
        "output_format": format,
        "testname": testname,
        "dest_acc": dest_acc,
        "math_fidelity" : math_fidelity
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")

    run_elf_files(testname)

    print(golden_tensor.view(32,32))

    res_from_L1 = collect_results(format)

    print(res_from_L1)

    run_shell_command("cd .. && make clean")

    assert len(res_from_L1) == len(golden_tensor)
    assert read_mailboxes() == True

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[format] if format in ["Float16", "Float16_b"] else torch.bfloat16)

    if(format == "Float16_b" or format == "Float16"):
        atol = 0.1
        rtol = 0.05
    elif(format == "Bfp8_b"):
        atol = 0.1
        rtol = 0.2

    for i in range(len(golden_tensor)):
        assert torch.isclose(golden_tensor[i],res_tensor[i], rtol = rtol, atol = atol), f"Failed at index {i} with values {golden_tensor[i]} and {res_from_L1[i]}"

    _ , pcc = compare_pcc(golden_tensor, res_tensor, pcc=0.99) 
    assert pcc > 0.98