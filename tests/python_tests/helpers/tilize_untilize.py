# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch

from .format_arg_mapping import format_dict
from .format_config import DataFormat


def tilize_block(input_tensor, dimensions, stimuli_format=DataFormat.Float16_b):

    if input_tensor.numel() != dimensions[0] * dimensions[1]:
        raise ValueError(
            f"Cannot reshape tensor of size {input_tensor.numel()} to shape {dimensions}."
        )
    input_reshaped = input_tensor.view(dimensions[0], dimensions[1])

    if input_reshaped.ndim != 2:
        raise ValueError(
            f"Expected a 2D tensor for tilize_block, got shape {input_tensor.shape}"
        )

    rows, cols = input_reshaped.shape
    if rows % 32 != 0 or cols % 32 != 0:
        raise ValueError(
            f"Input tensor dimensions must be divisible by 32. Got shape: {input_tensor.shape}"
        )

    output = torch.empty_like(input_reshaped)
    rows, cols = input_reshaped.shape

    # Extract all 32x32 submatrices from input_reshaped
    submatrices = []
    for i in range(0, rows, 32):
        for j in range(0, cols, 32):
            submat = input_reshaped[i : i + 32, j : j + 32]
            submatrices.append(submat)

    tilized_output = torch.empty_like(input_reshaped, dtype=format_dict[stimuli_format])
    tilized_submatrices = []

    for submat in submatrices:
        if submat.numel() != 1024:
            raise ValueError(
                f"Each submatrix must have 1024 elements, got {submat.numel()}."
            )
        submat = submat.flatten().view(-1)
        tilized_submat = tilize(submat, stimuli_format=stimuli_format)
        tilized_submatrices.append(tilized_submat)

    # Concatenate all flattened tilized submatrices into the output tensor
    tilized_output = torch.cat([t.flatten() for t in tilized_submatrices]).view(
        rows, cols
    )

    return tilized_output


def tilize(original_tensor, stimuli_format=DataFormat.Float16_b):

    if original_tensor.size(0) != 1024:
        raise ValueError("Input tensor must have 1024 elements.")

    matrix = original_tensor.view(32, 32)

    f0 = matrix[:16, :16]
    f1 = matrix[:16, 16:32]
    f2 = matrix[16:32, :16]
    f3 = matrix[16:32, 16:32]

    result = torch.cat((f0.reshape(-1), f1.reshape(-1), f2.reshape(-1), f3.reshape(-1)))

    return result.to(dtype=format_dict[stimuli_format])


def untilize(tilized_tensor, stimuli_format=DataFormat.Float16_b):

    if tilized_tensor.size(0) != 1024:
        raise ValueError(
            f"Input tensor must have 1024 elements. It has: {len(tilized_tensor)}"
        )

    tilized_tensor = tilized_tensor.view(-1)

    f0 = tilized_tensor[:256].view(16, 16)
    f1 = tilized_tensor[256:512].view(16, 16)
    f2 = tilized_tensor[512:768].view(16, 16)
    f3 = tilized_tensor[768:].view(16, 16)

    top = torch.cat((f0, f1), dim=1)
    bottom = torch.cat((f2, f3), dim=1)

    original_tensor = torch.cat((top, bottom), dim=0).view(1024)

    return original_tensor.to(dtype=format_dict[stimuli_format])


def untilize_block(
    input_tensor, stimuli_format=DataFormat.Float16_b, dimensions=[32, 32]
):
    # Assume input_tensor is 1D and already tilized (flattened tiles)
    # dimensions: [rows, cols] of the output matrix

    rows, cols = dimensions
    # Each tile is 32x32
    tile_rows = rows // 32
    tile_cols = cols // 32
    tile_size = 1024  # 32x32

    output = []

    for t in range(tile_rows):
        tile_start_index = t * tile_cols
        physical_start_for_tile_row = tile_start_index * tile_size

        # For each tile row, iterate over 32 rows (face_r_dim)
        for i in range(32):
            # For each tile column
            for j in range(tile_cols):
                # Untilize the tile
                tile_offset = physical_start_for_tile_row + j * tile_size
                tile = input_tensor[tile_offset : tile_offset + tile_size]
                untilized_tile = untilize(tile, stimuli_format=stimuli_format)
                reshaped_untilized = untilized_tile.view(32, 32)
                # Append the i-th row of this tile
                output.append(reshaped_untilized[i])

    # Stack all rows to form the output matrix
    full_rows = torch.cat(output).view(rows, cols)
    return full_rows
