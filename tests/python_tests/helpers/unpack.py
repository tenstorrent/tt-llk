# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

# unpack.py

import struct

import torch

from helpers.format_arg_mapping import format_dict
from helpers.format_config import DataFormat

from .format_arg_mapping import format_dict, format_tile_sizes
from .format_config import FP8Format


def unpack_fp16(packed_list):
    return [val[0] for val in struct.iter_unpack("<e", bytes(packed_list))]


def unpack_bfp16(packed_list):
    # Step 1: Promote each 2-byte bfloat16 to 4-byte float32
    # Place bfloat16 in high 2 bytes (little-endian)
    padded_bytes = bytearray()
    for i in range(0, len(packed_list), 2):
        hi, lo = packed_list[i], packed_list[i + 1]
        padded_bytes.extend([0x00, 0x00, hi, lo])  # float32 = [LSB, ..., MSB]

    # Use iter_unpack with "<f" to read float32
    return [val[0] for val in struct.iter_unpack("<f", padded_bytes)]


def unpack_fp32(packed_list):
    return [val[0] for val in struct.iter_unpack("<f", bytes(packed_list))]


def unpack_int32(packed_list):
    return [val[0] for val in struct.iter_unpack("<i", bytes(packed_list))]


def unpack_uint32(packed_list):
    return [val[0] for val in struct.iter_unpack("<I", bytes(packed_list))]


def unpack_uint16(packed_list):
    return [val[0] for val in struct.iter_unpack("<H", bytes(packed_list))]


def unpack_int8(packed_list):
    return [val[0] for val in struct.iter_unpack("<b", bytes(packed_list))]


def unpack_uint8(packed_list):
    return [val[0] for val in struct.iter_unpack("<B", bytes(packed_list))]


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


def _unpack_fp8(packed_list, format_type=FP8Format.E4M3):
    if format_type == FP8Format.E4M3:
        exponent_bits = 4
        mantissa_bits = 3
    elif format_type == FP8Format.E5M2:
        exponent_bits = 5
        mantissa_bits = 2
    else:
        raise ValueError(f"Unsupported format: {format_type}")

    bias = (1 << (exponent_bits - 1)) - 1
    total_bits = exponent_bits + mantissa_bits
    mant_mask = (1 << mantissa_bits) - 1

    result = []

    for fp8_byte in packed_list:
        sign = (fp8_byte >> total_bits) & 0x1
        exponent = (fp8_byte >> mantissa_bits) & ((1 << exponent_bits) - 1)
        mantissa = fp8_byte & mant_mask

        # Handle special cases
        if exponent == 0 and mantissa == 0:
            # Zero
            result.append(0.0 if sign == 0 else -0.0)
            continue
        elif exponent == ((1 << exponent_bits) - 1):
            # Infinity/NaN - return large float
            result.append(float("inf") if sign == 0 else float("-inf"))
            continue

        # Convert to IEEE 754 float32
        # Adjust exponent for float32 bias
        float32_exp = exponent + 127 - bias

        # Handle underflow (subnormal numbers)
        if float32_exp <= 0:
            # Subnormal number - need special handling
            # Shift mantissa into float32 mantissa field
            float32_exp = 0
            float32_mantissa = int((mantissa / (1 << mantissa_bits)) * (1 << 23))
        else:
            # Normal number - scale mantissa to 23 bits
            # The implicit leading 1 is NOT stored in mantissa bits
            float32_mantissa = int(mantissa * (1 << (23 - mantissa_bits)))

        # Pack into IEEE 754 single precision format
        # sign(1) + exponent(8) + mantissa(23)
        float32_bits = (sign << 31) | (float32_exp << 23) | float32_mantissa

        # Convert to float
        result.append(struct.unpack("<f", struct.pack("<I", float32_bits))[0])

    return result


def unpack_fp8_e4m3(packed_list):
    return _unpack_fp8(packed_list, format_type=FP8Format.E4M3)


def unpack_fp8_e5m2(packed_list):
    return _unpack_fp8(packed_list, format_type=FP8Format.E5M2)


_UNPACKERS = {
    DataFormat.Float16: unpack_fp16,
    DataFormat.Float16_b: unpack_bfp16,
    DataFormat.Float32: unpack_fp32,
    DataFormat.Int32: unpack_int32,
    DataFormat.UInt32: unpack_uint32,
    DataFormat.UInt16: unpack_uint16,
    DataFormat.Int8: unpack_int8,
    DataFormat.UInt8: unpack_uint8,
    DataFormat.Float8_E4M3: unpack_fp8_e4m3,
    DataFormat.Float8_E5M2: unpack_fp8_e5m2,
}


def unpack_res_tiles(packed_list, formats, tile_count=1, sfpu=False, num_faces=4):
    output_format = formats.output_format
    output_dtype = format_dict[output_format]
    tile_size = format_tile_sizes[output_format]
    face_size = tile_size // 4

    # Depending on the value of 'num_faces' (1, 2, 4), select the first 1, 2 or all 4 faces of a tile
    elements_per_tile_needed = face_size * num_faces
    total_elements_needed = tile_count * elements_per_tile_needed
    if total_elements_needed > len(packed_list):
        raise IndexError("Buffer access out of bounds")

    if output_format == DataFormat.Bfp8_b:
        unpack_func = unpack_bfp16 if sfpu else unpack_bfp8_b
    else:
        unpack_func = _UNPACKERS[output_format]

    unpacked_data = []

    # Write only values from the selected faces into unpacked_tile
    for tile in range(tile_count):
        start_idx = tile * tile_size
        end_idx = start_idx + elements_per_tile_needed
        tile_data = packed_list[start_idx:end_idx]

        if unpack_func == unpack_bfp8_b:
            unpacked_tile = unpack_func(tile_data, num_faces=num_faces)
        else:
            unpacked_tile = unpack_func(tile_data)

        unpacked_data.extend(unpacked_tile)

    return torch.tensor(unpacked_data, dtype=output_dtype)
