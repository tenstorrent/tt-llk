# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import List

import pytest
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.device import BootMode
from helpers.format_config import DataFormat, FormatConfig, is_dest_acc_needed
from helpers.golden_generators import MatmulGolden, get_golden_generator
from helpers.llk_params import DestAccumulation, MathFidelity, format_dict
from helpers.matmul_sweep import (
    generate_matmul_dimension_combinations,
    generate_tile_dims,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test


def generate_format_aware_matmul_combinations(
    formats_list: List[FormatConfig],
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

    for fmt in formats_list:
        base_max_tiles = 4 if is_dest_acc_needed(fmt) else 8

        for dest_acc in dest_acc_modes:
            max_tiles = 4 if dest_acc == DestAccumulation.Yes else base_max_tiles
            dimensions_list = generate_matmul_dimension_combinations(max_tiles)
            combinations.extend([(fmt, dest_acc, dims) for dims in dimensions_list])

    return combinations


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


# Skip if not Blackhole
@pytest.mark.skipif(
    get_chip_architecture() != ChipArchitecture.BLACKHOLE,
    reason="Custom matmul test only runs on Blackhole architecture",
)
@parametrize(
    cpp_test_source=["sources/matmul_custom_test.cpp"],
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
    format_dest_acc_and_dims=ALL_MATMUL_COMBINATIONS,
)
def test_matmul_custom(
    cpp_test_source,
    math_fidelity,
    format_dest_acc_and_dims,
    workers_tensix_coordinates,
    boot_mode=BootMode.DEFAULT,
):
    torch_format = format_dict[format_dest_acc_and_dims[0].output_format]

    formats = format_dest_acc_and_dims[0]
    dest_acc = format_dest_acc_and_dims[1]
    input_A_dimensions = format_dest_acc_and_dims[2][0]
    input_B_dimensions = format_dest_acc_and_dims[2][1]

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_A_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_B_dimensions,
        sfpu=False,
        debug=False,
    )

    test_config = TestConfig(
        test_name=cpp_test_source,
        test_params={
            "RT_DIM": input_A_dimensions[0] // 32,
            "CT_DIM": input_B_dimensions[1] // 32,
            "KT_DIM": input_A_dimensions[1] // 32,
            "num_faces_A": generate_tile_dims(formats.input_format)[0],
            "num_faces_B": generate_tile_dims(formats.input_format)[0],
            "MATH_FIDELITY": math_fidelity,
            "TILE_CNT": tile_cnt_A,
        },
        formats=formats,
        dest_acc=dest_acc,
        stimuli=[
            StimuliConfig(name="A", address=0x1A000, dtype=torch_format, data=src_A),
            StimuliConfig(name="B", address=0x1B000, dtype=torch_format, data=src_B),
        ],
        workers_tensix_coordinates=workers_tensix_coordinates,
        boot_mode=boot_mode,
    )

    test_config.run_and_wait()

    res = tilize_block(test_config.read_result(0x1C000, tile_cnt_A, torch_format))

    golden = get_golden_generator(test_config, MatmulGolden)
    expected = golden(src_A, src_B, input_A_dimensions, input_B_dimensions)

    assert passed_test(res, expected)
