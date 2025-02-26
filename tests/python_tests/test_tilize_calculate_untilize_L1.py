import pytest
import torch
from helpers.utils import format_kernel_list, run_shell_command, compare_pcc, tilize, untilize
from helpers.device import write_stimuli_to_l1, collect_results, run_elf_files, read_mailboxes
from helpers.format_arg_mapping import format_dict
from helpers.test_config import generate_make_command
from helpers.stimuli_generator import flatten_list, generate_stimuli

def generate_golden(op, operand1, operand2, data_format,math_fidelity):
    tensor1_float = operand1.clone().detach().to(format_dict.get(data_format, format_dict["Float16_b"]))
    tensor2_float = operand2.clone().detach().to(format_dict.get(data_format, format_dict["Float16_b"]))

    if data_format == "Float16_b":
        if math_fidelity in [0, 2]:  # LoFi or HiFi2
            for element in operand2:
                element = element.to(torch.int32)
                element &= 0xFFFE
        if math_fidelity == 0:  # LoFi
            for element in operand1:
                element = element.to(torch.int32)
                element &= 0xFFF8
    
    # First step is unpack tilize
    tensor1_float = tilize(tensor1_float, data_format)
    tensor2_float = tilize(tensor2_float, data_format)

    # Second step is to perform the operation
    if(op=="elwadd"):
        res = tensor1_float + tensor2_float
    elif(op=="elwsub"):
        res = tensor1_float - tensor2_float
    elif(op=="elwmul"):
        res = tensor1_float * tensor2_float
    else:
        raise ValueError("Unsupported operation!")
    
    # Third step is to pack untilize
    res = untilize(res, data_format)
    
    return res

param_combinations = [
    (mathop, tile_cnt, format, dest_acc, testname, math_fidelity)
    for mathop in ["elwadd"]
    for tile_cnt in range(1, 2)
    for format in ["Float16_b"] #, "Float16", "Bfp8_b"]
    for dest_acc in [""] #, "DEST_ACC"]
    for testname in ["tilize_calculate_untilize_L1"]
    for math_fidelity in  [4] #[0,2,3,4]
]

param_ids = [
    f"mathop={comb[0]} | tile_cnt={comb[1]} | format={comb[2]} | dest_acc={comb[3]} | math_fidelity={comb[5]}"
    for comb in param_combinations
]

@pytest.mark.parametrize(
    "mathop, tile_cnt, format, dest_acc, testname, math_fidelity",
    param_combinations,
    ids=param_ids
)

def test_tilize_calculate_untilize_L1(format, testname, tile_cnt, mathop, dest_acc, math_fidelity):

    src_A, src_B = generate_stimuli(format,tile_cnt,sfpu=False,const_face=True,const_value_A=1,const_value_B=2)
    #src_A, src_B = generate_stimuli(format,tile_cnt)#,sfpu=False,const_face=True,const_value_A=1,const_value_B=2)

    golden_tensor = generate_golden(mathop, src_A, src_B, format, math_fidelity)

    write_stimuli_to_l1(src_A,src_B, format, "0,0", tile_cnt)

    test_config = {
        "input_format": format,
        "output_format": format,
        "testname": testname,
        "dest_acc": dest_acc,
        "math_fidelity" : math_fidelity,
        "mathop" : mathop
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")

    run_elf_files(testname)

    res_from_L1 = collect_results(format,0x1e000)
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