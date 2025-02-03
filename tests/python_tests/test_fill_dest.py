import pytest
import torch
import os
import random
from helpers import *

def generate_golden(operations, operand1, operand2, data_format):
    if( data_format == "Float16" or data_format == "Float16_b"):
        tensor1_float = operand1.clone().detach().to(format_dict[data_format])
        tensor2_float = operand2.clone().detach().to(format_dict[data_format])
    else:
        tensor1_float = operand1.clone().detach().to(format_dict["Float16_b"])
        tensor2_float = operand2.clone().detach().to(format_dict["Float16_b"])
    res = []

    # to se why this encoding look at llk_defs.h -> enum EltwiseBinaryType

    for op in operations:
        if(op==0):
            res_tmp = tensor1_float * tensor2_float
        elif(op==1):
            res_tmp = tensor1_float / tensor2_float
        elif(op==2):
            res_tmp = tensor1_float + tensor2_float
        elif(op==3):
            res_tmp = tensor1_float - tensor2_float
        else:
            raise ValueError("Unsupported operation!")
        
        res.append(res_tmp.tolist())
    
    return res

@pytest.mark.parametrize("format", [ "Float16_b", "Float16", "Bfp8_b"])
@pytest.mark.parametrize("dest_acc", ["","DEST_ACC"])
@pytest.mark.parametrize("testname", ["fill_dest_test"])
def test_multiple_kernels(format, testname, dest_acc):

    pack_start_address = 0x1c000
    pack_addresses = [pack_start_address + 0x1000 * i for i in range(16)]

    src_A, src_B = generate_stimuli(format)
    golden = generate_golden([2]*16,src_A,src_B,format)
    write_stimuli_to_l1(src_A,src_B,format)

    test_config = {
        "input_format": format,
        "output_format": format,
        "testname": testname,
        "dest_acc": dest_acc,
    }

    make_cmd = generate_make_command(test_config)
    os.system(f"cd .. && {make_cmd}")

    run_elf_files(testname)

    os.system("cd .. && make clean")

    assert read_words_from_device("0,0", 0x19FF4, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'
    assert read_words_from_device("0,0", 0x19FF8, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'
    assert read_words_from_device("0,0", 0x19FFC, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'

    res_from_L1 = []

    for address in pack_addresses:
        res_from_L1.append(collect_results(format,src_A,address))
     
    res_from_L1 = flatten_list(res_from_L1)
    golden = flatten_list(golden)

    assert len(res_from_L1) == len(golden)

    golden_tensor = torch.tensor(golden, dtype=format_dict[format] if format in ["Float16", "Float16_b"] else torch.bfloat16)
    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[format] if format in ["Float16", "Float16_b"] else torch.bfloat16)

    if(format == "Float16_b" or format == "Float16"):
        atol = 0.05
        rtol = 0.1
    elif(format == "Bfp8_b"):
        atol = 0.1
        rtol = 0.2

    for i in range(len(golden)):
        assert torch.isclose(golden_tensor[i],res_tensor[i], rtol = rtol, atol = atol), f"Failed at index {i} with values {golden[i]} and {res_from_L1[i]}"

  
    _ , pcc = comp_pcc(golden_tensor, res_tensor, pcc=0.99) 
    assert pcc > 0.99