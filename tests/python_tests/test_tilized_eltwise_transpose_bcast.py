# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    BroadcastGolden,
    EltwiseBinaryGolden,
    TransposeGolden,
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
    DEST_SYNC,
    MATH_FIDELITY,
    MATH_OP,
    NUM_FACES,
    TILE_COUNT,
)
from helpers.tilize_untilize import tilize, tilize_block, untilize_block
from helpers.utils import passed_test


@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    dest_acc=[DestAccumulation.No],
    math_fidelity=[MathFidelity.LoFi],
    input_dimensions=[[32, 32]],
)
def test_tilized_eltwise_transpose_bcast(
    formats,
    dest_acc,
    math_fidelity,
    input_dimensions,
    workers_tensix_coordinates,
):
    """
    Test with both tiles tilized in L1.

    - srcA: normal tilized input
    - srcB: for golden calculation, we transpose srcB first, then apply column broadcast
    - Operation: element-wise subtraction (elwsub)

    The C++ test initially does NOT apply transpose on srcB, so the test will fail.
    The user will implement the transpose on srcB in hardware.
    """
    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    # src_A = torch.ones(1024) * 0
    # src_B = torch.ones(1024) * 0
    # src_B[0:16] = torch.arange(16)
    # src_B[16:32] = torch.arange(16) + 16

    print("src_A:")
    print(src_A.view(32, 32))
    print("src_B:")
    print(src_B.view(32, 32))

    # Tilize both inputs for hardware (both will be tilized in L1)
    src_A_tilized = tilize_block(src_A, input_dimensions, formats.input_format)
    src_B_tilized = tilize_block(src_B, input_dimensions, formats.input_format)

    # ========== Golden Calculation ==========
    # For golden: srcB needs to be transposed first, then column broadcast applied

    # Step 1: Tilize srcB for transpose operation
    src_B_tilized_for_golden = tilize(
        src_B, stimuli_format=formats.input_format, num_faces=4
    )

    # Step 2: Apply transpose to srcB (transpose faces, then transpose within faces)
    transpose_golden = get_golden_generator(TransposeGolden)

    # Transpose faces: f0, f1, f2, f3 -> f0, f2, f1, f3
    src_B_transposed = transpose_golden.transpose_faces(
        src_B_tilized_for_golden,
        formats.input_format,
        num_faces=4,
    )

    # Transpose within each face (16x16 transposition within each face)
    src_B_transposed = transpose_golden.transpose_within_faces(
        src_B_transposed,
        formats.input_format,
        num_faces=4,
    )

    # Step 3: Apply column broadcast to the transposed srcB
    broadcast_golden = get_golden_generator(BroadcastGolden)
    src_B_broadcasted_tilized = broadcast_golden(
        BroadcastType.Column,
        src_B_transposed,
        formats.input_format,
        num_faces=4,
        tile_cnt=tile_cnt_B,
        face_r_dim=16,
    )

    # Step 4: Tilize srcA for element-wise operation
    src_A_tilized_for_golden = tilize(
        src_A, stimuli_format=formats.input_format, num_faces=4
    )

    # Step 5: Compute element-wise subtraction in tilized format
    binary_golden = get_golden_generator(EltwiseBinaryGolden)
    golden_tensor = binary_golden(
        MathOperation.Elwsub,
        src_A_tilized_for_golden,  # Tilized srcA
        src_B_broadcasted_tilized,  # Transposed + column broadcasted srcB
        formats.output_format,
        math_fidelity,
    )

    # ========== Test Configuration ==========
    configuration = TestConfig(
        "sources/tilized_eltwise_transpose_bcast_test.cpp",
        formats,
        templates=[
            MATH_FIDELITY(math_fidelity),
            BROADCAST_TYPE(BroadcastType.Row),
            MATH_OP(mathop=MathOperation.Elwsub),
            DEST_SYNC(),
        ],
        runtimes=[
            TILE_COUNT(tile_cnt_A),
            NUM_FACES(4),
        ],
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

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    print("res_from_L1:")
    print(res_from_L1.view(32, 32))
    print("golden_tensor:")
    print(golden_tensor.view(32, 32))

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    print("UNTILIZED RES:")
    print(
        untilize_block(res_tensor, formats.output_format, input_dimensions).view(32, 32)
    )
    # Compare in tilized format
    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
