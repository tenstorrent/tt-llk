# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import pytest
import torch

from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.device import (
    collect_results,
    run_elf_files,
    wait_for_tensix_operations_finished,
    write_stimuli_to_l1,
)
from helpers.format_arg_mapping import (
    ApproximationMode,
    DestAccumulation,
    MathFidelity,
    MathOperation,
    format_dict,
)
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    MatmulGolden,
    UnarySFPUGolden,
    get_golden_generator,
)
from helpers.param_config import (
    clean_params,
    generate_param_ids,
    generate_params,
    input_output_formats,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import generate_make_command
from helpers.tilize_untilize import tilize
from helpers.utils import passed_test, run_shell_command

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

test_formats = input_output_formats(supported_formats, same=True)
all_params = generate_params(
    ["matmul_and_unary_sfpu_test"],
    test_formats,
    dest_acc=[DestAccumulation.Yes, DestAccumulation.No],
    approx_mode=[ApproximationMode.No, ApproximationMode.Yes],
    mathop=[
        MathOperation.Abs,
        MathOperation.Cos,
        MathOperation.Log,
        MathOperation.Reciprocal,
        MathOperation.Sin,
        MathOperation.Sqrt,
        MathOperation.Square,
    ],
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
)
param_ids = generate_param_ids(all_params)


@pytest.mark.parametrize(
    "testname, formats, dest_acc, approx_mode, mathop, math_fidelity",
    clean_params(all_params),
    ids=param_ids,
)
def test_matmul_and_unary_sfpu(
    testname, formats, dest_acc, approx_mode, mathop, math_fidelity
):
    if mathop in [MathOperation.Cos, MathOperation.Sin]:
        pytest.skip("Cos and Sin operations are not fully functional yet")
    if mathop == MathOperation.Square and math_fidelity == MathFidelity.LoFi:
        pytest.skip("Square operation in LoFi is not fully functional yet")
    if (
        formats.input_format == formats.output_format == DataFormat.Float16
        and mathop in [MathOperation.Log, MathOperation.Sqrt, MathOperation.Square]
        and dest_acc == DestAccumulation.No
        and get_chip_architecture() == ChipArchitecture.BLACKHOLE
    ):
        pytest.skip("BFP8 does not support Log and Reciprocal operations")

    torch_format = format_dict.get(formats.output_format)
    src_A, src_B = generate_stimuli(formats.input_format, formats.input_format)

    generate_matmul_golden = get_golden_generator(MatmulGolden)
    golden_tensor = generate_matmul_golden(
        src_A, src_B, formats.output_format, math_fidelity
    )
    golden_tensor = tilize(golden_tensor, formats.output_format)
    generate_sfpu_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_sfpu_golden(mathop, golden_tensor, formats.output_format)
    golden_tensor = golden_tensor.to(torch_format)

    write_stimuli_to_l1(
        tilize(src_A, formats.input_format),
        tilize(src_B, formats.input_format),
        formats.input_format,
        formats.input_format,
    )

    test_config = {
        "formats": formats,
        "testname": testname,
        "dest_acc": dest_acc,
        "math_fidelity": math_fidelity,
        "approx_mode": approx_mode,
        "mathop": mathop,
    }

    make_cmd = generate_make_command(test_config)
    run_shell_command(f"cd .. && {make_cmd}")

    run_elf_files(testname)

    wait_for_tensix_operations_finished()
    buffer_dest_address = 0x1E000
    res_from_L1 = collect_results(
        formats, tensor_size=len(src_A), address=buffer_dest_address
    )

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
