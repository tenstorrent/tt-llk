# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

# Test for eltwise binary operations with reuse_dest on Quasar.


import pytest
import torch
from helpers.format_config import DataFormat
from helpers.golden_generators import (
    TILE_DIM,
    EltwiseBinaryGolden,
    get_golden_generator,
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
    INPUT_DIMENSIONS,
    MATH_FIDELITY,
    MATH_OP,
    NUM_FACES,
    REUSE_DEST_TYPE,
    TEST_FACE_DIMS,
    TILE_COUNT,
)
from helpers.utils import passed_test


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
    math_fidelity=[
        MathFidelity.LoFi,
    ],
)
def test_eltwise_binary_reuse_dest_quasar(
    formats,
    mathop,
    reuse_dest_type,
    math_fidelity,
    boot_mode=BootMode.DEFAULT,
):
    # Math fidelity only affects multiplication operations
    if (
        mathop in [MathOperation.Elwadd, MathOperation.Elwsub]
        and math_fidelity != MathFidelity.LoFi
    ):
        pytest.skip("Math fidelity only affects multiplication operations")

    input_dimensions = [TILE_DIM, TILE_DIM]

    src_A, tile_cnt_A, src_B, _ = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        output_format=formats.output_format,
    )

    generate_golden = get_golden_generator(EltwiseBinaryGolden)

    # Compute golden based on reuse_dest type:
    #   DEST_TO_SRCA: srcA = DEST(A), srcB = CB(B) → golden = op(A, B)
    #   DEST_TO_SRCB: srcA = CB(B), srcB = DEST(A) → golden = op(B, A)
    if reuse_dest_type == EltwiseBinaryReuseDestType.DEST_TO_SRCA:
        golden_tensor = generate_golden(
            mathop,
            src_A,
            src_B,
            formats.output_format,
            math_fidelity,
            input_format=formats.input_format,
        )
    else:  # DEST_TO_SRCB
        golden_tensor = generate_golden(
            mathop,
            src_B,
            src_A,
            formats.output_format,
            math_fidelity,
            input_format=formats.input_format,
        )

    configuration = TestConfig(
        "sources/quasar/eltwise_binary_reuse_dest_quasar_test.cpp",
        formats,
        templates=[
            MATH_FIDELITY(math_fidelity),
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
            MATH_OP(mathop=mathop),
            IMPLIED_MATH_FORMAT(),
            REUSE_DEST_TYPE(reuse_dest_type),
            DEST_SYNC(),
        ],
        runtimes=[
            TILE_COUNT(tile_cnt_A),
            NUM_FACES(4),
            TEST_FACE_DIMS(),
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
            num_faces=4,
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
