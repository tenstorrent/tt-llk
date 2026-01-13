# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Test for binary FPU subtract with column broadcast + fast-approximate exponent on SFPU.

Pipeline:
1. Unpack srcA (regular unpack)
2. Unpack srcB with column broadcast
3. FPU subtraction: dest = srcA - broadcast_column(srcB)
4. SFPU fast-approx exponential on dest

Configuration:
- Format: Float16_b input and output
- Approximation mode: Yes (fast approximation)
- Dest accumulation: No
- Tile count: 1 (single 32x32 tile = 1024 elements)
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
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    APPROX_MODE,
    BROADCAST_TYPE,
    INPUT_DIMENSIONS,
    MATH_FIDELITY,
    MATH_OP,
    TILE_COUNT,
)
from helpers.tilize_untilize import tilize
from helpers.utils import passed_test


def test_binary_sub_bcast_col_fast_approx_exp(workers_tensix_coordinates: str):
    """
    Test binary FPU subtract with column broadcast followed by fast-approx exp SFPU.

    Pipeline:
    1. Unpack srcA (regular)
    2. Unpack srcB with column broadcast
    3. FPU sub: dest = srcA - broadcast_col(srcB)
    4. SFPU fast-approx exp on dest
    """
    torch.manual_seed(0)
    torch.set_printoptions(precision=10)

    # Single tile: 32x32 = 1024 elements
    input_dimensions = [32, 32]

    # Use Float16_b format for both input and output
    formats = InputOutputFormat(DataFormat.Float16_b, DataFormat.Float16_b)

    # Generate input stimuli
    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    # Clamp input values to avoid exp() overflow after subtraction
    # exp(x) overflows for x > ~88, so keep values in safe range
    src_A = torch.clamp(src_A, min=-2.0, max=2.0)
    src_B = torch.clamp(src_B, min=-2.0, max=2.0)

    # Tilize inputs before writing to L1 - this way broadcast works correctly in face space
    src_A_tilized = tilize(src_A, stimuli_format=formats.input_format, num_faces=4)
    src_B_tilized = tilize(src_B, stimuli_format=formats.input_format, num_faces=4)

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
        "sources/fast_approx_exp_test.cpp",
        formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
            BROADCAST_TYPE(BroadcastType.Column),
            MATH_FIDELITY(MathFidelity.LoFi),
            APPROX_MODE(ApproximationMode.Yes),  # Fast approximation mode
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
        dest_acc=DestAccumulation.No,
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
