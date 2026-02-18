# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from conftest import skip_for_blackhole
from helpers.format_config import DataFormat
from helpers.golden_generators import TilizeGolden, get_golden_generator
from helpers.llk_params import DestAccumulation, format_dict
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    INPUT_DIMENSIONS,
    LOOP_FACTOR,
    NUM_FACES,
    TILE_COUNT,
)
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
    formats=input_output_formats([DataFormat.Float32, DataFormat.Float16_b]),
    dest_acc=[DestAccumulation.Yes, DestAccumulation.No],
    dimensions=[(1, w) for w in WIDTHS],
)
def test_fast_tilize_tiny_tiles(
    formats, dest_acc, dimensions, workers_tensix_coordinates
):

    if (
        formats.input == DataFormat.Float32 or formats.output == DataFormat.Float32
    ) and dimensions[1] == 224:
        pytest.skip("Can't do 226 tiles for Float32")

    input_width, input_height = dimensions

    input_dimensions = [input_height * 16, input_width * 32]

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    generate_golden = get_golden_generator(TilizeGolden)
    golden_tensor = generate_golden(src_A, input_dimensions, formats.output)

    configuration = TestConfig(
        "sources/fast_tilize_test.cpp",
        formats,
        templates=[INPUT_DIMENSIONS(input_dimensions, input_dimensions)],
        runtimes=[TILE_COUNT(tile_cnt_A), LOOP_FACTOR(1), NUM_FACES(4)],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
        ),
        dest_acc=dest_acc,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output])

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
