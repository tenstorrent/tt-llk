import pytest
import torch
from helpers import *

def generate_golden(operand1, data_format):
    
    result = torch.zeros(1024, dtype=format_dict[data_format]).view(32, 32)

    f0 = operand1[:256].view(16, 16)
    f1 = operand1[256:512].view(16, 16)
    f2 = operand1[512:768].view(16, 16)
    f3 = operand1[768:].view(16, 16)

    left_half = torch.cat((f0, f2), 0) 
    right_half = torch.cat((f1, f3), 0)

    # print(left_half.view(32,16))
    # print("\n\n")
    # print(right_half.view(32,16))

    left_half_max = torch.max(left_half, dim=0).values
    right_half_max = torch.max(right_half, dim=0).values

    print(left_half_max)
    print(right_half_max)

    left_half = torch.where(left_half == left_half_max, left_half, torch.tensor(0.0))
    right_half = torch.where(right_half == right_half_max, right_half, torch.tensor(0.0))

    result[0][0:16] = left_half_max.view(1,16)
    result[0][16:32] = right_half_max.view(1,16)

    return result.view(1024)

param_combinations = [
    (mathop, format, dest_acc, testname)
    for mathop in ["reduce_col"]
    for format in ["Float16_b"]
    for dest_acc in [""]#, "DEST_ACC"]
    for testname in ["reduce_test"]
]

param_ids = [
    f"mathop={comb[0]}  | format={comb[1]} | dest_acc={comb[2]} "
    for comb in param_combinations
]

@pytest.mark.parametrize(
    "mathop, format, dest_acc, testname",
    param_combinations,
    ids=param_ids
)

def test_reduce(mathop, format, testname, dest_acc):
    
    src_A, src_B = generate_stimuli(format)
    src_B = torch.full((1024,), 1) # also for reduce sum
    # reduce average divides by length of elements in array we reduce

    golden_tensor = generate_golden(src_A, format)
    write_stimuli_to_l1(src_A, src_B, format)

    test_config = {
        "input_format": format,
        "output_format": format,
        "testname": testname,
        "dest_acc": dest_acc,
        "mathop": mathop
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")

    run_elf_files(testname)

    res_from_L1 = collect_results(format)

    run_shell_command("cd .. && make clean")

    assert len(res_from_L1) == len(golden_tensor)
    assert_tensix_operations_finished()

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[format] if format in ["Float16", "Float16_b"] else torch.bfloat16)
    res_tensor = untilize(res_tensor)

    if format == "Float16_b" or format == "Float16":
        atol = 0.1
        rtol = 0.05
    elif format == "Bfp8_b":
        atol = 0.1
        rtol = 0.2

    for i in range(len(golden_tensor)):
        assert torch.isclose(golden_tensor[i], res_tensor[i], rtol=rtol, atol=atol), f"Failed at index {i} with values {golden_tensor[i]} and {res_from_L1[i]}"

    _, pcc = compare_pcc(golden_tensor, res_tensor, pcc=0.99)
    assert pcc > 0.98
