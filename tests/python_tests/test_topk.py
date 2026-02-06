# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import math

import torch
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import (
    TilizeGolden,
    TopKGolden,
    TransposeGolden,
    UntilizeGolden,
    get_golden_generator,
)
from helpers.llk_params import DestAccumulation, TopKSortDirection, format_dict
from helpers.param_config import input_output_formats, parametrize
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    DEST_SYNC,
    INPUT_DIMENSIONS,
    TILE_COUNT,
    TOPK,
)
from helpers.utils import passed_test


def transform_result_tensor_to_right_form(
    res_tensor, formats, tile_cnt_A, input_dimensions
):

    transpose_util = get_golden_generator(TransposeGolden)

    # First: transpose faces (swap face positions).
    res_tensor = transpose_util.transpose_faces_multi_tile(
        res_tensor,
        formats.output_format,
        num_tiles=tile_cnt_A,
        tilize=False,
        untilize=False,
        input_dimensions=input_dimensions,
    )

    # Then: transpose within each face.
    res_tensor = transpose_util.transpose_within_faces_multi_tile(
        res_tensor,
        formats.output_format,
        num_tiles=tile_cnt_A,
        tilize=False,
        untilize=False,
        input_dimensions=input_dimensions,
    )

    return res_tensor


def prepare_input_tensor_for_topk(src_A, formats, input_dimensions=[32, 128]):

    if input_dimensions != [32, 128]:
        raise ValueError(
            "This function is specifically designed for input dimensions [32, 128]."
        )

    # Clone to avoid modifying the original tensor.
    src_A = src_A.clone()

    num_rows, num_cols = input_dimensions

    # Set columns 64-127 to 1, 2, 3, ..., 64 for each row.
    # These will be used as indices for the topk operation, and we want them to be in a known order for easier validation.
    # Create indices as uint16 and preserve bit representation when assigning to float tensor.
    for row in range(num_rows):
        indices_start = row * num_cols + num_cols // 2
        indices_end = indices_start + num_cols // 2
        uint16_indices = torch.arange(1, num_cols // 2 + 1, dtype=torch.int16).to(
            torch.uint16
        )
        src_A[indices_start:indices_end] = uint16_indices.view(src_A.dtype)

    src_tilizer = get_golden_generator(TilizeGolden)
    src_A = src_tilizer(src_A, input_dimensions, formats.input_format)

    return src_A


def validate_topk_indices(
    res_tensor, golden_tensor, formats, input_dimensions=[32, 128], K=32, atol=0.01
):
    """
    Validate TopK indices by comparing indices and values with golden reference.

    Args:
        res_tensor: Result tensor from hardware
        golden_tensor: Golden reference tensor
        input_dimensions: Input dimensions [rows, cols]
        K: Number of top elements
        atol: Absolute tolerance for considering values as ties (default: 0.01)
    """

    # Untilize both result and golden tensors to get them back to the original layout for easier(cleaner) comparison
    untilizer = get_golden_generator(UntilizeGolden)
    res_tensor_untilized = untilizer(
        res_tensor, formats.output_format, input_dimensions
    )
    golden_tensor_untilized = untilizer(
        golden_tensor, formats.output_format, input_dimensions
    )

    if input_dimensions != [32, 128]:
        raise ValueError(
            "This function is specifically designed for input dimensions [32, 128]."
        )

    values_offset = 0
    indices_offset = input_dimensions[1] // 2  # Indices stored in second half of row.

    for row_idx in range(input_dimensions[0]):
        for datum in range(K):  # Check top K values/indices for each row.
            value_idx = row_idx * input_dimensions[1] + values_offset + datum
            index_idx = row_idx * input_dimensions[1] + indices_offset + datum

            # Values: interpret as float
            result_value = res_tensor_untilized[value_idx].item()
            golden_value = golden_tensor_untilized[value_idx].item()

            # Indices: reinterpret float bits as uint16 as that's how we encoded them in the input tensor.
            result_index = (
                res_tensor_untilized[index_idx : index_idx + 1]
                .view(torch.uint16)
                .item()
            )
            golden_index = (
                golden_tensor_untilized[index_idx : index_idx + 1]
                .view(torch.uint16)
                .item()
            )

            if result_index != golden_index:
                if torch.isclose(
                    torch.tensor(result_value), torch.tensor(golden_value), atol=atol
                ):
                    # When doing topk, we can encounter cases where the values are extremely close/same.
                    # in those cases golden has its own way of deciding which index to pick first, and hardware might pick a different one.
                    # What we get in the end is that the same values are in the topk, but maybe in a different order, which means different indices.
                    # This is not an issue, just the difference between golden and hardware when handling ties in values.
                    continue
                else:
                    print(f"Mismatch at row {row_idx}, datum {datum}:")
                    print(
                        f"  Result value: {result_value}, Result index: {result_index}"
                    )
                    print(
                        f"  Golden value: {golden_value}, Golden index: {golden_index}"
                    )
                    return False
    return True


@parametrize(
    formats=input_output_formats(
        [
            DataFormat.Float16_b,
        ]
    ),
    input_dimensions=[[32, 128]],  # More dimensions coming soon....
    K=[32],  # More K values coming soon.... By definition K >= 32.
    sort_direction=[TopKSortDirection.Descending, TopKSortDirection.Ascending],
)
def test_topk_sfpu(
    formats: InputOutputFormat,
    input_dimensions: list,
    K: int,
    sort_direction: TopKSortDirection,
    workers_tensix_coordinates: str,
):
    """
    Test the hardware TopK SFPU operation using a bitonic sort algorithm.

    TopK Algorithm Overview:
    =======================
    This test validates a hardware implementation that extracts the top K values from a tensor
    using a bitonic sorting network. The base example is processing 4 tiles (4096 elements total) to
    extract K=32 top values in descending order (largest first).

    Test Pipeline Example:
    =============
    1. Input Preparation:
       - Generate a 32x128 tensor (4 tiles of 32x32)
       - First 64 columns: Random values to search for top K
       - Second 64 columns: Indices (1-64) tracking original positions
            Since hardware expects one format, we will pass indices as ints but hardware will look at their bits as float.
            This doesn't matter for hardware since it just moves indices around without comparing them.
            Why don't we use float indices? If we did, let's say values are bfloat16. Then we would be limited by
            8 bit mantissa for indices which ultimately leads to being exactly precise to up to index 256. Everything after that
            loses precision.
       - Tilize the input (convert row-major to tile-major layout)

    2. Golden Reference:
       - TopKGolden performs per-row topk on the input tensor
       - For each of 32 rows: extracts top K values from first 64 elements
       - Reorders corresponding indices from second 64 elements
       - Returns reference output for validation

    3. Hardware Execution (C++ kernel):
       a) UNPACK: Transpose input tiles
          - Face-level transpose (swap face positions within tiles)
          - Within-face transpose (transpose 16x16 elements within each face)

       b) MATH: Actual TopK sort Pipeline
          - TopK_Init: Initialize TopK SFPU state
          - Local sort: Perform bitonic sort.
          - Merge: Extract top K values from sorted sequence
          - Rebuild: Organize top K values with their indices

       c) PACK: Write result tiles back to L1 memory

    4. Result Transformation:
       - Transpose faces (undo face-level transpose from unpack)
       - Transpose within faces (undo within-face transpose)
       - Results now match golden's untilized layout

    5. Validation:
       - Compare indices: Hardware vs golden reference
       - Handle ties gracefully (when values are very close, index order may differ)
       - Compare values: First K values should match golden within tolerance

    Data Layout:
    ===========
    - Tilized: Data organized in 32x32 tiles, each tile has 4 faces of 16x16
    - Untilized: Standard row-major layout (easier for validation)

    Sort Direction:
    ==============
    - Descending (default): Largest values first (ArgMax mode)
    - Hardware uses TOPK_SORT_DIRECTION=0 for descending

    Parameters:
        formats: Input/output data formats (Float16_b)
        input_dimensions: Tensor dimensions [32, 128]
        K: Number of top elements to extract (32)
        sort_direction: Sorting direction (Descending)
        workers_tensix_coordinates: Hardware worker coordinates
    """

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        sfpu=False,
    )

    golden_generator = get_golden_generator(TopKGolden)
    golden_tensor = golden_generator(
        src_A,
        formats.input_format,
        K,
        sort_direction,
        input_dimensions=input_dimensions,
    )

    src_A = prepare_input_tensor_for_topk(src_A, formats, input_dimensions)

    configuration = TestConfig(
        test_name="sources/topk_test.cpp",
        formats=formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
            DEST_SYNC(),
            TOPK(
                topk_k=K,
                topk_logk=int(math.log2(K)),
                topk_sort_direction=sort_direction,
            ),
        ],
        runtimes=[
            TILE_COUNT(tile_cnt_A),
        ],
        variant_stimuli=StimuliConfig(
            src_A,
            formats.input_format,
            src_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
        ),
        dest_acc=DestAccumulation.No,
        unpack_to_dest=False,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates)
    res_tensor = torch.tensor(res_from_L1, dtype=format_dict[formats.output_format])

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    res_tensor = transform_result_tensor_to_right_form(
        res_tensor, formats, tile_cnt_A, input_dimensions
    )

    assert validate_topk_indices(
        res_tensor, golden_tensor, formats, input_dimensions, K
    )

    # Extracting values for the specific K = 32 case. TODO: test more in the future.
    tile_size = 1024  # Each tile has 1024 elements (32x32).
    res_values_tile = res_tensor[0:tile_size]
    golden_values_tile = golden_tensor[0:tile_size]

    # Validate topk values
    assert passed_test(
        golden_values_tile, res_values_tile, formats.output_format, print_erros=True
    )
