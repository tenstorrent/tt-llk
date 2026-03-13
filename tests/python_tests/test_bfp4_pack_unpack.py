#!/usr/bin/env python3
# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0
"""
Diagnostic test for BFP4_b pack/unpack round-trip correctness.

Focus areas:
  1. Large exponent delta (>= 3) — when the max exponent in a block dominates,
     small values lose all mantissa bits and become zero after the right-shift.
     The question is whether the golden's pack→unpack round-trip correctly
     models this same loss that the hardware packer produces.
  2. Nibble packing order: hardware packs element[i] into HIGH nibble and
     element[i+1] into LOW nibble, but unpack reads LOW nibble first —
     this is the "nibble swap" that swaps adjacent element pairs.
  3. Zero and near-zero values in a block alongside large values.
  4. All-zero blocks.
  5. Mixed-sign blocks.
"""

import os
import struct
import sys

import torch

# Allow running from the tests/python_tests directory directly
sys.path.insert(0, os.path.dirname(__file__))

from helpers.pack import float_to_bfp4_block, pack_bfp4_b
from helpers.unpack import bfp4_to_float_block, unpack_bfp4_b

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def bfloat16_to_binary(value: float) -> str:
    """Return 16-bit binary string of a bfloat16 value."""
    bits = struct.unpack("<I", struct.pack("<f", float(value)))[0]
    bf16 = (bits & 0xFFFF0000) >> 16
    return f"{(bf16 >> 8) & 0xFF:08b}{bf16 & 0xFF:08b}"


def roundtrip_block(values):
    """Pack a 16-element block and unpack it, returning reconstructed floats."""
    block = torch.tensor(values, dtype=torch.bfloat16)
    shared_exp, bfp4_mantissas = float_to_bfp4_block(block)
    reconstructed = bfp4_to_float_block(shared_exp, bfp4_mantissas, {})
    return shared_exp, bfp4_mantissas, reconstructed


def describe_block(values, shared_exp, bfp4_mantissas, reconstructed):
    """Print a detailed breakdown of one block."""
    print(f"  Shared exponent (raw): {shared_exp}  (biased-127 = {shared_exp - 127})")
    for i, (orig, mant, recon) in enumerate(zip(values, bfp4_mantissas, reconstructed)):
        orig_f = float(orig)
        bf16_bin = bfloat16_to_binary(orig_f)
        orig_exp = int(bf16_bin[1:9], 2)
        delta = shared_exp - orig_exp
        sign_bit = (mant >> 3) & 1
        mag_bits = mant & 0x7
        print(
            f"    [{i:2d}] orig={orig_f:8.4f}  exp={orig_exp} delta={delta:2d} "
            f"4-bit=0b{mant:04b}(s={sign_bit} m={mag_bits:03b})  recon={float(recon):8.4f}"
            + (
                "  *** LOST ***"
                if delta >= 3 and mag_bits == 0 and orig_f != 0.0
                else ""
            )
        )


def check_roundtrip(label, values, atol=0.0):
    """Run round-trip and report mismatches. Returns True if all OK."""
    block = torch.tensor(values, dtype=torch.bfloat16)
    shared_exp, bfp4_mantissas, reconstructed = roundtrip_block(block.tolist())
    ok = True
    for i, (orig, recon) in enumerate(zip(block.tolist(), reconstructed)):
        if abs(float(orig) - float(recon)) > atol:
            # Only flag if the discrepancy is NOT explained by precision loss
            orig_exp = int(bfloat16_to_binary(float(orig))[1:9], 2)
            delta = shared_exp - orig_exp
            if delta < 3:
                print(
                    f"  UNEXPECTED mismatch at [{i}]: orig={float(orig):.6f} recon={float(recon):.6f}  (delta={delta})"
                )
                ok = False
    return ok


# ---------------------------------------------------------------------------
# Test cases
# ---------------------------------------------------------------------------


def test_large_exponent_delta():
    """
    When one element has a much larger exponent than others, the smaller
    elements lose all mantissa bits. Verifies the golden correctly zeroes them.
    """
    print("\n=== Test: large exponent delta ===")
    # 1.0 has exponent 127 (2^0). 0.0039 ~ 2^-8 -> exponent 119. Delta = 8 >= 3.
    values = [1.0] + [0.0039] * 15
    shared_exp, bfp4_mantissas, reconstructed = roundtrip_block(
        [float(torch.tensor(v, dtype=torch.bfloat16)) for v in values]
    )
    describe_block(values, shared_exp, bfp4_mantissas, reconstructed)

    # Elements 1..15 should be reconstructed as 0.0 (delta >= 3, all bits lost)
    passed = True
    for i in range(1, 16):
        orig_exp = int(
            bfloat16_to_binary(float(torch.tensor(values[i], dtype=torch.bfloat16)))[
                1:9
            ],
            2,
        )
        delta = shared_exp - orig_exp
        recon = float(reconstructed[i])
        if delta >= 3 and recon != 0.0:
            print(f"  FAIL [{i}]: delta={delta}, expected recon=0.0 but got {recon}")
            passed = False
        elif delta < 3 and recon == 0.0 and values[i] != 0.0:
            print(f"  FAIL [{i}]: delta={delta}<3, unexpected zero reconstruction")
            passed = False
    print(f"  Result: {'PASS' if passed else 'FAIL'}")
    return passed


def test_adjacent_delta_boundary():
    """
    Test the exact boundary: delta=2 should preserve the sign bit at minimum;
    delta=3 means the implicit leading 1 shifts out entirely → 0.
    """
    print("\n=== Test: delta boundary (delta=2 vs delta=3) ===")
    # 1.0 = 2^0 exp=127. 0.25 = 2^-2 exp=125 -> delta=2. 0.125 = 2^-3 exp=124 -> delta=3.
    values_delta2 = [1.0, 0.25] + [1.0] * 14
    values_delta3 = [1.0, 0.125] + [1.0] * 14

    passed = True
    for label, values, elem_idx, expected_nonzero in [
        ("delta=2 (should preserve value)", values_delta2, 1, True),
        ("delta=3 (should collapse to 0)", values_delta3, 1, False),
    ]:
        bfp_values = [float(torch.tensor(v, dtype=torch.bfloat16)) for v in values]
        shared_exp, bfp4_mantissas, reconstructed = roundtrip_block(bfp_values)
        recon = float(reconstructed[elem_idx])
        is_nonzero = recon != 0.0
        status = "PASS" if is_nonzero == expected_nonzero else "FAIL"
        print(f"  {label}: recon={recon:.6f}  -> {status}")
        if status == "FAIL":
            passed = False
    return passed


def test_nibble_packing_order():
    """
    Verify that pack_bfp4_b → unpack_bfp4_b preserves element order.
    Hardware packs element[i] in HIGH nibble and element[i+1] in LOW nibble.
    unpack reads LOW nibble first → swaps adjacent pairs.
    This test checks what actually happens end-to-end.
    """
    print("\n=== Test: nibble packing order (full pack→unpack round-trip) ===")
    # Use values that are all representable exactly in BFP4_b at the same exponent
    # so we can distinguish element positions by value.
    # 0.25, 0.50, 0.75, 1.00 repeated
    pattern = [0.25, 0.50, 0.75, 1.00] * 4  # 16 elements
    tensor = torch.tensor(pattern, dtype=torch.bfloat16)

    packed = pack_bfp4_b(tensor, num_faces=1, face_r_dim=1)
    unpacked = unpack_bfp4_b(packed, num_faces=1, face_r_dim=1)

    print(f"  Input:    {[float(x) for x in tensor[:16]]}")
    print(f"  Unpacked: {[float(x) for x in unpacked[:16]]}")

    passed = torch.allclose(tensor[:16].float(), unpacked[:16].float(), atol=1e-3)
    print(f"  Result: {'PASS' if passed else 'FAIL - element order mismatch!'}")

    if not passed:
        # Check if it is exactly the adjacent-swap pattern
        swapped = torch.zeros_like(tensor[:16])
        for i in range(0, 16, 2):
            swapped[i] = tensor[i + 1]
            swapped[i + 1] = tensor[i]
        if torch.allclose(swapped.float(), unpacked[:16].float(), atol=1e-3):
            print("  DIAGNOSIS: adjacent pair swap confirmed — nibble order bug")
    return passed


def test_all_zero_block():
    """All-zero block should round-trip to all zeros."""
    print("\n=== Test: all-zero block ===")
    values = [0.0] * 16
    shared_exp, bfp4_mantissas, reconstructed = roundtrip_block(
        [float(torch.tensor(v, dtype=torch.bfloat16)) for v in values]
    )
    passed = all(float(r) == 0.0 for r in reconstructed)
    print(f"  Reconstructed: {[float(r) for r in reconstructed]}")
    print(f"  Result: {'PASS' if passed else 'FAIL'}")
    return passed


def test_mixed_sign_block():
    """Block with both positive and negative values."""
    print("\n=== Test: mixed sign block ===")
    values = [
        1.0,
        -1.0,
        0.5,
        -0.5,
        0.25,
        -0.25,
        0.75,
        -0.75,
        1.0,
        -1.0,
        0.5,
        -0.5,
        0.25,
        -0.25,
        0.75,
        -0.75,
    ]
    bfp_values = [float(torch.tensor(v, dtype=torch.bfloat16)) for v in values]
    shared_exp, bfp4_mantissas, reconstructed = roundtrip_block(bfp_values)
    describe_block(bfp_values, shared_exp, bfp4_mantissas, reconstructed)

    # Signs should be preserved
    passed = True
    for i, (orig, recon) in enumerate(zip(bfp_values, reconstructed)):
        if orig != 0.0 and recon != 0.0:
            if (orig > 0) != (recon > 0):
                print(
                    f"  FAIL [{i}]: sign flipped orig={orig:.4f} recon={float(recon):.4f}"
                )
                passed = False
    print(f"  Result: {'PASS' if passed else 'FAIL'}")
    return passed


def test_full_tile_roundtrip_1face():
    """
    Pack a 256-element (1-face, 16 rows × 16 cols) tile and unpack it.
    Checks that the element count and order are preserved (modulo precision loss).
    """
    print("\n=== Test: full 1-face tile round-trip (256 elements) ===")
    torch.manual_seed(42)
    tensor = torch.rand(256, dtype=torch.bfloat16)

    packed = pack_bfp4_b(tensor, num_faces=1, face_r_dim=16)
    unpacked = unpack_bfp4_b(packed, num_faces=1, face_r_dim=16)

    if len(unpacked) != 256:
        print(f"  FAIL: expected 256 elements, got {len(unpacked)}")
        return False

    # Count elements where precision loss > tolerance for BFP4
    atol = 0.2
    mismatches = sum(
        1
        for o, r in zip(tensor.tolist(), unpacked.tolist())
        if abs(float(o) - float(r)) > atol
    )
    print(f"  Mismatches (atol={atol}): {mismatches}/256")
    passed = mismatches == 0
    print(f"  Result: {'PASS' if passed else 'FAIL'}")
    return passed


def test_exponent_delta_table():
    """
    Print a table showing what happens for each exponent delta 0..7
    to make the precision loss explicit.
    """
    print("\n=== Table: mantissa survival per exponent delta ===")
    print(
        f"  {'delta':>5}  {'3-bit input':>11}  {'after >>delta':>13}  {'stored (3-bit)':>14}  {'survives?':>9}"
    )
    print("  " + "-" * 60)
    for delta in range(8):
        # The implicit leading 1 bit: mantissa starts as 0b1xx (3 bits = values 4..7)
        for mant_in in [4, 5, 6, 7]:  # 0b100, 0b101, 0b110, 0b111
            shifted = mant_in >> delta
            stored = shifted & 0x7
            survives = stored != 0
            print(
                f"  {delta:>5}  {mant_in:>6} (0b{mant_in:03b})  "
                f"{shifted:>6} (0b{shifted:03b})    "
                f"{stored:>6} (0b{stored:03b})   "
                f"{'YES' if survives else 'NO (zeroed)'}"
            )
        print()
    return True


def test_specific_failing_pattern():
    """
    Reproduce the pattern seen in failing tests: a block of values in [0, 1]
    where some values are very small relative to the max, causing exponent delta >= 3.
    """
    print("\n=== Test: pattern from failing datacopy tests ===")
    # Values similar to what appeared in the test failures
    # Some are 0.12, 0.62, 0.88 etc — typical BFP4 representable values
    # mixed with values that might not survive quantization
    failing_blocks = [
        # Block with 0.00 mixed with larger values (delta can be huge for 0.0)
        [
            0.25,
            0.25,
            0.62,
            0.88,
            0.88,
            0.25,
            0.25,
            0.25,
            0.25,
            0.38,
            0.38,
            0.25,
            0.88,
            0.12,
            0.75,
            0.62,
        ],
        # Block where some values have large delta
        [
            0.00,
            0.75,
            1.00,
            0.25,
            0.00,
            0.75,
            0.25,
            0.50,
            0.75,
            0.00,
            0.25,
            0.00,
            0.00,
            0.25,
            0.00,
            0.00,
        ],
        # Float32 input with high precision values that round differently
        [
            0.50,
            0.50,
            0.75,
            0.50,
            0.00,
            1.00,
            0.50,
            0.50,
            0.50,
            0.50,
            0.75,
            0.50,
            0.25,
            0.25,
            1.00,
            0.75,
        ],
    ]

    all_passed = True
    for idx, values in enumerate(failing_blocks):
        bfp_values = [float(torch.tensor(v, dtype=torch.bfloat16)) for v in values]
        shared_exp, bfp4_mantissas, reconstructed = roundtrip_block(bfp_values)
        print(f"\n  Block {idx+1}: shared_exp={shared_exp} (2^{shared_exp-127})")
        for i, (orig, mant, recon) in enumerate(
            zip(bfp_values, bfp4_mantissas, reconstructed)
        ):
            orig_exp = (
                int(bfloat16_to_binary(float(orig))[1:9], 2) if orig != 0.0 else 0
            )
            delta = shared_exp - orig_exp if orig != 0.0 else 999
            flag = ""
            if orig != float(recon):
                # Change is expected when: delta >= 3 (exponent shift zeroes out mantissa)
                # OR when delta < 3 but mantissa bits are truncated (normal BFP4 precision loss)
                # A truly unexpected change would be a sign flip or a value larger than original
                sign_flipped = (
                    orig != 0.0 and recon != 0.0 and (orig > 0) != (float(recon) > 0)
                )
                value_larger = abs(float(recon)) > abs(orig) + 1e-6
                if sign_flipped or value_larger:
                    flag = f" <- CHANGED ({orig:.4f} -> {float(recon):.4f}) *** UNEXPECTED (sign/magnitude error) ***"
                    all_passed = False
                else:
                    flag = f" <- CHANGED ({orig:.4f} -> {float(recon):.4f})  [precision loss OK]"
            print(
                f"    [{i:2d}] orig={orig:6.4f}  delta={'inf' if delta==999 else delta:>3}  "
                f"mant=0b{mant:04b}  recon={float(recon):6.4f}{flag}"
            )

    print(
        f"\n  Result: {'PASS (all changes explained by precision loss)' if all_passed else 'FAIL (unexpected mismatches)'}"
    )
    return all_passed


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main():
    print("BFP4_b pack/unpack diagnostic tests")
    print("=" * 60)

    results = {}
    results["large_exponent_delta"] = test_large_exponent_delta()
    results["adjacent_delta_boundary"] = test_adjacent_delta_boundary()
    results["nibble_packing_order"] = test_nibble_packing_order()
    results["all_zero_block"] = test_all_zero_block()
    results["mixed_sign_block"] = test_mixed_sign_block()
    results["full_tile_roundtrip_1f"] = test_full_tile_roundtrip_1face()
    test_exponent_delta_table()
    results["failing_pattern"] = test_specific_failing_pattern()

    print("\n" + "=" * 60)
    print("Summary:")
    all_pass = True
    for name, passed in results.items():
        status = "PASS" if passed else "FAIL"
        print(f"  {name:<30} {status}")
        if not passed:
            all_pass = False

    print()
    print("Overall:", "ALL PASS" if all_pass else "SOME FAILURES DETECTED")
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
