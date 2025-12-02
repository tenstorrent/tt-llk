# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

# unpack.py

import ml_dtypes
import numpy as np
import torch
from helpers.format_config import MXFP8_BLOCK_SIZE, DataFormat, decode_e8m0_scale

from .llk_params import format_dict, format_tile_sizes


def unpack_fp16(packed_list):
    return np.frombuffer(bytes(packed_list), dtype=np.float16).tolist()


def unpack_bfp16(packed_list):
    return (
        np.frombuffer(bytes(packed_list), dtype=ml_dtypes.bfloat16)
        .astype(np.float32)
        .tolist()
    )


def unpack_fp32(packed_list):
    return np.frombuffer(bytes(packed_list), dtype=np.float32).tolist()


def unpack_int32(packed_list):
    return np.frombuffer(bytes(packed_list), dtype=np.int32).tolist()


def unpack_uint32(packed_list):
    return np.frombuffer(bytes(packed_list), dtype=np.uint32).tolist()


def unpack_uint16(packed_list):
    return np.frombuffer(bytes(packed_list), dtype=np.uint16).tolist()


def unpack_int8(packed_list):
    return np.frombuffer(bytes(packed_list), dtype=np.int8).tolist()


def unpack_uint8(packed_list):
    return np.frombuffer(bytes(packed_list), dtype=np.uint8).tolist()


def bfp8_to_float_block(exponent, bfp8_mantissas, unpacked_bfp8):
    # Bug fix and improvement:
    # 1. Caching: If the (exponent, mantissa) pair is already processed, the precomputed value is reused.
    # 2. Sign and Fractional Calculation: The sign bit is extracted, and the fractional part is calculated by iterating
    #    over the mantissa bits, adding `1 / (2 ** i)` for each '1' bit.
    # 3. Exponent Scaling: The final value is scaled by `2^exponent` and adjusted by the sign bit.
    # 4. Efficient Storage: The computed value is stored in `unpacked_bfp8` for future use.

    bfloat16_values = []
    exponent = exponent - 127

    for mantissa in bfp8_mantissas:
        if (exponent, mantissa) in unpacked_bfp8:
            bfloat16_values.append(unpacked_bfp8[(exponent, mantissa)])
            continue

        sign_mantissa = str(format(mantissa, "08b"))
        # Extract the sign bit (most significant bit)
        sign = int(sign_mantissa[0], 2)
        # Get the remaining bits which represent the fractional part of the mantissa
        mantissa_value = sign_mantissa[1:]
        # Changed computation of mantissa to fix , accumulate fractional value
        fract_value = 0.0
        for i in range(len(mantissa_value)):
            # If the bit is '1', add the corresponding fractional value to fract_value
            if mantissa_value[i] == "1":
                fract_value += 1 / (2 ** (i))

        bfloat16_values.append(((-1.0) ** sign) * (2**exponent) * (fract_value))

        unpacked_bfp8[(exponent, mantissa)] = (
            ((-1.0) ** sign) * (2**exponent) * (fract_value)
        )

    return bfloat16_values


def unpack_bfp8_b(bfp8_block, sfpu=False, num_faces=4):

    exponents_per_face = 16
    if not sfpu:
        exponents = bfp8_block[: exponents_per_face * num_faces]
        mantissas = bfp8_block[exponents_per_face * num_faces :]
    else:
        exponents = bfp8_block[:16]
        mantissas = bfp8_block[16:272]

    unpacked_bfp8 = {}

    bfloat16_values = []
    for i in range(len(exponents)):
        exponent = exponents[i]
        bfp8_mantissas = mantissas[i * 16 : (i + 1) * 16]
        block_bfloat16_values = bfp8_to_float_block(
            exponent, bytes(bfp8_mantissas), unpacked_bfp8
        )
        bfloat16_values.extend(block_bfloat16_values)

    return torch.tensor(bfloat16_values, dtype=torch.bfloat16)


# ============================================================================
# MX (Microscaling) Format Support - OCP Specification
# ============================================================================


def _unpack_mxfp8(packed_bytes, fp8_dtype, num_faces=None):
    """
    Internal helper to unpack MXFP8 formats with FULLY SEPARATED layout.

    Layout (similar to BFP8_b): [all_scales][all_elements]
    - Scales are stored first (8 scales per face, num_faces total)
    - Elements follow (256 elements per face, num_faces total)

    Args:
        packed_bytes: List of bytes in [all scales][all elements] format
        fp8_dtype: ml_dtypes dtype (float8_e5m2 or float8_e4m3fn)
        num_faces: Number of faces to unpack (1, 2, or 4). If None, auto-calculated from packed_bytes size.

    Returns:
        torch.Tensor of bfloat16 values
    """
    # Auto-calculate num_faces from packed_bytes size if not provided
    # Each face = 8 scales + 256 elements = 264 bytes
    if num_faces is None:
        bytes_per_face = 8 + 256  # 8 scales + 256 FP8 elements
        num_faces = len(packed_bytes) // bytes_per_face
        assert num_faces in [
            1,
            2,
            4,
        ], f"Invalid num_faces={num_faces} calculated from {len(packed_bytes)} bytes"

    num_scales = num_faces * 8  # 8 scales per face

    # Extract scales and elements from FULLY SEPARATED layout
    all_scales_e8m0 = packed_bytes[:num_scales]
    all_elements_raw_bytes = packed_bytes[num_scales:]

    unpacked_values = []

    # Process each face
    for face_idx in range(num_faces):
        # Process 8 blocks within this face
        for block_idx in range(8):
            # Get scale for this block
            scale_idx = face_idx * 8 + block_idx
            scale_e8m0 = all_scales_e8m0[scale_idx]
            scale_factor = decode_e8m0_scale(scale_e8m0)

            if np.isnan(scale_factor) or scale_factor == 0:
                unpacked_values.extend([0.0] * MXFP8_BLOCK_SIZE)
                continue

            # Get elements for this block (32 elements per block)
            block_elements_start = (face_idx * 256) + (block_idx * MXFP8_BLOCK_SIZE)
            block_elements_end = block_elements_start + MXFP8_BLOCK_SIZE
            elements_bytes = all_elements_raw_bytes[
                block_elements_start:block_elements_end
            ]

            # Convert FP8 bytes to array
            fp8_array = np.frombuffer(bytes(elements_bytes), dtype=fp8_dtype)

            # Scale back to full precision
            scaled_values = fp8_array.astype(np.float32) * scale_factor

            unpacked_values.extend(scaled_values.tolist())

    return torch.tensor(unpacked_values, dtype=torch.bfloat16)


def unpack_mxfp8r(packed_bytes, num_faces=None):
    """
    Unpack MXFP8R format (MXFP8 E5M2 variant) to float32 tensor.

    Args:
        packed_bytes: Packed MX data [scale0, elem0-31, scale1, elem32-63, ...]
        num_faces: Number of faces to unpack (1, 2, or 4). If None, auto-calculated from packed_bytes size.

    Returns:
        torch.Tensor of float32 values
    """
    return _unpack_mxfp8(packed_bytes, ml_dtypes.float8_e5m2, num_faces)


def unpack_mxfp8p(packed_bytes, num_faces=None):
    """
    Unpack MXFP8P format (MXFP8 E4M3 variant) to float32 tensor.

    Args:
        packed_bytes: Packed MX data [scale0, elem0-31, scale1, elem32-63, ...]
        num_faces: Number of faces to unpack (1, 2, or 4). If None, auto-calculated from packed_bytes size.

    Returns:
        torch.Tensor of float32 values
    """
    return _unpack_mxfp8(packed_bytes, ml_dtypes.float8_e4m3fn, num_faces)


_UNPACKERS = {
    DataFormat.Float16: unpack_fp16,
    DataFormat.Float16_b: unpack_bfp16,
    DataFormat.Float32: unpack_fp32,
    DataFormat.Int32: unpack_int32,
    DataFormat.UInt32: unpack_uint32,
    DataFormat.UInt16: unpack_uint16,
    DataFormat.Int8: unpack_int8,
    DataFormat.UInt8: unpack_uint8,
}


def unpack_res_tiles(
    packed_list, formats, tile_count=1, sfpu=False, num_faces=4, face_r_dim=16
):
    output_format = formats.output_format
    output_dtype = format_dict[output_format]

    # Calculate tile size and determine elements per tile needed
    tile_size = format_tile_sizes[output_format]  # Full tile size in bytes

    if face_r_dim == 16:
        # Backward compatibility: calculate face size in bytes (original logic)
        face_size = tile_size // 4  # Each face is 1/4 of a tile in bytes
        elements_per_tile_needed = face_size * num_faces  # In bytes
    else:
        # Variable face dimensions: calculate in elements, convert to bytes
        face_c_dim = 16
        elements_per_face = face_r_dim * face_c_dim
        elements_per_tile_needed = (
            elements_per_face * num_faces * output_format.size
        )  # Convert to bytes
    total_elements_needed = tile_count * elements_per_tile_needed
    if total_elements_needed > len(packed_list):
        raise IndexError("Buffer access out of bounds")

    if output_format == DataFormat.Bfp8_b:
        unpack_func = unpack_bfp16 if sfpu else unpack_bfp8_b
    elif output_format == DataFormat.MxFp8R:
        unpack_func = unpack_mxfp8r
    elif output_format == DataFormat.MxFp8P:
        unpack_func = unpack_mxfp8p
    else:
        unpack_func = _UNPACKERS[output_format]

    unpacked_data = []

    # Write only values from the selected faces into unpacked_tile
    for tile in range(tile_count):
        # Both paths use byte-based indexing since tile_size and elements_per_tile_needed are in bytes
        start_idx = tile * tile_size
        end_idx = start_idx + elements_per_tile_needed
        tile_data = packed_list[start_idx:end_idx]

        if unpack_func in [unpack_bfp8_b, unpack_mxfp8r, unpack_mxfp8p]:
            unpacked_tile = unpack_func(tile_data, num_faces=num_faces)
        else:
            unpacked_tile = unpack_func(tile_data)

        unpacked_data.extend(unpacked_tile)

    return torch.tensor(unpacked_data, dtype=output_dtype)
