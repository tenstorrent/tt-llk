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
    MathOperation,
    format_dict,
)
from helpers.format_config import DataFormat
from helpers.param_config import (
    clean_params,
    generate_param_ids,
    generate_params,
    input_output_formats,
)
from helpers.test_config import generate_make_command
from helpers.utils import run_shell_command


# Helper function
def extend_tensor(condition, length=1024, dtype=torch.float32):
    condition_extended = torch.zeros(length, dtype=dtype)
    condition_extended[: condition.shape[0]] = condition
    return condition_extended.flatten()


def generate_golden(operand1, true_value, false_value):
    # operand1, true_value, and false_value are 1D tensors of floats
    mask = operand1.view(32, 32) != 0
    return torch.where(
        mask, true_value.view(32, 32), false_value.view(32, 32)
    ).flatten()


# Helper check functiion
def torch_equal_nan(a, b):
    # return torch.all(
    #     torch.isclose(a, b, rtol=1e-2, atol=1e-5) | (torch.isnan(a) & torch.isnan(b))
    # )
    return torch.all((a == b) | (torch.isnan(a) & torch.isnan(b)))


# Provided test cases
dtype = torch.float32
condition = torch.tensor([1, 0, -2, 0, 5, 0, 0, 8, 0, -1], dtype=dtype)
condition_all_ones = torch.tensor([1, 1, 1, 1, 1, 1, 1, 1, 1, 1], dtype=dtype)
condition_all_zeros = torch.tensor([0, 0, 0, 0, 0, 0, 0, 0, 0, 0], dtype=dtype)

# true and false value tensors
true_values = torch.tensor(
    [
        1.0,
        float("nan"),
        3.0,
        float("inf"),
        -float("inf"),
        -1.0,
        0.0,
        -0.0,
        42.49,
        -92.42,
    ],
    dtype=dtype,
)
# true_values = torch.tensor(
#     [
#         -0.0,
#         -0.0,
#         -0.0,
#         -0.0,
#         -0.0,
#         -0.0,
#         -0.0,
#         -0.0,
#         -0.0,
#         -0.0,
#     ],
#     dtype=dtype,
# )

false_values = torch.tensor(
    [
        -1.0,
        999.9,
        float("nan"),
        -float("inf"),
        float("inf"),
        1.0,
        -0.0,
        0.0,
        -3.14,
        7.84,
    ],
    dtype=dtype,
)


supported_formats = [DataFormat.Float32]  # , DataFormat.Float16_b]

test_formats = input_output_formats(supported_formats, same=True)
all_params = generate_params(
    ["ttnn_where_test"],
    test_formats,
    dest_acc=[DestAccumulation.Yes],  # DestAccumulation.No],
    mathop=[
        MathOperation.TTNNWhere,
    ],
)
param_ids = generate_param_ids(all_params)


@pytest.mark.parametrize(
    "testname, formats, dest_acc, mathop",
    clean_params(all_params),
    ids=param_ids,
)
@pytest.mark.parametrize(
    "test_tensors",
    [
        # [
        #     torch.randint(0, 2, (32, 32), dtype=torch.bfloat16)
        #     .flatten()
        #     .to(torch.bfloat16),
        #     torch.randint(-10, 10, (32, 32), dtype=torch.bfloat16)
        #     .flatten()
        #     .to(torch.bfloat16),
        #     torch.randint(-10, 10, (32, 32), dtype=torch.bfloat16)
        #     .flatten()
        #     .to(torch.bfloat16),
        # ],  # random test case
        [
            extend_tensor(condition.bool(), length=1024, dtype=dtype),
            extend_tensor(true_values, length=1024, dtype=dtype),
            extend_tensor(false_values, length=1024, dtype=dtype),
        ],  # provided test case
        [
            extend_tensor(condition_all_ones.bool(), length=1024, dtype=dtype),
            extend_tensor(true_values, length=1024, dtype=dtype),
            extend_tensor(false_values, length=1024, dtype=dtype),
        ],  # provided test case
        [
            extend_tensor(condition_all_zeros.bool(), length=1024, dtype=dtype),
            extend_tensor(true_values, length=1024, dtype=dtype),
            extend_tensor(false_values, length=1024, dtype=dtype),
        ],  # provided test case
    ],
)
def test_ttnn_where(testname, formats, dest_acc, mathop, test_tensors):

    src_A, src_B, src_C = test_tensors

    # Skipping test combinations that are not supported

    if (
        formats.output_format == DataFormat.Float32
        or formats.input_format == DataFormat.Float32
    ) and dest_acc == DestAccumulation.No:
        pytest.skip(
            "Skipping test for Float32 input format with NO dest_acc, as it is not supported."
        )

    if src_A.dtype != format_dict[formats.input_format]:
        pytest.skip()

    print(
        "\n\nTEST COMBINATION: ",
        src_A.dtype,
        format_dict[formats.input_format],
        dest_acc,
        "\n",
    )

    golden = generate_golden(src_A, src_B, src_C)
    write_stimuli_to_l1(
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        ternary_op=True,
        buffer_C=src_C,
        stimuli_C_format=formats.input_format,
    )

    test_config = {
        "formats": formats,
        "testname": testname,
        "dest_acc": dest_acc,
        "mathop": mathop,
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")
    run_elf_files(testname)

    wait_for_tensix_operations_finished()
    res_from_L1 = collect_results(formats, tensor_size=len(src_A), address=0x1D000)
    res_from_L1 = res_from_L1[:1024]
    assert len(res_from_L1) == len(golden)

    golden_tensor = torch.tensor(
        golden,
        dtype=(
            format_dict[formats.output_format]
            if formats.output_format
            in [DataFormat.Float16, DataFormat.Float16_b, DataFormat.Float32]
            else torch.bfloat16
        ),
    )
    res_tensor = torch.tensor(
        res_from_L1,
        dtype=(
            format_dict[formats.output_format]
            if formats.output_format
            in [DataFormat.Float16, DataFormat.Float16_b, DataFormat.Float32]
            else torch.bfloat16
        ),
    )
    print(
        "RESULT TENSOR (first 10 elements of 1st row):", res_tensor.view(32, 32)[0, :10]
    )
    print(
        "GOLDEN TENSOR (first 10 elements of 1st row):",
        golden_tensor.view(32, 32)[0, :10],
    )

    assert torch_equal_nan(golden_tensor, res_tensor)

    # assert passed_test(golden_tensor, res_tensor, formats.output_format)
