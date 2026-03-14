# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    ELEMENTS_PER_TILE,
    TILE_DIMENSIONS,
    EltwiseBinaryGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    DestAccumulation,
    MathFidelity,
    MathOperation,
    format_dict,
)
from helpers.param_config import (
    get_num_blocks_and_num_tiles_in_block,
    input_output_formats,
    parametrize,
)
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    MATH_FIDELITY,
    MATH_OP,
    NUM_BLOCKS,
    NUM_TILES_IN_BLOCK,
    TILE_COUNT,
    DestSync,
    generate_input_dim,
)
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test


@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16,
            DataFormat.Float16_b,
            DataFormat.Float32,
        ],  # Unpack Tilize & Pack Untilize does not work on Bfp8_b format
        same=True,
    ),
    mathop=[MathOperation.Elwadd, MathOperation.Elwsub, MathOperation.Elwmul],
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
    input_dimensions=[[32, 32], [64, 64], [64, 128]],
)
def test_tilize_calculate_untilize_L1(
    formats,
    dest_acc,
    mathop,
    math_fidelity,
    input_dimensions,
    workers_tensix_coordinates,
):

    if (
        formats.input_format == DataFormat.Float32
        and formats.output_format == DataFormat.Float32
        and (input_dimensions[0] * input_dimensions[1]) // ELEMENTS_PER_TILE > 4
    ):
        pytest.skip(
            "Float32 on both input and output with large input dimensions fails."
        )

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    # Tilize the input tensors
    tilized_A = tilize_block(
        src_A, input_dimensions, stimuli_format=formats.input_format
    ).flatten()
    tilized_B = tilize_block(
        src_B, input_dimensions, stimuli_format=formats.input_format
    ).flatten()

    generate_golden = get_golden_generator(EltwiseBinaryGolden)
    golden_tensor = generate_golden(
        mathop, tilized_A, tilized_B, formats.output_format, math_fidelity
    )

    num_blocks, num_tiles_in_block = get_num_blocks_and_num_tiles_in_block(
        DestSync.Half,
        dest_acc,
        formats,
        input_dimensions,
        TILE_DIMENSIONS,
    )

    configuration = TestConfig(
        "sources/tilize_calculate_untilize_L1.cpp",
        formats,
        templates=[
            MATH_FIDELITY(math_fidelity),
            MATH_OP(mathop=mathop),
        ],
        runtimes=[
            generate_input_dim(input_dimensions, input_dimensions),
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
        dest_acc=dest_acc,
        L1_to_L1_iterations=2,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(golden_tensor)

    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Test failed: Result does not match golden tensor"
