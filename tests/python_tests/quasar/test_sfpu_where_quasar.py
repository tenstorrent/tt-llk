# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

from typing import List, Tuple

import pytest
import torch
from helpers.format_config import DataFormat, FormatConfig
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


def generate_golden(condition, true_value, false_value):
    """Generate golden reference for the where operation.

    Implements: result[i] = (condition[i] != 0) ? true_value[i] : false_value[i]

    Args:
        condition: Condition tensor (1D, 1024 elements).
        true_value: Values selected where condition != 0 (1D, 1024 elements).
        false_value: Values selected where condition == 0 (1D, 1024 elements).

    Returns:
        1D golden tensor (1024 elements).
    """
    mask = condition.view(32, 32) != 0
    return torch.where(
        mask, true_value.view(32, 32), false_value.view(32, 32)
    ).flatten()


def _is_invalid_quasar_combination(
    fmt: FormatConfig, dest_acc: DestAccumulation
) -> bool:
    """Check if a format/dest_acc combination is invalid for Quasar where test.

    Args:
        fmt: Format configuration with input and output formats.
        dest_acc: Destination accumulation mode.

    Returns:
        True if the combination is invalid and should be skipped.
    """
    in_fmt = fmt.input_format
    out_fmt = fmt.output_format

    # 1. Quasar packer: non-Float32 -> Float32 needs dest_acc=Yes
    if (
        in_fmt != DataFormat.Float32
        and out_fmt == DataFormat.Float32
        and dest_acc == DestAccumulation.No
    ):
        return True

    # 2. Float32 input -> Float16 output requires dest_acc=Yes on Quasar
    if (
        in_fmt == DataFormat.Float32
        and out_fmt == DataFormat.Float16
        and dest_acc == DestAccumulation.No
    ):
        return True

    return False


def generate_sfpu_where_combinations(
    formats_list: List[FormatConfig],
) -> List[Tuple[FormatConfig, DestAccumulation, ImpliedMathFormat, str]]:
    """Generate test parameter combinations for the where operation.

    Produces (format, dest_acc, implied_math_format, test_case) tuples,
    filtering out invalid Quasar combinations.

    Args:
        formats_list: List of input-output format pairs.

    Returns:
        List of (format, dest_acc, implied_math_format, test_case) tuples.
    """
    combinations = []

    for fmt in formats_list:
        in_fmt = fmt.input_format

        # 32-bit formats require dest_acc=Yes
        dest_acc_modes = (
            (DestAccumulation.Yes,)
            if in_fmt.is_32_bit()
            else (DestAccumulation.No, DestAccumulation.Yes)
        )

        for dest_acc in dest_acc_modes:
            if _is_invalid_quasar_combination(fmt, dest_acc):
                continue

            # Float16_b with dest_acc=Yes is not supported
            if in_fmt == DataFormat.Float16_b and dest_acc == DestAccumulation.Yes:
                continue

            for implied_math_format in [ImpliedMathFormat.No, ImpliedMathFormat.Yes]:
                # MX formats require implied_math_format=Yes
                if (
                    in_fmt.is_mx_format()
                    and implied_math_format == ImpliedMathFormat.No
                ):
                    continue

                for test_case in ["mixed", "all_ones", "all_zeros"]:
                    combinations.append((fmt, dest_acc, implied_math_format, test_case))

    return combinations


# Format list from planner spec: codegen/artifacts/where_phase1_spec.md
# "where" is a universal conditional select -- all formats that the SFPU can
# load/store are semantically valid. Using same=True since the natural use
# case is same input/output format (select from same-format tensors).
#
# NOTE: Int32 is excluded because the SFPU kernel uses SFPLOAD/SFPSTORE in
# DEFAULT mode, which operates on float-encoded data in Dest. For Int32,
# the raw 32-bit integer bits are misinterpreted as float by the SFPU,
# producing all-zero output. Int32 where is supported via the FPU-based
# ttnn_where path (test_zzz_ttnn_where.py), not the SFPU ternary path.
SFPU_WHERE_FORMATS = input_output_formats(
    [
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Float32,
    ],
    same=True,
)


@pytest.mark.quasar
@parametrize(
    formats_dest_acc_imf_test_case=generate_sfpu_where_combinations(SFPU_WHERE_FORMATS),
)
def test_sfpu_where_quasar(formats_dest_acc_imf_test_case):
    """Test where (conditional select) operation on Quasar architecture.

    The where operation selects between true_value and false_value based on
    a condition tensor:
        result[i] = (condition[i] != 0) ? true_value[i] : false_value[i]

    Three test cases exercise the conditional logic:
    - mixed: random condition values (mix of zero and non-zero)
    - all_ones: all conditions true (output == true_value)
    - all_zeros: all conditions false (output == false_value)

    The test unpacks 3 tiles into Dest (condition at index 0, true_value at
    index 1, false_value at index 2), then runs the ternary SFPU kernel which
    writes the result to index 0, and packs the result tile.
    """
    (formats, dest_acc, implied_math_format, test_case) = (
        formats_dest_acc_imf_test_case[0]
    )

    # Set seed for reproducibility
    torch.manual_seed(42)

    input_dimensions = [32, 32]
    num_faces = 4

    # Generate 3 input tensors: condition (A), true_value (B), false_value (C)
    src_condition, _, src_true, _ = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        sfpu=True,
    )

    src_false, _, src_B_dummy, _ = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        sfpu=True,
    )

    # Modify the condition tensor based on test case
    if test_case == "all_ones":
        src_condition = torch.ones_like(src_condition)
    elif test_case == "all_zeros":
        src_condition = torch.zeros_like(src_condition)
    # For "mixed", use as-is (contains non-zero values from stimuli)

    # Compute golden reference
    golden = generate_golden(src_condition, src_true, src_false)

    # Concatenate 3 tiles into buffer_A: [condition | true_value | false_value]
    # The C++ test unpacks 3 tiles from buffer_A into Dest indices 0, 1, 2
    combined_buffer = torch.cat([src_condition, src_true, src_false])
    tile_count_combined = 3

    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    configuration = TestConfig(
        "sources/quasar/sfpu_where_quasar_test.cpp",
        formats,
        templates=[
            MATH_OP(mathop=MathOperation.SfpuWhere),
            IMPLIED_MATH_FORMAT(implied_math_format),
            DATA_COPY_TYPE(DataCopyType.A2D),
            UNPACKER_ENGINE_SEL(
                UnpackerEngine.UnpDest if unpack_to_dest else UnpackerEngine.UnpA
            ),
            DEST_SYNC(),
        ],
        runtimes=[
            TILE_COUNT(tile_count_combined),
            NUM_FACES(num_faces),
            TEST_FACE_DIMS(),
            DEST_INDEX(0),
        ],
        variant_stimuli=StimuliConfig(
            combined_buffer,
            formats.input_format,
            src_B_dummy,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_count_combined,
            tile_count_B=1,
            tile_count_res=1,
            num_faces=num_faces,
        ),
        unpack_to_dest=unpack_to_dest,
        dest_acc=dest_acc,
    )

    res_from_L1 = configuration.run().result

    # Verify results against golden
    assert len(res_from_L1) == len(
        golden
    ), "Result tensor and golden tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    assert passed_test(
        golden, res_tensor, formats.output_format
    ), "Assert against golden failed"
