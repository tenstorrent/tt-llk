# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.dimensions import (
    calculate_matmul_dimensions,
    generate_matmul_dimension_combinations,
)
from helpers.format_arg_mapping import DestAccumulation, MathFidelity, format_dict
from helpers.format_config import DataFormat
from helpers.golden_generators import MatmulGolden, get_golden_generator
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test

# Generate all valid dimension combinations for both dest_acc modes
# When 16-bit datums in dest can fit 8 tiles and 4 tiles for 32-bit datums
ALL_MATMUL_COMBINATIONS = [
    (DestAccumulation.No, dims) for dims in generate_matmul_dimension_combinations(8)
] + [(DestAccumulation.Yes, dims) for dims in generate_matmul_dimension_combinations(4)]


@parametrize(
    test_name="matmul_test",
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float16,
            DataFormat.Float32,
        ]
    ),
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
    dest_acc_and_dims=ALL_MATMUL_COMBINATIONS,
)
def test_matmul(test_name, formats, math_fidelity, dest_acc_and_dims):
    torch_format = format_dict[formats.output_format]

    dest_acc = dest_acc_and_dims[0]
    input_A_dimensions = dest_acc_and_dims[1][0]
    input_B_dimensions = dest_acc_and_dims[1][1]

    src_A, _, tile_cnt_A = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_A_dimensions
    )
    src_B, _, tile_cnt_B = generate_stimuli(
        formats.input_format, formats.input_format, input_dimensions=input_B_dimensions
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
    )
    golden_tensor = tilize_block(
        golden_tensor,
        dimensions=matmul_dims["output_dimensions"],
        stimuli_format=formats.output_format,
    ).to(torch_format)
    golden_tensor = golden_tensor.flatten()

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

    # Use the new helper function for writing stimuli
    res_address = write_stimuli_to_l1(
        tilized_A.flatten(),
        tilized_B.flatten(),
        formats.input_format,
        formats.input_format,
        tile_cnt_A,
        tile_cnt_B,
    )

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

    run_test(test_config)

    res_from_L1 = collect_results(
        formats, tile_count=matmul_dims["output_tile_cnt"], address=res_address
    )
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
