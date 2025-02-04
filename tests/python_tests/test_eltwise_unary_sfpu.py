import pytest
import torch
import os
import math
from helpers import *

def generate_golden(operation, operand1, data_format):
    tensor1_float = operand1.clone().detach().to(format_dict[data_format])

    res = []

    if(operation == "sqrt"):
        for number in tensor1_float.tolist():
            res.append(math.sqrt(number))
    elif(operation == "square"): 
        for number in tensor1_float.tolist():
            res.append(number*number)
    elif(operation == "log"):
        for number in tensor1_float.tolist():
            if(number != 0):
                res.append(math.log(number))
            else:
                res.append(float('nan'))
    else:
        raise ValueError("Unsupported operation!")

    return res

@pytest.mark.parametrize("format", ["Float16_b","Float16"])
@pytest.mark.parametrize("testname", ["eltwise_unary_sfpu_test"])
@pytest.mark.parametrize("mathop", ["sqrt", "log","square"])
@pytest.mark.parametrize("dest_acc", ["","DEST_ACC"])
@pytest.mark.parametrize("approx_mode", ["true","false"])
def test_all(format, mathop, testname, dest_acc, approx_mode):

    src_A,src_B = generate_stimuli(format,sfpu = True)
    golden = generate_golden(mathop, src_A, format)
    write_stimuli_to_l1(src_A, src_B, format)

    test_config = {
        "input_format": format,
        "output_format": format,
        "testname": testname,
        "dest_acc": dest_acc,
        "mathop": mathop,
        "approx_mode": approx_mode
    }

    make_cmd = generate_make_command(test_config)
    os.system(f"cd .. && {make_cmd} >/dev/null")
    run_elf_files(testname)
    
    res_from_L1 = collect_results(format,sfpu=True)

    os.system("cd .. && make clean")

    assert len(res_from_L1) == len(golden)

    # Mailbox checks
    assert read_words_from_device("0,0", 0x19FF4, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'
    assert read_words_from_device("0,0", 0x19FF8, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'
    assert read_words_from_device("0,0", 0x19FFC, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'

    golden_tensor = torch.tensor(golden, dtype=format_dict[format] if format in ["Float16", "Float16_b"] else torch.bfloat16)
    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[format] if format in ["Float16", "Float16_b"] else torch.bfloat16)

    if(format == "Float16_b" or format == "Float16"):
        atol = 0.05
        rtol = 0.1

    for i in range(len(golden)):
        assert torch.isclose(golden_tensor[i],res_tensor[i], rtol = rtol, atol = atol), f"Failed at index {i} with values {golden[i]} and {res_from_L1[i]}"

    _ , pcc = comp_pcc(golden_tensor, res_tensor, pcc=0.99) 
    assert pcc > 0.99