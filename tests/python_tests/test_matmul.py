import pytest
import torch
import os
from helpers import *

torch.set_printoptions(precision=2, linewidth=800, threshold=100000, sci_mode=False)

def generate_golden(operand1, operand2, data_format):
    # A_float = operand1.clone().detach()#.to(format_dict[data_format])
    # B_float = operand2.clone().detach()#.to(format_dict[data_format])

    A_untilized = untilize(operand1,data_format)
    B_untilized = untilize(operand2,data_format)

    result = torch.matmul(A_untilized, B_untilized )

    result = tilize(result)
    return result

@pytest.mark.parametrize("format", ["Float16_b", "Float16"])
@pytest.mark.parametrize("testname", ["matmul_test"])
@pytest.mark.parametrize("dest_acc", ["","DEST_ACC"])
def test_all(format, testname, dest_acc):

    #context = init_debuda()
    src_A, src_B = generate_stimuli(format)
    golden_tensor = generate_golden(src_A, src_B, format)

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
        atol = 0.4
        rtol = 0.3

    rel_errs = []

    # for i in range(len(golden_tensor)):
    #     assert torch.isclose(golden_tensor[i],res_tensor[i], rtol = rtol, atol = atol), f"Failed at index {i} with values {golden_tensor[i]} and {res_from_L1[i]}"

    _ , pcc = comp_pcc(golden_tensor, res_tensor, pcc=0.99) 
    assert pcc > 0.98