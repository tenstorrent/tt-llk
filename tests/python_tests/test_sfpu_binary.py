import pytest
import torch
import os
from helpers import *

def generate_golden(operation, operand1, operand2, data_format):
    if( data_format in ["Float16","Float16_b", "Float32"]):
        tensor1_float = operand1.clone().detach().to(format_dict[data_format])
        tensor2_float = operand2.clone().detach().to(format_dict[data_format])
    else:
        tensor1_float = operand1.clone().detach().to(format_dict["Float16_b"])
        tensor2_float = operand2.clone().detach().to(format_dict["Float16_b"])
    
    operations = {
        "elwadd": tensor1_float + tensor2_float,
        "elwsub": tensor1_float - tensor2_float,
        "elwmul": tensor1_float * tensor2_float,
    }
    
    if operation not in operations:
        raise ValueError("Unsupported operation!")

    return operations[operation].tolist()

# @pytest.mark.parametrize("format", ["Float32", "Float16_b", "Float16"])
# @pytest.mark.parametrize("testname", ["sfpu_binary_test"])
# @pytest.mark.parametrize("mathop", ["elwadd", "elwsub", "elwmul"])
# @pytest.mark.parametrize("dest_acc", ["DEST_ACC", ""])

param_combinations = [
    (mathop, format, dest_acc, testname)
    for mathop in  ["elwadd", "elwsub", "elwmul"]
    for format in ["Float32"]
    for dest_acc in ["DEST_ACC"]
    for testname in ["sfpu_binary_test"]
]

param_ids = [
    f"mathop={comb[0]} | format={comb[1]} | dest_acc={comb[2]} "
    for comb in param_combinations
]

@pytest.mark.parametrize(
    "mathop, format, dest_acc, testname",
    param_combinations,
    ids=param_ids
)

def test_all(format, mathop, testname, dest_acc):
    
    src_A, src_B = generate_stimuli(format)
    golden = generate_golden(mathop, src_A, src_B, format)
    write_stimuli_to_l1(src_A, src_B, format)

    test_config = {
        "input_format": format,
        "output_format": format,
        "testname": testname,
        "dest_acc": dest_acc,
        "mathop": mathop
    }

    if( format in ["Float32", "Int32"] and dest_acc!="DEST_ACC"):
        pytest.skip("SKipping test for 32 bit wide data without 32 bit accumulation in Dest")

    make_cmd = generate_make_command(test_config)
    os.system(f"cd .. && {make_cmd} >/dev/null")

    run_elf_files(testname)
    
    res_from_L1 = collect_results(format)

    assert len(res_from_L1) == len(golden)

    os.system("cd .. && make clean")

    # Mailbox checks
    assert read_words_from_device("0,0", 0x19FF4, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'
    assert read_words_from_device("0,0", 0x19FF8, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'
    assert read_words_from_device("0,0", 0x19FFC, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'

    if(format in ["Float16_b","Float16", "Float32"]):
        atol = 0.05
        rtol = 0.1
    elif(format == "Bfp8_b"):
        atol = 0.1
        rtol = 0.2

    golden_tensor = torch.tensor(golden, dtype=format_dict[format] if format in ["Float16", "Float16_b","Float32"] else torch.bfloat16)
    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[format] if format in ["Float16", "Float16_b","Float32"] else torch.bfloat16)

    for i in range(len(golden)):
        assert torch.isclose(golden_tensor[i],res_tensor[i], rtol = rtol, atol = atol), f"Failed at index {i} with values {golden[i]} and {res_from_L1[i]}"

    _ , pcc = comp_pcc(golden_tensor, res_tensor, pcc=0.99) 
    assert pcc > 0.99