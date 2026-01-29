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
    face_r_dim=16,
):
    """Tilize a block of data into face-based tile layout.

    Args:
        input_tensor: Input tensor to tilize
        dimensions: [rows, cols] of the input tensor
        stimuli_format: Data format for output
        num_faces: Number of faces per tile (1, 2, or 4)
        tile_dimensions: Optional [tile_rows, tile_cols] for tile size
        face_r_dim: Number of rows per face (1, 2, 4, 8, or 16)

    Returns:
        Tilized tensor with tiles laid out sequentially
    """
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

    # Determine tile dimensions
    if tile_dimensions is not None:
        tile_rows, tile_cols = tile_dimensions
    else:
        tile_rows, tile_cols = 32, 32  # Default to standard 32x32 tiles

    if rows % tile_rows != 0 or cols % tile_cols != 0:
        raise ValueError(
            f"Input dimensions {dimensions} must be divisible by tile dimensions "
            f"[{tile_rows}, {tile_cols}]."
        )

    # Calculate number of tiles
    row_tiles = rows // tile_rows
    col_tiles = cols // tile_cols
    total_tiles = row_tiles * col_tiles

    # Calculate elements per tile and face
    elements_per_tile = tile_rows * tile_cols
    face_c_dim = 16
    elements_per_face = face_r_dim * face_c_dim

    # Reshape into tiles: (row_tiles, tile_rows, col_tiles, tile_cols)
    blocked_tensor = input_reshaped.reshape(row_tiles, tile_rows, col_tiles, tile_cols)

    # Permute to get tiles in row-major order: (row_tiles, col_tiles, tile_rows, tile_cols)
    blocked_tensor = blocked_tensor.permute(0, 2, 1, 3)

    # Reshape to get all tiles as sequential entities: (total_tiles, tile_rows, tile_cols)
    all_tiles = blocked_tensor.reshape(total_tiles, tile_rows, tile_cols)

    # Flatten each tile for tilization
    flat_tiles = all_tiles.reshape(total_tiles, -1)

    # Tilize each tile
    # For 32x32 tiles (default), use original tilize signature for backward compatibility
    if tile_rows == 32 and tile_cols == 32:
        tilized_tiles = torch.stack(
            [
                tilize(tile, stimuli_format=stimuli_format, num_faces=num_faces)
                for tile in flat_tiles
            ]
        )
    else:
        # For smaller tiles, pass tile_dimensions to enable proper face slicing
        tilized_tiles = torch.stack(
            [
                tilize(
                    tile,
                    stimuli_format=stimuli_format,
                    num_faces=num_faces,
                    face_r_dim=face_r_dim,
                    tile_dimensions=[tile_rows, tile_cols],
                )
                for tile in flat_tiles
            ]
        )

    # Each tilized tile has num_faces * elements_per_face elements
    tilized_elements_per_tile = num_faces * elements_per_face
    expected_elements = total_tiles * tilized_elements_per_tile

    tilized_output = (
        tilized_tiles.flatten()[:expected_elements]
        .reshape(total_tiles, tilized_elements_per_tile)
        .to(format_dict[stimuli_format])
    )

    return tilized_output


def tilize(
    original_tensor,
    stimuli_format=DataFormat.Float16_b,
    num_faces=4,
    face_r_dim=16,
    tile_dimensions=None,
):
    """Tilize a tensor into face-based layout.

    Args:
        original_tensor: Input tensor to tilize
        stimuli_format: Data format for output
        num_faces: Number of faces (1, 2, or 4)
        face_r_dim: Number of rows per face (1, 2, 4, 8, or 16)
        tile_dimensions: Optional [tile_rows, tile_cols] to auto-calculate parameters
                         Only used for non-32x32 tiles when num_faces and face_r_dim
                         are at their default values.

    For a tile with dimensions [tile_rows, tile_cols]:
        - face_r_dim = min(tile_rows, 16) for single row of faces
        - num_faces_r_dim = ceil(tile_rows / 16) (1 or 2)
        - num_faces_c_dim = tile_cols // 16 (typically 2 for 32-col tiles)
        - num_faces = num_faces_r_dim * num_faces_c_dim

    Examples:
        - 32x32 tile: 4 faces of 16x16 each (standard)
        - 16x32 tile: 2 faces of 16x16 each
        - 8x32 tile: 2 faces of 8x16 each
        - 4x32 tile: 2 faces of 4x16 each
    """
    # Auto-calculate from tile_dimensions only for non-32x32 tiles
    # This preserves backward compatibility for 32x32 tiles
    if tile_dimensions is not None and tile_dimensions != [32, 32]:
        tile_rows, tile_cols = tile_dimensions
        face_c_dim = 16  # Always 16
        num_faces_c_dim = tile_cols // face_c_dim
        num_faces_r_dim = (tile_rows + 15) // 16  # ceil division
        face_r_dim = min(tile_rows, 16)
        num_faces = num_faces_r_dim * num_faces_c_dim

    if num_faces not in (1, 2, 4):
        raise ValueError(f"num_faces must be 1, 2, or 4, got {num_faces}")

    if face_r_dim not in (1, 2, 4, 8, 16):
        raise ValueError(f"face_r_dim must be 1, 2, 4, 8, or 16, got {face_r_dim}")

    face_c_dim = 16  # Always 16
    elements_per_face = face_r_dim * face_c_dim

    # For standard 32x32 tiles (backward compatibility)
    if face_r_dim == 16 and num_faces == 4:
        if original_tensor.size(0) != 1024:
            raise ValueError("Input tensor must have 1024 elements for 32x32 tiles.")
        matrix = original_tensor.view(32, 32)
        face_slices = [
            matrix[:16, :16],  # f0
            matrix[:16, 16:],  # f1
            matrix[16:, :16],  # f2
            matrix[16:, 16:],  # f3
        ]
        selected_faces = [face.flatten() for face in face_slices[:num_faces]]
        result = (
            torch.cat(selected_faces) if len(selected_faces) > 1 else selected_faces[0]
        )
        return result.to(dtype=format_dict[stimuli_format])

    # For smaller tiles (e.g., 16x32, 8x32, 4x32, etc.)
    # These have num_faces_r_dim=1 and num_faces_c_dim=2
    if num_faces == 2:
        # Tile is [face_r_dim x 32] with 2 faces of [face_r_dim x 16]
        tile_rows = face_r_dim
        tile_cols = 32
        expected_elements = tile_rows * tile_cols

        if original_tensor.size(0) != expected_elements:
            raise ValueError(
                f"Input tensor must have {expected_elements} elements for "
                f"{tile_rows}x{tile_cols} tiles. Got {original_tensor.size(0)}."
            )

        matrix = original_tensor.view(tile_rows, tile_cols)
        face_slices = [
            matrix[:face_r_dim, :16],  # f0: left half
            matrix[:face_r_dim, 16:],  # f1: right half
        ]
        selected_faces = [face.flatten() for face in face_slices]
        result = torch.cat(selected_faces)
        return result.to(dtype=format_dict[stimuli_format])

    # For single face tiles (e.g., 16x16)
    if num_faces == 1:
        expected_elements = elements_per_face
        if original_tensor.size(0) != expected_elements:
            raise ValueError(
                f"Input tensor must have {expected_elements} elements for "
                f"single face tiles. Got {original_tensor.size(0)}."
            )
        return original_tensor.to(dtype=format_dict[stimuli_format])

    raise ValueError(
        f"Unsupported combination: num_faces={num_faces}, face_r_dim={face_r_dim}"
    )


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
