# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0


import pytest
import torch
from conftest import skip_for_blackhole
from helpers.format_config import DataFormat
from helpers.llk_params import DestAccumulation, format_dict
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    INPUT_DIMENSIONS,
    LOOP_FACTOR,
    TILE_COUNT,
)
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test

# Width in tiles (16x32); height fixed at 1 row of tiles
WIDTHS = [
    2,
    10,
    18,
    56,
    224,
]  # base case, two banks, three banks, and Deepseek model sizes


@skip_for_blackhole
@parametrize(
    formats=input_output_formats([DataFormat.Float16_b]),
    dest_acc=[DestAccumulation.No],
    dimensions=[(w, 1) for w in WIDTHS],
)
def test_fast_tilize_tiny_tiles(
    formats, dest_acc, dimensions, workers_tensix_coordinates
):
    input_width, input_height = dimensions

    if formats.input == DataFormat.Bfp8_b:
        pytest.skip("Bfp8_b input format is not supported for fast tilize")

    # Tiny tiles: 16 rows x 32 cols per tile (2 faces instead of 4)
    input_dimensions = [input_height * 16, input_width * 32]

    # For tiny tiles, tile count is calculated differently:
    # Each tile is 16 rows x 32 cols = 512 elements (2 faces of 16x16)
    tile_cnt_A = input_height * input_width
    tile_cnt_B = tile_cnt_A

    # Generate stimuli directly (generate_stimuli assumes 32x32 tiles)
    num_elements = input_dimensions[0] * input_dimensions[1]
    src_A = torch.arange(num_elements, dtype=format_dict[formats.input_format])
    src_B = src_A

    # Use tilize_block directly for tiny tiles (TilizeGolden has a bug with num_faces < 4)
    golden_tensor = tilize_block(
        src_A, input_dimensions, formats.output, num_faces=2
    ).flatten()

    # For fast tilize, input is row-major (untilized), written contiguously.
    # Calculate how many "full tile equivalents" (1024 elements each) we need to write.
    # For Float16_b, 1024 elements = 2048 bytes = one full tile spacing, so this writes contiguously.
    write_tile_count = (num_elements + 1023) // 1024  # Ceiling division

    configuration = TestConfig(
        "sources/fast_tilize_test.cpp",
        formats,
        templates=[INPUT_DIMENSIONS(input_dimensions, input_dimensions)],
        runtimes=[TILE_COUNT(tile_cnt_A), LOOP_FACTOR(1)],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=write_tile_count,  # Write all input data contiguously
            tile_count_B=write_tile_count,
            tile_count_res=tile_cnt_A,
            num_faces=2,
            write_full_tiles=True,
        ),
        dest_acc=dest_acc,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)
    # Force full tensor print even when very large (no truncation)
    # torch.set_printoptions(threshold=sys.maxsize)
    # print(res_from_L1)
    # torch.set_printoptions(threshold=1000)  # restore default

    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output])

    assert passed_test(golden_tensor, res_tensor, formats.output_format, tile_size=512)
