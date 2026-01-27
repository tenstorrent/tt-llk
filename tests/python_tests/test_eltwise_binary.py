# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
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
    Transpose,
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
    NUM_BLOCKS,
    NUM_FACES_C_DIM,
    NUM_FACES_R_DIM,
    NUM_TILES_IN_BLOCK,
    TEST_FACE_DIMS,
    UNPACK_TRANS_FACES,
    UNPACK_TRANS_WITHIN_FACE,
)
from helpers.tile_constants import get_tile_params
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test


@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float32,
        ],
        same=False,
    ),
    broadcast_type=[
        BroadcastType.None_,
        BroadcastType.Row,
        BroadcastType.Column,
        BroadcastType.Scalar,
    ],
    dest_acc=[DestAccumulation.No],
    math_fidelity=[
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ],
    transpose_srca=[Transpose.No],
    math_op=[MathOperation.Elwadd, MathOperation.Elwsub, MathOperation.Elwmul],
    input_dimensions=[[512, 32]],
    tile_dimensions=[[1, 32], [2, 32], [4, 32], [8, 32], [16, 32], [32, 32]],
)
def test_eltwise_binary(
    formats,
    broadcast_type,
    dest_acc,
    math_fidelity,
    transpose_srca,
    math_op,
    input_dimensions,
    tile_dimensions,
    workers_tensix_coordinates,
):

    if math_fidelity != MathFidelity.LoFi and math_op != MathOperation.Elwmul:
        pytest.skip("High fidelity operations are only supported for Elwmul")
    face_r_dim, num_faces_r_dim, num_faces_c_dim = get_tile_params(tile_dimensions)

    # Calculate tile count based on tile_dimensions (not hardcoded 32x32)
    tile_rows, tile_cols = tile_dimensions
    tile_cnt_A = (input_dimensions[0] // tile_rows) * (input_dimensions[1] // tile_cols)
    tile_cnt_B = tile_cnt_A

    # Generate stimuli has hardcoded tile dims of 32x32
    src_A, _, src_B, _ = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    MAX_TILES_IN_BLOCK = (
        4
        if (formats.input_format == DataFormat.Float32)
        or (formats.input_format == DataFormat.Int32)
        else 8
    )

    num_blocks = (tile_cnt_A + MAX_TILES_IN_BLOCK - 1) // MAX_TILES_IN_BLOCK
    num_tiles_in_block = tile_cnt_A % MAX_TILES_IN_BLOCK or MAX_TILES_IN_BLOCK

    # Compute element-wise operation
    binary_golden = get_golden_generator(EltwiseBinaryGolden)
    num_faces = num_faces_r_dim * num_faces_c_dim

    if broadcast_type == BroadcastType.None_:
        # No broadcast: compute golden directly on raw data, send raw data to device
        golden_tensor = binary_golden(
            math_op,
            src_A,
            src_B,
            formats.output_format,
            math_fidelity,
        )
        stimuli_A = src_A
        stimuli_B = src_B
    else:
        # Broadcast modes: need to tilize inputs, apply broadcast, then compute golden
        # Use tilize_block to handle multi-tile inputs with smaller tile sizes
        src_A_tilized = tilize_block(
            src_A,
            dimensions=input_dimensions,
            stimuli_format=formats.input_format,
            num_faces=num_faces,
            tile_dimensions=tile_dimensions,
            face_r_dim=face_r_dim,
        )
        src_B_tilized = tilize_block(
            src_B,
            dimensions=input_dimensions,
            stimuli_format=formats.input_format,
            num_faces=num_faces,
            tile_dimensions=tile_dimensions,
            face_r_dim=face_r_dim,
        )

        # Flatten tilized tensors for broadcast and golden calculation
        src_A_tilized_flat = src_A_tilized.flatten()
        src_B_tilized_flat = src_B_tilized.flatten()

        # Apply broadcast to SrcB
        broadcast_golden = get_golden_generator(BroadcastGolden)
        src_B_broadcasted_tilized = broadcast_golden(
            broadcast_type,
            src_B_tilized_flat,
            formats.input_format,
            num_faces=num_faces,
            tile_cnt=tile_cnt_A,
            face_r_dim=face_r_dim,
        )

        # Compute golden on tilized data
        golden_tensor = binary_golden(
            math_op,
            src_A_tilized_flat,
            src_B_broadcasted_tilized,
            formats.output_format,
            math_fidelity,
        )

        # Send tilized data to device for broadcast modes
        stimuli_A = src_A_tilized_flat
        stimuli_B = src_B_tilized_flat

    configuration = TestConfig(
        "sources/eltwise_binary_test.cpp",
        formats,
        templates=[
            MATH_FIDELITY(math_fidelity),
            BROADCAST_TYPE(broadcast_type),
            MATH_OP(mathop=math_op),
            DEST_SYNC(),
        ],
        runtimes=[
            UNPACK_TRANS_FACES(transpose_srca),
            UNPACK_TRANS_WITHIN_FACE(transpose_srca),
            NUM_TILES_IN_BLOCK(num_tiles_in_block),
            NUM_BLOCKS(num_blocks),
            NUM_FACES_R_DIM(num_faces_r_dim),
            NUM_FACES_C_DIM(num_faces_c_dim),
            TEST_FACE_DIMS(face_r_dim=face_r_dim),
        ],
        variant_stimuli=StimuliConfig(
            stimuli_A,
            formats.input_format,
            stimuli_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
            num_faces=num_faces,
            face_r_dim=face_r_dim,
            tile_dimensions=tile_dimensions,
        ),
        dest_acc=dest_acc,
        unpack_to_dest=False,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    # Compare in tilized format
    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
