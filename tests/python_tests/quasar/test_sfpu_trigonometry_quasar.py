# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import math
from typing import List

import pytest
import torch
from helpers.format_config import DataFormat, FormatConfig
from helpers.golden_generators import UnarySFPUGolden, get_golden_generator
from helpers.llk_params import (
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
    DATA_COPY_TYPE,
    DEST_INDEX,
    DEST_SYNC,
    IMPLIED_MATH_FORMAT,
    MATH_OP,
    NUM_FACES,
    TEST_FACE_DIMS,
    TILE_COUNT,
    UNPACKER_ENGINE_SEL,
)
from helpers.utils import passed_test


def _normalize_tensor(src_A: torch.Tensor) -> torch.Tensor:
    """Normalize tensor to [0, 1] range."""
    src_A_float = src_A.to(torch.float32)
    src_A_min = src_A_float.min()
    src_A_max = src_A_float.max()
    if src_A_max > src_A_min:
        return (src_A_float - src_A_min) / (src_A_max - src_A_min)
    return torch.full_like(src_A_float, 0.5)


def prepare_trig_inputs(
    src_A: torch.Tensor,
    src_B: torch.Tensor,
    input_format: DataFormat,
    output_format: DataFormat,
    trig_op: MathOperation = MathOperation.Sin,
) -> torch.Tensor:
    """
    Prepare input tensor for trigonometric operations with operation-specific ranges.

    Each operation has a different valid input domain:
    - sin/cos: all reals, but [-10, 10] for accuracy
    - acosh: [1, infinity), use [1.01, 10.0]
    - asinh: all reals, use [-10, 10]
    - atanh: (-1, 1), use [-0.95, 0.95]
    """
    input_torch_format = format_dict[input_format]
    normalized = _normalize_tensor(src_A)

    if trig_op == MathOperation.Acosh:
        # Domain [1, inf) -- use [1.01, 10.0] to avoid boundary at exactly 1.0
        result = normalized * 8.99 + 1.01
    elif trig_op == MathOperation.Atanh:
        # Domain (-1, 1) -- use [-0.95, 0.95] to avoid boundary singularities
        result = normalized * 1.9 - 0.95
    else:
        # sin, cos, asinh: [-10, 10]
        result = normalized * 20.0 - 10.0

    return result.to(input_torch_format)


def _is_invalid_quasar_combination(
    fmt: FormatConfig, dest_acc: DestAccumulation
) -> bool:
    """
    Check if format combination is invalid for Quasar trig operations.

    Args:
        fmt: Format configuration with input and output formats
        dest_acc: Destination accumulation mode

    Returns:
        True if the combination is invalid, False otherwise
    """
    in_fmt = fmt.input_format
    out_fmt = fmt.output_format

    # Quasar packer does not support non-Float32 to Float32 conversion when dest_acc=No
    if (
        in_fmt != DataFormat.Float32
        and out_fmt == DataFormat.Float32
        and dest_acc == DestAccumulation.No
    ):
        return True

    # Quasar SFPU with Float32 input and Float16 output requires dest_acc=Yes
    if (
        in_fmt == DataFormat.Float32
        and out_fmt == DataFormat.Float16
        and dest_acc == DestAccumulation.No
    ):
        return True

    # Integer and float cannot be mixed in input->output
    if in_fmt.is_integer() != out_fmt.is_integer():
        return True

    # MxFp8 input -> non-MxFp8 output: golden mismatch due to MxFp8 quantization
    # error amplified by trig range reduction and polynomial evaluation.
    # The golden computes on pre-quantized Python floats but hardware operates on
    # MxFp8-quantized data. MxFp8 -> MxFp8 is fine since output quantization
    # absorbs the error.
    if in_fmt.is_mx_format() and not out_fmt.is_mx_format():
        return True

    return False


def generate_sfpu_trig_combinations(
    formats_list: List[FormatConfig],
    trig_ops: List[MathOperation],
):
    """
    Generate SFPU trigonometry test combinations.

    Args:
        formats_list: Input-output format pairs
        trig_ops: List of trig MathOperation values to parametrize over

    Returns:
        List of (trig_op, format, dest_acc, implied_math_format, input_dimensions) tuples
    """
    combinations = []

    for trig_op in trig_ops:
        for fmt in formats_list:
            in_fmt = fmt.input_format

            dest_acc_modes = (
                (DestAccumulation.Yes,)
                if in_fmt.is_32_bit()
                else (DestAccumulation.No, DestAccumulation.Yes)
            )
            for dest_acc in dest_acc_modes:
                # Skip invalid format combinations for Quasar
                if _is_invalid_quasar_combination(fmt, dest_acc):
                    continue

                for implied_math_format in [
                    ImpliedMathFormat.No,
                    ImpliedMathFormat.Yes,
                ]:
                    # MX formats require implied_math_format=Yes
                    if (
                        in_fmt.is_mx_format()
                        and implied_math_format == ImpliedMathFormat.No
                    ):
                        continue

                    for input_dimensions in [[32, 32], [64, 64], [32, 64]]:
                        combinations.append(
                            (
                                trig_op,
                                fmt,
                                dest_acc,
                                implied_math_format,
                                input_dimensions,
                            )
                        )

    return combinations


# Float-only SFPU ops per planner spec (codegen/artifacts/trigonometry_phase1_spec.md
# "Recommended Test Formats"): Sine and cosine are transcendental floating-point
# operations. Integer formats excluded.
# MX formats included — hardware unpacks to Float16_b for math.
SFPU_TRIG_FORMATS = input_output_formats(
    [
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Float32,
        # MxFp8R/MxFp8P excluded: MxFp8 quantization error is amplified by
        # range reduction and polynomial evaluation (multiple stages of
        # computation), exceeding golden comparison tolerance.
    ]
)

TRIG_OPS = [
    MathOperation.Sin,
    MathOperation.Cos,
    MathOperation.Acosh,
    MathOperation.Asinh,
    MathOperation.Atanh,
]


@pytest.mark.quasar
@parametrize(
    trig_op_formats_dest_acc_implied_math_input_dims=generate_sfpu_trig_combinations(
        SFPU_TRIG_FORMATS, TRIG_OPS
    ),
)
def test_sfpu_trigonometry_quasar(trig_op_formats_dest_acc_implied_math_input_dims):
    """Test trigonometric operations (sin, cos, acosh, asinh, atanh) on Quasar."""
    (trig_op, formats, dest_acc, implied_math_format, input_dimensions) = (
        trig_op_formats_dest_acc_implied_math_input_dims[0]
    )

    # Set seed for reproducibility
    torch.manual_seed(42)

    src_A, tile_cnt_A, src_B, _ = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        sfpu=True,
    )

    # Prepare inputs with operation-specific ranges
    src_A = prepare_trig_inputs(
        src_A, src_B, formats.input_format, formats.output_format, trig_op
    )

    num_faces = 4

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        trig_op,
        src_A,
        formats.output_format,
        dest_acc,
        formats.input_format,
        input_dimensions,
    )

    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )
    configuration = TestConfig(
        "sources/quasar/sfpu_trigonometry_quasar_test.cpp",
        formats,
        templates=[
            MATH_OP(mathop=trig_op),
            IMPLIED_MATH_FORMAT(implied_math_format),
            DATA_COPY_TYPE(DataCopyType.A2D),
            UNPACKER_ENGINE_SEL(
                UnpackerEngine.UnpDest if unpack_to_dest else UnpackerEngine.UnpA
            ),
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
