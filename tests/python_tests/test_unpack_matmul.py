# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch

from helpers.device import (
    collect_results,
    write_stimuli_to_l1,
)
from helpers.dimensions import (
    calculate_matmul_dimensions,
)
from helpers.format_arg_mapping import (
    DestAccumulation,
    MathFidelity,
    StochasticRounding,
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
from helpers.stimuli_generator import generate_face_matmul_data
from helpers.test_config import run_test
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
STOCHASTIC_ROUNDING_MODES = [
    StochasticRounding.No,
    StochasticRounding.Fpu,
    StochasticRounding.Pack,
    StochasticRounding.All,
]

ALL_MATMUL_COMBINATIONS = generate_format_aware_matmul_combinations(
    MATMUL_FORMATS,
    DEST_ACC_MODES,
    STOCHASTIC_ROUNDING_MODES,
    face_modes=[1, 2, 4],  # Test all face modes
    transpose_modes=[
        Transpose.No,
        Transpose.Yes,
    ],  # Test both transpose modes (but only when num_faces=4)
)


@pytest.mark.nightly
@parametrize(
    test_name="unpack_matmul_test",
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
    format_dest_acc_and_dims=ALL_MATMUL_COMBINATIONS,
)
def test_unpack_matmul(test_name, math_fidelity, format_dest_acc_and_dims):

    formats = format_dest_acc_and_dims[0]
    dest_acc = format_dest_acc_and_dims[1]

    input_A_dimensions = format_dest_acc_and_dims[2][0]
    input_B_dimensions = format_dest_acc_and_dims[2][1]
    stochastic_rnd = format_dest_acc_and_dims[3]
    num_faces = format_dest_acc_and_dims[4]  # Get num_faces from combination
    transpose = format_dest_acc_and_dims[
        5
    ]  # Get transpose from combination (already a Transpose enum)
    partial_face = format_dest_acc_and_dims[6]  # Get partial_face from combination
    torch_format = format_dict[formats.output_format]

    # Calculate all matmul dimensions using helper function
    matmul_dims = calculate_matmul_dimensions(input_A_dimensions, input_B_dimensions)

    tile_cnt_A = input_A_dimensions[0] // 32 * input_A_dimensions[1] // 32
    tile_cnt_B = input_B_dimensions[0] // 32 * input_B_dimensions[1] // 32

    # Generate test data for all tiles with the right faces zeroed out
    src_A = generate_face_matmul_data(
        num_faces=num_faces,
        stimuli_format=formats.input_format,
        input_dimensions=input_A_dimensions,  # This will generate the right number of tiles
        is_matrix_A=True,  # Matrix A (SrcB) uses f0,f2 for 2-face mode
    )
    src_B = generate_face_matmul_data(
        num_faces=num_faces,
        stimuli_format=formats.input_format,
        input_dimensions=input_B_dimensions,  # This will generate the right number of tiles
        is_matrix_A=False,  # Matrix B (SrcA) uses f0,f1 for 2-face mode
    )

    src_B_golden = src_B
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

    # Generate standard golden reference (PCC validation will handle stochastic tolerance)
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
        "stochastic_rnd": stochastic_rnd,
        "unpack_transpose_faces": transpose,
        "unpack_transpose_within_face": transpose,  # matmul transposes both faces and within faces, there is no option for one or the other
        "num_faces": num_faces,  # Number of active faces to process
        "partial_face": partial_face,  # Get partial_face from combination
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

    bfloat_in_dest = formats.input_format in [
        DataFormat.Float16_b,
        DataFormat.Float32,
    ] and formats.output_format in [
        DataFormat.Float16_b,
        DataFormat.Float32,
    ]  # according to data format inference model

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    # Only compare the active faces in each tile since that's what the hardware processes
    num_elements_per_tile = num_faces * 256  # Each face is 16x16 = 256 elements
    tile_cnt = matmul_dims["output_tile_cnt"]

    # Compare each tile separately
    for i in range(tile_cnt):
        start = i * 1024  # Each tile is 1024 elements
        assert passed_test(
            golden_tensor[
                start : start + num_elements_per_tile
            ],  # Only compare active faces in this tile
            res_tensor[
                start : start + num_elements_per_tile
            ],  # Only compare active faces in this tile
            formats.output_format,
        )
