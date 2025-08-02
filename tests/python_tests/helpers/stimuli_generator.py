# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch

from .format_arg_mapping import format_dict
from .format_config import DataFormat


def flatten_list(sublists):
    return [item for sublist in sublists for item in sublist]


def generate_random_face(
    stimuli_format=DataFormat.Float16_b, const_value=1, const_face=False, face_size=256
):
    size = face_size
    if stimuli_format != DataFormat.Bfp8_b:
        if stimuli_format.is_integer():
            max = 127 if stimuli_format == DataFormat.Int8 else 255
            srcA_face = torch.randint(
                low=0, high=max, size=(size,), dtype=format_dict[stimuli_format]
            )
        else:
            if const_face:
                srcA_face = (
                    torch.ones(size, dtype=format_dict[stimuli_format]) * const_value
                )
            else:  # random for both faces
                srcA_face = torch.rand(size, dtype=format_dict[stimuli_format]) + 0.1

    else:

        integer_part = torch.randint(0, 3, (size,))
        fraction = torch.randint(0, 16, (size,)).to(dtype=torch.bfloat16) / 16.0
        if const_face:
            srcA_face = torch.ones(size, dtype=torch.bfloat16) * const_value
        else:
            srcA_face = integer_part.to(dtype=torch.bfloat16) + fraction

    return srcA_face


def generate_random_face_ab(
    stimuli_format_A,
    stimuli_format_B,
    const_face=False,
    const_value_A=1,
    const_value_B=2,
    face_size=256,
):
    return generate_random_face(
        stimuli_format_A, const_value_A, const_face, face_size
    ), generate_random_face(stimuli_format_B, const_value_B, const_face, face_size)


def calculate_tile_config(tile_width, tile_height):
    """
    Calculate number of faces per tile and face dimensions based on tile size.

    Examples:
    - 32x32 → 4 faces of 16x16 each
    - 16x32 → 2 faces of 16x16 each
    - 8x32 → 2 faces of 8x16 each
    - 4x32 → 2 faces of 4x16 each
    - 2x32 → 2 faces of 2x16 each
    - 1x32 → 2 faces of 1x32 each
    """
    if tile_width == 32 and tile_height == 32:
        # Special case: 32x32 tile has 4 faces of 16x16
        faces_per_tile = 4
        face_width = 16
        face_height = 16
    else:
        # General case: 2 faces per tile
        faces_per_tile = 2
        face_width = tile_width
        face_height = tile_height // 2

    face_size = face_width * face_height
    return faces_per_tile, face_size, face_width, face_height


def generate_stimuli(
    stimuli_format_A=DataFormat.Float16_b,
    stimuli_format_B=DataFormat.Float16_b,
    input_dimensions=[32, 32],
    tile_dimensions=[32, 32],
    const_face=False,
    const_value_A=1,
    const_value_B=1,
):

    srcA = []
    srcB = []

    # Calculate number of tiles based on input and tile dimensions
    tile_width, tile_height = tile_dimensions
    tiles_x = input_dimensions[0] // tile_width
    tiles_y = input_dimensions[1] // tile_height
    tile_cnt = tiles_x * tiles_y

    # Calculate faces per tile and face size based on tile dimensions
    faces_per_tile, face_size, face_width, face_height = calculate_tile_config(
        tile_width, tile_height
    )

    # Generate faces for all tiles
    total_faces = faces_per_tile * tile_cnt
    for _ in range(total_faces):
        face_a, face_b = generate_random_face_ab(
            stimuli_format_A,
            stimuli_format_B,
            const_face,
            const_value_A,
            const_value_B,
            face_size,
        )
        srcA.extend(face_a.tolist())
        srcB.extend(face_b.tolist())

    dtype_A = (
        format_dict[stimuli_format_A]
        if stimuli_format_A != DataFormat.Bfp8_b
        else torch.bfloat16
    )
    dtype_B = (
        format_dict[stimuli_format_B]
        if stimuli_format_B != DataFormat.Bfp8_b
        else torch.bfloat16
    )
    return (
        torch.tensor(srcA, dtype=dtype_A),
        torch.tensor(srcB, dtype=dtype_B),
        tile_cnt,
    )
