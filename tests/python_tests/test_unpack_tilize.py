# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0
import pytest
import torch
from helpers.format_config import DataFormat
from helpers.golden_generators import TilizeGolden, get_golden_generator
from helpers.llk_params import DestAccumulation, DestSync, format_dict
from helpers.param_config import (
    get_num_blocks,
    get_num_tiles_in_block,
    input_output_formats,
    parametrize,
)
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    INPUT_DIMENSIONS,
    NUM_BLOCKS,
    NUM_TILES_IN_BLOCK,
    TILE_COUNT,
)
from helpers.utils import passed_test


# TODO: Extend this test to accept input dimensions larger than dest register.
@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float16,
            DataFormat.Float32,
            DataFormat.Bfp8_b,  # Unpack Tilize doesn't work for block float formats (Bfp8_b) due to shared exponent at start of input tensor
        ]
    ),
    input_dimensions=[[32, 32], [64, 64]],
)
def test_unpack_tilize_float(formats, input_dimensions, workers_tensix_coordinates):
    if formats.input_format == DataFormat.Bfp8_b:
        pytest.skip("Unpack Tilize does not support Bfp8_b input format")

    unpack_tilize(formats, input_dimensions, workers_tensix_coordinates)


# TODO: Extend this test to accept input dimensions larger than dest register.
@parametrize(
    formats=input_output_formats([DataFormat.Int32]),
    input_dimensions=[[32, 32], [64, 64]],
)
def test_unpack_tilize_int(formats, input_dimensions, workers_tensix_coordinates):
    unpack_tilize(formats, input_dimensions, workers_tensix_coordinates)


def unpack_tilize(formats, input_dimensions, workers_tensix_coordinates):

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    generate_golden = get_golden_generator(TilizeGolden)
    golden_tensor = generate_golden(src_A, input_dimensions, formats.output_format)

    # Calculate block parameters for destination register banking
    num_blocks = get_num_blocks(
        dest_sync=DestSync.Half,
        dest_acc=DestAccumulation.No,
        formats=formats,
        input_dimensions=input_dimensions,
        tile_dimensions=[32, 32],
    )

    num_tiles_in_block = get_num_tiles_in_block(
        dest_sync=DestSync.Half,
        dest_acc=DestAccumulation.No,
        formats=formats,
        input_dimensions=input_dimensions,
        tile_dimensions=[32, 32],
    )

    configuration = TestConfig(
        "sources/unpack_tilize_test.cpp",
        formats,
        templates=[INPUT_DIMENSIONS(input_dimensions, input_dimensions)],
        runtimes=[
            TILE_COUNT(tile_cnt_A),
            NUM_BLOCKS(num_blocks),
            NUM_TILES_IN_BLOCK(num_tiles_in_block),
        ],
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
        unpack_to_dest=(formats.input_format == DataFormat.Int32),
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golder tensor are not of the same length"

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
