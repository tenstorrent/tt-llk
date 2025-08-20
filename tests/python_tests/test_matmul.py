# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch

from helpers.device import (
    BootMode,
    collect_results,
    write_stimuli_to_l1,
)
from helpers.dimensions import (
    calculate_matmul_dimensions,
)
from helpers.format_arg_mapping import DestAccumulation, MathFidelity, format_dict
from helpers.format_config import DataFormat
from helpers.golden_generators import MatmulGolden, get_golden_generator
from helpers.param_config import (
    generate_format_aware_matmul_combinations,
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import ProfilerBuild, run_test
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test

# Generate format-aware combinations
MATMUL_FORMATS = input_output_formats(
    [
        DataFormat.Float16_b,
        DataFormat.Float16,
        DataFormat.Float32,
    ]
)

DEST_ACC_MODES = [DestAccumulation.No, DestAccumulation.Yes]
ALL_MATMUL_COMBINATIONS = generate_format_aware_matmul_combinations(
    MATMUL_FORMATS, DEST_ACC_MODES
)


@parametrize(
    test_name="matmul_test",
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
    format_dest_acc_and_dims=ALL_MATMUL_COMBINATIONS,
)
# Note: this test is used to test boot modes, that is why it has them piped as default arguments to the test itself
def test_matmul(
    test_name, math_fidelity, format_dest_acc_and_dims, boot_mode=BootMode.BRISC
):
    torch_format = format_dict[format_dest_acc_and_dims[0].output_format]

    formats = format_dest_acc_and_dims[0]
    dest_acc = format_dest_acc_and_dims[1]
    input_A_dimensions = format_dest_acc_and_dims[2][0]
    input_B_dimensions = format_dest_acc_and_dims[2][1]

    src_A, _, tile_cnt_A = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_A_dimensions,
        sfpu=False,
    )
    src_B, _, tile_cnt_B = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_B_dimensions,
        sfpu=False,
    )

    # Calculate all matmul dimensions using helper function
    matmul_dims = calculate_matmul_dimensions(input_A_dimensions, input_B_dimensions)

    generate_golden = get_golden_generator(MatmulGolden)
    golden_tensor = generate_golden(
        src_A,
        src_B,
        formats.output_format,
        math_fidelity,
        input_A_dimensions=input_A_dimensions,
        input_B_dimensions=input_B_dimensions,
        tilize=True,  # Golden cannot model FPU strided for tilized data computation, so we tilize output after computation
    )

    if formats.input_format != DataFormat.Bfp8_b:
        tilized_A = tilize_block(
            src_A, dimensions=input_A_dimensions, stimuli_format=formats.input_format
        )
        tilized_B = tilize_block(
            src_B, dimensions=input_B_dimensions, stimuli_format=formats.input_format
        )
    else:
        # BFP8 format requires special handling for tilization
        tilized_A = src_A
        tilized_B = src_B

    test_config = {
        "formats": formats,
        "testname": test_name,
        "dest_acc": dest_acc,
        "math_fidelity": math_fidelity,
        "tile_cnt": matmul_dims["output_tile_cnt"],
        "input_A_dimensions": input_A_dimensions,
        "input_B_dimensions": input_B_dimensions,
        "output_dimensions": matmul_dims["output_dimensions"],
        "rt_dim": matmul_dims["rt_dim"],
        "ct_dim": matmul_dims["ct_dim"],
        "kt_dim": matmul_dims["kt_dim"],
    }

    # Use the new helper function for writing stimuli
    res_address = write_stimuli_to_l1(
        test_config,
        tilized_A.flatten(),
        tilized_B.flatten(),
        formats.input_format,
        formats.input_format,
        tile_cnt_A,
        tile_cnt_B,
    )

    run_test(test_config, ProfilerBuild.No, boot_mode)

    res_from_L1 = collect_results(
        formats, tile_count=matmul_dims["output_tile_cnt"], address=res_address
    )
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
