# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

# Test for eltwise binary operations with reuse_dest on Quasar.


import pytest
import torch
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    TILE_DIM,
)
from helpers.llk_params import (
    DestAccumulation,
    EltwiseBinaryReuseDestType,
    MathFidelity,
    MathOperation,
    format_dict,
)
from helpers.param_config import (
    input_output_formats,
    parametrize,
)
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import BootMode, TestConfig
from helpers.test_variant_parameters import (
    DEST_SYNC,
    IMPLIED_MATH_FORMAT,
    MATH_FIDELITY,
    MATH_OP,
    NUM_FACES,
    REUSE_DEST_TYPE,
    TEST_FACE_DIMS,
    TILE_COUNT,
    generate_input_dim,
)
from helpers.tile_constants import FACE_C_DIM, get_tile_params
from helpers.utils import passed_test

REUSE_DEST_DIMENSIONS = [
    [TILE_DIM, TILE_DIM],
    [2 * TILE_DIM, TILE_DIM],
    [TILE_DIM, 2 * TILE_DIM],
    [2 * TILE_DIM, 2 * TILE_DIM],
]


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
    input_dimensions=REUSE_DEST_DIMENSIONS,
)
def test_eltwise_binary_reuse_dest_quasar(
    formats,
    mathop,
    reuse_dest_type,
    math_fidelity,
    input_dimensions,
    boot_mode=BootMode.DEFAULT,
):
    if math_fidelity != MathFidelity.LoFi:
        pytest.skip("Quasar reuse_dest eltwise binary supports LoFi only")

    src_A, tile_cnt_A, src_B, _ = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        output_format=formats.output_format,
    )

    face_r_dim, num_faces_r_dim, num_faces_c_dim = get_tile_params((TILE_DIM, TILE_DIM))
    num_faces = num_faces_r_dim * num_faces_c_dim
    tile_elements = num_faces * face_r_dim * FACE_C_DIM
    torch_format = format_dict[formats.output_format]
    src_A_t = (
        src_A
        if isinstance(src_A, torch.Tensor)
        else torch.tensor(src_A, dtype=torch_format)
    )
    src_B_t = (
        src_B
        if isinstance(src_B, torch.Tensor)
        else torch.tensor(src_B, dtype=torch_format)
    )
    golden_tensor = torch.zeros(tile_cnt_A * tile_elements, dtype=torch_format)

    for out_t in range(tile_cnt_A):
        start = out_t * tile_elements
        end = start + tile_elements
        a_tile = src_A_t[start:end].to(torch_format)
        b_tile = src_B_t[start:end].to(torch_format)
        # Initial dest = A (from datacopy in kernel); inner_dim=1 for Quasar
        dest = a_tile.clone()

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

        golden_tensor[start:end] = dest

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
            TILE_COUNT(tile_cnt_A),
            NUM_FACES(num_faces),
            TEST_FACE_DIMS(face_r_dim=face_r_dim, face_c_dim=FACE_C_DIM),
        ],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_A,
            tile_count_res=tile_cnt_A,
            num_faces=num_faces,
        ),
        unpack_to_dest=False,
        dest_acc=DestAccumulation.No,
        boot_mode=boot_mode,
    )

    res_from_L1 = configuration.run()

    # Verify results match golden
    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(
        golden_tensor, res_tensor, formats.output_format
    ), "Assert against golden failed"
