import pytest
import torch
import os
from helpers import *

def generate_golden(operand1, operand2, data_format,math_fidelity):

    A_untilized = untilize(operand1,data_format)
    B_untilized = untilize(operand2,data_format)

    if( format == "Float16_b"):
        if math_fidelity == 0: # LoFi
            for element in operand1:
                element &= 0xFFF8
            for element in operand2:
                element &= 0xFFFE
        elif math_fidelity == 2: # HiFi2
            for element in operand2:
                element &= 0xFFFE
        elif math_fidelity == 3: # HiFi3
            pass
        elif math_fidelity == 4: # HiFi4
            pass

    result = torch.matmul(A_untilized, B_untilized )
    result = tilize(result,data_format)

    return result

# @pytest.mark.parametrize("format", ["Float16_b", "Float16","Bfp8_b"])
# @pytest.mark.parametrize("testname", ["matmul_test"])
# @pytest.mark.parametrize("dest_acc", [""])#,"DEST_ACC"])

# This setup allows easier tracking of test cases when run
param_combinations = [
    (input_format, output_format, dest_acc, testname, math_fidelity)
    for input_format in ["Float16_b", "Float16"]
    for output_format in ["Float16_b", "Float16"]
    for dest_acc in ["", "DEST_ACC"]
    for testname in ["matmul_test"]
    for math_fidelity in [0,2,3,4]
]

param_ids = [
    f" input_format={comb[0]} | output_format={comb[1]} | dest_acc={comb[2]} | math_fidelity={comb[4]}"
    for comb in param_combinations
]

@pytest.mark.parametrize(
    "input_format, output_format, dest_acc, testname, math_fidelity",
    param_combinations,
    ids=param_ids
)

def test_all(input_format, output_format, testname, dest_acc, math_fidelity):
    if input_format != output_format:
        pytest.skip("")
    src_A, src_B = generate_stimuli(input_format)
    golden_tensor = generate_golden(src_A, src_B, output_format,math_fidelity)

    write_stimuli_to_l1(src_A, src_B, input_format)

    test_config = {
        "input_format": input_format,
        "output_format": output_format,
        "testname": testname,
        "dest_acc": dest_acc,
        "math_fidelity" : math_fidelity
    }

    make_cmd = generate_make_command(test_config)
    os.system(f"cd .. && {make_cmd} >/dev/null")

    run_elf_files(testname)

    res_from_L1 = collect_results(output_format)

    os.system("cd .. && make clean")

    assert len(res_from_L1) == len(golden_tensor)

    # Mailbox checks
    assert read_words_from_device("0,0", 0x19FF4, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'
    assert read_words_from_device("0,0", 0x19FF8, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'
    assert read_words_from_device("0,0", 0x19FFC, word_count=1)[0].to_bytes(4, 'big') == b'\x00\x00\x00\x01'

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[output_format] if output_format in ["Float16", "Float16_b"] else torch.bfloat16)

    print(golden_tensor.type(), res_tensor.type())

    if(output_format == "Float16_b" or output_format == "Float16"):
        atol = 0.1
        rtol = 0.05
    elif(output_format == "Bfp8_b"):
        atol = 0.1
        rtol = 0.2

    for i in range(len(golden_tensor)):
        assert torch.isclose(golden_tensor[i],res_tensor[i], rtol = rtol, atol = atol), f"Failed at index {i} with values {golden_tensor[i]} and {res_from_L1[i]}"

    _ , pcc = comp_pcc(golden_tensor, res_tensor, pcc=0.99) 
    assert pcc > 0.98