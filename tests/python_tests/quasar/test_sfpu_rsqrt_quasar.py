# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import List

import pytest
import torch
from helpers.format_config import DataFormat, FormatConfig
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
from helpers.test_config import TestConfig
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


def generate_sfpu_rsqrt_combinations(
    formats_list: List[FormatConfig],
):
    """
    Generate SFPU rsqrt test combinations.

    Args: Input-output format pairs

    Returns: List of (format, dest_acc, implied_math_format, input_dimensions) tuples
    """
    combinations = []

    for fmt in formats_list:
        in_fmt = fmt.input_format

        dest_acc_modes = (
            (DestAccumulation.Yes,)
            if in_fmt.is_32_bit()
            else (DestAccumulation.No, DestAccumulation.Yes)
        )
        for dest_acc in dest_acc_modes:
            # Skip invalid format combinations for Quasar
            if (
                in_fmt != DataFormat.Float32
                and fmt.output_format == DataFormat.Float32
                and dest_acc == DestAccumulation.No
            ):
                # Quasar packer does not support non-Float32 to Float32 conversion when dest_acc=No
                continue

            # For Quasar SFPU, input Float32 must be with dest_acc=Yes
            if (
                in_fmt == DataFormat.Float32
                and fmt.output_format == DataFormat.Float16
                and dest_acc == DestAccumulation.No
            ):
                # Quasar SFPU with Float32 input and Float16 output requires dest_acc=Yes
                continue

            for implied_math_format in [ImpliedMathFormat.No, ImpliedMathFormat.Yes]:
                for input_dimensions in [[32, 32], [64, 64]]:
                    combinations.append(
                        (fmt, dest_acc, implied_math_format, input_dimensions)
                    )

    return combinations


SFPU_RSQRT_FORMATS = input_output_formats(
    [
        DataFormat.Float16,
        DataFormat.Float32,
        DataFormat.Float16_b,
    ]
)


@pytest.mark.quasar
@parametrize(
    formats_dest_acc_implied_math_input_dims=generate_sfpu_rsqrt_combinations(
        SFPU_RSQRT_FORMATS
    ),
)
def test_sfpu_rsqrt_quasar(
    formats_dest_acc_implied_math_input_dims, approx_mode=ApproximationMode.Yes
):
    """
    Test reciprocal square root (rsqrt) operation on Quasar architecture.

    Uses PyTorch's rsqrt as the golden reference and generates input stimuli
    in the range [0.1, 10] to test rsqrt behavior.
    """
    (formats, dest_acc, implied_math_format, input_dimensions) = (
        formats_dest_acc_implied_math_input_dims[0]
    )

    # Generate stimuli with random values in range [0.1, 10] for Approximation Mode
    src_A, tile_cnt_A, src_B, _ = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        sfpu=True,
    )

    # Override src_A with random values in valid range for rsqrt [0.1, 10]
    torch_format = format_dict[formats.input_format]
    src_A = (
        torch.rand(input_dimensions[0] * input_dimensions[1], dtype=torch_format) * 9.9
        + 0.1
    )

    num_faces = 4

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        MathOperation.Rsqrt,
        src_A,
        formats.output_format,
        dest_acc,
        formats.input_format,
        input_dimensions,
    )

    configuration = TestConfig(
        "sources/quasar/sfpu_rsqrt_quasar_test.cpp",
        formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
            MATH_OP(mathop=MathOperation.Rsqrt),
            APPROX_MODE(approx_mode),
            IMPLIED_MATH_FORMAT(implied_math_format),
            DATA_COPY_TYPE(DataCopyType.A2D),
            UNPACKER_ENGINE_SEL(UnpackerEngine.UnpA),
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
        ),
        unpack_to_dest=(
            formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
        ),
        dest_acc=dest_acc,
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
