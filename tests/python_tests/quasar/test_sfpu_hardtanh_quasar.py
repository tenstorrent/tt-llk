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

# Hardtanh constants matching the C++ test source.
# min_val=-1.0 and max_val=1.0 match PyTorch torch.nn.functional.hardtanh defaults.
MIN_VAL = -1.0
MAX_VAL = 1.0


def prepare_hardtanh_inputs(
    src_A: torch.Tensor,
    src_B: torch.Tensor,
    input_format: DataFormat,
    output_format: DataFormat,
) -> torch.Tensor:
    """
    Prepare input tensor for hardtanh operation with values spanning both
    sides of the clamp range [-1.0, 1.0].

    Distributes values across three regions:
    - Below min_val (-1.0): will be clamped to -1.0
    - Between min_val and max_val: will pass through unchanged
    - Above max_val (1.0): will be clamped to 1.0

    Args:
        src_A: Source tensor A (used for magnitude distribution)
        src_B: Source tensor B (used for region selection)
        input_format: Input data format
        output_format: Output data format

    Returns:
        Prepared tensor with values spanning both sides of the clamp range
    """
    input_torch_format = format_dict[input_format]
    output_torch_format = format_dict[output_format]
    input_finfo = torch.finfo(input_torch_format)
    output_finfo = torch.finfo(output_torch_format)

    # Output must fit in the output format. For hardtanh, the output is
    # clamped to [min_val, max_val], so output range is bounded.
    # But we want input values that go well outside the clamp range.
    max_safe_value = min(input_finfo.max, output_finfo.max) * 0.9

    # Limit to reasonable bounds for bfloat16 precision
    if input_torch_format == torch.bfloat16:
        max_safe_value = min(max_safe_value, 1e4)
    else:
        max_safe_value = min(max_safe_value, input_finfo.max * 0.9)

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

    # Normalize src_B to [0, 1] for region selection
    src_B_min = src_B_float.min()
    src_B_max = src_B_float.max()
    src_B_normalized = (
        (src_B_float - src_B_min) / (src_B_max - src_B_min)
        if src_B_max > src_B_min
        else torch.zeros_like(src_B_float)
    )

    # Create values that span three regions:
    # ~33% below min_val (will be clamped to min_val)
    # ~34% between min_val and max_val (will pass through)
    # ~33% above max_val (will be clamped to max_val)
    result = torch.zeros_like(src_A_float)

    below_mask = src_B_normalized < 0.33
    middle_mask = (src_B_normalized >= 0.33) & (src_B_normalized < 0.67)
    above_mask = src_B_normalized >= 0.67

    # Below min_val: range from -max_safe_value to just below min_val
    below_values = -max_safe_value + src_A_normalized * (
        MIN_VAL - 0.1 + max_safe_value
    )

    # Between min_val and max_val: linear spread
    middle_values = MIN_VAL + src_A_normalized * (MAX_VAL - MIN_VAL)

    # Above max_val: range from just above max_val to max_safe_value
    above_values = (MAX_VAL + 0.1) + src_A_normalized * (
        max_safe_value - MAX_VAL - 0.1
    )

    result = torch.where(below_mask, below_values, result)
    result = torch.where(middle_mask, middle_values, result)
    result = torch.where(above_mask, above_values, result)
    result = torch.clamp(result, -max_safe_value, max_safe_value)
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


def generate_sfpu_hardtanh_combinations(
    formats_list: List[FormatConfig],
):
    """
    Generate SFPU hardtanh test combinations.

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


# Float-only SFPU op per planner spec (codegen/artifacts/hardtanh_phase1_spec.md
# "Recommended Test Formats"): hardtanh operates on FP32 in SFPU, constants
# are loaded as BF16. Integer formats excluded.
SFPU_HARDTANH_FORMATS = input_output_formats(
    [
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Float32,
    ]
)


@pytest.mark.quasar
@parametrize(
    formats_dest_acc_implied_math_input_dims=generate_sfpu_hardtanh_combinations(
        SFPU_HARDTANH_FORMATS
    ),
)
def test_sfpu_hardtanh_quasar(formats_dest_acc_implied_math_input_dims):
    """
    Test hardtanh operation on Quasar architecture.

    Hardtanh clamps input values to [-1.0, 1.0]:
    - Values below -1.0 are replaced with -1.0
    - Values above 1.0 are replaced with 1.0
    - Values between -1.0 and 1.0 pass through unchanged

    Uses PyTorch's torch.nn.functional.hardtanh(input, -1.0, 1.0) as the golden
    reference.
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

    # Prepare inputs with values spanning both sides of the clamp range
    src_A = prepare_hardtanh_inputs(
        src_A, src_B, formats.input_format, formats.output_format
    )

    num_faces = 4

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        MathOperation.Hardtanh,
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
        "sources/quasar/sfpu_hardtanh_quasar_test.cpp",
        formats,
        templates=[
            MATH_OP(mathop=MathOperation.Hardtanh),
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
