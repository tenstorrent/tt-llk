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
from helpers.utils import passed_test


def generate_input_dimensions(max_size: int) -> list[tuple[int, int]]:
    """
    Generates a list of tuples representing width and height in tiles for input tensors,
    up to the specified maximum size in tiles.
    For tilize tensor width is important so all widths from 1 to max_size are generated.
    In the interest of reducing the number of test cases, instead of generating all possible heights
    critical subsets of valid heights are generated (three smallest, three largest and three middle heights).
    Parameters:
    max_size (int): Maximum number of tiles the resulting tensor can have.
    Returns:
    List[tuple[int, int]]: A list of tuples representing the width and height of the input tensor in tiles.
    """
    dimensions = []
    for width in range(1, max_size + 1):
        dimensions.append((width * 2, 1))
    return dimensions


@skip_for_blackhole
@parametrize(
    formats=input_output_formats([DataFormat.Float16_b]),
    dest_acc=[DestAccumulation.No],
    dimensions=generate_input_dimensions(1),
)
def test_fast_tilize(formats, dest_acc, dimensions, workers_tensix_coordinates):

    input_width, input_height = dimensions

    if formats.input == DataFormat.Bfp8_b:
        pytest.skip("Bfp8_b input format is not supported for fast tilize")

    input_dimensions = [input_height * 16, input_width * 32]

    src_A = torch.arange(
        input_dimensions[0] * input_dimensions[1],
        dtype=format_dict[formats.input_format],
    )
    tile_cnt_A = input_width
    src_B = src_A
    tile_cnt_B = tile_cnt_A

    golden_tensor = []
    for face in range(tile_cnt_A * 2):
        for row in range(16):
            base = face * 16 + row * 32 * tile_cnt_A
            golden_tensor.append(src_A[base : base + 16])

    golden_tensor = torch.cat(golden_tensor)  # Concatenate into one tensor

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
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
            num_faces=2,
        ),
        dest_acc=dest_acc,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    print(f"res_from_L1: {res_from_L1}")

    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output])

    # print(f"res_tensor: {res_tensor}")

    assert passed_test(golden_tensor, res_tensor, formats.output_format, tile_size=512)
