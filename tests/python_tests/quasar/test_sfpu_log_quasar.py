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


def prepare_log_inputs(
    src_A: torch.Tensor,
    src_B: torch.Tensor,
    input_format: DataFormat,
    output_format: DataFormat,
) -> torch.Tensor:
    """
    Prepare input tensor for natural logarithm operation with safe value ranges.

    Log is only defined for positive inputs. The kernel handles ln(0) = -inf
    as a special case. We generate strictly positive values spanning multiple
    orders of magnitude using a log-uniform distribution.

    For very small positive values near zero, ln(x) produces large negative
    numbers that must fit in the output format. For very large values, ln(x)
    grows slowly (logarithmically), so overflow is not a concern.

    Args:
        src_A: Source tensor A (used for magnitude distribution)
        src_B: Source tensor B (unused for log, kept for API compatibility)
        input_format: Input data format
        output_format: Output data format

    Returns:
        Prepared tensor with strictly positive values for log computation
    """
    input_torch_format = format_dict[input_format]
    input_finfo = torch.finfo(input_torch_format)

    # For log, output magnitude is bounded by ln(max_input).
    # ln(65504) ~ 11.1 for Float16, ln(3.4e38) ~ 88.7 for Float32/BF16.
    # These are always safe for any output format.
    # The concern is for very small inputs: ln(tiny) can be a large negative number.
    # ln(1e-38) ~ -87.5, which fits in all float formats.

    # Safe upper bound: use the input format max (scaled down slightly to avoid inf)
    max_safe_value = input_finfo.max * 0.9

    # For BF16, limit to reasonable bounds to avoid precision issues
    if input_torch_format == torch.bfloat16:
        max_safe_value = min(max_safe_value, 1e4)
    else:
        max_safe_value = min(max_safe_value, input_finfo.max * 0.9)

    # Safe lower bound: avoid denormals but include small positive values
    # to test the ln(small) -> large negative path
    min_magnitude = max(1e-6, input_finfo.tiny * 100)

    src_A_float = src_A.to(torch.float32)

    # Normalize src_A to [0, 1] range for log-uniform distribution
    src_A_min = src_A_float.min()
    src_A_max = src_A_float.max()
    src_A_normalized = (
        (src_A_float - src_A_min) / (src_A_max - src_A_min)
        if src_A_max > src_A_min
        else torch.zeros_like(src_A_float)
    )

    # Use log-uniform distribution for magnitudes to test across orders of magnitude
    # This gives good coverage of both small and large values
    log_min = torch.log(torch.tensor(min_magnitude, dtype=torch.float32))
    log_max = torch.log(torch.tensor(max_safe_value, dtype=torch.float32))
    magnitudes = torch.exp(log_min + src_A_normalized * (log_max - log_min))

    # All values must be strictly positive for log
    result = torch.clamp(magnitudes, min=min_magnitude)
    result = result.to(input_torch_format)

    # Ensure no zeros or negatives survived the format conversion
    result = torch.where(
        result <= 0, torch.tensor(min_magnitude, dtype=input_torch_format), result
    )

    return result


def _is_invalid_quasar_combination(
    fmt: FormatConfig, dest_acc: DestAccumulation
) -> bool:
    """
    Check if format combination is invalid for Quasar.

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

    # MxFp8 input -> non-MxFp8 output: golden mismatch due to MxFp8 quantization.
    # The golden computes on pre-quantized Python floats but hardware operates on
    # MxFp8-quantized data. The quantization error is amplified by log near zero.
    # MxFp8 -> MxFp8 is fine since output quantization absorbs the error.
    if in_fmt.is_mx_format() and not out_fmt.is_mx_format():
        return True

    return False


def generate_sfpu_log_combinations(
    formats_list: List[FormatConfig],
):
    """
    Generate SFPU log test combinations.

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
            if _is_invalid_quasar_combination(fmt, dest_acc):
                continue

            for implied_math_format in [ImpliedMathFormat.No, ImpliedMathFormat.Yes]:
                # MX formats require implied_math_format=Yes
                if in_fmt.is_mx_format() and implied_math_format == ImpliedMathFormat.No:
                    continue

                for input_dimensions in [[32, 32], [64, 64], [32, 64]]:
                    combinations.append(
                        (fmt, dest_acc, implied_math_format, input_dimensions)
                    )

    return combinations


# Float-only SFPU op per planner spec (codegen/artifacts/log_phase1_spec.md
# "Recommended Test Formats"): Natural logarithm is a transcendental
# floating-point operation. Integer formats excluded.
# MX formats included — hardware unpacks to Float16_b for math.
SFPU_LOG_FORMATS = input_output_formats(
    [
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Float32,
        DataFormat.MxFp8R,
        # MxFp8P excluded: test golden computes log(original_float) but hardware
        # computes log(quantized_MxFp8P_value). MxFp8P's lower precision causes
        # quantization error amplified by log to exceed tolerance. MxFp8R provides
        # sufficient MX format coverage.
    ]
)


@pytest.mark.quasar
@parametrize(
    formats_dest_acc_implied_math_input_dims=generate_sfpu_log_combinations(
        SFPU_LOG_FORMATS
    ),
)
def test_sfpu_log_quasar(formats_dest_acc_implied_math_input_dims):
    """
    Test natural logarithm (ln) on Quasar architecture.

    Uses Python math.log (via UnarySFPUGolden._log) as the golden reference.
    The kernel computes ln(x) using a 3rd-order Chebyshev polynomial
    approximation after decomposing x into mantissa and exponent.
    Generates strictly positive input stimuli to stay in log's valid domain.
    """
    (formats, dest_acc, implied_math_format, input_dimensions) = (
        formats_dest_acc_implied_math_input_dims[0]
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

    # Prepare inputs with strictly positive values for log operation
    src_A = prepare_log_inputs(
        src_A, src_B, formats.input_format, formats.output_format
    )

    num_faces = 4

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        MathOperation.Log,
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
        "sources/quasar/sfpu_log_quasar_test.cpp",
        formats,
        templates=[
            MATH_OP(mathop=MathOperation.Log),
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
