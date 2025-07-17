# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import torch

from .format_arg_mapping import format_dict
from .format_config import DataFormat


def flatten_list(sublists):
    return [item for sublist in sublists for item in sublist]


def generate_random_face(
    stimuli_format=DataFormat.Float16_b, const_value=1, const_face=False, size=256
):
    if stimuli_format != DataFormat.Bfp8_b:
        if stimuli_format.is_integer():
            max_val = 127 if stimuli_format == DataFormat.Int8 else 255
            if const_face:
                srcA_face = (
                    torch.ones(size, dtype=format_dict[stimuli_format]) * const_value
                )
            else:
                srcA_face = torch.randint(
                    low=0, high=max_val, size=(size,), dtype=format_dict[stimuli_format]
                )
        else:
            if const_face:
                srcA_face = (
                    torch.ones(size, dtype=format_dict[stimuli_format]) * const_value
                )
            else:  # random for both faces
                srcA_face = torch.rand(size, dtype=format_dict[stimuli_format]) + 0.3

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
    size=256,
):
    # Ensure srcA > srcB elementwise
    if const_face:
        face_a = generate_random_face(stimuli_format_A, const_value_A, const_face, size)
        face_b = generate_random_face(stimuli_format_B, const_value_B, const_face, size)
        # If const_face, just ensure srcA > srcB by adding 1 if needed
        mask = face_a <= face_b
        face_a = face_a + mask.to(face_a.dtype)
        return face_a, face_b

    # For random faces, generate srcB first, then srcA > srcB
    if stimuli_format_A != DataFormat.Bfp8_b and stimuli_format_B != DataFormat.Bfp8_b:
        # For integer types
        if stimuli_format_A.is_integer() and stimuli_format_B.is_integer():
            max_val_A = 127 if stimuli_format_A == DataFormat.Int8 else 255
            max_val_B = 127 if stimuli_format_B == DataFormat.Int8 else 255
            # Generate srcB in [0, max_val_B-1]
            face_b = torch.randint(
                low=0, high=max_val_B, size=(size,), dtype=format_dict[stimuli_format_B]
            )
            # srcA in [srcB+1, max_val_A], but if srcB == max_val_A, set srcA = max_val_A
            face_a = face_b + 1
            face_a = torch.where(
                face_a > max_val_A, torch.full_like(face_a, max_val_A), face_a
            )
            face_a = face_a.to(format_dict[stimuli_format_A])
            return face_a, face_b
        else:
            # For float types, generate srcB, then srcA = srcB + positive random
            face_b = torch.rand(size, dtype=format_dict[stimuli_format_B]) + 0.1
            # Add a positive random offset to ensure srcA > srcB
            offset = torch.rand(size, dtype=format_dict[stimuli_format_A]) + 1
            face_a = face_b.to(format_dict[stimuli_format_A]) + offset
            return face_a, face_b
    else:
        # For Bfp8_b, treat as bfloat16
        # Generate srcB
        integer_part_b = torch.randint(0, 3, (size,))
        fraction_b = torch.randint(0, 16, (size,)).to(dtype=torch.bfloat16) / 16.0
        face_b = integer_part_b.to(dtype=torch.bfloat16) + fraction_b
        # Add a positive offset to ensure srcA > srcB
        offset = torch.rand(size, dtype=torch.bfloat16) + 0.01
        face_a = face_b + offset
        return face_a, face_b


def generate_stimuli(
    stimuli_format_A=DataFormat.Float16_b,
    stimuli_format_B=DataFormat.Float16_b,
    input_dimensions=[32, 32],
    const_face=False,
    const_value_A=1,
    const_value_B=1,
):

    srcA = []
    srcB = []

    tile_cnt = input_dimensions[0] // 32 * input_dimensions[1] // 32

    for _ in range(4 * tile_cnt):
        face_a, face_b = generate_random_face_ab(
            stimuli_format_A, stimuli_format_B, const_face, const_value_A, const_value_B
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
