# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.chip_architecture import ChipArchitecture
from helpers.format_config import DataFormat
from helpers.llk_params import DestAccumulation, MaxMode, format_dict
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    INPUT_DIMENSIONS,
    MAX_MODE,
    TILE_COUNT,
)
from helpers.utils import passed_test


@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float32,
            DataFormat.Float16,
            DataFormat.Float16_b,
            DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    input_dimensions=[
        [32, 32],
        [32, 64],
        [64, 32],
        [64, 64],
    ],  # Base dimensions for srcA and srcB (each 1 tile)
    max_mode=[MaxMode.FullTile, MaxMode.Row0],
)
def test_sfpu_eltwise_max_float(
    formats, dest_acc, input_dimensions, max_mode, workers_tensix_coordinates
):
    if (
        TestConfig.CHIP_ARCH == ChipArchitecture.BLACKHOLE
        and formats.input_format == DataFormat.Float16
        and dest_acc == DestAccumulation.No
    ):
        pytest.skip(
            "Float16_a isn't supported for SFPU on Blackhole without being converted to 32-bit intermediate format in dest register"
        )

    sfpu_eltwise_max(
        formats, dest_acc, input_dimensions, max_mode, workers_tensix_coordinates
    )


def get_row0_indices_per_tile():
    """Get tilized indices for row 0 of faces 0 and 1 in a 32x32 tile.

    Tile layout (1024 elements):
    - Face 0 (indices 0-255): rows 0-15, cols 0-15
    - Face 1 (indices 256-511): rows 0-15, cols 16-31
    - Face 2 (indices 512-767): rows 16-31, cols 0-15
    - Face 3 (indices 768-1023): rows 16-31, cols 16-31

    Row 0 within each face is the first 16 elements.
    """
    face0_row0 = list(range(0, 16))  # Face 0, row 0: cols 0-15
    face1_row0 = list(range(256, 272))  # Face 1, row 0: cols 16-31
    return face0_row0 + face1_row0


def sfpu_eltwise_max(
    formats, dest_acc, input_dimensions, max_mode, workers_tensix_coordinates
):

    # Generate stimuli - src_A and src_B each have input_dimensions
    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    golden_full = torch.maximum(src_A, src_B).flatten()
    src_A_combined = torch.cat([src_A, src_B], dim=0)

    # Output tile count = input tile count (one output per A,B pair)
    output_tile_cnt = tile_cnt_A

    # Tile counts: srcA has 2x tiles (A tiles + B tiles), srcB not used
    tile_cnt_A = tile_cnt_A * 2
    tile_cnt_B = 0

    # Dimensions for combined srcA (doubled in first dimension)
    srcA_dimensions = [input_dimensions[0] * 2, input_dimensions[1]]

    # Force dest_acc for certain formats
    if formats.input_format in [DataFormat.Float16, DataFormat.Float32]:
        dest_acc = DestAccumulation.Yes

    configuration = TestConfig(
        "sources/sfpu_eltwise_max_test.cpp",
        formats,
        templates=[
            INPUT_DIMENSIONS(srcA_dimensions, input_dimensions),
            MAX_MODE(max_mode),
        ],
        runtimes=[
            TILE_COUNT(tile_cnt_A),
        ],
        variant_stimuli=StimuliConfig(
            src_A_combined,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=output_tile_cnt,
        ),
        dest_acc=dest_acc,
        unpack_to_dest=formats.input_format.is_32_bit(),
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format).flatten()

    if max_mode == MaxMode.FullTile:
        # Full tile mode: compare entire tensor
        golden_tensor = golden_full
        assert len(res_tensor) == len(
            golden_tensor
        ), "Result tensor and golden tensor are not of the same length"
        assert passed_test(
            golden_tensor, res_tensor, formats.output_format
        ), "Assert against golden failed"
    else:
        # Row0 mode: only compare row 0 of faces 0 and 1 per tile
        row0_indices = get_row0_indices_per_tile()

        # Build indices for all tiles
        all_row0_indices = []
        for tile_idx in range(output_tile_cnt):
            tile_offset = tile_idx * 1024  # 1024 elements per tile
            all_row0_indices.extend([tile_offset + i for i in row0_indices])

        golden_row0 = golden_full[all_row0_indices]
        res_row0 = res_tensor[all_row0_indices]

        assert passed_test(
            golden_row0, res_row0, formats.output_format
        ), "Assert against golden failed for Row0 mode"
