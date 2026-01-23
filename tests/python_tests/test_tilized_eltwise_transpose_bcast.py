# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch
from conftest import skip_for_blackhole
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
    Transpose,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    BROADCAST_TYPE,
    DEST_SYNC,
    MATH_FIDELITY,
    MATH_OP,
    NUM_FACES,
    ROW_INDEX,
    TILE_COUNT,
    UNPACK_TRANS_FACES,
    UNPACK_TRANS_WITHIN_FACE,
)
from helpers.tilize_untilize import tilize, tilize_block
from helpers.utils import passed_test

# Constants for tile/face dimensions
FACE_DIM = 16
ELEMENTS_PER_FACE = 256


def extract_row_from_tilized(tilized_tensor, row_index, data_format):
    """
    Extract a specific row from a tilized tensor and create a tensor
    with that row in the appropriate faces for broadcast.

    For row_index 0-15: Row is in F0 and F1 (top half)
    For row_index 16-31: Row is in F2 and F3 (bottom half)

    Returns a tensor where only the selected row contains data,
    positioned to match what the unpacker will read.
    """
    torch_format = format_dict[data_format]
    result = torch.zeros(1024, dtype=torch_format)

    # Determine which faces contain the row
    if row_index < 16:
        face_row = row_index
        # F0 contains left half (columns 0-15), F1 contains right half (columns 16-31)
        f0_start = 0
        f1_start = ELEMENTS_PER_FACE
    else:
        face_row = row_index - 16
        # F2 contains left half, F3 contains right half
        f0_start = 2 * ELEMENTS_PER_FACE
        f1_start = 3 * ELEMENTS_PER_FACE

    # Extract the row from F0 (or F2)
    row_start_f0 = f0_start + face_row * FACE_DIM
    row_data_f0 = tilized_tensor[row_start_f0 : row_start_f0 + FACE_DIM]

    # Extract the row from F1 (or F3)
    row_start_f1 = f1_start + face_row * FACE_DIM
    row_data_f1 = tilized_tensor[row_start_f1 : row_start_f1 + FACE_DIM]

    # Place the row data at the start of F0 and F1 (where unpacker expects it)
    # After applying the row_index offset in hardware, F0R0 and F1R0 positions
    # will actually read from F0Rn and F1Rn (or F2Rn and F3Rn)
    result[0:FACE_DIM] = row_data_f0  # F0R0 position
    result[ELEMENTS_PER_FACE : ELEMENTS_PER_FACE + FACE_DIM] = (
        row_data_f1  # F1R0 position
    )

    return result


@skip_for_blackhole
@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            # DataFormat.Float16,
        ]
    ),
    dest_acc=[DestAccumulation.No],  # , DestAccumulation.Yes],
    math_fidelity=[MathFidelity.LoFi],
    input_dimensions=[[32, 32]],
    row_index=[0],  # Debug: single row_index  # list(range(32)),  # Sweep from 0 to 31
    transpose_A=[
        Transpose.Yes
    ],  # Transpose.Yes],  # Test with and without srcA transpose
)
def test_tilized_eltwise_transpose_bcast(
    formats,
    dest_acc,
    math_fidelity,
    input_dimensions,
    row_index,
    transpose_A,
    workers_tensix_coordinates,
):
    """
    Test with both tiles tilized in L1.

    - srcA: normal tilized input (optionally transposed via unpacker)
    - srcB: for golden calculation, we extract the specific row (based on row_index),
            transpose it, then apply column broadcast
    - Operation: element-wise subtraction (elwsub)

    row_index determines which row of the 32x32 tile to broadcast:
    - 0-15: Row from top half (F0Rn and F1Rn)
    - 16-31: Row from bottom half (F2R(n-16) and F3R(n-16))

    transpose_A: If Yes, srcA will be transposed during unpack (both face transpose
                 and within-face transpose)
    """
    # src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
    #     stimuli_format_A=formats.input_format,
    #     input_dimensions_A=input_dimensions,
    #     stimuli_format_B=formats.input_format,
    #     input_dimensions_B=input_dimensions,
    # )

    # ========== Debug: Deterministic stimuli ==========
    # srcA: row i has all values = i (row 0 = 0, row 1 = 1, ..., row 31 = 31)
    # srcB: constant 1
    # Expected result: srcA transposed - 1
    torch_format = format_dict[formats.input_format]
    tile_cnt_A = 1
    tile_cnt_B = 1

    # Create srcA: 32x32 where row i has value i
    src_A = torch.zeros(32, 32, dtype=torch_format)
    for row in range(32):
        src_A[row, :] = row
    src_A = src_A.flatten()

    # Create srcB: constant 1
    src_B = torch.ones(32 * 32, dtype=torch_format)

    # Tilize both inputs for hardware (both will be tilized in L1)
    src_A_tilized = tilize_block(src_A, input_dimensions, formats.input_format)
    src_B_tilized = tilize_block(src_B, input_dimensions, formats.input_format)

    # ========== Golden Calculation ==========
    # For golden: extract the specific row from srcB, transpose, then column broadcast

    # Step 1: Tilize srcB for row extraction
    src_B_tilized_for_golden = tilize(
        src_B, stimuli_format=formats.input_format, num_faces=4
    )

    # Step 2: Extract the specific row based on row_index
    # This simulates what the unpacker will read when given the row_index offset
    src_B_row_extracted = extract_row_from_tilized(
        src_B_tilized_for_golden, row_index, formats.input_format
    )

    # Step 3: Apply transpose to the extracted row data
    transpose_golden = get_golden_generator(TransposeGolden)

    # Transpose faces: f0, f1, f2, f3 -> f0, f2, f1, f3
    src_B_transposed = transpose_golden.transpose_faces(
        src_B_row_extracted,
        formats.input_format,
        num_faces=4,
    )

    # Transpose within each face (16x16 transposition within each face)
    src_B_transposed = transpose_golden.transpose_within_faces(
        src_B_transposed,
        formats.input_format,
        num_faces=4,
    )

    # Step 4: Apply column broadcast to the transposed srcB
    broadcast_golden = get_golden_generator(BroadcastGolden)
    src_B_broadcasted_tilized = broadcast_golden(
        BroadcastType.Column,
        src_B_transposed,
        formats.input_format,
        num_faces=4,
        tile_cnt=tile_cnt_B,
        face_r_dim=16,
    )

    # Step 5: Tilize srcA for element-wise operation
    src_A_tilized_for_golden = tilize(
        src_A, stimuli_format=formats.input_format, num_faces=4
    )

    # Step 6: Apply transpose to srcA if enabled
    if transpose_A == Transpose.Yes:
        # Transpose faces: f0, f1, f2, f3 -> f0, f2, f1, f3
        src_A_tilized_for_golden = transpose_golden.transpose_faces(
            src_A_tilized_for_golden,
            formats.input_format,
            num_faces=4,
        )
        # Transpose within each face (16x16 transposition within each face)
        src_A_tilized_for_golden = transpose_golden.transpose_within_faces(
            src_A_tilized_for_golden,
            formats.input_format,
            num_faces=4,
        )

    # Step 7: Compute element-wise subtraction in tilized format
    binary_golden = get_golden_generator(EltwiseBinaryGolden)
    golden_tensor = binary_golden(
        MathOperation.Elwsub,
        src_A_tilized_for_golden,  # Tilized srcA (optionally transposed)
        src_B_broadcasted_tilized,  # Row-extracted + transposed + column broadcasted srcB
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
            UNPACK_TRANS_FACES(transpose_A),
            UNPACK_TRANS_WITHIN_FACE(transpose_A),
        ],
        runtimes=[
            TILE_COUNT(tile_cnt_A),
            NUM_FACES(4),
            ROW_INDEX(row_index),
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

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    torch_format_out = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format_out)

    # ========== Debug: Print results ==========
    print("srcA:")
    print(src_A.view(32, 32))
    print("srcB:")
    print(src_B.view(32, 32))
    print("result:")
    print(res_tensor.view(32, 32))
    print("golden:")
    print(golden_tensor.view(32, 32))

    # Compare in tilized format
    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
