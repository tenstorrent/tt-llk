# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import struct

import ml_dtypes
import numpy as np
import torch


def pack_bfp16(torch_tensor):
    fp32_array = torch_tensor.cpu().to(torch.float32).numpy()
    bfp16_array = fp32_array.astype(ml_dtypes.bfloat16)
    return bfp16_array.tobytes()


def pack_fp16(torch_tensor):
    return torch_tensor.cpu().numpy().astype(np.float16).tobytes()


def pack_fp32(torch_tensor):
    return torch_tensor.cpu().numpy().astype(np.float32).tobytes()


def pack_int32(torch_tensor):
    return torch_tensor.cpu().numpy().astype(np.int32).tobytes()


def pack_uint32(torch_tensor):
    return torch_tensor.cpu().numpy().astype(np.uint32).tobytes()


def pack_uint16(torch_tensor):
    return torch_tensor.cpu().numpy().astype(np.uint16).tobytes()


def pack_int8(torch_tensor):
    return torch_tensor.cpu().numpy().astype(np.int8).tobytes()


def pack_uint8(torch_tensor):
    return torch_tensor.cpu().numpy().astype(np.uint8).tobytes()


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
    """Pack tensor into BFP8_b format.

    BFP8_b uses 16-element blocks, each with a shared exponent and 8-bit mantissas.
    Only the first (256 * num_faces) elements are packed.

    Args:
        tensor: Input tensor (typically 1024 elements for full tile)
        block_size: Elements per block (always 16 for BFP8_b)
        num_faces: Number of faces to pack (1, 2, or 4)

    Returns:
        List of packed bytes: [exponents...] + [mantissas...]
    """
    flattened_tensor = tensor.flatten()

    # Only pack the first (256 * num_faces) elements
    elements_to_pack = 256 * num_faces
    assert (
        len(flattened_tensor) >= elements_to_pack
    ), f"Tensor has {len(flattened_tensor)} elements, but need at least {elements_to_pack} for {num_faces} face(s)"
    flattened_tensor = flattened_tensor[:elements_to_pack]

    num_blocks = len(flattened_tensor) // block_size

    exponents = []
    mantissas = []

    for i in range(num_blocks):
        block = flattened_tensor[i * block_size : (i + 1) * block_size]
        shared_exponent, bfp8_mantissas = float_to_bfp8_block(block)
        exponents.append(shared_exponent)
        mantissas.extend(bfp8_mantissas)

    return exponents + mantissas


# ============================================================================
# MX (Microscaling) Format Support - OCP Specification
# ============================================================================


def encode_e8m0_scale(max_abs_value, element_max_normal):
    """
    Encode a scale factor as E8M0 (8-bit exponent, no mantissa, bias=127).

    Per OCP MX spec Section 6.3:
    Scale = largest power-of-2 <= (max_value / element_max_normal)

    Args:
        max_abs_value: Maximum absolute value in the block
        element_max_normal: Maximum normal value for element format (e.g., 57344 for E5M2)

    Returns:
        E8M0 encoded scale (0-255), where 255 = NaN
    """
    if max_abs_value == 0 or np.isnan(max_abs_value):
        return 127  # Scale = 2^0 = 1 (neutral scale)

    # Calculate required scale as power of 2
    import math

    scale_ratio = max_abs_value / element_max_normal

    # Find largest power-of-2 <= scale_ratio
    if scale_ratio <= 0:
        exponent = -127
    else:
        exponent = math.floor(math.log2(scale_ratio))

    # Clamp to E8M0 range: 2^(-127) to 2^(127)
    exponent = max(-127, min(127, exponent))

    # E8M0 encoding: exponent + 127 (bias)
    e8m0 = exponent + 127
    return int(e8m0)


def pack_mxfp8r(tensor, block_size=32, num_faces=4):
    """
    Pack tensor into MXFP8R format (MXFP8 E5M2 variant).

    MXFP8 uses 32-element blocks per OCP MX spec, each with:
    - 1 shared E8M0 scale (8 bits)
    - 32 × float8_e5m2 elements (8 bits each)

    Element format E5M2:
    - 1 sign bit, 5 exponent bits (bias=15), 2 mantissa bits
    - Max normal: ±57,344
    - Has Inf and NaN support

    Args:
        tensor: Input tensor (typically 1024 elements for full tile)
        block_size: Elements per block (always 32 for MXFP8)
        num_faces: Number of faces to pack (1, 2, or 4)

    Returns:
        List of packed bytes: [scale0, elem0-31, scale1, elem32-63, ...]
    """
    flattened_tensor = tensor.cpu().to(torch.float32).numpy().flatten()

    # Calculate elements to pack based on faces
    elements_to_pack = 256 * num_faces  # 256 elements per face
    assert (
        len(flattened_tensor) >= elements_to_pack
    ), f"Tensor has {len(flattened_tensor)} elements, need at least {elements_to_pack} for {num_faces} face(s)"

    flattened_tensor = flattened_tensor[:elements_to_pack]
    num_blocks = len(flattened_tensor) // block_size

    packed_bytes = []

    # MXFP8 E5M2 max normal value (from OCP spec Table 2)
    MXFP8_E5M2_MAX_NORMAL = 57344.0  # 2^15 × 1.75

    for i in range(num_blocks):
        # Extract block of 32 elements
        block = flattened_tensor[i * block_size : (i + 1) * block_size]

        # Step 1: Calculate E8M0 scale per OCP spec
        max_abs_value = np.max(np.abs(block))
        scale_e8m0 = encode_e8m0_scale(max_abs_value, MXFP8_E5M2_MAX_NORMAL)

        # Step 2: Decode scale to get scale factor
        scale_exponent = int(scale_e8m0) - 127
        scale_factor = 2.0**scale_exponent

        # Step 3: Scale elements by dividing by scale_factor
        scaled_block = block / scale_factor if scale_factor != 0 else block

        # Step 4: Convert to float8_e5m2 using ml_dtypes
        fp8_elements = scaled_block.astype(ml_dtypes.float8_e5m2)

        # Step 5: Pack into bytes [scale, elements...]
        packed_bytes.append(scale_e8m0)  # 1 byte for E8M0 scale
        packed_bytes.extend(fp8_elements.tobytes())  # 32 bytes for elements

    return packed_bytes


def pack_mxfp8p(tensor, block_size=32, num_faces=4):
    """
    Pack tensor into MXFP8P format (MXFP8 E4M3 variant).

    MXFP8 uses 32-element blocks per OCP MX spec, each with:
    - 1 shared E8M0 scale (8 bits)
    - 32 × float8_e4m3fn elements (8 bits each)

    Element format E4M3:
    - 1 sign bit, 4 exponent bits (bias=7), 3 mantissa bits
    - Max normal: ±448
    - No Inf support, NaN represented by 0bS1111111

    Args:
        tensor: Input tensor (typically 1024 elements for full tile)
        block_size: Elements per block (always 32 for MXFP8)
        num_faces: Number of faces to pack (1, 2, or 4)

    Returns:
        List of packed bytes: [scale0, elem0-31, scale1, elem32-63, ...]
    """
    flattened_tensor = tensor.cpu().to(torch.float32).numpy().flatten()

    # Calculate elements to pack based on faces
    elements_to_pack = 256 * num_faces  # 256 elements per face
    assert (
        len(flattened_tensor) >= elements_to_pack
    ), f"Tensor has {len(flattened_tensor)} elements, need at least {elements_to_pack} for {num_faces} face(s)"

    flattened_tensor = flattened_tensor[:elements_to_pack]
    num_blocks = len(flattened_tensor) // block_size

    packed_bytes = []

    # MXFP8 E4M3 max normal value (from OCP spec Table 2)
    MXFP8_E4M3_MAX_NORMAL = 448.0  # 2^8 × 1.75

    for i in range(num_blocks):
        # Extract block of 32 elements
        block = flattened_tensor[i * block_size : (i + 1) * block_size]

        # Step 1: Calculate E8M0 scale per OCP spec
        max_abs_value = np.max(np.abs(block))
        scale_e8m0 = encode_e8m0_scale(max_abs_value, MXFP8_E4M3_MAX_NORMAL)

        # Step 2: Decode scale to get scale factor
        scale_exponent = int(scale_e8m0) - 127
        scale_factor = 2.0**scale_exponent

        # Step 3: Scale elements by dividing by scale_factor
        scaled_block = block / scale_factor if scale_factor != 0 else block

        # Step 4: Convert to float8_e4m3fn using ml_dtypes
        fp8_elements = scaled_block.astype(ml_dtypes.float8_e4m3fn)

        # Step 5: Pack into bytes [scale, elements...]
        packed_bytes.append(scale_e8m0)  # 1 byte for E8M0 scale
        packed_bytes.extend(fp8_elements.tobytes())  # 32 bytes for elements

    return packed_bytes
