# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Simple SFPU abs test.

Pipeline:
1. Unpack srcA
2. Datacopy srcA to dest
3. SFPU abs on dest
4. Pack result to L1

Configuration:
- Format: Float16_b (bfloat16)
- Dest accumulation: No
- Tile count: 1 (single 32x32 tile)
"""

import torch
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import UnarySFPUGolden, get_golden_generator
from helpers.llk_params import (
    ApproximationMode,
    DestAccumulation,
    MathOperation,
    format_dict,
)
from helpers.param_config import parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    APPROX_MODE,
    INPUT_DIMENSIONS,
    MATH_OP,
    TILE_COUNT,
)
from helpers.utils import passed_test


@parametrize(
    formats=[InputOutputFormat(DataFormat.Float16_b, DataFormat.Float16_b)],
    dest_acc=[DestAccumulation.No],
)
def test_simple_sfpu_abs(
    formats: InputOutputFormat,
    dest_acc: DestAccumulation,
    workers_tensix_coordinates: str,
):
    """
    Simple test: unpack srcA -> datacopy to dest -> sfpu abs -> pack

    Validates the basic SFPU abs operation pipeline.
    """
    torch.manual_seed(0)
    torch.set_printoptions(precision=10)

    # Single 32x32 tile
    input_dimensions = [32, 32]

    # Generate input stimuli (we only use src_A)
    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    # Generate golden reference (abs of input)
    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        MathOperation.Abs,
        src_A,
        formats.output_format,
        dest_acc,
        formats.input_format,
        input_dimensions,
    )

    # Configure test
    configuration = TestConfig(
        "sources/simple_sfpu_abs_test.cpp",
        formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
            APPROX_MODE(ApproximationMode.No),
            MATH_OP(mathop=MathOperation.Abs),
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
            tile_count_res=tile_cnt_A,
        ),
        dest_acc=dest_acc,
        unpack_to_dest=False,
    )

    # Run test on device
    res_from_L1 = configuration.run(workers_tensix_coordinates)

    # Verify results
    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
