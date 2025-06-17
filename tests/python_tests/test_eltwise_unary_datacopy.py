# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import DestAccumulation, format_dict
from helpers.format_config import DataFormat
from helpers.golden_generators import DataCopyGolden, get_golden_generator
from helpers.param_config import (
    clean_params,
    generate_param_ids,
    generate_params,
    input_output_formats,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import generate_make_command
from helpers.utils import passed_test, run_shell_command


def generate_golden(operand1, format):
    return operand1


# SUPPORTED FORMATS FOR TEST
supported_formats = [
    DataFormat.Float32,
]

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
dest_acc = [DestAccumulation.Yes]
testname = ["eltwise_unary_datacopy_test"]
all_params = generate_params(testname, test_formats, dest_acc)
param_ids = generate_param_ids(all_params)


@pytest.mark.parametrize(
    "testname, formats, dest_acc", clean_params(all_params), ids=param_ids
)
def test_unary_datacopy(testname, formats, dest_acc):

    src_A, src_B = generate_stimuli(formats.input_format, formats.input_format)
    src_A = torch.arange(1, 33, 1 / 32, dtype=torch.float32)

    golden = generate_golden(src_A, formats.output_format)
    write_stimuli_to_l1(src_A, src_B, formats.input_format, formats.input_format)

    unpack_to_dest = formats.input_format.is_32_bit()

    test_config = {
        "formats": formats,
        "testname": testname,
        "dest_acc": dest_acc,
        "unpack_to_dest": unpack_to_dest,
    }

    run_test(test_config)

    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)
    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict.get(
        formats.output_format, format_dict[DataFormat.Float16_b]
    )
    golden_tensor = torch.tensor(golden, dtype=(torch_format))
    res_tensor = torch.tensor(res_from_L1, dtype=(torch_format))

    def to_hex_matrix(tensor):
        arr = tensor.cpu().to(torch.float32).reshape(32, 32)
        hex_matrix = arr.view(torch.uint32)
        return hex_matrix.apply_(lambda x: int(x))

    print("Golden (hex):")
    golden_hex = to_hex_matrix(golden_tensor)
    for i in range(32):
        print(" ".join(f"0x{golden_hex[i, j].item():08x}" for j in range(32)))
    print("Result (hex):")
    result_hex = to_hex_matrix(res_tensor)
    for i in range(32):
        print(" ".join(f"0x{result_hex[i, j].item():08x}" for j in range(32)))

    assert 1 == 2

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
