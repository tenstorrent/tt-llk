# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass

import pytest
import torch
from helpers.chip_architecture import ChipArchitecture
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    BroadcastGolden,
    EltwiseBinaryGolden,
    get_golden_generator,
)
from helpers.llk_params import (
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
    BROADCAST_TYPE,
    INPUT_DIMENSIONS,
    MATH_FIDELITY,
    MATH_OP,
    TILE_COUNT,
    TemplateParameter,
)
from helpers.tilize_untilize import tilize_block, untilize_block
from helpers.utils import passed_test


# Custom template parameter for CT_DIM
@dataclass
class CT_DIM(TemplateParameter):
    ct_dim: int

    def covert_to_cpp(self) -> str:
        return f"constexpr std::uint32_t CT_DIM = {self.ct_dim};"


@parametrize(
    cpp_source=[
        "sources/multiple_tiles_eltwise_custom_test.cpp",
    ],
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    mathop=[MathOperation.Elwsub],
    dest_acc=[DestAccumulation.No],
    math_fidelity=[
        MathFidelity.LoFi,
    ],
    broadcast_type=[BroadcastType.Column],
    input_dimensions_A=[[32, 64]],  # 32 x 8*32 = [32, 128] TODO: EXPAND TO 256
    input_dimensions_B=[[32, 32]],  # 32 x 32 = [32, 32]
)
def test_eltwise_bcast_col_custom(
    cpp_source,
    formats,
    mathop,
    dest_acc,
    math_fidelity,
    broadcast_type,
    input_dimensions_A,
    input_dimensions_B,
    workers_tensix_coordinates,
):
    """
    Test eltwise broadcast column operation with custom inputs:
    - srcA: [32, 256] filled with 3s
    - srcB: [32, 32] filled with 2s, but first column filled with 1s
    - Operation: srcA - broadcast_column(srcB)
    - Golden calculation:
      1. Broadcast srcB's first column to all columns (within each tile)
      2. Subtract the broadcasted srcB from every tile of srcA
    """
    if (
        TestConfig.CHIP_ARCH == ChipArchitecture.WORMHOLE
        and cpp_source == "sources/multiple_tiles_eltwise_custom_test.cpp"
    ):
        pytest.skip("Custom test not supported on Wormhole")

    # Generate stimuli with different dimensions for srcA and srcB
    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions_A,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions_B,
    )

    # Override stimuli: srcA = all 3s, srcB = all 2s with first column = 1s
    # src_A = torch.full_like(src_A, 3.0)
    # src_B = torch.full_like(src_B, 2.0)
    # src_B_2d_override = src_B.view(input_dimensions_B[0], input_dimensions_B[1])
    # src_B_2d_override[:, 0] = 1.0  # First column filled with 1s
    # src_B = src_B_2d_override.flatten()

    print(f"src_A: {src_A.view(input_dimensions_A[0], input_dimensions_A[1])}")
    print(f"src_B: {src_B.view(input_dimensions_B[0], input_dimensions_B[1])}")

    # Tilize inputs for hardware (hardware expects tilized format)
    src_A_tilized = tilize_block(
        src_A, input_dimensions_A, formats.input_format
    ).flatten()
    src_B_tilized = tilize_block(
        src_B, input_dimensions_B, formats.input_format
    ).flatten()

    print(src_B_tilized.view(input_dimensions_B[0], input_dimensions_B[1]))

    # For broadcast: hardware broadcasts in tilized space, so we need to simulate that for golden
    # Step 1: Broadcast column - replicate first column of srcB to all columns within each tile
    broadcast_golden = get_golden_generator(BroadcastGolden)
    src_B_broadcasted_tilized = broadcast_golden(
        broadcast_type,
        src_B_tilized,  # Tilized data
        formats.input_format,
        num_faces=4,
        tile_cnt=tile_cnt_B,
        face_r_dim=16,
    )

    # Untilize the broadcasted result to get what we expect in untilized space
    src_B_golden = untilize_block(
        src_B_broadcasted_tilized, formats.input_format, input_dimensions_B
    ).flatten()

    # Replicate srcB across the width dimension to match srcA's width (8 tiles = 256 columns)
    # srcB is [32, 32], srcA is [32, 256], so we need to repeat srcB 8 times horizontally
    src_B_2d = src_B_golden.view(input_dimensions_B[0], input_dimensions_B[1])
    src_B_expanded = src_B_2d.repeat(1, input_dimensions_A[1] // input_dimensions_B[1])
    src_B_golden_expanded = src_B_expanded.flatten()

    # Step 2: Compute golden - subtract broadcasted srcB from srcA
    # Golden: srcA - src_B_golden_expanded
    generate_golden = get_golden_generator(EltwiseBinaryGolden)
    golden_tensor = generate_golden(
        mathop, src_A, src_B_golden_expanded, formats.output_format, math_fidelity
    )

    configuration = TestConfig(
        cpp_source,
        formats,
        templates=[
            MATH_FIDELITY(math_fidelity),
            INPUT_DIMENSIONS(input_dimensions_A, input_dimensions_B),
            MATH_OP(mathop=mathop),
            BROADCAST_TYPE(broadcast_type),
            CT_DIM(input_dimensions_A[1] // 32),  # Number of tiles in column dimension
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
    )
    res_from_L1 = configuration.run(workers_tensix_coordinates)

    res_from_L1 = untilize_block(
        res_from_L1, formats.output_format, input_dimensions_A
    ).flatten()

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
