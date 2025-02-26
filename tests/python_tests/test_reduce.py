import pytest
import torch
from helpers import *

def generate_golden(operand1, data_format):
    result = torch.ones(1024)
    return result

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
    src_B = torch.full((1024,), 0)

    src_A = torch.tensor([1]*256 + [2]*256 + [3]*256 + [4]*256, dtype=torch.bfloat16)

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
