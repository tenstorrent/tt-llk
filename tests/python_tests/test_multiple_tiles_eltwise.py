# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers import *
from helpers.check_hw import *

mathop_map = {1: "elwadd", 2: "elwsub", 3: "elwmul"}

formats = ["Float16_b", "Float16", "Bfp8_b"]

def generate_golden(op, operand1, operand2, data_format, math_fidelity):
    tensor1_float = (
        operand1.clone()
        .detach()
        .to(format_dict.get(data_format, format_dict["Float16_b"]))
    )
    tensor2_float = (
        operand2.clone()
        .detach()
        .to(format_dict.get(data_format, format_dict["Float16_b"]))
    )

    if data_format == "Float16_b":
        if math_fidelity in [0, 2]:  # LoFi or HiFi2
            for element in operand2:
                element = element.to(torch.int32)
                element &= 0xFFFE
        if math_fidelity == 0:  # LoFi
            for element in operand1:
                element = element.to(torch.int32)
                element &= 0xFFF8

    if op == 1:
        res = tensor1_float + tensor2_float
    elif op == 2:
        res = tensor1_float - tensor2_float
    elif op == 3:
        res = tensor1_float * tensor2_float
    else:
        raise ValueError("Unsupported operation!")

    return res.tolist()


param_combinations = [
    (mathop, tile_cnt, unpack_src, unpack_dst, math, pack_src, pack_dst, dest_acc, testname, math_fidelity)
    for mathop in range(1, 4)
    for tile_cnt in range(1, 4)
    for unpack_src in formats
    for unpack_dst in formats
    for math in formats
    for pack_src in formats
    for pack_dst in formats
    for dest_acc in ["", "DEST_ACC"]
    for testname in ["multiple_tiles_eltwise_test"]
    for math_fidelity in [0, 2, 3, 4]
]

param_ids = [
    f"mathop={mathop_map[comb[0]]} | tile_cnt={comb[1]} | unpack_src={comb[2]} | unpack_dst={comb[3]} | math={comb[4]} | pack_src={comb[5]} | pack_dst={comb[6]} | dest_acc={comb[3]} | math_fidelity={comb[5]}"
    for comb in param_combinations
]


@pytest.mark.parametrize(
    "mathop, tile_cnt, unpack_src, unpack_dst, math, pack_src, pack_dst, dest_acc, testname, math_fidelity",
    param_combinations,
    ids=param_ids,
)
def test_multiple_tiles(unpack_src, unpack_dst, math, pack_src, pack_dst, testname, tile_cnt, mathop, dest_acc, math_fidelity, test_results):
    if not (unpack_src == unpack_dst and unpack_dst == math and math == pack_src and pack_src == pack_dst):
        pytest.skip(reason = "This test is only for uniform format")
        
    if mathop in range(1, 4) and unpack_src == "Float16" and dest_acc == "DEST_ACC":
        pytest.skip(reason="This combination is not fully implemented in testing")
        
    # if (mathop == 3 and unpack_src in ["Float16", "Bfp8_b"]):
    #     pytest.skip("")
    
    
    run_shell_command("cd .. && make clean")  
    run_shell_command("tt-smi -r 0")  

    hw = hw_support(unpack_src, unpack_dst, pack_src, pack_dst, True if dest_acc == "DEST_ACC" else False)
    test_results.append([
        "FAIL",  # Result
        hw, # is format combination supported by HW
        unpack_src,  # Input Format
        pack_dst,  # Output Format
        0,  # PCC placeholder until calculated
        "deadbeef",  # Placeholder for error (replace if failure)
        unpack_src,  # Unpack Src
        unpack_dst,  # Unpack Dst
        math,  # FPU (use output format for FPU)
        pack_src,  # Pack Src
        pack_dst,  # Pack Dst
        mathop,  # Math Operation
        "ON" if dest_acc != "" else "OFF" # Destination Accumulation
    ])

    #  prepare setup for running kernels

    pack_start_address = 0x1A000 + 2 * 4096 * tile_cnt
    pack_addresses = [pack_start_address + 0x1000 * i for i in range(tile_cnt)]
    pack_addresses_formatted = format_kernel_list(pack_addresses, as_hex=True)

    src_A, src_B = generate_stimuli(
        unpack_src, tile_cnt=tile_cnt
    )  # , const_face=True, const_value_A=3, const_value_B=2)
    golden = generate_golden(mathop, src_A, src_B, pack_dst, math_fidelity)
    write_stimuli_to_l1(src_A, src_B, unpack_src, "0,0", tile_cnt)

    if mathop != 3:
        math_fidelity = 0

    test_config = {
        "unpack_src": unpack_src,
        "unpack_dst": unpack_dst,
        "math": math,
        "pack_src": pack_src,
        "pack_dst": pack_dst,
        "testname": testname,
        "dest_acc": dest_acc,
        "mathop": mathop,
        "kern_cnt": tile_cnt,
        "pack_addr_cnt": len(pack_addresses),
        "pack_addrs": pack_addresses_formatted,
        "math_fidelity": math_fidelity,
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")

    run_elf_files(testname)

    run_shell_command("cd .. && make clean")

    assert_tensix_operations_finished()

    # check resluts from multiple tiles
    res_from_L1 = []

    for address in pack_addresses:
        res_from_L1.append(collect_results(unpack_src, pack_dst,address)) # Bug patchup in (unpack.py): Added unpack_src argument to distinguish when input and output formats have different exponent widths, reading from L1 changes
        
    res_from_L1 = flatten_list(res_from_L1)

    golden_tensor = torch.tensor(
        golden,
        dtype=(
            format_dict[pack_dst]
            if pack_dst in ["Float16", "Float16_b"]
            else torch.bfloat16
        ),
    )
    res_tensor = torch.tensor(
        res_from_L1,
        dtype=(
            format_dict[pack_dst]
            if pack_dst in ["Float16", "Float16_b"]
            else torch.bfloat16
        ),
    )

    if pack_dst == "Float16_b" or pack_dst == "Float16":
        atol = 0.05
        rtol = 0.1
    elif pack_dst == "Bfp8_b":
        atol = 0.1
        rtol = 0.2

    for i in range(len(golden_tensor)):
        test_results[-1][5] = (golden_tensor[i].item(), res_tensor[i].item())
        assert torch.isclose(
            golden_tensor[i], res_tensor[i], rtol=rtol, atol=atol
        ), f"Failed at index {i} with values {golden_tensor[i]} and {res_from_L1[i]}"

    _, pcc = compare_pcc(golden_tensor, res_tensor, pcc=0.99)
    assert pcc > 0.99
    test_results[-1][4] = pcc
    test_results[-1][0] = "PASS"