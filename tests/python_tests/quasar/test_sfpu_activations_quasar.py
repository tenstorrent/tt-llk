# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

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

# CELU alpha constant matching the C++ test source.
# alpha=1.0 means CELU is mathematically identical to ELU with alpha=1.0.
# Golden generator uses torch.nn.functional.celu(input, alpha=1.0).
CELU_ALPHA = 1.0


def prepare_celu_inputs(
    src_A: torch.Tensor,
    src_B: torch.Tensor,
    input_format: DataFormat,
    output_format: DataFormat,
) -> torch.Tensor:
    """
    Prepare input tensor for CELU operation.

    CELU(x) = x if x >= 0, alpha * (exp(x / alpha) - 1) if x < 0.
    For negative inputs, exp(x/alpha) must not underflow. With alpha=1.0,
    this is identical to ELU. The SFPU hardware exp approximation works well
    for moderate negative values. Limit input range to avoid extreme values
    that cause overflow/underflow.

    Uses a log-uniform distribution to ensure good coverage of both positive
    and negative branches.

    Args:
        src_A: Source tensor A (used for magnitude distribution)
        src_B: Source tensor B (used for sign/branch distribution)
        input_format: Input data format
        output_format: Output data format

    Returns:
        Prepared tensor with values spanning both positive and negative ranges
    """
    input_torch_format = format_dict[input_format]
    output_torch_format = format_dict[output_format]
    input_finfo = torch.finfo(input_torch_format)
    output_finfo = torch.finfo(output_torch_format)

    # For CELU, the positive path is identity so positive values must fit in output.
    # For negative path, output approaches -alpha (=-1.0), which is always safe.
    # Limit positive values to fit in the output format.
    max_safe_value = min(input_finfo.max, output_finfo.max) * 0.9

    # Limit to reasonable bounds for bfloat16 precision
    if input_torch_format == torch.bfloat16:
        max_safe_value = min(max_safe_value, 1e4)
    else:
        max_safe_value = min(max_safe_value, input_finfo.max * 0.9)

    # For negative inputs, limit magnitude to avoid exp() underflow.
    # exp(-10) ~ 4.5e-5, which is representable in bfloat16. Beyond -80 or so,
    # exp(x) underflows to 0 and CELU saturates at -alpha.
    max_neg_magnitude = 10.0

    src_A_float = src_A.to(torch.float32)
    src_B_float = src_B.to(torch.float32)

    # Normalize src_A to [0, 1]
    src_A_min = src_A_float.min()
    src_A_max = src_A_float.max()
    src_A_normalized = (
        (src_A_float - src_A_min) / (src_A_max - src_A_min)
        if src_A_max > src_A_min
        else torch.zeros_like(src_A_float)
    )

    # Normalize src_B to [0, 1] for branch selection
    src_B_min = src_B_float.min()
    src_B_max = src_B_float.max()
    src_B_normalized = (
        (src_B_float - src_B_min) / (src_B_max - src_B_min)
        if src_B_max > src_B_min
        else torch.zeros_like(src_B_float)
    )

    # Create values spanning both sides of zero:
    # ~50% negative (will go through exp/sub/mul path)
    # ~50% positive (will pass through as identity)
    result = torch.zeros_like(src_A_float)

    # Negative values: range from -max_neg_magnitude to just below 0
    neg_mask = src_B_normalized < 0.5
    neg_values = -max_neg_magnitude + src_A_normalized * (max_neg_magnitude - 0.01)

    # Positive values: range from just above 0 to max_safe_value
    pos_values = 0.01 + src_A_normalized * (max_safe_value - 0.01)

    result = torch.where(neg_mask, neg_values, pos_values)
    result = result.to(input_torch_format)

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

    # Float32 input -> Float16 output requires dest_acc=Yes on Quasar
    if (
        in_fmt == DataFormat.Float32
        and out_fmt == DataFormat.Float16
        and dest_acc == DestAccumulation.No
    ):
        return True

    return False


def generate_sfpu_celu_combinations(
    formats_list: List[FormatConfig],
):
    """
    Generate SFPU CELU test combinations.

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
                for input_dimensions in [[32, 32], [64, 64], [32, 64]]:
                    combinations.append(
                        (fmt, dest_acc, implied_math_format, input_dimensions)
                    )

    return combinations


# Float-only SFPU op per planner spec (codegen/artifacts/activations_phase1_spec.md
# "Recommended Test Formats"): CELU requires exp() which is float-only.
# Integer formats excluded. MX formats excluded for initial testing.
SFPU_CELU_FORMATS = input_output_formats(
    [
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Float32,
    ]
)


@pytest.mark.quasar
@parametrize(
    formats_dest_acc_implied_math_input_dims=generate_sfpu_celu_combinations(
        SFPU_CELU_FORMATS
    ),
)
def test_sfpu_activations_quasar(formats_dest_acc_implied_math_input_dims):
    """
    Test CELU activation on Quasar architecture.

    CELU(x) = x if x >= 0, alpha * (exp(x / alpha) - 1) if x < 0.
    Uses alpha=1.0 matching the C++ test constant and golden generator default.
    Uses PyTorch's torch.nn.functional.celu(input, alpha=1.0) as the golden reference.
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

    # Prepare inputs with values spanning both positive and negative ranges
    src_A = prepare_celu_inputs(
        src_A, src_B, formats.input_format, formats.output_format
    )

    num_faces = 4

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        MathOperation.Celu,
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
        "sources/quasar/sfpu_activations_quasar_test.cpp",
        formats,
        templates=[
            MATH_OP(mathop=MathOperation.Celu),
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
