# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

# pack.py

import array
import struct

from .format_config import FP8Format


def pack_bfp16(torch_tensor):
    return array.array(
        "B",
        b"".join(
            struct.pack("<f", value.item())[2:] for value in torch_tensor.view(-1)
        ),
    ).tolist()


def pack_fp16(torch_tensor):
    return array.array(
        "B",
        b"".join(struct.pack("<e", value.item()) for value in torch_tensor.view(-1)),
    ).tolist()


def pack_fp32(torch_tensor):
    return array.array(
        "B",
        b"".join(struct.pack("<f", value.item()) for value in torch_tensor.view(-1)),
    ).tolist()


def _pack_fp8(torch_tensor, format_type=FP8Format.E4M3):
    if format_type == FP8Format.E4M3:
        exp_bits = 4
        mant_bits = 3
        max_exp = 15  # 2^4 - 1
    elif format_type == FP8Format.E5M2:
        exp_bits = 5
        mant_bits = 2
        max_exp = 31  # 2^5 - 1
    else:
        raise ValueError(f"Unsupported format: {format_type}")

    # Precompute constants
    bias = (1 << (exp_bits - 1)) - 1
    total_bits = exp_bits + mant_bits
    mant_mask = (1 << mant_bits) - 1
    max_value = (1 << total_bits) - 1

    # Vectorized processing for better performance
    float_values = torch_tensor.view(-1).float()

    # Batch process floats to 32-bit integers
    float_bits = struct.unpack(
        "<" + "I" * len(float_values),
        struct.pack("<" + "f" * len(float_values), *float_values),
    )

    # Vectorized bit operations
    signs = [(bits >> 31) & 0x1 for bits in float_bits]
    exponents = [(bits >> 23) & 0xFF for bits in float_bits]
    mantissas = [bits & 0x7FFFFF for bits in float_bits]

    # Pre-allocate result array
    result = array.array("B")

    for i in range(len(float_values)):
        sign = signs[i]
        exponent = exponents[i]
        mantissa = mantissas[i]

        # Handle special cases
        if exponent == 0 and mantissa == 0:
            result.append(0)
            continue
        elif exponent == 255:
            # Infinity/NaN - clamp to max finite value
            result.append((sign << total_bits) | (max_exp << mant_bits) | mant_mask)
            continue

        # Convert to target format
        target_exp = exponent - 127 + bias

        # Handle underflow (too small for target format)
        if target_exp < 0:
            result.append(0)
            continue

        # Handle overflow (too large for target format)
        if target_exp > max_exp:
            result.append((sign << total_bits) | (max_exp << mant_bits) | mant_mask)
            continue

        # Extract and scale mantissa bits
        float32_mantissa = 1.0 + (mantissa / (1 << 23))  # Normalized mantissa
        target_mantissa = round(float32_mantissa * (1 << mant_bits)) - (1 << mant_bits)

        # Clamp mantissa
        target_mantissa = max(0, min(mant_mask, target_mantissa))

        # Pack into target bits: sign(1) + exponent(exp_bits) + mantissa(mant_bits)
        fp8_value = (sign << total_bits) | (target_exp << mant_bits) | target_mantissa
        result.append(fp8_value)

    return result.tolist()


def pack_fp8_e4m3(torch_tensor):
    return _pack_fp8(torch_tensor, format_type=FP8Format.E4M3)


def pack_fp8_e5m2(torch_tensor):
    return _pack_fp8(torch_tensor, format_type=FP8Format.E5M2)


def pack_int32(torch_tensor):
    return array.array(
        "B", b"".join(struct.pack("<i", value) for value in torch_tensor)
    ).tolist()


def pack_uint32(torch_tensor):
    return array.array(
        "B", b"".join(struct.pack("<I", value) for value in torch_tensor)
    ).tolist()


def pack_uint16(torch_tensor):
    return array.array(
        "B", b"".join(struct.pack("<H", value) for value in torch_tensor)
    ).tolist()


def pack_int8(torch_tensor):
    return array.array(
        "B", b"".join(struct.pack("<b", value) for value in torch_tensor)
    ).tolist()


def pack_uint8(torch_tensor):
    return array.array(
        "B", b"".join(struct.pack("<B", value) for value in torch_tensor)
    ).tolist()


def float_to_bfp8_block(block):
    def bfloat16_to_binary(value):
        float_value = struct.unpack("<I", struct.pack("<f", value))[0]
        bfloat16_value = (float_value & 0xFFFF0000) >> 16
        return f"{(bfloat16_value >> 8) & 0xFF:08b}{bfloat16_value & 0xFF:08b}"

    exponents = []
    mantissas = []
    signs = []
    max_exponent = -float("inf")

    for value in block:
        binary_str = bfloat16_to_binary(value)
        sign = binary_str[0]
        signs.append(int(sign, 2))
        exponent = int(binary_str[1:9], 2)
        mantissa = binary_str[9:-1]  # remove last
        mantissa = "1" + mantissa  ## add 1
        exponents.append(exponent)
        mantissas.append(mantissa)
        max_exponent = max(max_exponent, exponent)

    shared_exponent = max_exponent

    mantissas_explicit = [int(mantissa, 2) for mantissa in mantissas]

    bfp8_mantissas = []
    for i in range(len(block)):
        exponent_delta = shared_exponent - exponents[i]
        mantissa = mantissas_explicit[i] >> exponent_delta
        mantissa = (signs[i] << 7) | mantissa
        bfp8_mantissas.append(mantissa)

    return shared_exponent, bfp8_mantissas


def pack_bfp8_b(tensor, block_size=16, num_faces=4):
    faces_per_tile = 4
    flattened_tensor = tensor.flatten()
    num_blocks = len(flattened_tensor) // block_size
    num_blocks = num_blocks * num_faces // faces_per_tile

    exponents = []
    mantissas = []

    for i in range(num_blocks):
        block = flattened_tensor[i * block_size : (i + 1) * block_size]
        shared_exponent, bfp8_mantissas = float_to_bfp8_block(block)
        exponents.append(shared_exponent)
        mantissas.extend(bfp8_mantissas)

    return exponents + mantissas
