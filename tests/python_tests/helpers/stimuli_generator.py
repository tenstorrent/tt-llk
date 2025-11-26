# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch

from .format_config import (
    MXFP8_E4M3_MAX_NORMAL,
    MXFP8_E5M2_MAX_NORMAL,
    DataFormat,
)
from .llk_params import format_dict


def flatten_list(sublists):
    return [item for sublist in sublists for item in sublist]


def _mask_tile(tile: torch.Tensor, num_faces: int, is_matrix_A: bool) -> torch.Tensor:
    masked = tile.clone()
    if num_faces == 1:
        # Keep only f0
        masked[:16, 16:] = 0  # Zero f1
        masked[16:, :] = 0  # Zero f2, f3
    elif num_faces == 2:
        if is_matrix_A:
            # Matrix A: keep f0, f2
            masked[:16, 16:] = 0  # Zero f1
            masked[16:, 16:] = 0  # Zero f3
        else:
            # Matrix B: keep f0, f1
            masked[16:, :] = 0  # Zero f2, f3
    return masked


def generate_random_face(
    stimuli_format=DataFormat.Float16_b,
    const_value=1,
    const_face=False,
    sfpu=True,
    face_r_dim=16,
):
    size = face_r_dim * 16  # face_r_dim rows × 16 columns

    if stimuli_format in [DataFormat.MXFP8R, DataFormat.MXFP8P]:
        # MXFP8 optimized stimuli generation
        return _generate_mxfp8_face(stimuli_format, size, const_face, const_value, sfpu)
    elif stimuli_format != DataFormat.Bfp8_b:
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
            else:
                # random for both faces
                srcA_face = torch.rand(size, dtype=format_dict[stimuli_format])
                if sfpu:
                    srcA_face += 0.1
    else:

        integer_part = torch.randint(0, 3, (size,))
        fraction = torch.randint(0, 16, (size,)).to(dtype=torch.bfloat16) / 16.0
        if const_face:
            srcA_face = torch.ones(size, dtype=torch.bfloat16) * const_value
        else:
            srcA_face = integer_part.to(dtype=torch.bfloat16) + fraction

    return srcA_face


def _generate_mxfp8_face(stimuli_format, size, const_face, const_value, sfpu):
    """
    Generate test data for MXFP8 formats using normal distribution scaled to format range.

    Uses conservative scaling (5% of max normal) to avoid saturation while creating
    diverse test data with realistic dynamic range. Max values from format_config.py.
    """
    if const_face:
        return torch.ones(size, dtype=torch.bfloat16) * const_value

    # Scale factor: use 5% of format's max normal value
    # This ensures values are well within representable range while maintaining diversity
    if stimuli_format == DataFormat.MXFP8R:
        scale = 0.05 * MXFP8_E5M2_MAX_NORMAL
    else:  # MXFP8P
        scale = 0.05 * MXFP8_E4M3_MAX_NORMAL

    # Generate normal distribution: ~68% within ±1σ, ~95% within ±2σ
    # With σ=scale, most values stay well below max while creating realistic variance
    face_data = torch.randn(size, dtype=torch.bfloat16) * scale

    # Add SFPU-friendly offset if needed
    if sfpu:
        face_data += 0.1

    return face_data


def generate_random_face_ab(
    stimuli_format_A,
    stimuli_format_B,
    const_face=False,
    const_value_A=1,
    const_value_B=2,
    sfpu=True,
    face_r_dim=16,
):
    return generate_random_face(
        stimuli_format_A, const_value_A, const_face, sfpu, face_r_dim
    ), generate_random_face(
        stimuli_format_B, const_value_B, const_face, sfpu, face_r_dim
    )


def generate_face_matmul_data(
    num_faces: int,
    stimuli_format: DataFormat,
    input_dimensions=[32, 32],  # Add input_dimensions parameter
    is_matrix_A=True,  # True for matrix A (SrcB), False for matrix B (SrcA)
) -> torch.Tensor:

    # Validate num_faces
    if num_faces not in [1, 2, 4]:
        raise ValueError(f"num_faces must be 1, 2, or 4, got {num_faces}")

    # Validate input_dimensions
    rows, cols = input_dimensions
    if rows % 32 != 0 or cols % 32 != 0:
        raise ValueError(
            f"Input dimensions must be multiples of 32, got {input_dimensions}"
        )

    # Calculate number of tiles needed
    tile_cnt = input_dimensions[0] // 32 * input_dimensions[1] // 32

    # Create list to store tiles
    tiles = []

    # Create list to store tiles --> generate each tile with the right faces zeroed out
    tiles = [
        _mask_tile(
            torch.rand(32, 32, dtype=format_dict[stimuli_format]),
            num_faces,
            is_matrix_A,
        ).flatten()
        for _ in range(tile_cnt)
    ]

    # Concatenate all tiles
    src = torch.cat(tiles)

    return src


def generate_stimuli(
    stimuli_format_A=DataFormat.Float16_b,
    stimuli_format_B=DataFormat.Float16_b,
    input_dimensions=[32, 32],
    const_face=False,
    const_value_A=1,
    const_value_B=1,
    sfpu=True,
    face_r_dim=16,  # Add face_r_dim parameter
    num_faces=4,  # Add num_faces parameter for partial faces
):

    srcA = []
    srcB = []

    # Handle partial faces
    height, width = input_dimensions
    if face_r_dim < 16:
        # Partial face case: generate exactly num_faces worth of data
        tile_cnt = 1
        faces_to_generate = num_faces  # Generate exactly the right number of faces
    else:
        # Full tile case
        tile_cnt = height // 32 * width // 32
        faces_to_generate = 4

    for _ in range(faces_to_generate * tile_cnt):
        face_a, face_b = generate_random_face_ab(
            stimuli_format_A,
            stimuli_format_B,
            const_face,
            const_value_A,
            const_value_B,
            sfpu,
            face_r_dim,
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
