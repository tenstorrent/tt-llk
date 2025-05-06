# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
import math
from helpers import *


def generate_golden(operation, operand1, data_format):
    tensor1_float = (
        operand1.clone()
        .detach()
        .to(format_dict[data_format] if data_format != "Bfp8_b" else torch.bfloat16)
    )
    ops = {
        MathOperation.Sqrt: lambda x: math.sqrt(x),
        MathOperation.Square: lambda x: x * x,
        MathOperation.Log: lambda x: math.log(x) if x != 0 else float("nan"),
    }
    if operation not in ops:
        raise ValueError("Unsupported operation!")
    return [ops[operation](num) for num in tensor1_float.tolist()][:256]


# SUPPORTED FORMATS FOR TEST
supported_formats = [DataFormat.Float32, DataFormat.Float16, DataFormat.Float16_b]

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
    ["eltwise_unary_sfpu_test"],
    test_formats,
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    approx_mode=[ApproximationMode.No, ApproximationMode.Yes],
    mathop=[MathOperation.Sqrt, MathOperation.Log, MathOperation.Square],
)
param_ids = generate_param_ids(all_params)


@pytest.mark.parametrize(
    "testname, formats, dest_acc, approx_mode, mathop",
    clean_params(all_params),
    ids=param_ids,
)
def test_eltwise_unary_sfpu(testname, formats, dest_acc, approx_mode, mathop):
    arch = get_chip_architecture()
    if (
        formats.input_format in [DataFormat.Float32, DataFormat.Int32]
        and dest_acc != DestAccumulation.Yes
    ):
        pytest.skip(
            reason="Skipping test for 32 bit wide data without 32 bit accumulation in Dest"
        )

    if formats.input_format == DataFormat.Float16 and (
        dest_acc == DestAccumulation.No and arch == "blackhole"
    ):
        pytest.skip(reason="This combination is not fully implemented in testing")

    src_A, src_B = generate_stimuli(
        formats.input_format, formats.input_format, sfpu=True
    )
    golden = generate_golden(mathop, src_A, formats.output_format)
    write_stimuli_to_l1(src_A, src_B, formats.input_format, formats.input_format)

    test_config = {
        "formats": formats,
        "testname": testname,
        "dest_acc": dest_acc,
        "mathop": mathop,
        "approx_mode": approx_mode,
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")
    run_elf_files(testname)

    wait_for_tensix_operations_finished()
    res_from_L1 = collect_results(
        formats, tensor_size=len(src_A)
    )  # Bug patchup in (unpack.py): passing formats struct to check unpack_src with pack_dst and distinguish when input and output formats have different exponent widths then reading from L1 changes
    res_from_L1 = res_from_L1[
        :256
    ]  # this will be removed once we implement to read bytes from L1 according to data format (size of datum) which will be added in next PR
    assert len(res_from_L1) == len(golden)

    golden_tensor = torch.tensor(
        golden,
        dtype=(
            format_dict[formats.output_format]
            if formats.output_format in [DataFormat.Float16, DataFormat.Float16_b]
            else torch.bfloat16
        ),
    )
    res_tensor = torch.tensor(
        res_from_L1,
        dtype=(
            format_dict[formats.output_format]
            if formats.output_format in [DataFormat.Float16, DataFormat.Float16_b]
            else torch.bfloat16
        ),
    )

    if formats.output_format in [
        DataFormat.Float16_b,
        DataFormat.Float16,
        DataFormat.Float32,
    ]:
        atol = 0.05
        rtol = 0.1
    elif formats.output_format == DataFormat.Bfp8_b:
        atol = 0.05
        rtol = 0.1

    for i in range(len(golden)):
        assert torch.isclose(
            golden_tensor[i], res_tensor[i], rtol=rtol, atol=atol
        ), f"Failed at index {i} with values {golden[i]} and {res_from_L1[i]}"

    _, pcc = compare_pcc(golden_tensor, res_tensor, pcc=0.99)
    assert pcc > 0.99
