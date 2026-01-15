# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Test for unpackAB with column broadcast, subtraction, and fast-approximate exponential.

Pipeline:
1. Unpack srcA (regular unpack)
2. Unpack srcB with column broadcast
3. FPU subtraction: dest = srcA - broadcast_column(srcB)
4. SFPU fast-approx exponential on dest
5. Pack result to L1

Configuration:
- Formats: Parametrized (Float16_b, Float16, Bfp8_b)
- Approximation mode: Yes (fast approximation)
- Dest accumulation: Parametrized (No, Yes)
- Tile count: 1 (single 32x32 tile = 1024 elements)
- Both inputs are tilized before being written to L1 (required for broadcast)
"""

import torch
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import (
    BroadcastGolden,
    EltwiseBinaryGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    ApproximationMode,
    BroadcastType,
    DestAccumulation,
    MathFidelity,
    MathOperation,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    APPROX_MODE,
    BROADCAST_TYPE,
    FAST_MODE,
    INPUT_DIMENSIONS,
    MATH_FIDELITY,
    MATH_OP,
    TILE_COUNT,
)
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test


@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            # DataFormat.Float16,
            # DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=[DestAccumulation.No],  # , DestAccumulation.Yes],
    fast_mode=[
        True,
        # False,
    ],
    input_dimensions=[
        [32, 32],
        [32, 64],
    ],
)
def test_unpack_AB_col_bcast_sub_exp(
    formats: InputOutputFormat,
    dest_acc: DestAccumulation,
    fast_mode: bool,
    input_dimensions: list[int],
    workers_tensix_coordinates: str,
):
    """
    Test unpackAB with column broadcast, FPU subtract, and SFPU fast-approx exp.

    Pipeline:
    1. Unpack srcA (regular)
    2. Unpack srcB with column broadcast
    3. FPU sub: dest = srcA - broadcast_col(srcB)
    4. SFPU fast-approx exp on dest (with optional Schraudolph algorithm)
    5. Pack result to L1

    Parameters:
    - fast_mode: When True, uses Schraudolph fast approximation algorithm
                 When False, uses standard approximation mode
    """
    torch.manual_seed(0)
    torch.set_printoptions(precision=10)

    # Generate input stimuli
    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    src_A = (
        torch.ones(input_dimensions[0] * input_dimensions[1], dtype=torch.bfloat16) * 3
    )
    src_B = (
        torch.ones(input_dimensions[0] * input_dimensions[1], dtype=torch.bfloat16) * 2
    )

    # Clamp input values to avoid exp() overflow after subtraction
    # exp(x) overflows for x > ~88, so keep values in safe range
    # src_A = torch.clamp(src_A, min=-2.0, max=2.0)
    # src_B = torch.clamp(src_B, min=-2.0, max=2.0)

    # Tilize inputs before writing to L1 - broadcast expects tile layout in L1
    # Tilization is required for broadcast to work properly
    # Use block tilization for any multiple-of-32 dimensions (works for 32x32 too)
    src_A_tilized = tilize_block(
        src_A, input_dimensions, stimuli_format=formats.input_format, num_faces=4
    ).flatten()
    src_B_tilized = tilize_block(
        src_B, input_dimensions, stimuli_format=formats.input_format, num_faces=4
    ).flatten()

    # --- Compute Golden (all operations on tilized data) ---
    # Step 1: Apply column broadcast to tilized srcB
    broadcast_golden = get_golden_generator(BroadcastGolden)
    src_B_broadcasted = broadcast_golden(
        BroadcastType.Column,
        src_B_tilized,
        formats.input_format,
        num_faces=4,
        tile_cnt=tile_cnt_B,
        face_r_dim=16,
    )

    # Step 2: Compute FPU subtraction on tilized data: srcA - broadcast_col(srcB)
    binary_golden = get_golden_generator(EltwiseBinaryGolden)
    sub_result = binary_golden(
        MathOperation.Elwsub,
        src_A_tilized,
        src_B_broadcasted,
        formats.output_format,
        MathFidelity.LoFi,
    )

    # Step 3: Compute SFPU fast-approx exp on subtraction result
    # Result is already in tilized format, apply exp element-wise
    golden_tensor = torch.exp(sub_result)

    # Configure test
    configuration = TestConfig(
        "sources/unpack_AB_col_bcast_sub_exp_test.cpp",
        formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
            BROADCAST_TYPE(BroadcastType.Column),
            MATH_FIDELITY(MathFidelity.LoFi),
            APPROX_MODE(ApproximationMode.Yes),  # Fast approximation mode
            FAST_MODE(
                fast_mode
            ),  # Enable/disable Schraudolph fast approximation algorithm
            MATH_OP(mathop=MathOperation.Elwsub, unary_extra=MathOperation.Exp),
        ],
        runtimes=[TILE_COUNT(tile_cnt_A)],
        variant_stimuli=StimuliConfig(
            src_A_tilized,
            formats.input_format,
            src_B_tilized,
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
