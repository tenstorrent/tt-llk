# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.device import (
    collect_results,
    run_elf_files,
    wait_for_tensix_operations_finished,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import (
    DestAccumulation,
    MathFidelity,
    TileCount,
    format_dict,
)
from helpers.format_config import DataFormat
from helpers.param_config import (
    clean_params,
    generate_param_ids,
    generate_params,
    input_output_formats,
)
from helpers.stimuli_generator import flatten_list
from helpers.test_config import generate_make_command
from helpers.tilize_untilize import tilize
from helpers.utils import compare_pcc, run_shell_command, format_kernel_list


def generate_golden(operand1, operand2, data_format, math_fidelity):

    if data_format == DataFormat.Float16_b:
        if math_fidelity in [MathFidelity.LoFi, MathFidelity.HiFi2]:  # LoFi or HiFi2
            for element in operand2:
                element = element.to(torch.int32)
                element &= 0xFFFE
        if math_fidelity == MathFidelity.LoFi:  # LoFi
            for element in operand1:
                element = element.to(torch.int32)
                element &= 0xFFF8

    operand1_matrix = operand1.view(32, 32).to(format_dict[data_format])
    operand2_matrix = operand2.view(32, 32).to(format_dict[data_format])

    result_matrix = torch.matmul(operand1_matrix, operand2_matrix)

    return result_matrix.view(1024).to(format_dict[data_format])


# SUPPORTED FORMATS FOR TEST
supported_formats = [DataFormat.Float16, DataFormat.Float16_b]

#   INPUT-OUTPUT FORMAT SWEEP
#   input_output_formats(supported_formats)

#   FULL FORMAT SWEEP
#   format_combination_sweep(formats=supported_formats, all_same=False, same_src_reg_format=True)

#   SPECIFIC FORMAT COMBINATION
#   generate_combination(
#       [(DataFormat.Float16_b,  # index 0 is for unpack_A_src
#         DataFormat.Float16_b,  # index 1 is for unpack_A_dst
#         DataFormat.Float16_b,  # index 2 is for pack_src (if src registers have same formats)
#         DataFormat.Bfp8_b,  # index 3 is for pack_dst
#         DataFormat.Float16_b,  # index 4 is for math format)])

#   SPECIFIC INPUT-OUTPUT COMBINATION
#   [InputOutputFormat(DataFormat.Float16, DataFormat.Float32)]

test_formats = input_output_formats(supported_formats)
all_params = generate_params(
    ["matmul_test"],
    test_formats,
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
    tile_cnt=[TileCount.Two],
)
param_ids = generate_param_ids(all_params)


@pytest.mark.parametrize(
    "testname, formats, dest_acc, math_fidelity, tile_cnt",
    clean_params(all_params),
    ids=param_ids,
)
def test_matmul(testname, formats, dest_acc, math_fidelity, tile_cnt):
    pack_start_address = 0x1A000 + 2 * 4096 * tile_cnt.value
    pack_addresses = [pack_start_address + 0x1000 * i for i in range(tile_cnt.value)]
    pack_addresses_formatted = format_kernel_list(pack_addresses, as_hex=True)

    # src_A, src_B = generate_stimuli(
    #     formats.input_format, formats.input_format, tile_cnt=tile_cnt
    # )
    # print(src_A, "\n")
    # a1, a2 = torch.split(src_A, 1024)
    # b1, b2 = torch.split(src_B, 1024)

    b1, b2 = torch.eye(32).flatten(), torch.eye(32).flatten()
    print("b1 \n", b1.view(32, 32))
    print("b2 \n", b2.view(32, 32))
    a1, a2 = [], []
    for i in range(32):
        if i % 2 == 0:
            num = 0.5
        else:
            num = 1
        tens = [num] * 16
        a1.extend(tens)
        a2.extend(tens)

    for i in range(32):
        if i % 2 == 0:
            num = 1.5
        else:
            num = 2
        tens = [num] * 16
        a1.extend(tens)
        a2.extend(tens)

    a1, a2 = torch.tensor(a1, dtype=format_dict[formats.input_format]), torch.tensor(
        a2, dtype=format_dict[formats.input_format]
    )
    golden1 = generate_golden(a1, b1, formats.output_format, math_fidelity)
    golden2 = generate_golden(a2, b2, formats.output_format, math_fidelity)
    golden_tensor = torch.add(golden1, golden2)

    a1, a2 = tilize(a1, format_dict[formats.input_format]), tilize(
        a2, format_dict[formats.input_format]
    )
    src_A = torch.cat((a1, a2), dim=0)
    src_B = torch.cat((b1, b2), dim=0)
    print("src_B \n", src_B.view(128, 16))
    # b1, b2 = tilize(b1, format_dict[formats.input_format]), tilize(b2, format_dict[formats.input_format])
    src_B = torch.cat((b1, b2), dim=0)

    # print("src_B \n", src_B.view(128,16))
    write_stimuli_to_l1(
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        "0,0",
        tile_cnt,
    )

    # golden_tensor = generate_golden(src_A, src_B, formats.output_format, math_fidelity)
    golden_tensor = tilize(golden_tensor, format_dict[formats.input_format])
    golden_tensor = golden_tensor.to(format_dict[formats.output_format])
    print("GOLDEN \n", golden_tensor.view(64, 16))

    test_config = {
        "formats": formats,
        "testname": testname,
        "dest_acc": dest_acc,
        "math_fidelity": math_fidelity,
        "kern_cnt": tile_cnt,
        "pack_addr_cnt": len(pack_addresses),
        "pack_addrs": pack_addresses_formatted,
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")

    run_elf_files(testname)

    wait_for_tensix_operations_finished()
    res_from_L1 = []

    print("\n size of result from L1 = ", len(src_A) // len(pack_addresses))

    res_from_L1.append(
        collect_results(
            formats,
            tensor_size=len(src_A) // len(pack_addresses),
            address=pack_start_address,
        )
    )
    res_from_L1 = flatten_list(res_from_L1)

    res_tensor = torch.tensor(
        res_from_L1,
        dtype=(
            format_dict[formats.output_format]
            if formats.output_format in [DataFormat.Float16, DataFormat.Float16_b]
            else torch.bfloat16
        ),
    )
    print("RESULT \n", res_tensor.view(64, 16))
    if formats.output_format in [DataFormat.Float16_b, DataFormat.Float16]:
        atol = 0.1
        rtol = 0.05
    elif formats.output_format == DataFormat.Bfp8_b:
        atol = 0.1
        rtol = 0.2

    for i in range(len(golden_tensor)):
        assert torch.isclose(
            golden_tensor[i], res_tensor[i], rtol=rtol, atol=atol
        ), f"Failed at index {i} with values {golden_tensor[i]} and {res_from_L1[i]}"

    _, pcc = compare_pcc(golden_tensor, res_tensor, pcc=0.99)
    assert pcc > 0.98
