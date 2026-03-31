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


def prepare_hardsigmoid_inputs(
    src_A: torch.Tensor,
    src_B: torch.Tensor,
    input_format: DataFormat,
    output_format: DataFormat,
) -> torch.Tensor:
    """
    Prepare input tensor for Hardsigmoid operation.

    Hardsigmoid(x) = clamp(x/6 + 0.5, 0, 1)
    - For x <= -3: output = 0
    - For x >= 3: output = 1
    - For -3 < x < 3: output = x/6 + 0.5 (linear region)

    We want good coverage across all three regions, so we generate values
    spanning roughly [-6, 6] which covers both saturation regions and the
    linear region well.

    Args:
        src_A: Source tensor A (used for magnitude distribution)
        src_B: Source tensor B (used for sign/branch distribution)
        input_format: Input data format
        output_format: Output data format

    Returns:
        Prepared tensor with values spanning the hardsigmoid input range
    """
    input_torch_format = format_dict[input_format]

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

    # Create values spanning [-6, 6] to cover all three hardsigmoid regions:
    # ~33% saturated low (x <= -3 -> output = 0)
    # ~33% linear (-3 < x < 3 -> output = x/6 + 0.5)
    # ~33% saturated high (x >= 3 -> output = 1)
    result = torch.zeros_like(src_A_float)

    # Region 1: saturated low, x in [-6, -3]
    low_mask = src_B_normalized < 0.33
    low_values = -6.0 + src_A_normalized * 3.0  # range [-6, -3]

    # Region 2: linear, x in [-3, 3]
    mid_mask = (src_B_normalized >= 0.33) & (src_B_normalized < 0.66)
    mid_values = -3.0 + src_A_normalized * 6.0  # range [-3, 3]

    # Region 3: saturated high, x in [3, 6]
    high_values = 3.0 + src_A_normalized * 3.0  # range [3, 6]

    result = torch.where(low_mask, low_values, torch.where(mid_mask, mid_values, high_values))
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


def generate_sfpu_hardsigmoid_combinations(
    formats_list: List[FormatConfig],
):
    """
    Generate SFPU Hardsigmoid test combinations.

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


# Float-only SFPU op: Hardsigmoid uses multiply/add/clamp which are float operations.
# Integer formats excluded. MX formats excluded for initial testing.
SFPU_HARDSIGMOID_FORMATS = input_output_formats(
    [
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Float32,
    ]
)


@pytest.mark.quasar
@parametrize(
    formats_dest_acc_implied_math_input_dims=generate_sfpu_hardsigmoid_combinations(
        SFPU_HARDSIGMOID_FORMATS
    ),
)
def test_sfpu_activations_hardsigmoid_quasar(formats_dest_acc_implied_math_input_dims):
    """
    Test Hardsigmoid activation on Quasar architecture.

    Hardsigmoid(x) = clamp(x * (1/6) + 0.5, 0, 1)
    Uses PyTorch's torch.nn.functional.hardsigmoid(input) as the golden reference.
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

    # Prepare inputs with values spanning all three hardsigmoid regions
    src_A = prepare_hardsigmoid_inputs(
        src_A, src_B, formats.input_format, formats.output_format
    )

    num_faces = 4

    generate_golden = get_golden_generator(UnarySFPUGolden)
    golden_tensor = generate_golden(
        MathOperation.Hardsigmoid,
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
        "sources/quasar/sfpu_activations_hardsigmoid_quasar_test.cpp",
        formats,
        templates=[
            MATH_OP(mathop=MathOperation.Hardsigmoid),
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
