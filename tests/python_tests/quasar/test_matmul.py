# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import List

import pytest
import torch
from helpers.device import BootMode, collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
from helpers.golden_generators import MatmulGolden, get_golden_generator
from helpers.llk_params import DestAccumulation, MathFidelity, format_dict
from helpers.matmul_sweep import (
    generate_matmul_dimension_combinations,
    generate_tile_dims,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test

from tests.python_tests.helpers.golden_generators import TransposeGolden
from tests.python_tests.helpers.llk_params import (
    DestSync,
    ImpliedMathFormat,
    Transpose,
)


def generate_format_aware_matmul_combinations(
    dest_acc_modes: List[DestAccumulation],
):
    """
    Generate matmul dimension combinations for multiple tiles.

    Rules:
    1. Format outliers (Float16_b->Float16, Bfp8_b->Float16) MUST use dest_acc=Yes
    2. Running matmul tests on DestSync.Half, max tile count is 8
    3. When dest_acc=Yes: max 4 tiles (32-bit dest register)
    4. When dest_acc=No: max 8 tiles (16-bit dest register)

    Returns: List of (format, dest_acc, dimensions) tuples
    """
    combinations = []

    for dest_acc in dest_acc_modes:
        max_tiles = 4 if dest_acc == DestAccumulation.Yes else 8
        dimensions_list = generate_matmul_dimension_combinations(max_tiles)
        combinations.extend([(dest_acc, dims) for dims in dimensions_list])

    return combinations


# Generate format-aware combinations
MATMUL_FORMAT = input_output_formats(
    [
        DataFormat.Float16_b,
        # DataFormat.Float16,
        # DataFormat.Float32,
    ],
    same=True,
)
DEST_ACC_MODES = [DestAccumulation.No, DestAccumulation.Yes]
MATMUL_DIMENSIONS_AND_DEST_ACC_MODES = generate_format_aware_matmul_combinations(
    DEST_ACC_MODES
)
IMPLIED_MATH_FORMAT = [ImpliedMathFormat.No, ImpliedMathFormat.Yes]
DEST_SYNC_MODES = ([DestSync.Half],)  # , DestSync.Full]
TRANSPOSE_MODES = [Transpose.Yes]  # , Transpose.No]


@parametrize(
    test_name="matmul_test",
    implied_math_format=IMPLIED_MATH_FORMAT,
    math_fidelity=[
        MathFidelity.LoFi,
        # MathFidelity.HiFi2,
        # MathFidelity.HiFi3,
        # MathFidelity.HiFi4,
    ],
    dimensions_and_dest_acc_mode=MATMUL_DIMENSIONS_AND_DEST_ACC_MODES,
    format=MATMUL_FORMAT,
    dest_sync_mode=DEST_SYNC_MODES,
    transpose=TRANSPOSE_MODES,
)
# Note: this test is used to test boot modes, that is why it has them piped as default arguments to the test itself
def test_matmul(
    test_name,
    math_fidelity,
    dimensions_and_dest_acc_mode,
    format,
    implied_math_format,
    dest_sync_mode,
    transpose,
    boot_mode=BootMode.TRISC,
):
    dest_acc, (input_A_dimensions, input_B_dimensions) = dimensions_and_dest_acc_mode

    if (format.input_format, dest_acc) in [
        (DataFormat.Float16, DestAccumulation.Yes),
        (DataFormat.Float32, DestAccumulation.No),
    ]:
        pytest.skip(
            "Float16 with 32-bit dest or Float32 without 32-bit dest is not supported"
        )

    torch_format = format_dict[format.output_format]

    src_A, _, tile_cnt_A = generate_stimuli(
        format.input_format,
        format.input_format,
        input_dimensions=input_A_dimensions,
        sfpu=False,
    )
    src_B, _, tile_cnt_B = generate_stimuli(
        format.input_format,
        format.input_format,
        input_dimensions=input_B_dimensions,
        sfpu=False,
    )

    src_B_golden = src_B
    if transpose == Transpose.Yes:
        # Manually register TransposeGolden if not already registered
        from helpers.golden_generators import golden_registry

        if TransposeGolden not in golden_registry:
            golden_registry[TransposeGolden] = TransposeGolden()
        t_matrix = get_golden_generator(TransposeGolden)

        src_B_golden = t_matrix.transpose_faces_multi_tile(
            src_B,
            format.input_format,
            num_tiles=tile_cnt_B,
            tilize=True,
            input_dimensions=input_B_dimensions,
        )
        src_B_golden = t_matrix.transpose_within_faces_multi_tile(
            src_B_golden,
            format.input_format,
            num_tiles=tile_cnt_B,
            untilize=True,
            input_dimensions=input_B_dimensions,
        )

    generate_golden = get_golden_generator(MatmulGolden)
    golden_tensor = generate_golden(
        src_A,
        src_B_golden,
        format.output_format,
        math_fidelity,
        input_A_dimensions=input_A_dimensions,
        input_B_dimensions=input_B_dimensions,
        tilize=True,  # Golden cannot model FPU strided for tilized data computation, so we tilize output after computation
    )

    # Calculate all matmul dimensions using helper function
    matmul_dims = generate_tile_dims((input_A_dimensions, input_B_dimensions))

    generate_golden = get_golden_generator(MatmulGolden)
    golden_tensor = generate_golden(
        src_A,
        src_B,
        format.output_format,
        math_fidelity,
        input_A_dimensions=input_A_dimensions,
        input_B_dimensions=input_B_dimensions,
        tilize=True,  # Golden cannot model FPU strided for tilized data computation, so we tilize output after computation
    )

    tilized_A = tilize_block(
        src_A, dimensions=input_A_dimensions, stimuli_format=format.input_format
    )
    tilized_B = tilize_block(
        src_B, dimensions=input_B_dimensions, stimuli_format=format.input_format
    )

    test_config = {
        "formats": format,
        "testname": test_name,
        "dest_acc": dest_acc,
        "math_fidelity": math_fidelity,
        "tile_cnt": matmul_dims.output_tile_cnt,
        "input_A_dimensions": input_A_dimensions,
        "input_B_dimensions": input_B_dimensions,
        "output_dimensions": matmul_dims.output_dimensions,
        "rt_dim": matmul_dims.rt_dim,
        "ct_dim": matmul_dims.ct_dim,
        "kt_dim": matmul_dims.kt_dim,
        "implied_math_format": implied_math_format,
        "dest_sync_mode": dest_sync_mode,
    }

    # Use the new helper function for writing stimuli
    res_address = write_stimuli_to_l1(
        test_config,
        tilized_A.flatten(),
        tilized_B.flatten(),
        format.input_format,
        format.input_format,
        tile_cnt_A,
        tile_cnt_B,
    )

    run_test(test_config, boot_mode)

    res_from_L1 = collect_results(
        format, tile_count=matmul_dims.output_tile_cnt, address=res_address
    )
    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, format.output_format)
