# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from conftest import skip_for_blackhole, skip_for_wormhole
from helpers.chip_architecture import ChipArchitecture, get_chip_architecture
from helpers.format_config import DataFormat
from helpers.golden_generators import UnarySFPUGolden, get_golden_generator
from helpers.llk_params import (
    ApproximationMode,
    DataCopyType,
    DestAccumulation,
    ImpliedMathFormat,
    MathOperation,
    UnpackerEngine,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import BootMode, TestConfig
from helpers.test_variant_parameters import (
    APPROX_MODE,
    DATA_COPY_TYPE,
    DEST_INDEX,
    DEST_SYNC,
    IMPLIED_MATH_FORMAT,
    INPUT_DIMENSIONS,
    MATH_OP,
    NUM_FACES,
    TEST_FACE_DIMS,
    TILE_COUNT,
    UNPACKER_ENGINE_SEL,
)
from helpers.utils import passed_test


@pytest.mark.quasar
@skip_for_blackhole
@skip_for_wormhole
@parametrize(
    test_name="sfpu_rsqrt_quasar_test",
    formats=input_output_formats(
        [
            # DataFormat.Float32,
            DataFormat.Float16,
            # DataFormat.Float16_b,
        ],
    ),
    approx_mode=[
        ApproximationMode.Yes
    ],  # Only approximation mode is supported for rsqrt quasar implementation
    dest_acc=[DestAccumulation.Yes],
    implied_math_format=[ImpliedMathFormat.No, ImpliedMathFormat.Yes],
)
def test_sfpu_rsqrt_quasar(
    test_name, formats, approx_mode, dest_acc, implied_math_format
):
    """
    Test reciprocal square root (rsqrt) operation on Quasar architecture.

    Uses PyTorch's rsqrt as the golden reference and generates input stimuli
    in the range (0, 1] to match PyTorch's rsqrt behavior.
    """
    chip_arch = get_chip_architecture()
    if chip_arch != ChipArchitecture.QUASAR:
        pytest.skip(f"This test is Quasar-specific, but running on {chip_arch}")

    input_dimensions = [32, 32]

    # Generate stimuli using the standard helper function
    src_A, tile_cnt_A, src_B, _ = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
    )

    # Generate random values in range (0, 1] for rsqrt testing
    # Avoid exactly 0 to prevent division by zero in rsqrt = 1/sqrt(x)
    # Preserve the original dtype from generate_stimuli
    src_A = (torch.rand_like(src_A) * 0.99 + 0.01).to(src_A.dtype)

    # Generate golden reference using UnarySFPUGolden for rsqrt operation
    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        MathOperation.Rsqrt,
        src_A,
        formats.output_format,
        dest_acc,
        formats.input_format,
        input_dimensions,
    )

    num_faces = 4

    configuration = TestConfig(
        "sources/quasar/sfpu_rsqrt_quasar_test.cpp",
        formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
            MATH_OP(mathop=MathOperation.Rsqrt),
            APPROX_MODE(approx_mode),
            IMPLIED_MATH_FORMAT(implied_math_format),
            DATA_COPY_TYPE(DataCopyType.A2D),
            UNPACKER_ENGINE_SEL(UnpackerEngine.UnpA),  # Unary SFPU operations use UnpA
            DEST_SYNC(),
        ],
        runtimes=[
            TILE_COUNT(tile_cnt_A),
            NUM_FACES(num_faces),
            TEST_FACE_DIMS(),
            DEST_INDEX(0),
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
            sfpu=True,
        ),
        unpack_to_dest=(
            formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
        ),
        dest_acc=dest_acc,
        boot_mode=BootMode.DEFAULT,
    )

    res_from_L1 = configuration.run()

    # Verify results match golden
    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(golden_tensor, res_tensor, formats.output_format)
