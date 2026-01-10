# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

# unpack.py

import ml_dtypes
import numpy as np
import torch
from helpers.format_config import DataFormat

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


def bfp4_to_float_block(exponent, bfp4_mantissas_bytes, unpacked_bfp4):
    # Similar to bfp8_to_float_block but handles BFP4 format:
    # - 1 bit for sign, 3 bits for mantissa (4 bits total per value)
    # - 2 mantissas packed per byte
    # - 8 bits for common exponent (same as BFP8)
    # 1. Caching: If the (exponent, mantissa) pair is already processed, the precomputed value is reused.
    # 2. Sign and Fractional Calculation: The sign bit is extracted, and the fractional part is calculated by iterating
    #    over the mantissa bits, adding `1 / (2 ** i)` for each '1' bit.
    # 3. Exponent Scaling: The final value is scaled by `2^exponent` and adjusted by the sign bit.
    # 4. Efficient Storage: The computed value is stored in `unpacked_bfp4` for future use.

    bfloat16_values = []
    exponent = exponent - 127

    # Unpack 4-bit mantissas from bytes (2 mantissas per byte)
    bfp4_mantissas = []
    for byte_val in bfp4_mantissas_bytes:
        # Extract upper 4 bits (first mantissa)
        upper_mantissa = (byte_val >> 4) & 0x0F
        # Extract lower 4 bits (second mantissa)
        lower_mantissa = byte_val & 0x0F
        bfp4_mantissas.append(upper_mantissa)
        bfp4_mantissas.append(lower_mantissa)

    for mantissa in bfp4_mantissas:
        if (exponent, mantissa) in unpacked_bfp4:
            bfloat16_values.append(unpacked_bfp4[(exponent, mantissa)])
            continue

        sign_mantissa = str(format(mantissa, "04b"))
        # Extract the sign bit (most significant bit)
        sign = int(sign_mantissa[0], 2)
        # Get the remaining 3 bits which represent the mantissa (including implicit 1 at MSB)
        mantissa_value = sign_mantissa[1:]
        # Accumulate fractional value (MSB=1.0, next=0.5, next=0.25)
        fract_value = 0.0
        for i in range(len(mantissa_value)):
            # If the bit is '1', add the corresponding fractional value to fract_value
            if mantissa_value[i] == "1":
                fract_value += 1 / (2**i)

        bfloat16_values.append(((-1.0) ** sign) * (2**exponent) * (fract_value))

        unpacked_bfp4[(exponent, mantissa)] = (
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


def unpack_bfp4_b(bfp4_block, sfpu=False, num_faces=4):
    # Similar to unpack_bfp8_b but handles BFP4 format:
    # - 16 mantissas per exponent (same as BFP8)
    # - But each mantissa is 4 bits, so 8 bytes per block (16 entries × 4 bits = 64 bits = 8 bytes)

    exponents_per_face = 16
    if not sfpu:
        exponents = bfp4_block[: exponents_per_face * num_faces]
        mantissas = bfp4_block[exponents_per_face * num_faces :]
    else:
        exponents = bfp4_block[:16]
        mantissas = bfp4_block[
            16:144
        ]  # 16 exponents + (16 blocks × 8 bytes per block) = 16 + 128 = 144

    unpacked_bfp4 = {}

    bfloat16_values = []
    for i in range(len(exponents)):
        exponent = exponents[i]
        # 16 mantissas × 4 bits = 64 bits = 8 bytes per block
        bfp4_mantissas_bytes = mantissas[i * 8 : (i + 1) * 8]
        block_bfloat16_values = bfp4_to_float_block(
            exponent, bytes(bfp4_mantissas_bytes), unpacked_bfp4
        )
        bfloat16_values.extend(block_bfloat16_values)

    return torch.tensor(bfloat16_values, dtype=torch.bfloat16)


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
    packed_list,
    output_format: DataFormat,
    tile_count: int = 1,
    sfpu: bool = False,
    num_faces: int = 4,
    face_r_dim: int = 16,
):
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
    elif output_format == DataFormat.Bfp4_b:
        unpack_func = unpack_bfp16 if sfpu else unpack_bfp4_b
    else:
        unpack_func = _UNPACKERS[output_format]

    unpacked_data = []

    # Write only values from the selected faces into unpacked_tile
    for tile in range(tile_count):
        # Both paths use byte-based indexing since tile_size and elements_per_tile_needed are in bytes
        start_idx = tile * tile_size
        end_idx = start_idx + elements_per_tile_needed
        tile_data = packed_list[start_idx:end_idx]

        if unpack_func == unpack_bfp8_b or unpack_func == unpack_bfp4_b:
            unpacked_tile = unpack_func(tile_data, sfpu=sfpu, num_faces=num_faces)
        else:
            unpacked_tile = unpack_func(tile_data)

        unpacked_data.extend(unpacked_tile)

    return torch.tensor(unpacked_data, dtype=output_dtype)
