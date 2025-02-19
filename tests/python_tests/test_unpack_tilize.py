import pytest
import torch
import os
from helpers import *

def generate_golden(operand1, data_format):

    A_matrix = operand1.reshape(32,32)
    print("\n\n", type(A_matrix))
    A_tilized  = tilize(A_matrix,data_format)

    return A_tilized

param_combinations = [
    (format, testname)
    for format in ["Float16_b"]
    for testname in ["unpack_tilize_test"]
]

param_ids = [
    f" format={comb[0]} "
    for comb in param_combinations
]

@pytest.mark.parametrize(
    "format,testname",
    param_combinations,
    ids=param_ids
)

def test_all(format, testname):

    src_A, src_B = generate_stimuli(format, tile_cnt = 1, sfpu = False, const_face = True, const_value_A=4, const_value_B=3)
    # src_A, src_B = generate_stimuli(format)#, tile_cnt = 1, sfpu = False, const_face = True, const_value_A=4, const_value_B=3)
    golden_tensor = generate_golden(src_A, format)

    write_stimuli_to_l1(src_A, src_B, format)

    test_config = {
        "input_format": format,
        "output_format": format,
        "testname": testname,
    }

    make_cmd = generate_make_command(test_config)
    os.system(f"cd .. && {make_cmd} >/dev/null")

    run_elf_files(testname)

    res_from_L1 = collect_results(format)

    os.system("cd .. && make clean")

    assert len(res_from_L1) == len(golden_tensor)

    # Mailbox checks
    assert read_words_from_device("0,0", 0x19FF4, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'
    assert read_words_from_device("0,0", 0x19FF8, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'
    assert read_words_from_device("0,0", 0x19FFC, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[format] if format in ["Float16", "Float16_b"] else torch.bfloat16)

    if(format == "Float16_b" or format == "Float16"):
        atol = 0.1
        rtol = 0.05
    elif(format == "Bfp8_b"):
        atol = 0.1
        rtol = 0.2

    for i in range(len(golden_tensor)):
        assert torch.isclose(golden_tensor[i],res_tensor[i], rtol = rtol, atol = atol), f"Failed at index {i} with values {golden_tensor[i]} and {res_from_L1[i]}"

    _ , pcc = comp_pcc(golden_tensor, res_tensor, pcc=0.99) 
    assert pcc > 0.98