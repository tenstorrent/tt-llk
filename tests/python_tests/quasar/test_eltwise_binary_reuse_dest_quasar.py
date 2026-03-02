# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

# Test for eltwise binary operations with reuse_dest on Quasar.


import pytest
import torch
from helpers.format_config import DataFormat
from helpers.llk_params import (
    DestAccumulation,
    DestSync,
    EltwiseBinaryReuseDestType,
    MathFidelity,
    MathOperation,
    format_dict,
)
from helpers.param_config import (
    BlocksCalculationAlgorithm,
    get_num_blocks_and_num_tiles_in_block,
    input_output_formats,
    parametrize,
)
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli_w_tile_dimensions
from helpers.test_config import BootMode, TestConfig
from helpers.test_variant_parameters import (
    DEST_SYNC,
    IMPLIED_MATH_FORMAT,
    INPUT_TILE_CNT,
    MATH_FIDELITY,
    MATH_OP,
    NUM_BLOCKS,
    NUM_FACES,
    NUM_TILES_IN_BLOCK,
    OUTPUT_TILE_CNT,
    REUSE_DEST_TYPE,
    TEST_FACE_DIMS,
    generate_input_dim,
)
from helpers.tile_constants import FACE_C_DIM, get_tile_params
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test

INPUT_DIMENSIONS = [
    [512, 32],
    [256, 32],
    [128, 32],
    [256, 64],
]
OUTPUT_DIMENSIONS = [
    [128, 32],
    [256, 32],
    [64, 32],
    [128, 64],
]

TILE_DIMENSIONS = [32, 32]


@pytest.mark.quasar
@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
            DataFormat.Float16,
        ],
    ),
    mathop=[
        MathOperation.Elwadd,
        MathOperation.Elwsub,
        MathOperation.Elwmul,
    ],
    reuse_dest_type=[
        EltwiseBinaryReuseDestType.DEST_TO_SRCA,
        EltwiseBinaryReuseDestType.DEST_TO_SRCB,
    ],
    math_fidelity=[MathFidelity.LoFi],
    input_dimensions=INPUT_DIMENSIONS,
    output_dimensions=OUTPUT_DIMENSIONS,
)
def test_eltwise_binary_reuse_dest_quasar(
    formats,
    mathop,
    reuse_dest_type,
    math_fidelity,
    input_dimensions,
    output_dimensions,
    boot_mode=BootMode.DEFAULT,
):
    if math_fidelity != MathFidelity.LoFi:
        pytest.skip("Quasar reuse_dest eltwise binary supports LoFi only")

    tile_rows, tile_cols = TILE_DIMENSIONS
    face_r_dim, num_faces_r_dim, num_faces_c_dim = get_tile_params(
        [tile_rows, tile_cols]
    )
    num_faces = num_faces_r_dim * num_faces_c_dim

    tile_cnt_input = (input_dimensions[0] // tile_rows) * (
        input_dimensions[1] // tile_cols
    )
    tile_cnt_output = (output_dimensions[0] // tile_rows) * (
        output_dimensions[1] // tile_cols
    )

    if tile_cnt_input % tile_cnt_output != 0:
        pytest.skip(
            f"Input tile count ({tile_cnt_input}) must be divisible by "
            f"output tile count ({tile_cnt_output})"
        )
    inner_dim = tile_cnt_input // tile_cnt_output
    if inner_dim == 1:
        pytest.skip("reuse_dest requires inner_dim > 1")

    tile_dimensions_tuple = (tile_rows, tile_cols)
    output_num_blocks, output_tiles_in_block = get_num_blocks_and_num_tiles_in_block(
        DestSync.Half,
        DestAccumulation.No,
        formats,
        output_dimensions,
        tile_dimensions_tuple,
        BlocksCalculationAlgorithm.Standard,
    )
    if output_num_blocks > 1:
        pytest.skip(
            "Quasar reuse_dest kernel supports single output block only; "
            "multi-block uses block-relative indexing and wrong accumulation"
        )
    input_tiles_in_block = inner_dim * output_tiles_in_block
    input_num_blocks = output_num_blocks

    src_A, _, src_B, _ = generate_stimuli_w_tile_dimensions(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        tile_dimensions=[tile_rows, tile_cols],
    )
    src_A_tilized = tilize_block(
        src_A,
        dimensions=input_dimensions,
        stimuli_format=formats.input_format,
        num_faces=num_faces,
        tile_dimensions=[tile_rows, tile_cols],
        face_r_dim=face_r_dim,
    )
    src_B_tilized = tilize_block(
        src_B,
        dimensions=input_dimensions,
        stimuli_format=formats.input_format,
        num_faces=num_faces,
        tile_dimensions=[tile_rows, tile_cols],
        face_r_dim=face_r_dim,
    )
    src_A_flat = src_A_tilized.flatten()
    src_B_flat = src_B_tilized.flatten()

    tile_elements = num_faces * face_r_dim * FACE_C_DIM
    torch_format = format_dict[formats.output_format]
    src_A_t = (
        src_A_flat
        if isinstance(src_A_flat, torch.Tensor)
        else torch.tensor(src_A_flat, dtype=torch_format)
    )
    src_B_t = (
        src_B_flat
        if isinstance(src_B_flat, torch.Tensor)
        else torch.tensor(src_B_flat, dtype=torch_format)
    )
    golden_tensor = torch.zeros(tile_cnt_output * tile_elements, dtype=torch_format)

    for out_t in range(tile_cnt_output):
        first_input_tile_idx = out_t
        dest = src_A_t[
            first_input_tile_idx
            * tile_elements : (first_input_tile_idx + 1)
            * tile_elements
        ].to(torch_format)

        for n in range(inner_dim):
            input_tile_idx = n * tile_cnt_output + out_t
            a_tile = src_A_t[
                input_tile_idx * tile_elements : (input_tile_idx + 1) * tile_elements
            ].to(torch_format)
            b_tile = src_B_t[
                input_tile_idx * tile_elements : (input_tile_idx + 1) * tile_elements
            ].to(torch_format)

            if reuse_dest_type == EltwiseBinaryReuseDestType.DEST_TO_SRCA:
                srcA, srcB = dest.clone(), b_tile
            else:
                srcA, srcB = a_tile, dest.clone()

            if mathop == MathOperation.Elwadd:
                dest = srcA + srcB
            elif mathop == MathOperation.Elwsub:
                dest = srcA - srcB
            elif mathop == MathOperation.Elwmul:
                dest = dest + srcA * srcB

        golden_tensor[out_t * tile_elements : (out_t + 1) * tile_elements] = dest

    configuration = TestConfig(
        "sources/quasar/eltwise_binary_reuse_dest_quasar_test.cpp",
        formats,
        templates=[
            MATH_FIDELITY(math_fidelity),
            generate_input_dim(input_dimensions, input_dimensions),
            MATH_OP(mathop=mathop),
            IMPLIED_MATH_FORMAT(),
            REUSE_DEST_TYPE(reuse_dest_type),
            DEST_SYNC(),
        ],
        runtimes=[
            INPUT_TILE_CNT(tile_cnt_input),
            OUTPUT_TILE_CNT(tile_cnt_output),
            NUM_TILES_IN_BLOCK(
                output_tiles_in_block,
                input_num_tiles_in_block=input_tiles_in_block,
                output_num_tiles_in_block=output_tiles_in_block,
            ),
            NUM_BLOCKS(
                output_num_blocks,
                input_num_blocks=input_num_blocks,
                output_num_blocks=output_num_blocks,
            ),
            NUM_FACES(num_faces),
            TEST_FACE_DIMS(face_r_dim=face_r_dim, face_c_dim=FACE_C_DIM),
        ],
        variant_stimuli=StimuliConfig(
            src_A_flat,
            formats.input_format,
            src_B_flat,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_input,
            tile_count_B=tile_cnt_input,
            tile_count_res=tile_cnt_output,
            num_faces=num_faces,
            face_r_dim=face_r_dim,
            tile_dimensions=[tile_rows, tile_cols],
            use_dense_tile_dimensions=True,
        ),
        unpack_to_dest=False,
        dest_acc=DestAccumulation.No,
        boot_mode=boot_mode,
    )

    res_from_L1 = configuration.run().result

    # Verify results match golden
    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
