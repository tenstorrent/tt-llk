# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch

from .format_config import DataFormat
from .llk_params import format_dict


def tilize_block(
    input_tensor,
    dimensions,
    stimuli_format=DataFormat.Float16_b,
    num_faces=4,
    tile_dimensions=None,
):
    """
    Tilize input tensor into blocks based on tile dimensions.

    Args:
        input_tensor: Input tensor to tilize
        dimensions: Input dimensions [rows, cols]
        stimuli_format: Data format
        num_faces: Number of faces per tile (1, 2, or 4)
        tile_dimensions: Tile dimensions [tile_rows, tile_cols].
                        If None, defaults to [32, 32]
    """
    if tile_dimensions is None:
        tile_dimensions = [32, 32]

    tile_rows, tile_cols = tile_dimensions

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
    if rows % tile_rows != 0 or cols % tile_cols != 0:
        raise ValueError(
            f"Input dimensions must be divisible by tile dimensions. "
            f"Got input shape {(rows, cols)}, tile dimensions {tile_dimensions}"
        )

    # Calculate number of blocks in each dimension based on tile_dimensions
    row_blocks = rows // tile_rows
    col_blocks = cols // tile_cols
    total_blocks = row_blocks * col_blocks

    # Reshape into blocks of tile_dimensions
    blocked_tensor = input_reshaped.reshape(
        row_blocks, tile_rows, col_blocks, tile_cols
    )

    # Permute to get blocks in the right order: (row_blocks, col_blocks, tile_rows, tile_cols)
    blocked_tensor = blocked_tensor.permute(0, 2, 1, 3)

    # Reshape to get all blocks as sequential entities: (total_blocks, tile_rows, tile_cols)
    all_blocks = blocked_tensor.reshape(total_blocks, tile_rows, tile_cols)

    # Tilize each block
    tilized_blocks = torch.stack(
        [
            tilize_tile(
                block,
                stimuli_format=stimuli_format,
                num_faces=num_faces,
                tile_dimensions=tile_dimensions,
            )
            for block in all_blocks
        ]
    )

    return tilized_blocks.flatten().to(format_dict[stimuli_format])


def tilize_tile(
    tile_tensor, stimuli_format=DataFormat.Float16_b, num_faces=4, tile_dimensions=None
):
    """
    Tilize a single tile by extracting and flattening faces.

    Supports variable tile dimensions:
    - [32, 32] with num_faces=4: 4 faces of 16x16
    - [16, 32] with num_faces=2: 2 faces of 16x16
    - [8, 32] with num_faces=2: 2 faces of 8x16
    - etc.
    """
    if tile_dimensions is None:
        tile_dimensions = [32, 32]

    tile_rows, tile_cols = tile_dimensions
    face_r_dim = min(tile_rows, 16)
    face_c_dim = 16  # Always 16 columns per face

    if num_faces not in (1, 2, 4):
        raise ValueError(f"num_faces must be 1, 2, or 4, got {num_faces}")

    # Ensure tile_tensor has the right shape
    if tile_tensor.shape != (tile_rows, tile_cols):
        tile_tensor = tile_tensor.view(tile_rows, tile_cols)

    # Extract faces based on tile dimensions
    # For 32x32 with 4 faces: f0(0:16,0:16), f1(0:16,16:32), f2(16:32,0:16), f3(16:32,16:32)
    # For 16x32 with 2 faces: f0(0:16,0:16), f1(0:16,16:32)
    # For 8x32 with 2 faces: f0(0:8,0:16), f1(0:8,16:32)

    faces = []
    if num_faces >= 1:
        faces.append(tile_tensor[:face_r_dim, :face_c_dim])  # f0
    if num_faces >= 2:
        faces.append(tile_tensor[:face_r_dim, face_c_dim : 2 * face_c_dim])  # f1
    if num_faces >= 3:
        faces.append(tile_tensor[face_r_dim : 2 * face_r_dim, :face_c_dim])  # f2
    if num_faces >= 4:
        faces.append(
            tile_tensor[face_r_dim : 2 * face_r_dim, face_c_dim : 2 * face_c_dim]
        )  # f3

    # Flatten and concatenate faces
    selected_faces = [face.flatten() for face in faces]
    result = torch.cat(selected_faces) if len(selected_faces) > 1 else selected_faces[0]

    return result.to(dtype=format_dict[stimuli_format])


def tilize(original_tensor, stimuli_format=DataFormat.Float16_b, num_faces=4):

    if num_faces not in (1, 2, 4):
        raise ValueError(f"num_faces must be 1, 2, or 4, got {num_faces}")

    if original_tensor.size(0) != 1024:
        raise ValueError("Input tensor must have 1024 elements.")

    matrix = original_tensor.view(32, 32)

    # Define all face slices
    face_slices = [
        matrix[:16, :16],  # f0
        matrix[:16, 16:],  # f1
        matrix[16:, :16],  # f2
        matrix[16:, 16:],  # f3
    ]

    # Select and flatten required faces
    selected_faces = [face.flatten() for face in face_slices[:num_faces]]
    result = torch.cat(selected_faces) if len(selected_faces) > 1 else selected_faces[0]

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
    """Optimized function to untilize blocks of data.

    Args:
        input_tensor: Input tensor to be untilized
        stimuli_format: Data format
        dimensions: Target dimensions for the output

    Returns:
        Untilized tensor with specified dimensions and data format
    """
    if input_tensor.numel() != dimensions[0] * dimensions[1]:
        raise ValueError(
            f"Cannot reshape tensor of size {input_tensor.numel()} to shape {dimensions}."
        )

    # Calculate number of 32x32 tiles
    rows, cols = dimensions
    row_blocks = rows // 32
    col_blocks = cols // 32
    total_blocks = row_blocks * col_blocks

    if rows % 32 != 0 or cols % 32 != 0:
        raise ValueError(
            f"Dimensions must be divisible by 32. Got dimensions: {dimensions}"
        )

    # Reshape input to have one block per 1024 elements
    input_reshaped = input_tensor.reshape(total_blocks, 1024)

    untilized_blocks = torch.stack(
        [untilize(block, stimuli_format=stimuli_format) for block in input_reshaped]
    )

    output = untilized_blocks.reshape(row_blocks, col_blocks, 32, 32)

    # Then permute and reshape to get the final dimensions
    output = output.permute(0, 2, 1, 3).reshape(rows, cols)

    return output.to(dtype=format_dict[stimuli_format])
