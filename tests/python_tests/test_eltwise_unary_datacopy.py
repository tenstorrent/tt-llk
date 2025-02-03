import pytest
import torch
import os
from helpers import *

torch.set_printoptions(linewidth=500)

def generate_golden(operand1,format):
    return operand1

@pytest.mark.parametrize("format",  ["Bfp8_b", "Float16_b", "Float16","Float32"])
@pytest.mark.parametrize("testname", ["eltwise_unary_datacopy_test"])
@pytest.mark.parametrize("dest_acc", [""])
def test_all(format, testname, dest_acc):
    #context = init_debuda()
    src_A,src_B = generate_stimuli(format)
    srcB = torch.full((1024,), 0)
    golden = generate_golden(src_A,format)
    write_stimuli_to_l1(src_A, src_B, format)

    test_config = {
        "input_format": format,
        "output_format": format,
        "testname": testname,
        "dest_acc": dest_acc,
    }


    make_cmd = generate_make_command(test_config)
    os.system(f"cd .. && {make_cmd}")
    run_elf_files(testname)

    res_from_L1 = collect_results(format,src_A)

    os.system("cd .. && make clean")

    assert len(res_from_L1) == len(golden)

    # Mailbox checks
    assert read_words_from_device("0,0", 0x19FF4, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'
    assert read_words_from_device("0,0", 0x19FF8, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'
    assert read_words_from_device("0,0", 0x19FFC, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'

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
    
    _ , pcc = comp_pcc(golden_tensor, res_tensor, pcc=0.99) 
    assert pcc > 0.99