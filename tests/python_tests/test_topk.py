# test_topk.py
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.llk_params import DestAccumulation, format_dict
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import INPUT_DIMENSIONS, TILE_COUNT


def build_topk_llk_tiles(x: torch.Tensor):

    assert x.shape == torch.Size([4096])

    vals_tiles = []
    idx_tiles = []

    for tile_id in range(2):
        t_val = torch.zeros((64, 16), dtype=torch.bfloat16)
        t_idx = torch.zeros((64, 16), dtype=torch.int32)

        if tile_id == 0:
            start = tile_id * 64
            end = start + 64

            # Put values in first column
            t_val[:, 0] = x[start:end].to(torch.bfloat16)

            # Indices 0..63 spread across 2 tiles
            t_idx[:, 0] = torch.arange(start, end, dtype=torch.int32)

        vals_tiles.append(t_val)
        idx_tiles.append(t_idx)

    return vals_tiles, idx_tiles


def check_topk_validity(input_x: torch.Tensor, res_tensor: torch.Tensor, k: int = 32):
    """Check that the TopK results are valid."""

    # Extract the 64 important elements from input_x (first tile, rows of 16, first element of each row)
    input_tile = input_x[:1024].reshape(64, 16)
    input_values = input_tile[:64, 0]  # First 64 elements (first column of each row)

    # Perform reference TopK on input values
    ref_topk_values, ref_topk_indices = torch.topk(
        input_values, k, largest=True, sorted=True
    )

    # Extract the 32 important elements from res_tensor
    # Tile 0: values (first 1024 elements)
    res_tile_0 = res_tensor[:1024].reshape(64, 16)
    output_values = res_tile_0[:k, 0]  # First k elements (first column of each row)

    # Tile 2: indices (elements 2048-3072)
    res_tile_2 = res_tensor[2048:3072].reshape(64, 16)
    output_indices = res_tile_2[:k, 0].to(torch.int32)  # First k indices

    print(f"\nReference TopK values (first 10): {ref_topk_values[:10]}")
    print(f"Output values (first 10): {output_values[:10]}")
    print(f"Output indices (first 10): {output_indices[:10]}")

    # Check 1: Every element in reference topk should appear in output values
    for i, ref_val in enumerate(ref_topk_values):
        if not torch.any(torch.isclose(output_values, ref_val, rtol=1e-3, atol=1e-5)):
            raise AssertionError(
                f"Reference TopK value {ref_val} at position {i} not found in output values."
            )

    # Check 2: Verify that indices point to correct values in the input
    for i in range(k):
        output_val = output_values[i]
        output_idx = output_indices[i].item()

        # The index should point to the corresponding value in input_values
        if output_idx < len(input_values):
            expected_val = input_values[output_idx]
            if not torch.isclose(output_val, expected_val, rtol=1e-3, atol=1e-5):
                raise AssertionError(
                    f"Value mismatch at position {i}: output_val={output_val}, "
                    f"but input_values[{output_idx}]={expected_val}"
                )

    print(f"\n✓ TopK validation passed: all {k} values and indices are correct!")


@pytest.mark.parametrize(
    "formats",
    [
        InputOutputFormat(DataFormat.Float16_b, DataFormat.Float16_b),
    ],
)
def test_topk_sfpu(formats: InputOutputFormat, workers_tensix_coordinates: str):
    torch.manual_seed(0)
    torch.set_printoptions(precision=5, linewidth=200, sci_mode=False)

    input_dimensions = [128, 32]

    src_A, tile_cnt_A, src_B, tile_cnt_B = generate_stimuli(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        sfpu=False,
    )

    vals_tiles, idx_tiles = build_topk_llk_tiles(src_A)

    src_A = torch.cat(
        [
            vals_tiles[0].flatten(),
            vals_tiles[1].flatten(),
            idx_tiles[0]
            .flatten()
            .to(torch.bfloat16),  # Convert int32 to bfloat16 for packing
            idx_tiles[1].flatten().to(torch.bfloat16),
        ]
    )

    def print_tensor_as_matrix(tensor, name: str, cols=32):
        """Print tensor as a matrix with specified number of columns."""
        print("\n" + "=" * 120)
        print(f"{name} (shape: {tensor.shape}, dtype: {tensor.dtype})")
        print("=" * 120)
        flat = tensor.flatten()
        num_rows = (flat.numel() + cols - 1) // cols  # Ceiling division
        for row_idx in range(num_rows):
            start = row_idx * cols
            end = min(start + cols, flat.numel())
            row_data = flat[start:end]
            # Print only the first value in each row
            first_val = row_data[0].item()
            print(f"Row {row_idx:3d} [{start:4d}:{end:4d}]: {first_val:7.5f}")
        print("=" * 120 + "\n")

    print_tensor_as_matrix(src_A, "Input Tensor (src_A)", cols=16)

    # 3) Run kernel (TopK LLK math kernel under test)
    configuration = TestConfig(
        test_name="sources/topk_test.cpp",
        formats=formats,
        templates=[
            INPUT_DIMENSIONS(input_dimensions, input_dimensions),
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
    print("______________________________")
    print_tensor_as_matrix(res_tensor, "Output Tensor (res_tensor)", cols=16)

    # Validate TopK results
    assert check_topk_validity(src_A, res_tensor, k=32)
