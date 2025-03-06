# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers import *

torch.set_printoptions(linewidth=500, sci_mode=False, precision=2, threshold=10000)


def generate_golden(operand1, data_format):

    A_untilized = untilize(operand1, data_format)
    return A_untilized.flatten()

formats = ["Float16_b", "Float16"]
param_combinations = [
    (unpack_src, unpack_dst, math, pack_src, pack_dst, testname)
    for unpack_src in formats
    for unpack_dst in formats
    for math in formats
    for pack_src in formats
    for pack_dst in formats
    for testname in ["unpack_untilize_test"]
]

param_ids = [f" unpack_src={comb[0]} | unpack_dst={comb[1]} | math={comb[2]} | pack_src={comb[3]} | pack_dst={comb[4]}" for comb in param_combinations]


@pytest.mark.parametrize("unpack_src, unpack_dst, math, pack_src, pack_dst, testname", param_combinations, ids=param_ids)
def test_unpack_untilze(unpack_src, unpack_dst, math, pack_src, pack_dst, testname):
    
    if not (unpack_src == unpack_dst == math == pack_src == pack_dst):
        pytest.skip("Test not supported for different formats")

    src_A, src_B = generate_stimuli(unpack_src)
    src_B = torch.full((1024,), 0)

    golden_tensor = generate_golden(src_A, pack_dst)

    write_stimuli_to_l1(src_A, src_B, unpack_src)

    test_config = {
        "unpack_src": unpack_src,
        "unpack_dst": unpack_dst,
        "math": math,
        "pack_src": pack_src,
        "pack_dst": pack_dst,
        "testname": testname,
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")

    run_elf_files(testname)

    res_from_L1 = collect_results(unpack_src, pack_dst)

    run_shell_command("cd .. && make clean")

    assert len(res_from_L1) == len(golden_tensor)
    assert_tensix_operations_finished()

    res_tensor = torch.tensor(
        res_from_L1,
        dtype=(
            format_dict[pack_dst]
            if pack_dst in ["Float16", "Float16_b"]
            else torch.bfloat16
        ),
    )

    if pack_dst == "Float16_b" or pack_dst == "Float16":
        atol = 0.1
        rtol = 0.05
    elif pack_dst == "Bfp8_b":
        atol = 0.1
        rtol = 0.2

    for i in range(len(golden_tensor)):
        assert torch.isclose(
            golden_tensor[i], res_tensor[i], rtol=rtol, atol=atol
        ), f"Failed at index {i} with values {golden_tensor[i]} and {res_from_L1[i]}"

    _, pcc = compare_pcc(golden_tensor, res_tensor, pcc=0.99)
    assert pcc > 0.98
