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
    MathOperation,
    format_dict,
)
from helpers.format_config import DataFormat
from helpers.golden_generators import EltwiseBinaryGolden, get_golden_generator
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
supported_formats = [DataFormat.Bfp8_b, DataFormat.Float16, DataFormat.Float16_b]

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
    ["multiple_tiles_eltwise_test"],
    test_formats,
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    mathop=[MathOperation.Elwadd, MathOperation.Elwsub, MathOperation.Elwmul],
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
)
param_ids = generate_param_ids(all_params)


@pytest.mark.parametrize("input_dimensions", [[32, 32], [32, 64], [64, 64]])
@pytest.mark.parametrize(
    "testname, formats, dest_acc, mathop, math_fidelity",
    clean_params(all_params),
    ids=param_ids,
)
def test_multiple_tiles(
    testname, formats, dest_acc, mathop, math_fidelity, input_dimensions
):

    if mathop != MathOperation.Elwmul and math_fidelity != MathFidelity.LoFi:
        pytest.skip("Fidelity does not affect Elwadd and Elwsub operations")

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_dimensions
    )

    generate_golden = get_golden_generator(EltwiseBinaryGolden)
    golden_tensor = generate_golden(
        mathop, src_A, src_B, formats.output_format, math_fidelity
    )
    res_address = write_stimuli_to_l1(
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        "0,0",
        tile_count=tile_cnt,
    )

    test_config = {
        "formats": formats,
        "testname": testname,
        "dest_acc": dest_acc,
        "mathop": mathop,
        "math_fidelity": math_fidelity,
        "tile_cnt": tile_cnt,
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")

    run_elf_files(testname)
    wait_for_tensix_operations_finished()

    res_from_L1 = collect_results(formats, tile_count=tile_cnt, address=res_address)
    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
