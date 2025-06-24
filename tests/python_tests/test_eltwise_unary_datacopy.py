# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch

from helpers.device import (
    collect_results,
    run_elf_files,
    wait_for_tensix_operations_finished,
    write_stimuli_to_l1,
    read_regfile,
    print_regfile,
)
from helpers.format_arg_mapping import DestAccumulation, format_dict
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import DataCopyGolden, get_golden_generator
from helpers.param_config import (
    clean_params,
    generate_combination,
    generate_param_ids,
    generate_params,
    input_output_formats,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import generate_make_command
from helpers.utils import passed_test, run_shell_command

# SUPPORTED FORMATS FOR TEST
supported_formats = [
    DataFormat.Float32,
    # DataFormat.Float16,
    # DataFormat.Float16_b,
    # DataFormat.Bfp8_b,
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
#         DataFormat.Float16_b)])   # index 4 is for math format

#   SPECIFIC INPUT-OUTPUT COMBINATION
#   [InputOutputFormat(DataFormat.Float16, DataFormat.Float32)]


test_formats = input_output_formats(supported_formats)
# test_formats = [InputOutputFormat(DataFormat.Float32, DataFormat.Float16)]
test_formats = generate_combination(
      [(DataFormat.UInt32,  # index 0 is for unpack_A_src
        DataFormat.UInt32,  # index 1 is for unpack_A_dst
        DataFormat.UInt32,  # index 2 is for pack_src (if src registers have same formats)
        DataFormat.UInt32,  # index 3 is for pack_dst
        DataFormat.UInt32)])   # index 4 is for math format
dest_acc = [DestAccumulation.Yes]#, DestAccumulation.No]
testname = ["eltwise_unary_datacopy_test"]
all_params = generate_params(testname, test_formats, dest_acc)
param_ids = generate_param_ids(all_params)


@pytest.mark.parametrize(
    "testname, formats, dest_acc", clean_params(all_params), ids=param_ids
)
def test_unary_datacopy(testname, formats, dest_acc):

    src_A, src_B = generate_stimuli(formats.input_format, formats.input_format)
    src_A = torch.tensor([700]*160, dtype=format_dict[formats.input_format])
    # src_A = torch.ones(256, dtype=format_dict[formats.input_format])
    
    generate_golden = get_golden_generator(DataCopyGolden)
    golden_tensor = generate_golden(src_A, formats.output_format)
    write_stimuli_to_l1(src_A, src_B, formats.input_format, formats.input_format)

    unpack_to_dest = formats.input_format.is_32_bit()

    test_config = {
        "formats": formats,
        "testname": testname,
        "dest_acc": dest_acc,
        "unpack_to_dest": unpack_to_dest,  # This test does a datacopy and unpacks input into dest register
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")
    run_elf_files(testname)

    wait_for_tensix_operations_finished()
    res_from_L1 = collect_results(formats, tensor_size=len(src_A))
    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)
    
    reg_file = read_regfile(2)
    print_regfile(reg_file)
    # print(reg_file)
    print("\nlength of reg file: ", len(reg_file))
    
    print(f"res_tensor: {res_tensor.view(16,10)}")

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
