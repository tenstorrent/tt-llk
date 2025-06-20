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
from helpers.format_arg_mapping import format_dict
from helpers.format_config import DataFormat
from helpers.golden_generators import UntilizeGolden, get_golden_generator
from helpers.param_config import (
    clean_params,
    generate_param_ids,
    generate_params,
    input_output_formats,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import generate_make_command
from helpers.utils import passed_test, run_shell_command

# SUPPORTED FORMATS FOR TEST
supported_formats = [DataFormat.Float16_b, DataFormat.Float16]

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
all_params = generate_params(["pack_untilize_test"], test_formats)
param_ids = generate_param_ids(all_params)


@pytest.mark.parametrize("testname, formats", clean_params(all_params), ids=param_ids)
def test_pack_untilize(testname, formats):

    input_dimensions = [32, 64]

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )
    # src_A = torch.cat(
    #     [
    #         torch.full((256,), i, dtype=format_dict[formats.input_format])
    #         for i in range(input_dimensions[0] * input_dimensions[1] // 256)
    #     ]
    # )

    src_A = torch.arange(
        0,
        input_dimensions[0] * input_dimensions[1] / 256,
        1 / 256,
        dtype=format_dict[formats.input_format],
    )

    # src_B = torch.full((1024,), 0)
    # Print src_A as a 32x64 matrix with | and - separating 32x32 submatrices
    def print_block(tensor, rows, cols, block_size=32):
        mat = tensor.view(rows, cols)
        for i, row in enumerate(mat):
            row_str = ""
            for j, val in enumerate(row):
                row_str += f"{val.item():6.1f}"
                if (j + 1) % block_size == 0 and j != cols - 1:
                    row_str += " |"
            print(row_str)
            if (i + 1) % block_size == 0 and i != rows - 1:
                print("-" * (7 * cols + 2))

    print_block(src_A, input_dimensions[0], input_dimensions[1])

    generate_golden = get_golden_generator(UntilizeGolden)
    golden_tensor = generate_golden(src_A, formats.output_format, input_dimensions)

    print("\n" * 5)
    print_block(golden_tensor, input_dimensions[0], input_dimensions[1])

    res_address = write_stimuli_to_l1(
        src_A, src_B, formats.input_format, formats.input_format, tile_cnt=tile_cnt
    )

    test_config = {
        "formats": formats,
        "testname": testname,
        "tile_cnt": tile_cnt,
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")

    run_elf_files(testname)
    wait_for_tensix_operations_finished()

    res_from_L1 = collect_results(formats, tile_cnt=tile_cnt, address=res_address)
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    print("\n" * 5)
    print_block(res_tensor, input_dimensions[0], input_dimensions[1])

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
