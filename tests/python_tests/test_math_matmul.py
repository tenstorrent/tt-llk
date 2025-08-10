# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.dimensions import (
    calculate_matmul_dimensions,
)
from helpers.format_arg_mapping import (
    MathFidelity,
    Transpose,
    format_dict,
)
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    MatmulGolden,
    TransposeGolden,
    get_golden_generator,
)
from helpers.param_config import (
    generate_format_aware_matmul_combinations,
    input_output_formats,
    parametrize,
)
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test

MATMUL_FORMATS = input_output_formats(
    [
        DataFormat.Float16_b,
        DataFormat.Float16,
        DataFormat.Float32,
    ]
)
ALL_MATMUL_COMBINATIONS = generate_format_aware_matmul_combinations(MATMUL_FORMATS)


@parametrize(
    test_name="math_matmul_test",
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
    transpose=[Transpose.Yes, Transpose.No],
    format_dest_acc_and_dims=ALL_MATMUL_COMBINATIONS,
    throttle=list(
        range(0, 6)
    ),  # Throttle levels include 1-->5, level 0 doesn't throttle
)
def test_matmul(
    test_name, math_fidelity, transpose, format_dest_acc_and_dims, throttle
):

    formats = format_dest_acc_and_dims[0]
    dest_acc = format_dest_acc_and_dims[1]
    input_A_dimensions = format_dest_acc_and_dims[2][0]
    input_B_dimensions = format_dest_acc_and_dims[2][1]

    torch_format = format_dict[formats.output_format]

    # Calculate all matmul dimensions using helper function
    matmul_dims = calculate_matmul_dimensions(input_A_dimensions, input_B_dimensions)

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

    if transpose == Transpose.Yes:
        t_matrix = get_golden_generator(TransposeGolden)

        src_B_golden = t_matrix.transpose_faces_multi_tile(
            src_B,
            formats.input_format,
            num_tiles=tile_cnt_B,
            tilize=True,
            input_dimensions=input_B_dimensions,
        )
        src_B_golden = t_matrix.transpose_within_faces_multi_tile(
            src_B_golden,
            formats.input_format,
            num_tiles=tile_cnt_B,
            untilize=True,
            input_dimensions=input_B_dimensions,
        )

    generate_golden = get_golden_generator(MatmulGolden)
    golden_tensor = generate_golden(
        src_A,
        src_B_golden,
        formats.output_format,
        math_fidelity,
        input_A_dimensions=input_A_dimensions,
        input_B_dimensions=input_B_dimensions,
        tilize=True,  # Golden cannot model FPU strided for tilized data computation, so we tilize output after computation
    )

    tilized_A = tilize_block(
        src_A, dimensions=input_A_dimensions, stimuli_format=formats.input_format
    )
    tilized_B = tilize_block(
        src_B, dimensions=input_B_dimensions, stimuli_format=formats.input_format
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
        "unpack_transpose_faces": transpose.value,
        "unpack_transpose_within_face": transpose.value,  # matmul transposes both faces and within faces, there is no option for one or the other
        "throttle": throttle,
    }

    res_address = write_stimuli_to_l1(
        test_config,
        tilized_A.flatten(),
        tilized_B.flatten(),
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt_A,
        tile_count_B=tile_cnt_B,
    )

    run_test(test_config)

    res_from_L1 = collect_results(
        formats, tile_count=matmul_dims["output_tile_cnt"], address=res_address
    )
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
