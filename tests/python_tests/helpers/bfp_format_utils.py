# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

"""
Block floating-point quantization utilities.

These functions simulate the round-trip precision loss that occurs when the
hardware packs values into BFP format and then unpacks them back.  Within
each 16-element block the shared exponent is the maximum element exponent,
so elements with smaller exponents lose mantissa bits via right-shifting.
"""

import torch

from .format_config import DataFormat
from .tilize_untilize import untilize_block


def bfp8b_to_float16b(operand: torch.Tensor, dimensions=None) -> torch.Tensor:
    """
    Simulate BFP8_b pack/unpack round-trip quantization.

    Processes values in blocks of 16, determines a shared exponent per block
    (the maximum element exponent), and right-shifts each element's mantissa
    by the delta between its exponent and the shared exponent, matching the
    hardware packer/unpacker behaviour.

    Args:
        operand: Input tensor (any shape).
        dimensions: If provided, untilize the result back to these dimensions.

    Returns:
        Quantized bfloat16 tensor (same number of elements as input).
    """
    BFP8_BLOCK = 16
    flat = operand.flatten().to(torch.float32)
    n = flat.numel()

    u32 = flat.view(torch.int32)
    bf16_bits = (u32 >> 16) & 0xFFFF

    signs = (bf16_bits >> 15) & 1
    exps = (bf16_bits >> 7) & 0xFF
    mants = ((bf16_bits & 0x7F) >> 1) | 0x40

    exps_blocks = exps.view(-1, BFP8_BLOCK)
    mants_blocks = mants.view(-1, BFP8_BLOCK)
    signs_blocks = signs.view(-1, BFP8_BLOCK)

    shared_exps = exps_blocks.max(dim=1, keepdim=True).values

    deltas = shared_exps - exps_blocks

    # Round-to-nearest, ties away from zero (per ISA spec for BFP8 packing).
    # When delta > 0, check the highest bit being shifted out (guard bit).
    guard = torch.where(
        deltas > 0, (mants_blocks >> (deltas - 1)) & 1, torch.zeros_like(mants_blocks)
    )
    shifted = (mants_blocks >> deltas) + guard
    shifted = shifted & 0x7F

    values = shifted.float() * torch.exp2((shared_exps - 133).float())
    values = torch.where(signs_blocks.bool(), -values, values)

    quantized = values.flatten()[:n].to(torch.bfloat16)

    if dimensions is not None:
        quantized = untilize_block(
            quantized,
            stimuli_format=DataFormat.Float16_b,
            dimensions=dimensions,
        ).flatten()

    return quantized


def bfp4b_to_float16b(operand: torch.Tensor, dimensions=None) -> torch.Tensor:
    """
    Simulate BFP4_b pack/unpack round-trip quantization.

    Per the ISA spec, BFP4 packing is a two-stage process:
      1. Convert to BFP8 with rounding (round-to-nearest, ties away from zero)
      2. Truncate BFP8 mantissa (7 bits) down to BFP4 mantissa (3 bits)

    Args:
        operand: Input tensor (any shape).
        dimensions: If provided, untilize the result back to these dimensions.

    Returns:
        Quantized bfloat16 tensor (same number of elements as input).
    """
    BFP4_BLOCK = 16
    flat = operand.flatten().to(torch.float32)
    n = flat.numel()

    u32 = flat.view(torch.int32)
    bf16_bits = (u32 >> 16) & 0xFFFF

    signs = (bf16_bits >> 15) & 1
    exps = (bf16_bits >> 7) & 0xFF
    # BFP8 mantissa: 7 bits = 1 implicit + 6 explicit (same as bfp8b_to_float16b)
    mants = ((bf16_bits & 0x7F) >> 1) | 0x40

    exps_blocks = exps.view(-1, BFP4_BLOCK)
    mants_blocks = mants.view(-1, BFP4_BLOCK)
    signs_blocks = signs.view(-1, BFP4_BLOCK)

    shared_exps = exps_blocks.max(dim=1, keepdim=True).values

    # Stage 1: BF16 -> BFP8 with rounding (round-to-nearest, ties away from zero)
    deltas = shared_exps - exps_blocks
    guard = torch.where(
        deltas > 0, (mants_blocks >> (deltas - 1)) & 1, torch.zeros_like(mants_blocks)
    )
    bfp8_mants = (mants_blocks >> deltas) + guard
    bfp8_mants = bfp8_mants & 0x7F

    # Stage 2: BFP8 -> BFP4 by truncating (drop the 4 LSBs of the 7-bit mantissa)
    bfp4_mants = bfp8_mants >> 4

    # Scale: 2^(shared_exp - 127 - 2) = 2^(shared_exp - 129)
    # BFP4 mantissa is 3 bits with implicit leading 1 worth 4 (0x4),
    # so divide by 4 -> exponent offset is 129
    values = bfp4_mants.float() * torch.exp2((shared_exps - 129).float())
    values = torch.where(signs_blocks.bool(), -values, values)

    quantized = values.flatten()[:n].to(torch.bfloat16)

    if dimensions is not None:
        quantized = untilize_block(
            quantized,
            stimuli_format=DataFormat.Float16_b,
            dimensions=dimensions,
        ).flatten()

    return quantized
