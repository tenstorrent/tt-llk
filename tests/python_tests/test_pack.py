# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0
"""Test pack operation with various configurations.

Tests the LLK pack kernel with:
- Different data formats (Float16_b, Float16, Float32, Int32, Bfp8_b)
- Destination accumulation modes
- Variable tile dimensions
- ReLU activation
- Destination sync modes (SyncHalf for double-buffering, SyncFull for single-buffering)
"""

import pytest
import torch
from helpers.constraints import (
    get_valid_dest_accumulation_modes,
    get_valid_dest_indices,
)
from helpers.data_format_inference import infer_data_formats
from helpers.device import collect_results, write_stimuli_to_l1
from helpers.format_config import DataFormat
from helpers.golden_generators import PackGolden, get_golden_generator
from helpers.llk_params import (
    DestAccumulation,
    DstSync,
    PackerReluType,
    format_dict,
)
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import run_test
from helpers.utils import passed_test


def is_relu_threshold_tolerance_issue(
    golden_tensor,
    result_tensor,
    relu_config,
    threshold,
    data_format,
    tolerance_factor=0.01,
):
    """
    Check if test failure is due to threshold rounding/format conversion issues in ReLU.

    When a value is very close to the threshold, golden (Python) and hardware (Tensix)
    may make different decisions due to:
    - FP16/BF16 precision differences
    - Rounding during format conversions
    - Threshold encoding/decoding precision loss
    With values relatively close to the threshold, these small differences can lead to
    one side being clamped to zero while the other retains a small non-zero value.
    This function checks if all mismatches between golden and result tensors
    can be explained by such near-threshold issues.

    Args:
        golden_tensor: Expected output tensor
        result_tensor: Actual hardware output tensor
        relu_config: The ReLU configuration value
        threshold: The threshold value used
        data_format: The data format being used
        tolerance_factor: Relative tolerance around threshold (default 1% of threshold)

    Returns:
        bool: True if all mismatches are near-threshold rounding issues, False otherwise
    """
    relu_type = PackerReluType(relu_config & 0x3)

    # Only applicable for threshold-based ReLU modes
    # Zero relu is exact because of the sign bit, so no tolerance issues there.
    if relu_type not in [
        PackerReluType.MinThresholdRelu,
        PackerReluType.MaxThresholdRelu,
    ]:
        return False

    mismatches = ~torch.isclose(golden_tensor, result_tensor, rtol=0.05, atol=0.05)

    # Define tolerance band around threshold
    threshold_tolerance = abs(threshold) * tolerance_factor if threshold != 0 else 0.01
    threshold_upper_boundary = threshold + threshold_tolerance

    # Get the original input values that correspond to mismatches
    # Check both golden and result to see if they're in the tolerance band
    golden_near_threshold = (
        golden_tensor[mismatches].abs() <= threshold_upper_boundary
    ) | ((golden_tensor[mismatches] - threshold).abs() <= threshold_tolerance)
    result_near_threshold = (
        result_tensor[mismatches].abs() <= threshold_upper_boundary
    ) | ((result_tensor[mismatches] - threshold).abs() <= threshold_tolerance)

    if relu_type == PackerReluType.MinThresholdRelu:
        # One side should be 0, other should be near threshold
        golden_is_zero = golden_tensor[mismatches] == 0.0
        result_is_zero = result_tensor[mismatches] == 0.0

        acceptable = (golden_is_zero & result_near_threshold) | (
            result_is_zero & golden_near_threshold
        )

        return acceptable.all().item()

    # For MAX_THRESHOLD_RELU: Check if mismatch is near the threshold clamp point
    elif relu_type == PackerReluType.MaxThresholdRelu:
        both_near_threshold = golden_near_threshold & result_near_threshold

        allowed_difference = (
            golden_tensor[mismatches] - result_tensor[mismatches]
        ).abs() <= threshold_tolerance

        acceptable = both_near_threshold & allowed_difference

        return acceptable.all().item()

    return False


@parametrize(
    test_name="pack_test",
    formats=input_output_formats(
        [
            # DataFormat.Float16_b,
            # DataFormat.Float16,
            # DataFormat.Float32,
            DataFormat.Int32,
            # DataFormat.Bfp8_b,
        ]
    ),
    dest_acc=lambda formats: get_valid_dest_accumulation_modes(formats),
    input_dimensions=[[32, 32], [64, 64], [32, 64], [64, 32]],
    relu_type=[
        PackerReluType.NoRelu,
        PackerReluType.ZeroRelu,
        PackerReluType.MinThresholdRelu,
        PackerReluType.MaxThresholdRelu,
    ],
    dst_sync=[DstSync.SyncHalf, DstSync.SyncFull],
    dest_index=lambda dest_acc, dst_sync, input_dimensions: get_valid_dest_indices(
        dest_sync=dst_sync,
        dest_acc=dest_acc,
        tile_count=(input_dimensions[0] * input_dimensions[1]) // (32 * 32),
    ),
)
def test_pack(
    test_name, formats, dest_acc, input_dimensions, relu_type, dst_sync, dest_index
):

    if (formats.input_format == DataFormat.Int32) ^ (
        formats.output_format == DataFormat.Int32
    ):
        pytest.skip(
            "Pack does not support mixing Int32 with other formats. Check format conversions in packer for more information."
        )

    src_A, src_B, tile_cnt = generate_stimuli(
        formats.input_format,
        formats.input_format,
        input_dimensions=input_dimensions,
        negative_values=True,
    )

    generate_golden = get_golden_generator(PackGolden)
    golden_tensor = generate_golden(
        src_A,
        formats.output_format,
        input_dimensions=input_dimensions,
    )

    unpack_to_dest = (
        formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes
    )

    # To come as close as possible to actual hardware behavior, we infer data formats here
    # and use the inferred pack_src format for ReLU operations.
    data_formats = infer_data_formats(
        formats.input_format, formats.output_format, dest_acc, unpack_to_dest
    )

    if data_formats.pack_src.is_integer() and relu_type in [
        PackerReluType.MinThresholdRelu,
        PackerReluType.MaxThresholdRelu,
    ]:
        pytest.skip(
            "Pack source format cannot be an integer format with ReLu Type: "
            + str(relu_type)
        )

    tensor_average = (
        torch.mean(golden_tensor).item()
        if not formats.output_format.is_integer()
        else 0.0
    )

    relu_config = PackGolden.generate_relu_config(
        relu_type,
        relu_threshold=tensor_average,  # We use the average value for this.
        intermediate_format=data_formats.pack_src,
    )

    # Perform relu.
    golden_tensor = PackGolden.apply_relu(
        golden_tensor,
        relu_config,
        data_formats.pack_src,
    )

    test_config = {
        "formats": formats,
        "testname": test_name,
        "tile_cnt": tile_cnt,
        "input_A_dimensions": input_dimensions,
        "input_B_dimensions": input_dimensions,
        "unpack_to_dest": unpack_to_dest,
        "dest_acc": dest_acc,
        "relu_config": relu_config,
        "dst_sync": dst_sync,
        "dest_index": dest_index,
    }

    res_address = write_stimuli_to_l1(
        test_config,
        src_A,
        src_B,
        formats.input_format,
        formats.input_format,
        tile_count_A=tile_cnt,
        tile_count_B=tile_cnt,
    )

    run_test(test_config)

    res_from_L1 = collect_results(
        formats,
        tile_count=tile_cnt,
        address=res_address,
    )
    assert len(res_from_L1) == len(golden_tensor)

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    test_passed = passed_test(golden_tensor, res_tensor, formats.output_format)

    if not test_passed and relu_type in [
        PackerReluType.MinThresholdRelu,
        PackerReluType.MaxThresholdRelu,
    ]:

        # When a datum is extremely close to the ReLU threshold, differences can arise due to
        # floating point precision limitations and rounding during format conversions.
        # We check if all mismatches are within a small tolerance of the threshold. If so, we consider the test as passed.
        is_tolerance_issue = is_relu_threshold_tolerance_issue(
            golden_tensor,
            res_tensor,
            relu_config,
            tensor_average,
            formats.output_format,
        )

        if is_tolerance_issue:
            print(
                f"This is a packer RELU threshold precision difference between hardware and software and it's not an issue. "
            )
            test_passed = True

    assert test_passed
