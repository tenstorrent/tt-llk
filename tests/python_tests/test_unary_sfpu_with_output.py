# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0
#
# Test for _llk_math_eltwise_unary_sfpu_params_with_output_
# Validates that unary SFPU abs can write to a different dest tile than the input.
# The test loads 1 tile to dest[0], applies abs from dest[0]->dest[1], packs both.
# Checks: tile 1 == abs(input), tile 0 is preserved (all negative values stay negative).

import torch
from helpers.format_config import DataFormat
from helpers.golden_generators import UnarySFPUGolden, get_golden_generator
from helpers.llk_params import DestAccumulation, MathOperation, format_dict
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import TILE_COUNT, generate_input_dim
from helpers.utils import passed_test


@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    dest_acc=[DestAccumulation.Yes],
)
def test_unary_sfpu_with_output(formats, dest_acc, workers_tensix_coordinates):
    """Test that unary SFPU abs writes to a separate output tile while preserving input."""
    torch.manual_seed(42)

    input_dimensions = [32, 32]  # 1 tile

    # Generate stimuli with negative values so abs produces a visible change
    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    # Make all input values negative to clearly test abs and preservation
    src_A = -torch.abs(src_A.float())

    # Generate golden for the abs result (tile 1)
    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_abs = generate_golden(
        MathOperation.Abs,
        src_A,
        formats.output_format,
        dest_acc,
        formats.input_format,
        input_dimensions,
    )

    configuration = TestConfig(
        "sources/unary_sfpu_with_output_test.cpp",
        formats,
        templates=[
            generate_input_dim(input_dimensions, input_dimensions),
        ],
        runtimes=[TILE_COUNT(tile_cnt_A)],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=2,  # 2 output tiles: preserved input + abs result
        ),
        dest_acc=dest_acc,
        unpack_to_dest=(
            formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
        ),
        compile_time_formats=True,
    )
    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    torch_format = format_dict[formats.output_format]

    # Split result into 2 tiles (1024 elements each for 32x32)
    elements_per_tile = 1024
    assert (
        len(res_from_L1) >= 2 * elements_per_tile
    ), f"Expected at least {2 * elements_per_tile} result elements, got {len(res_from_L1)}"

    tile_0 = torch.tensor(res_from_L1[:elements_per_tile], dtype=torch_format)
    tile_1 = torch.tensor(
        res_from_L1[elements_per_tile : 2 * elements_per_tile], dtype=torch_format
    )

    # Check 1: tile 1 (abs result) matches golden
    assert passed_test(
        golden_abs, tile_1, formats.output_format
    ), "Tile 1 (abs result) does not match golden"

    # Check 2: tile 0 (preserved input) should have all non-positive values
    # (since we made all inputs negative, if tile 0 was overwritten by abs it would be positive)
    num_positive = (tile_0 > 0).sum().item()
    assert num_positive == 0, (
        f"Tile 0 (preserved input) has {num_positive} positive values - "
        "input tile was corrupted by the abs operation"
    )
