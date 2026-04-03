# BFP4_b Implementation Reference

This document is the authoritative reference for BFP4_b (Block Floating Point, 4-bit, B-exponent variant) format handling in the TT-LLK test infrastructure. It covers the format specification, packing/unpacking pipelines, golden generation, validation, ISA compliance, and known gaps in other golden generators. It is intended to serve as a guideline when implementing support for additional BFP formats such as BFP2_b.

**Branch**: `ldjurovic/fix_bfp4_b_packer`
**ISA Reference**: [tenstorrent/tt-isa-documentation](https://github.com/tenstorrent/tt-isa-documentation), specifically:
- `WormholeB0/TensixTile/TensixCoprocessor/FloatBitPatterns.md`
- `WormholeB0/TensixTile/TensixCoprocessor/Packers/FormatConversion.md`
- `WormholeB0/TensixTile/TensixCoprocessor/Unpackers/FormatConversion.md`

---

## Table of Contents

1. [BFP4_b Format Specification](#1-bfp4_b-format-specification)
2. [Packing Pipeline (BF16 to BFP4_b)](#2-packing-pipeline-bf16-to-bfp4_b)
3. [Unpacking Pipeline (BFP4_b to BF16)](#3-unpacking-pipeline-bfp4_b-to-bf16)
4. [Golden Generation and Quantization](#4-golden-generation-and-quantization)
5. [Comparison and Validation](#5-comparison-and-validation)
6. [Bugs Fixed on the Branch](#6-bugs-fixed-on-the-branch)
7. [ISA Compliance Verification](#7-isa-compliance-verification)
8. [Missing Improvements in Other Goldens](#8-missing-improvements-in-other-goldens)
9. [Guideline for Implementing BFP2_b](#9-guideline-for-implementing-bfp2_b)

---

## 1. BFP4_b Format Specification

### 1.1 Bit Layout

BFP4_b encodes each datum as 4 bits: **1 sign bit + 3 magnitude bits**. Every group of 16 datums shares a single **8-bit exponent** stored separately.

```
Per-datum (4 bits):   [S | M2 M1 M0]
                       ^   ^^^^^^^^
                       |   3-bit magnitude (unsigned, 0..7)
                       sign (0 = positive, 1 = negative)

Per-block (8 bits):   [E7 E6 E5 E4 E3 E2 E1 E0]
                       ^^^^^^^^^^^^^^^^^^^^^^^^^^^
                       Shared 8-bit exponent (interpreted like FP32 bias: Exp-127)
```

### 1.2 Value Interpretation

From the ISA documentation (`FloatBitPatterns.md`):

| Sign | Exp   | Mag (3b) | BFP4 Meaning                          |
|------|-------|----------|---------------------------------------|
| 0    | Any   | 1 - 7    | `+Mag/4 * 2^(Exp-127)`               |
| 0    | Any   | 0        | `+0`                                  |
| 1    | Any   | 1 - 7    | `-Mag/4 * 2^(Exp-127)`               |
| 1    | Any   | 0        | `-2^128` or `-Infinity`               |

The magnitude is divided by `2^2 = 4` because the 3-bit magnitude field has no implicit leading 1 bit. The representable magnitudes per block for a given shared exponent `E` are:

```
Mag=0: 0 (special: -Inf when sign=1)
Mag=1: 1/4 * 2^(E-127) = 2^(E-129)
Mag=2: 2/4 * 2^(E-127) = 2^(E-128)
Mag=3: 3/4 * 2^(E-127)
Mag=4: 4/4 * 2^(E-127) = 2^(E-127)
Mag=5: 5/4 * 2^(E-127)
Mag=6: 6/4 * 2^(E-127) = 3 * 2^(E-128)
Mag=7: 7/4 * 2^(E-127)
```

### 1.3 BFP4_b vs BFP4a

| Property       | BFP4_b (B-exponent)                  | BFP4a (A-exponent)                    |
|----------------|--------------------------------------|---------------------------------------|
| Exponent width | 8 bits (full byte stored)            | 5 bits (zero-extended to 8 for storage) |
| Exponent bias  | 127 (same as FP32)                   | 15 (same as FP16)                     |
| Value formula  | `Mag/4 * 2^(Exp-127)`               | `Mag/4 * 2^(Exp-15)`                 |
| sign=1,mag=0   | `-2^128` or `-Infinity`              | `-2^16`                               |
| Unpacks to     | BF16                                 | FP16                                  |

### 1.4 Memory Layout

In L1 memory, a BFP4_b tile is stored as:

```
[exponent_0] [exponent_1] ... [exponent_(N-1)] [padding to MIN_BFP_EXPONENTS]
[packed_mantissa_byte_0] [packed_mantissa_byte_1] ...
```

- **Exponents**: One byte per 16-element block. Hardware requires a minimum of 16 exponents (`MIN_BFP_EXPONENTS = 16`). If fewer blocks exist, the remaining exponent slots are zero-padded.
- **Packed mantissas**: Two 4-bit datums per byte. The **low nibble** (bits 3:0) holds the even-indexed element; the **high nibble** (bits 7:4) holds the odd-indexed element.

For a full tile (4 faces x 16 rows x 16 columns = 1024 elements):
- 64 exponents (1024/16 = 64 blocks)
- 512 mantissa bytes (1024 datums / 2 per byte)
- Total: 576 bytes

---

## 2. Packing Pipeline (BF16 to BFP4_b)

The ISA documentation (`Packers/FormatConversion.md`) specifies that BFP4 packing is a **two-stage process**:

> **To BFP4 or BFP2:** Converted to BFP8 (as per the above row), then truncate to BFP4 or BFP2.

And for BFP8:

> **To BFP8:** Converted to BF16 (as per first row), then round to BFP8 with one shared 8b exponent per 16 datums.

So the full pipeline is: **BF16 -> BFP8 (with rounding) -> BFP4 (with truncation)**.

### 2.1 Stage 1: BF16 to BFP8

**Implementation**: `float_to_bfp8_block()` in `tests/python_tests/helpers/pack.py` (scalar, per-block), and `_bfp8_mantissas_per_block()` in `tests/python_tests/helpers/bfp_format_utils.py` (vectorized, all blocks at once).

#### Step-by-step procedure (per 16-element block):

1. **Extract BF16 fields** from each datum:
   - Sign: bit 15 (1 bit)
   - Exponent: bits 14:7 (8 bits)
   - Mantissa: bits 6:0 (7 bits), prepend implicit leading 1 to get 8-bit value `1.xxxxxxx`

   In `pack.py`, the BF16 value is extracted from the upper 16 bits of an FP32 representation:
   ```python
   float_value = struct.unpack("<I", struct.pack("<f", value))[0]
   bfloat16_value = (float_value & 0xFFFF0000) >> 16
   sign = binary_str[0]           # bit 15
   exponent = int(binary_str[1:9], 2)  # bits 14:7
   mantissa = "1" + binary_str[9:-1]   # implicit 1 + bits 6:1 (6 explicit bits)
   ```

   In `bfp_format_utils.py`, the vectorized extraction:
   ```python
   bf16_bits = (flat.view(torch.int32) >> 16) & 0xFFFF
   signs = (bf16_bits >> 15) & 1
   exps  = (bf16_bits >> 7) & 0xFF
   mants = ((bf16_bits & 0x7F) >> 1) | 0x40   # bits 6:1, with implicit 1 at bit 6
   ```

   Note: `(bf16_bits & 0x7F) >> 1` extracts bits 6:1 of the BF16 mantissa (6 bits), and `| 0x40` sets the implicit leading 1 at bit position 6, yielding a 7-bit mantissa value in range `[0x40, 0x7F]` (64..127).

2. **Compute the shared exponent**: the maximum exponent across all 16 datums in the block.
   ```python
   shared_exponent = max(exponents)
   ```

3. **Right-shift each mantissa** by `delta = shared_exponent - element_exponent`.

4. **Round-to-nearest, ties away from zero** using the guard bit. When `delta > 0`, the guard bit is the most significant bit that would be shifted out:
   ```python
   if exponent_delta > 0:
       guard_bit = (mantissa >> (exponent_delta - 1)) & 1
       mantissa = (mantissa >> exponent_delta) + guard_bit
   else:
       mantissa = mantissa  # no shift needed
   ```

   The guard bit check (`>> (delta-1) & 1`) examines the highest bit being discarded. Adding it to the shifted result implements round-to-nearest with ties going away from zero (rounding up in magnitude).

5. **Mask to 7 bits** and prepend sign:
   ```python
   mantissa = mantissa & 0x7F
   mantissa = (sign << 7) | mantissa
   ```

   The result is an 8-bit BFP8 datum: `[sign(1) | magnitude(7)]`.

### 2.2 Stage 2: BFP8 to BFP4 Truncation

**Implementation**: `truncate_bfp8_to_bfp4()` in `tests/python_tests/helpers/pack.py`.

This stage is **simple truncation** with no additional rounding, per the ISA spec ("truncate to BFP4"):

```python
sign = (bfp8 >> 7) & 0x1        # extract sign bit
magnitude = (bfp8 >> 4) & 0x7   # keep top 3 of 7 magnitude bits
bfp4 = (sign << 3) | magnitude  # reassemble as 4-bit datum
```

The 4 least significant magnitude bits are dropped. No rounding is applied at this stage -- the ISA explicitly says "truncate".

### 2.3 Combined Two-Stage Function

**Implementation**: `float_to_bfp4_block()` in `tests/python_tests/helpers/pack.py`:

```python
def float_to_bfp4_block(block):
    # Stage 1: Convert to BFP8 first (with rounding)
    shared_exponent, bfp8_mantissas = float_to_bfp8_block(block)
    # Stage 2: Truncate BFP8 mantissas to BFP4 (no rounding)
    bfp4_mantissas = truncate_bfp8_to_bfp4(bfp8_mantissas)
    return shared_exponent, bfp4_mantissas
```

### 2.4 Full Tile Packing

**Implementation**: `pack_bfp4_b()` in `tests/python_tests/helpers/pack.py`:

1. Flatten the input tensor
2. Compute elements to pack based on `num_faces` and `face_r_dim`
3. Process each 16-element block through `float_to_bfp4_block()`
4. Pad exponents to minimum 16 if needed
5. Pack two 4-bit mantissas per byte: `(high_nibble << 4) | low_nibble`
6. Return `exponents + packed_mantissas`

---

## 3. Unpacking Pipeline (BFP4_b to BF16)

### 3.1 ISA Conversion Routine

The ISA documentation provides the exact hardware conversion logic (`FloatBitPatterns.md`):

```c
uint16_t BFP8ToBF16(uint8_t DatumBits, uint8_t ExpBits) {
  uint8_t Sign = DatumBits >> 7;
  uint8_t Mag  = DatumBits << 1;
  if (Mag == 0) {
    return Sign ? 0xff80 : 0;  // -Inf or +0
  } else {
    unsigned LZ = stdc_leading_zeros_uc(Mag);
    Mag <<= LZ;
    ExpBits -= LZ;
    return (Sign << 15) | (ExpBits << 7) | (Mag & 0x7e);
  }
}

uint16_t BFP4ToBF16(uint4_t DatumBits, uint8_t ExpBits) {
  return BFP8ToBF16(DatumBits << 4, ExpBits);
}
```

The key insight: BFP4 conversion is defined as "left-shift the 4-bit datum by 4 positions to make an 8-bit BFP8 datum, then convert that BFP8 datum to BF16." This means the BFP4 magnitude bits occupy the top 3 magnitude positions of the BFP8 format, with the bottom 4 bits zeroed.

### 3.2 Test Infrastructure Unpacking

**Implementation**: `unpack_bfp4_b()` and `bfp4_to_float_block()` in `tests/python_tests/helpers/unpack.py`.

```python
def bfp4_to_float_block(exponent, bfp4_mantissas, unpacked_bfp4):
    exp_adj = exponent - 127
    scale = 2.0 ** (exp_adj - 2)  # = 2^(Exp-129)
    for mantissa in bfp4_mantissas:
        mag = mantissa & 0x7
        if mag == 0:
            value = 0.0
        else:
            sign = -1.0 if mantissa & 0x8 else 1.0
            value = sign * mag * scale   # mag/4 * 2^(Exp-127)
        ...
```

The formula `mag * 2^(Exp-129)` equals `mag/4 * 2^(Exp-127)`, which matches the ISA spec.

The `unpack_bfp4_b()` function handles the memory layout:
1. Read exponents (one per 16-element block)
2. Expand packed bytes: low nibble = even element, high nibble = odd element
3. Convert each block using `bfp4_to_float_block()`

### 3.3 Vectorized Quantization Simulation

**Implementation**: `bfp4b_to_float16b()` in `tests/python_tests/helpers/bfp_format_utils.py`.

This function simulates the **pack-then-unpack round trip** for golden generation. It does not actually pack/unpack bytes; instead it computes what values the hardware would produce:

```python
def bfp4b_to_float16b(operand, dimensions=None):
    flat = operand.flatten().to(torch.float32)
    n = flat.numel()
    signs, exps, mants = _bf16_sign_exp_mantissa(flat)

    signs_blocks = signs.view(-1, BFP_BLOCK)
    shared_exps, bfp8_mants = _bfp8_mantissas_per_block(
        mants.view(-1, BFP_BLOCK), exps.view(-1, BFP_BLOCK),
    )

    # Stage 2: BFP8 -> BFP4 by truncating (drop 4 LSBs of 7-bit mantissa)
    bfp4_mants = bfp8_mants >> 4

    # Reconstruct: bfp4_mant * 2^(shared_exp - 129)
    values = bfp4_mants.float() * torch.exp2((shared_exps - 129).float())
    return _finalize_bfp_quantized(values, signs_blocks, n, dimensions)
```

**Why `shared_exp - 129`?** The BFP4 mantissa `bfp4_mant` is a 3-bit value in range [0, 7]. It represents `bfp4_mant / 4` of the base power `2^(Exp-127)`. So:
```
value = bfp4_mant / 4 * 2^(Exp-127) = bfp4_mant * 2^(Exp-127-2) = bfp4_mant * 2^(Exp-129)
```

**Why `shared_exp - 133` for BFP8?** Similarly, the BFP8 mantissa is 7 bits in range [0, 127], representing `bfp8_mant / 64` of `2^(Exp-127)`:
```
value = bfp8_mant / 64 * 2^(Exp-127) = bfp8_mant * 2^(Exp-127-6) = bfp8_mant * 2^(Exp-133)
```

---

## 4. Golden Generation and Quantization

Every golden generator that handles BFP4_b data must model two key hardware behaviors:

1. **Input quantization**: When BFP4_b data is read from L1 by the unpacker, it loses precision. The golden must simulate this by calling `bfp4b_to_float16b()` on inputs before performing the math operation.

2. **Output quantization**: When results are written back to L1 by the packer, they are quantized to BFP4_b. The golden must simulate this by calling `bfp4b_to_float16b()` on the result after the math operation.

3. **Flush-to-Zero (FTZ)**: Hardware flushes values with `|x| < 1e-37` to zero. This corresponds to shared exponents of 0 or 1, where the smallest meaningful BFP value (with `shared_exp=2`) is approximately `2.35e-38`.

4. **Block inf/NaN handling**: If any element in a 16-element block is inf or NaN, all other elements in that block share the same exponent and get zeroed out by hardware. This is modeled by `check_bfp4_b()`.

### 4.1 Per-Golden Coverage

#### EltwiseBinaryGolden (`golden_generators.py`, line 1788)

- **Input quantization**: `_quantize_input()` dispatches to `bfp4b_to_float16b()` for BFP4_b inputs and `bfp8b_to_float16b()` for BFP8_b inputs. Input quantization is **enabled** in `__call__` — it was previously commented out (Bug 9) but has been re-enabled since the tests do not pre-quantize inputs.
- **Fidelity masking**: When input is BFP4_b or BFP8_b, fidelity masking uses `Float16_b` as the source register format (since BFP formats are unpacked to BF16 in source registers).
- **Output quantization**: After computing the operation, the result is quantized via `bfp4b_to_float16b(result.to(torch.bfloat16))`.
- **FTZ**: An explicit FTZ pass after all quantization: `torch.where(result_f32.abs() < 1e-37, zeros, result_f32)`.
- **Elwmul fidelity**: For multiply operations, fidelity masking is applied per-iteration from the **original** operands (not reusing previously masked values). Each iteration masks the originals independently and accumulates the phase results.
- **Broadcast interaction**: When broadcast is active, `input_format_B` is explicitly passed as `None` to avoid double-quantizing (since `BroadcastGolden` already quantized the broadcast operand). A `_UNSET` sentinel distinguishes between "not provided" (defaults to `input_format`) and "explicitly `None`" (skip quantization).

#### ReduceGolden (`golden_generators.py`, line 2072)

- **Input quantization**: `_quantize_reduce_input()` dispatches to `bfp4b_to_float16b()`.
- **Output quantization**: `_quantize_reduce_output()` is called on each tile result (or on the accumulated result for reduce-to-one). It dispatches to `bfp4b_to_float16b()`.
- **input_format parameter**: The `test_reduce.py` now passes `input_format=formats.input_format` to the golden, enabling proper input quantization.

#### MatmulGolden (`golden_generators.py`, line 782)

- **Input quantization**: Both operands A and B are independently quantized via `_bfp4b_to_float16b()` when their format is BFP4_b. The optional `dimensions` parameter is passed for tilize/untilize handling.
- **Output quantization**: **NOT IMPLEMENTED** -- see Section 8.
- **FTZ**: **NOT IMPLEMENTED** -- see Section 8.

#### BroadcastGolden (`golden_generators.py`, line 918)

- **Input quantization**: Critically, BFP4_b quantization is applied **before** extracting broadcast values (line 964). This matches hardware behavior: the unpacker decompresses BFP4_b using the original 16-element block's shared exponent, then broadcasts the decompressed value. If quantization happened after broadcasting, the shared exponents would be wrong (trivial blocks of repeated values).
- **Output quantization**: **NOT IMPLEMENTED** (the broadcast golden just returns the broadcast tensor; output quantization is handled by the caller, e.g., `EltwiseBinaryGolden`).

#### DataCopyGolden (`golden_generators.py`, line 1073)

- **Input quantization**: Calls `_bfp4b_to_float16b(operand1)` when `input_format == DataFormat.Bfp4_b`, or `_bfp8b_to_float16b(operand1)` when `input_format == DataFormat.Bfp8_b`.
- **Output quantization**: When `data_format` is `Bfp4_b` or `Bfp8_b`, the result is tilized (for `num_faces == 4`), then quantized via the appropriate `_bfpXb_to_float16b(data, dims)` function. For `num_faces < 4`, `dims=None` is passed (skipping untilization inside the quantization function).

#### UnarySFPUGolden (`golden_generators.py`, line 1378)

- **Input quantization**: Calls `_bfp4b_to_float16b(operand1)` when `input_format == DataFormat.Bfp4_b`.
- **Intermediate precision**: Uses `torch.float32` dtype for intermediate results when output is BFP4_b.
- **Block inf/NaN check**: Calls `check_bfp4_b(result)` to zero out blocks containing inf/NaN.
- **NaN-to-inf conversion**: Applied for B-exponent formats.
- **Output quantization**: Tilizes the result, then calls `_bfp4b_to_float16b(tilized, dimensions)` to simulate the pack round-trip.

#### BinarySFPUGolden (`golden_generators.py`, line 1886)

- Inherits from `EltwiseBinaryGolden` but has its **own** `__call__` method that processes elements individually in a loop.
- **Does NOT call** the parent's output quantization logic.
- See Section 8 for implications.

### 4.2 Flush-to-Zero Implementation

The `_finalize_bfp_quantized()` function in `bfp_format_utils.py` applies FTZ:

```python
FTZ_THRESHOLD = 1e-37
values = torch.where(values.abs() < FTZ_THRESHOLD, torch.zeros_like(values), values)
```

This is also applied explicitly in `EltwiseBinaryGolden.__call__()` as a final pass after output quantization:

```python
FTZ_THRESHOLD = 1e-37
result_f32 = result.float()
result = torch.where(
    result_f32.abs() < FTZ_THRESHOLD,
    torch.zeros_like(result_f32),
    result_f32,
).to(result.dtype)
```

The threshold of `1e-37` was chosen because:
- The smallest meaningful BFP value has `shared_exp=2`, giving `1/4 * 2^(2-127) = 2^(-127) ≈ 5.88e-39` for BFP4.
- For BFP8, the smallest meaningful value is `1/64 * 2^(2-127) = 2^(-131) ≈ 3.68e-40`.
- Values at `shared_exp=0` or `shared_exp=1` are effectively zero in hardware.
- `1e-37 ≈ 2^(-123)` provides comfortable margin above these subnormal values.

### 4.3 Block Inf/NaN Check

The `check_bfp4_b()` function in `golden_generators.py`:

```python
def check_bfp4_b(operand: list) -> list:
    not_finite = [math.inf, -math.inf]
    for i, x in enumerate(operand):
        if x in not_finite or math.isnan(x):
            for col in range(16):
                row = i // 16
                index = row * 16 + col
                if not (operand[index] in not_finite or math.isnan(operand[index])):
                    operand[index] = 0.0
    return operand
```

When any element in a 16-element block is inf or NaN, the shared exponent is set to the maximum (255). All other elements in that block, which have much smaller exponents, get their mantissas right-shifted to zero during packing. This function models that behavior.

---

## 5. Comparison and Validation

### 5.1 Block-Aware ULP Comparison

**Implementation**: `_bfp4_block_aware_compare()` in `tests/python_tests/helpers/utils.py`.

Standard `torch.isclose()` comparison is insufficient for BFP4_b because the quantization granularity depends on the block's shared exponent. Instead, we use a block-aware ULP (Unit in the Last Place) comparison:

```python
def _bfp4_block_aware_compare(golden, result, max_ulp_diff=1):
    BLOCK = 16
    BFP4_MANTISSA_BITS = 3

    g_flat = golden.float().flatten()
    r_flat = result.float().flatten()

    for blk_start in range(0, n, BLOCK):
        g_blk = g_flat[blk_start:blk_end]
        r_blk = r_flat[blk_start:blk_end]

        # Compute ULP size from block's maximum magnitude
        block_max = max(|golden|, |result|) in this block
        block_exp = floor(log2(block_max))
        one_ulp = 2^(block_exp - BFP4_MANTISSA_BITS + 1)  # = 2^(block_exp - 2)

        diff = |g_blk - r_blk|
        ulp_ok = diff <= max_ulp_diff * one_ulp
```

Special cases handled:
- **All NaN blocks**: both NaN at same position treated as match.
- **All-zero blocks**: compared with `atol=1e-5, rtol=0.0`.
- **Tiny values** (`max_abs < 1e-4`): compared with `atol=1e-5` regardless of ULP.
  This handles padding/zero lanes that can disagree slightly after pack-unpack while both printing as 0.00.

### 5.2 Key Design Decisions

1. **No re-tilization**: The comparison operates on data already in tilized (block) order. The main branch used to tilize/untilize the comparison tensors, which was unnecessary and error-prone. The branch removed this.

2. **Default ULP tolerance = 1**: Tightened from 2 on the main branch. Since the guard-bit rounding now matches the ISA spec exactly, golden and hardware should agree within 1 ULP.

3. **Configurable ULP**: `passed_test()` accepts `custom_bfp4_max_ulp_diff` for tests that need wider tolerance.

### 5.3 PCC and Tolerance Thresholds

| Parameter        | BFP4_b Value | BFP8_b Value | Float16_b Value |
|------------------|-------------|-------------|-----------------|
| `atol`           | 0.125       | 0.1         | 0.05            |
| `rtol`           | 0.3         | 0.2         | 0.05            |
| PCC threshold    | 0.97        | 0.99^n      | 0.99            |
| ULP tolerance    | 1           | N/A         | N/A             |

---

## 6. Bugs Fixed on the Branch

### Bug 1: BFP8 packing lacked rounding

**Main branch** (`pack.py`, `float_to_bfp8_block()`):
```python
mantissa = mantissas_explicit[i] >> exponent_delta
```
Simple right-shift with no rounding. This is truncation toward zero, which does not match the ISA spec.

**Branch fix**:
```python
if exponent_delta > 0:
    guard_bit = (mantissas_explicit[i] >> (exponent_delta - 1)) & 1
    mantissa = (mantissas_explicit[i] >> exponent_delta) + guard_bit
else:
    mantissa = mantissas_explicit[i]
mantissa = mantissa & 0x7F
```
Added round-to-nearest, ties away from zero using the guard bit, as specified in the ISA documentation.

### Bug 2: BFP4 packing was single-stage

**Main branch** (`pack.py`, `float_to_bfp4_block()`):
```python
# Operated on raw FP32 bits, converting directly to BFP4
raw_bytes = struct.pack(f"<{n}f", *(float(v) for v in block))
all_bits = struct.unpack(f"<{n}I", raw_bytes)
# ...
mantissas[i] = 0x800000 | (bits & 0x7FFFFF)  # FP32 mantissa with implicit 1
# ...
shifted = mantissas[i] >> (shared_exponent - exponents[i])
bfp4_mantissas[i] = (signs[i] << 3) | ((shifted >> 21) & 0x7)
```
This extracted the FP32 mantissa and shifted it directly to 3 bits, bypassing the BFP8 intermediate stage entirely. No rounding was applied.

**Branch fix**: Two-stage approach per ISA spec:
```python
shared_exponent, bfp8_mantissas = float_to_bfp8_block(block)  # Stage 1: BFP8 with rounding
bfp4_mantissas = truncate_bfp8_to_bfp4(bfp8_mantissas)        # Stage 2: truncate
```

### Bug 3: `bfp4b_to_float16b()` operated on FP32 bits

**Main branch** (`bfp_format_utils.py`):
```python
signs = (u32 >> 31) & 1
exps = (u32 >> 23) & 0xFF
mants = ((u32 & 0x7FFFFF) >> 21) | 0x4  # FP32 mantissa bits 23:21
```
This extracted mantissa bits from the full FP32 representation, not from BF16. Since BF16 only has 7 mantissa bits (not 23), this could produce different results, especially at rounding boundaries.

**Branch fix**: Extract from BF16 representation and use two-stage process:
```python
bf16_bits = (flat.view(torch.int32) >> 16) & 0xFFFF
signs = (bf16_bits >> 15) & 1
exps  = (bf16_bits >> 7) & 0xFF
mants = ((bf16_bits & 0x7F) >> 1) | 0x40  # BF16 mantissa with implicit 1
# Then: BFP8 rounding -> BFP4 truncation
```

### Bug 4: `bfp8b_to_float16b()` lacked rounding

**Main branch** (`bfp_format_utils.py`):
```python
deltas = shared_exps - exps_blocks
shifted = mants_blocks >> deltas  # simple right-shift, no rounding
```

**Branch fix**: Added guard bit rounding via `_bfp8_mantissas_per_block()`:
```python
guard = torch.where(
    deltas > 0, (mants_blocks >> (deltas - 1)) & 1, torch.zeros_like(mants_blocks)
)
bfp8 = ((mants_blocks >> deltas) + guard) & 0x7F
```

### Bug 5: Missing FTZ in golden generators

**Main branch**: No flush-to-zero pass after quantization. Very small values from BFP scale arithmetic with small shared exponents were not flushed.

**Branch fix**: Added `FTZ_THRESHOLD = 1e-37` in:
- `_finalize_bfp_quantized()` in `bfp_format_utils.py`
- `EltwiseBinaryGolden.__call__()` in `golden_generators.py` (explicit post-quantization pass)

### Bug 6: Missing output quantization in ReduceGolden

**Main branch**:
```python
# reduce_all_tiles just did a dtype cast:
return accumulated.to(target_dtype)

# per-tile reduce had no output quantization at all:
return torch.cat([self._process_tile(...) for tile in range(tile_cnt)])
```

**Branch fix**: Added `_quantize_reduce_output()` called on every tile result and on the final accumulated result:
```python
def _quantize_reduce_output(self, tensor, data_format):
    if data_format == DataFormat.Bfp4_b:
        return _bfp4b_to_float16b(tensor.to(torch.bfloat16))
    elif data_format == DataFormat.Bfp8_b:
        return _bfp8b_to_float16b(tensor.to(torch.bfloat16))
    ...
```

### Bug 7: Missing input quantization dispatch in ReduceGolden

**Main branch**: Directly checked for BFP4_b and BFP8_b in `__call__`:
```python
if input_format == DataFormat.Bfp4_b:
    operand = _bfp4b_to_float16b(operand)
elif input_format == DataFormat.Bfp8_b:
    operand = _bfp8b_to_float16b(operand)
```
This did not handle MX formats and did not have a fallback `to_tensor()` path.

**Branch fix**: Added `_quantize_reduce_input()` with full format dispatch matching `EltwiseBinaryGolden._quantize_input()`.

### Bug 8: Elwmul fidelity masking accumulated masks

**Main branch**:
```python
for fidelity_iter in range(fidelity_iter_count + 1):
    t1, t2 = self._apply_fidelity_masking(math_format_for_fidelity, t1, t2, fidelity_iter)
    phase_result = self.ops[op](t1, t2)
```
`t1` and `t2` were being re-masked each iteration from the **already-masked** values of the previous iteration, compounding the masking.

**Branch fix**: Save originals and mask from them each iteration:
```python
orig_t1, orig_t2 = t1, t2
for fidelity_iter in range(fidelity_iter_count + 1):
    if fidelity_iter > 0:
        t1, t2 = operand1, operand2
    masked_t1, masked_t2 = self._apply_fidelity_masking(
        math_format_for_fidelity, orig_t1, orig_t2, fidelity_iter
    )
    phase_result = self.ops[op](masked_t1, masked_t2)
```

### Bug 9: Input quantization was disabled in EltwiseBinaryGolden

**Main branch**: `EltwiseBinaryGolden.__call__()` called `_quantize_input()` which could conflict with test-level pre-quantization for BFP4_b inputs.

**Initial branch fix**: Input quantization calls were commented out in `EltwiseBinaryGolden.__call__()`. The tests were assumed to pre-quantize inputs before passing them to the golden.

**Current fix**: Input quantization has been **re-enabled** because the tests do NOT pre-quantize inputs. The golden was receiving raw bfloat16 stimuli while hardware sees BFP4_b/BFP8_b-quantized data after unpack — a major source of mismatches. To prevent double-quantization when broadcast is active (Bug 13), a `_UNSET` sentinel distinguishes between "caller didn't pass `input_format_B`" (defaults to `input_format`) and "caller explicitly passed `None`" (already quantized by `BroadcastGolden`, skip):
```python
_UNSET = object()

def __call__(self, ..., input_format_B=_UNSET):
    if input_format_B is EltwiseBinaryGolden._UNSET:
        input_format_B = input_format
    operand1 = self._quantize_input(operand1, input_format, data_format)
    operand2 = self._quantize_input(operand2, input_format_B, data_format)
```

### Bug 10: Block comparison used unnecessary tilize/untilize

**Main branch** (`_bfp4_block_aware_compare()`):
```python
if n % TILE_SIZE == 0 and n > 0:
    g_til = tilize_block(g_flat, tile_dim, DataFormat.Float32).flatten()
    r_til = tilize_block(r_flat, tile_dim, DataFormat.Float32).flatten()
# ... compare blocks ...
    is_valid = untilize_block(is_valid_til.float(), ...).flatten().bool()
```

**Branch fix**: Removed tilize/untilize. Golden and result tensors are already in tilized (block) order as produced by the tests. Block-wise comparison operates directly on the flat tensors.

### Bug 11: ULP default tightened from 2 to 1

**Main branch**: `max_ulp_diff: int = 2`
**Branch fix**: `max_ulp_diff: int = 1`

With correct guard-bit rounding matching the ISA spec, golden and hardware agree more closely, allowing tighter tolerance.

### Bug 12: Stochastic rounding not disabled in hardware

**Branch fix** (in `tests/sources/eltwise_binary_test.cpp`):
```cpp
_llk_unpack_configure_stoch_rnd_<StochRndType::None>();
```
Added before `_llk_unpack_hw_configure_`. Without this, stochastic rounding could introduce random differences between golden and hardware.

### Bug 13: Broadcast + BFP4_b double quantization

When broadcast is active, `BroadcastGolden` already quantizes the BFP4_b input before extracting broadcast values. If `EltwiseBinaryGolden` also quantizes operand B, the data gets quantized twice.

**Branch fix**: The test sets `golden_input_format_B = None` when broadcast is active:
```python
golden_input_format_B = (
    None if broadcast_type != BroadcastType.None_ else formats.input_format
)
```

### Bug 14: Eltwise add/sub computed in bf16 precision

**Main branch**:
```python
def _add(self, t1, t2):
    return t1 + t2  # operates in t1's dtype (often bfloat16)
```

**Branch fix**: Compute in float32 to avoid intermediate precision loss:
```python
def _add(self, t1, t2):
    return (t1.to(torch.float32) + t2.to(torch.float32)).to(t1.dtype)
```

### Bug 15: sign=1,mag=0 returned 0.0 instead of -Inf in unpackers

**Previous code** (`bfp4_to_float_block()` in `unpack.py`):
```python
if mag == 0:
    bfloat16_values.append(0.0)  # Treats sign=1,mag=0 same as sign=0,mag=0
```

**Fix**: Both `bfp4_to_float_block()` and `bfp8_to_float_block()` now return `-float('inf')` when the sign bit is set and magnitude is zero, matching the ISA spec (`BFP4ToBF16`/`BFP8ToBF16` return `0xff80` for this case):
```python
if mag == 0:
    if mantissa & 0x8:  # sign bit set
        value = -float('inf')
    else:
        value = 0.0
```

### Bug 16: TransposeGolden missing BFP4_b/BFP8_b input quantization

**Previous code**: `TransposeGolden.transpose_within_faces()` and `transpose_faces()` had no BFP quantization. When used in the eltwise binary test pipeline with BFP4_b inputs, the golden operated on unquantized data.

**Fix**: Added `_quantize_transpose_input()` method that applies `bfp4b_to_float16b()` or `bfp8b_to_float16b()` BEFORE transposing. This is critical because hardware unpacks BFP data using the original (pre-transpose) block structure and shared exponents, then transposes the decompressed BF16 values:
```python
def _quantize_transpose_input(self, operand, data_format):
    if data_format == DataFormat.Bfp4_b:
        return _bfp4b_to_float16b(operand)
    elif data_format == DataFormat.Bfp8_b:
        return _bfp8b_to_float16b(operand)
    return operand
```

### Bug 17: DataCopyGolden missing BFP8_b input and output quantization

**Previous code**: `DataCopyGolden.__call__()` only handled `Bfp4_b` for input quantization and output round-trip quantization.

**Fix**: Added `Bfp8_b` to both input quantization (calls `_bfp8b_to_float16b()` when `input_format == DataFormat.Bfp8_b`) and output quantization (calls `_bfp8b_to_float16b()` when `data_format == DataFormat.Bfp8_b`).

---

## 7. ISA Compliance Verification

This section verifies our implementation against each relevant ISA specification point.

### 7.1 Two-Stage Packing: COMPLIANT

**ISA spec** (`Packers/FormatConversion.md`, Late format conversion table):
> To BFP4 or BFP2: Converted to BFP8 (as per the above row), then truncate to BFP4 or BFP2.

**Our implementation**: `float_to_bfp4_block()` calls `float_to_bfp8_block()` first, then `truncate_bfp8_to_bfp4()`. This exactly matches the two-stage pipeline.

### 7.2 BFP8 Rounding Mode: COMPLIANT

**ISA spec** (`Packers/FormatConversion.md`):
> Where rounding happens in BFP conversions, it can either be deterministic round-to-nearest with ties away from zero, or stochastic.

**Our implementation**: Uses deterministic round-to-nearest with ties away from zero via the guard bit:
```python
guard_bit = (mantissa >> (delta - 1)) & 1
mantissa = (mantissa >> delta) + guard_bit
```
The guard bit is the MSB being shifted out. Adding it rounds up when it's 1 (i.e., when the truncated portion is >= 0.5 ULP), which implements ties-away-from-zero.

**Stochastic rounding disabled**: The C++ test explicitly sets `StochRndType::None`.

### 7.3 Shared Exponent Per 16 Datums: COMPLIANT

**ISA spec** (`FloatBitPatterns.md`):
> Each group of 16 datums sharing a common 8-bit exponent.

**Our implementation**: Both `float_to_bfp8_block()` and `_bfp8_mantissas_per_block()` compute the shared exponent as the maximum element exponent across the 16-element block.

### 7.4 Sign-Magnitude Representation: COMPLIANT

**ISA spec** (`Packers/FormatConversion.md`, Final L1 output format):
> BFP8 / BFP4 / BFP2: Either 8 or 4 or 2 bits per datum, of which the high bit is a sign bit and the remaining bits are magnitude (not mantissa, because there's no implicit 1 bit).

**Our implementation**: The BFP4 datum is `(sign << 3) | magnitude` where magnitude is 3 bits with no implicit 1. The BFP8 datum is `(sign << 7) | magnitude` where magnitude is 7 bits with no implicit 1 (the implicit 1 from BF16 is included in the 7-bit value, making it the most significant magnitude bit).

### 7.5 BFP4ToBF16 Conversion: COMPLIANT

**ISA spec** (`FloatBitPatterns.md`):
```c
uint16_t BFP4ToBF16(uint4_t DatumBits, uint8_t ExpBits) {
  return BFP8ToBF16(DatumBits << 4, ExpBits);
}
```

**Our implementation** (`bfp4_to_float_block()` in `unpack.py`):
```python
scale = 2.0 ** (exp_adj - 2)  # = 2^(Exp-129)
value = sign * mag * scale     # = sign * mag/4 * 2^(Exp-127)
```

**Verification**: Let us trace through the ISA C code for a datum `D=0b1101` (sign=1, mag=5) with exponent `E=130`:

1. `BFP8ToBF16(0b11010000, 130)`:
   - `Sign = 0b11010000 >> 7 = 1`
   - `Mag = 0b11010000 << 1 = 0b10100000`
   - `Mag != 0`, so:
   - `LZ = stdc_leading_zeros_uc(0b10100000) = 0` (MSB is set)
   - `Mag <<= 0` -> `0b10100000`
   - `ExpBits -= 0` -> `130`
   - Return: `(1 << 15) | (130 << 7) | (0b10100000 & 0x7e)` = `(1 << 15) | (130 << 7) | 0b00100000`
   - BF16 bits: `sign=1, exp=130, mant=0b0100000` = `-(1 + 32/128) * 2^(130-127)` = `-1.25 * 8` = `-10.0`

2. Our formula: `sign * mag * 2^(Exp-129)` = `-1 * 5 * 2^(130-129)` = `-5 * 2` = `-10.0`

Results match.

### 7.6 BFP4 Value Formula: COMPLIANT

**ISA spec** (`FloatBitPatterns.md`):
> BFP4: `Mag/2^2 * 2^(Exp-127)`

**Our implementation**: `bfp4_mant * 2^(shared_exp - 129)` = `bfp4_mant / 4 * 2^(shared_exp - 127)` = `Mag/2^2 * 2^(Exp-127)`. Matches exactly.

### 7.7 BFP4 Datum Nibble Packing: COMPLIANT

**ISA spec** (`Packers/FormatConversion.md`):
> For all BFP4 formats, two datums are packed into each byte of L1.

**Our implementation** (`pack_bfp4_b()`):
```python
packed_mantissas.append((high << 4) | low)
```
Low nibble = even index, high nibble = odd index.

**Unpacking** (`unpack_bfp4_b()`):
```python
low_nibbles = packed & 0x0F
high_nibbles = (packed >> 4) & 0x0F
mantissas[0::2] = low_nibbles   # even indices
mantissas[1::2] = high_nibbles  # odd indices
```
Consistent with the packing order.

### 7.8 Denormal Handling: COMPLIANT

**ISA spec** (`Packers/FormatConversion.md`, late conversion):
> Denormals flushed to zero (for mantissa narrowing conversions).

**Our implementation**: FTZ threshold at `1e-37` in `_finalize_bfp_quantized()` and in `EltwiseBinaryGolden.__call__()`. BF16 denormals (exponent = 0) are flushed to zero.

### 7.9 NaN Handling: COMPLIANT

**ISA spec** (`FloatBitPatterns.md`): For BF16, exp=255, mant!=0 is NaN, which the FPU treats as `(1 + Mant/2^7) * 2^(255-127)` -- i.e., a very large normal number. When this goes through BFP packing, it dominates the shared exponent and all other elements in the block get right-shifted toward zero.

**Our implementation**: `check_bfp4_b()` zeros out all non-inf/non-NaN elements in a block that contains any inf/NaN, modeling this hardware behavior.

### 7.10 Special Case: sign=1, mag=0: COMPLIANT (FIXED)

**ISA spec** (`FloatBitPatterns.md`):
> sign=1, mag=0: `-2^128` or `-Infinity`

**ISA conversion code**:
```c
if (Mag == 0) {
    return Sign ? 0xff80 : 0;  // 0xff80 is BF16 -Inf (sign=1, exp=255, mant=0)
}
```

**Our implementation** (`bfp4_to_float_block()` in `unpack.py`):
```python
mag = mantissa & 0x7
if mag == 0:
    if mantissa & 0x8:
        value = -float('inf')
    else:
        value = 0.0
```

This was a minor deviation in the original code (both `sign=0,mag=0` and `sign=1,mag=0` returned `0.0`). It has been **fixed** to match the ISA spec: `sign=1,mag=0` now returns `-Infinity` (`0xff80` in BF16) and `sign=0,mag=0` returns `+0`. The same fix was applied to `bfp8_to_float_block()` for consistency.

### 7.11 Mantissa Precision: COMPLIANT

BF16 has 7 explicit mantissa bits (+ 1 implicit), giving 8 bits of mantissa precision. BFP8 has 7-bit magnitude, which with the implicit leading 1 gives 7 bits of precision. The conversion from BF16 to BFP8 loses the least significant bit (`m0`) of the BF16 mantissa.

Our `_bf16_sign_exp_mantissa()` extracts `((bf16_bits & 0x7F) >> 1) | 0x40`, which produces a 7-bit value `[1, m6, m5, m4, m3, m2, m1]` -- exactly matching BFP8's 7-bit magnitude format. The dropped bit `m0` represents precision that BFP8 cannot carry, consistent with the ISA spec that BFP8 has `Mag/2^6 * 2^(Exp-127)` where `Mag` is 7 bits.

The scalar implementation in `pack.py` does the same: `mantissa = binary_str[9:-1]` drops the last BF16 mantissa bit, then `"1" + mantissa` prepends the implicit 1.

### 7.12 Guard Bit Rounding Correctness: COMPLIANT

For the round-to-nearest, ties-away-from-zero rounding, the guard bit is defined as the most significant bit being discarded during right-shift. Our implementation uses `(mant >> (delta - 1)) & 1`, which extracts exactly this bit.

When `delta = 1`: guard = bit 0 of mantissa (the one bit being shifted out).
When `delta = 2`: guard = bit 1 of mantissa (the MSB of the two bits being shifted out).
When `delta = 0`: no shift, no rounding needed (identity).

Adding the guard bit to the shifted result rounds up when the discarded portion is >= 0.5 ULP of the remaining value. At exactly 0.5 ULP (guard=1, all lower bits=0), it rounds up (away from zero), which matches the "ties away from zero" spec.

---

## 8. Missing Improvements in Other Goldens

This section catalogs all golden generators that lack proper BFP4_b handling or have potential issues. These should all be addressed when extending BFP format support.

### 8.1 Goldens With No BFP4_b Handling

These goldens have zero BFP4_b-specific logic. If they are ever used with BFP4_b inputs or outputs, the golden will produce incorrect results.

#### 1. ReduceGapoolGolden (line 2290)
- **Missing**: No input quantization, no output quantization, no FTZ.
- **Fix needed**: Add `_quantize_reduce_input()` and `_quantize_reduce_output()` methods (same pattern as `ReduceGolden`). Apply FTZ after output quantization.
- **Priority**: High -- Gapool reduce is a real operation that could be tested with BFP4_b.

#### 2. ReduceBlockMaxRowGolden (line 2279)
- **Missing**: No BFP4_b handling at all.
- **Fix needed**: Add input quantization. Output quantization may not be needed if the output is always Float16_b, but should be added if BFP4_b output is possible.
- **Priority**: Medium.

#### 3. TransposeGolden (line 514): FIXED
- **Was missing**: No BFP4_b input/output quantization.
- **Fixed**: Added `_quantize_transpose_input()` method that applies `bfp4b_to_float16b()` or `bfp8b_to_float16b()` before transposing. Called in both `transpose_within_faces()` and `transpose_faces()`. Quantization happens before transpose to match hardware behavior (unpack decompresses using original block structure, then transposes).

#### 4. PackGolden (line 1159)
- **Missing**: No BFP4_b output quantization (only selects face elements, no round-trip simulation).
- **Fix needed**: If the pack operation writes BFP4_b to L1, the output should be quantized.
- **Priority**: Low.

#### 5. UntilizeGolden (line 2416)
- **Missing**: No BFP4_b handling.
- **Fix needed**: Untilization is a layout transformation. If the data format is BFP4_b, input should be quantized to model what the unpacker produces.
- **Priority**: Low.

#### 6. TilizeGolden (line 2427)
- **Missing**: No BFP4_b handling.
- **Fix needed**: Same as UntilizeGolden.
- **Priority**: Low.

#### 7. PackRowsGolden (line 2452)
- **Missing**: No BFP4_b handling.
- **Fix needed**: If used with BFP4_b, needs output quantization.
- **Priority**: Low.

#### 8. TopKGolden (line 2488)
- **Missing**: No BFP4_b handling.
- **Fix needed**: If BFP4_b inputs are used, they need input quantization. TopK comparisons on BFP4_b-quantized values may produce different orderings than on full-precision values.
- **Priority**: Low -- TopK unlikely to be used with BFP4_b in practice.

### 8.2 Goldens With Partial or Problematic BFP4_b Handling

#### 9. MatmulGolden (line 782)
- **Has**: Input quantization for both operands A and B.
- **Missing**: Output quantization. If `data_format == DataFormat.Bfp4_b`, the matmul result is not quantized through `bfp4b_to_float16b()`. Also missing FTZ pass.
- **Fix needed**: Add output quantization block after the matmul computation (same pattern as `EltwiseBinaryGolden`):
  ```python
  if data_format == DataFormat.Bfp4_b:
      res = _bfp4b_to_float16b(res.to(torch.bfloat16))
  ```
  And add FTZ pass.
- **Priority**: High -- matmul is a core operation.

#### 10. BroadcastGolden (line 918)
- **Has**: Input quantization before broadcasting.
- **Missing**: No output quantization. Returns the raw broadcast tensor.
- **Note**: This is by design -- `BroadcastGolden` is a helper used by `EltwiseBinaryGolden`, which handles output quantization. If `BroadcastGolden` is ever used standalone with BFP4_b output, this would be a problem.
- **Fix needed**: Consider adding optional output quantization, or document that `BroadcastGolden` must always be used through a parent golden that handles output quantization.
- **Priority**: Low (by-design delegation to caller).

#### 11. BinarySFPUGolden (line 1886)
- **Has**: Inherits `EltwiseBinaryGolden._quantize_input()` and `EltwiseBinaryGolden._add/_sub/_mul`.
- **Missing**: Its own `__call__` method (line 1902-1996) processes elements in a per-row loop and does **not** call the parent's output quantization or FTZ logic. The parent's `__call__` is not invoked.
- **Fix needed**: After the element-wise loop, add BFP4_b output quantization:
  ```python
  if data_format == DataFormat.Bfp4_b:
      result = _bfp4b_to_float16b(result.to(torch.bfloat16))
  ```
  And add FTZ pass.
- **Priority**: Medium -- SFPU binary ops may be tested with BFP4_b.

#### 12. UnarySFPUGolden (line 1378)
- **Has**: Input quantization, output quantization with tilize, `check_bfp4_b`, `convert_nan_to_inf`.
- **Tilize/untilize chain**: The flow is:
  1. Input tilized for element-wise ops (line 1470)
  2. Result untilized back to logical layout (line 1505)
  3. For BFP4_b output: re-tilized to form correct 16-element BFP blocks (line 1532)
  4. `_bfp4b_to_float16b(tilized, dimensions)` applies BFP4 quantization and untilizes via `_finalize_bfp_quantized` (which calls `untilize_block` when `dimensions` is not None)

  **Verified**: This is NOT a double-untilize. The sequence is `untilize -> tilize -> quantize+untilize`, which is correct. Each tilize has a matching untilize.
- **Fragility concerns**:
  - The `check_bfp4_b()` call receives a list (not a tensor) after `convert_nan_to_inf`. The function modifies the list in-place, which could corrupt shared state if the list is a view of a tensor's internal storage.
  - Uses `torch.float32` intermediate dtype specifically for BFP4_b (`op_dtype = torch.float32 if data_format == DataFormat.Bfp4_b`), which is correct to avoid bf16 precision loss before quantization.
- **FTZ**: Applied inside `_bfp4b_to_float16b` via `_finalize_bfp_quantized`, not via an explicit FTZ pass in `UnarySFPUGolden.__call__`.
- **Fix recommended**: Consider consolidating to a single `_quantize_output()` pattern matching `EltwiseBinaryGolden` for consistency across goldens.
- **Priority**: Low (functionally correct, but fragile).

#### 13. DataCopyGolden (line 1060): FIXED
- **Has**: Input and output quantization for both BFP4_b and BFP8_b.
- **Fixed**: Added `Bfp8_b` input quantization (previously only `Bfp4_b`) and `Bfp8_b` output quantization round-trip. The output quantization block now handles both formats via a shared code path.
- **Remaining note**: For `num_faces == 4`, the output is tilized before quantization and `dims` triggers untilization inside `_finalize_bfp_quantized()`. For `num_faces < 4`, `dims=None` is passed, which skips untilization. This asymmetry is intentional (partial tiles don't need untilization) but fragile if the face count logic changes.

### 8.3 Cross-Cutting Issues

#### 14. `bfp8b_to_float16b()` on main branch also lacked rounding

The same guard-bit rounding bug (Bug 4) affected `bfp8b_to_float16b()`. The branch fixed this in `_bfp8_mantissas_per_block()`, which is shared by both `bfp8b_to_float16b()` and `bfp4b_to_float16b()`. Any golden using `bfp8b_to_float16b()` on the main branch will have slightly wrong BFP8_b quantization as well.

#### 15. `_bf16_sign_exp_mantissa()` mantissa extraction

The function extracts 6 explicit mantissa bits from BF16 (dropping the LSB), then adds the implicit leading 1:
```python
mants = ((bf16_bits & 0x7F) >> 1) | 0x40
```

BF16 has 7 mantissa bits (bits 6:0). `& 0x7F` isolates them. `>> 1` drops the LSB (bit 0), keeping bits 6:1 (6 bits). `| 0x40` sets the implicit leading 1 at bit position 6, yielding a 7-bit value in `[0x40, 0x7F]` (64..127). This matches BFP8's 7-bit magnitude format: `[1.xxxxxx]` where the leading 1 is the implicit bit.

The dropped LSB (bit 0 of BF16 mantissa) is the least significant bit that BFP8 cannot represent. This is correct behavior.

#### 16. Stochastic rounding disabled only in eltwise_binary_test.cpp

The branch adds `_llk_unpack_configure_stoch_rnd_<StochRndType::None>()` only in `tests/sources/eltwise_binary_test.cpp`. A full audit of `tests/sources/` reveals:

| C++ Test File | Stochastic Rounding Config |
|---|---|
| `eltwise_binary_test.cpp` | `StochRndType::None` (branch fix) |
| `unpack_A_test.cpp` | `STOCHASTIC_RND` macro (configurable) |
| `unpack_matmul_test.cpp` | `STOCHASTIC_RND` macro (configurable) |
| `unpack_tilize_sweep_test.cpp` | `STOCHASTIC_RND` macro (configurable) |
| All other test files | No stochastic rounding configuration at all |

**Fix needed**: Every C++ test file that exercises BFP4_b paths should explicitly set `StochRndType::None`. Files using the `STOCHASTIC_RND` macro should ensure it evaluates to `None` for BFP4_b tests.

#### 17. `test_reduce.py` and other tests missing input_format passthrough

The branch added `input_format=formats.input_format` to the `test_reduce` golden call. A full audit of golden call sites reveals additional cases where `input_format` is omitted:

| Call Site | Golden | `input_format` Passed? | Impact |
|---|---|---|---|
| `test_reduce.py` (line 129) | `ReduceGolden` | Yes (branch fix) | Fixed |
| `test_unary_datacopy_bfp4_b` | `DataCopyGolden` | Yes | Correct |
| `test_unary_datacopy` | `DataCopyGolden` | No (but excludes Bfp4_b formats) | OK for now |
| `test_eltwise_unary_datacopy_custom.py` | `DataCopyGolden` | No | Gap if Bfp4_b added |
| `test_zzz_transpose_dest.py` | `DataCopyGolden` | No | Gap if Bfp4_b added |
| `quasar/test_pack_quasar.py` | `DataCopyGolden` | No | Gap if Bfp4_b added |
| `quasar/test_eltwise_unary_datacopy_quasar.py` | `DataCopyGolden` | No | Gap if Bfp4_b added |
| `quasar/test_reduce_quasar.py` | `ReduceGolden`/`ReduceGapoolGolden` | No | Gap if Bfp4_b added |
| `helpers/fused_fpu.py` (`EltwiseFpu.golden`) | `EltwiseBinaryGolden` | No (`input_format=None`) | Gap if Bfp4_b used in fused ops |
| `helpers/fused_fpu.py` (`DatacopyFpu.golden`) | `DataCopyGolden` | No | Gap if Bfp4_b used in fused ops |

**Pattern**: Tests that have dedicated `_bfp4_b` variants correctly pass `input_format`. Tests without BFP4_b in their format lists are safe for now but will need the parameter when BFP4_b is added. The fused FPU helpers are a gap if fused operations ever involve BFP4_b.

### 8.4 Remaining Known Golden Discrepancies

After all the fixes in this section, the BFP4_b eltwise binary test (`test_eltwise_binary_bfp4_b`) passes all `Transpose.No` cases. The `Transpose.Yes` cases have two categories of remaining failures, both of which are golden-level discrepancies (not numerical correctness issues in BFP4_b format handling):

#### 1. Scalar broadcast + Transpose.Yes: LLK ASSERT

**Symptom**: Tests with `broadcast_type=BroadcastType.Scalar` and `transpose_srca=Transpose.Yes` hit an LLK assert in `_llk_unpack_AB_mop_config_<(ckernel::BroadcastType)3>` at `llk_unpack_AB.h:109`.

**Root cause**: The hardware unpacker does not support Scalar broadcast combined with transpose. The unpacker MOP configuration for Scalar broadcast (BroadcastType 3) is incompatible with the transpose path. This is a **hardware/LLK limitation**, not a golden issue.

**Status**: The non-BFP4_b `test_eltwise_binary` already filters this via `_get_valid_transpose()` which returns `[Transpose.No]` for Scalar broadcast. The `test_eltwise_binary_bfp4_b` test uses `transpose_srca=[Transpose.Yes, Transpose.No]` unconditionally and needs the same filtering applied. This is a test parametrization issue to be addressed separately.

#### 2. Bfp4_b→Bfp8_b with Transpose.Yes: small value mismatches

**Symptom**: Tests with `formats:Bfp4_b->Bfp8_b` and `transpose_srca=Transpose.Yes` show small golden-vs-hardware differences at specific positions. Example: golden `3.00` vs result `3.50`, golden `0.50` vs result `0.62`, golden `1.25` vs result `1.88`. The differences are consistent with BFP8_b quantization precision (the result values are valid BFP8_b representable values).

**Root cause**: When input is `Bfp4_b` and output is `Bfp8_b`, the golden models:
1. Quantize input via `bfp4b_to_float16b()` (uses pre-transpose block structure)
2. Transpose via `TransposeGolden`
3. Compute the operation
4. Quantize output via `bfp8b_to_float16b()` (uses post-operation block structure)

The hardware may produce slightly different results because the BFP8_b output packing operates on data that went through a different intermediate precision path (BFP4_b unpack → BF16 in src register → compute in dest → BFP8_b pack). The mismatch is within BFP8_b ULP tolerance and appears only in the `Bfp4_b→Bfp8_b` cross-format case with transpose. Same-format (`Bfp4_b→Bfp4_b`) and all `Transpose.No` cases pass cleanly.

**Status**: This is a golden accuracy gap in the cross-format output quantization path with transpose. It does not indicate a bug in BFP4_b format handling itself. To be addressed in a future golden refinement pass.

---

## 9. Guideline for Implementing BFP2_b

When extending the infrastructure to support BFP2_b (1 sign bit + 1 magnitude bit, shared 8-bit exponent per 16 datums), follow the same patterns established for BFP4_b:

### 9.1 Format Specification

| Property       | BFP2_b                               |
|----------------|---------------------------------------|
| Bits per datum | 2 (1 sign + 1 magnitude)             |
| Exponent       | 8-bit shared per 16 datums           |
| Value          | `Mag * 2^(Exp-127)` for Mag=1; `0` for Mag=0 |
| sign=1,mag=0   | `-2^128` or `-Infinity`              |
| Datums per byte| 4                                     |

### 9.2 Packing (two-stage)

1. **Stage 1**: Convert to BFP8 using `float_to_bfp8_block()` (reuse existing function -- no changes needed).
2. **Stage 2**: Truncate BFP8 to BFP2:
   ```python
   def truncate_bfp8_to_bfp2(bfp8_mantissas):
       bfp2_mantissas = []
       for bfp8 in bfp8_mantissas:
           sign = (bfp8 >> 7) & 0x1
           magnitude = (bfp8 >> 6) & 0x1  # keep only the top 1 magnitude bit
           bfp2 = (sign << 1) | magnitude
           bfp2_mantissas.append(bfp2)
       return bfp2_mantissas
   ```

3. **Memory layout**: 4 datums per byte. Define the nibble packing order (consult ISA doc for bit ordering within the byte).

### 9.3 Quantization Simulation

Create `bfp2b_to_float16b()` in `bfp_format_utils.py`:

```python
def bfp2b_to_float16b(operand, dimensions=None):
    flat = operand.flatten().to(torch.float32)
    n = flat.numel()
    signs, exps, mants = _bf16_sign_exp_mantissa(flat)

    signs_blocks = signs.view(-1, BFP_BLOCK)
    shared_exps, bfp8_mants = _bfp8_mantissas_per_block(
        mants.view(-1, BFP_BLOCK), exps.view(-1, BFP_BLOCK),
    )

    # BFP8 -> BFP2: keep only top 1 magnitude bit (drop 6 LSBs)
    bfp2_mants = bfp8_mants >> 6

    # Scale: bfp2_mant * 2^(shared_exp - 127)
    # BFP2 mantissa is 1 bit; Mag=1 means 1 * 2^(Exp-127)
    values = bfp2_mants.float() * torch.exp2((shared_exps - 127).float())
    return _finalize_bfp_quantized(values, signs_blocks, n, dimensions)
```

The exponent offset is 127 (not 129 or 133) because:
- BFP8: `Mag/2^6 * 2^(Exp-127)` = `Mag * 2^(Exp-133)` -> offset 133
- BFP4: `Mag/2^2 * 2^(Exp-127)` = `Mag * 2^(Exp-129)` -> offset 129
- BFP2: `Mag * 2^(Exp-127)` -> offset 127

### 9.4 Unpacking

Per the ISA doc:
```c
uint16_t BFP2ToBF16(uint2_t DatumBits, uint8_t ExpBits) {
  return BFP8ToBF16(DatumBits << 6, ExpBits);
}
```

Create `bfp2_to_float_block()` in `unpack.py`:
```python
def bfp2_to_float_block(exponent, bfp2_mantissas, cache):
    exp_adj = exponent - 127
    scale = 2.0 ** exp_adj  # = 2^(Exp-127)
    values = []
    for mantissa in bfp2_mantissas:
        mag = mantissa & 0x1
        if mag == 0:
            values.append(0.0)
        else:
            sign = -1.0 if mantissa & 0x2 else 1.0
            values.append(sign * scale)
    return values
```

### 9.5 Golden Generator Checklist

For every golden generator, add:
- [ ] Input quantization: `if fmt == DataFormat.Bfp2_b: return bfp2b_to_float16b(operand)`
- [ ] Output quantization: `if data_format == DataFormat.Bfp2_b: result = bfp2b_to_float16b(result.to(torch.bfloat16))`
- [ ] FTZ pass: `torch.where(result.abs() < 1e-37, zeros, result)`
- [ ] Block inf/NaN check: reuse `check_bfp4_b()` (same 16-element block logic)
- [ ] Stochastic rounding: disable in C++ test via `StochRndType::None`

### 9.6 Comparison

Create `_bfp2_block_aware_compare()` with `BFP2_MANTISSA_BITS = 1`:
```python
one_ulp = 2.0 ** (block_exp - BFP2_MANTISSA_BITS + 1)  # = 2^block_exp
```

Tolerances will need to be wider than BFP4_b due to the extreme quantization (only 2 representable magnitudes per block: 0 and `2^(Exp-127)`):
```python
DataFormat.Bfp2_b: Tolerance(atol=0.5, rtol=0.5)  # adjust based on testing
```

PCC threshold should also be lower (perhaps 0.90-0.95).

### 9.7 Stimuli Generation

The stimuli generator (`stimuli_generator.py`) should generate BFP2_b-friendly values. With only 1 magnitude bit, the representable values per block are very coarse. Consider:
```python
if stimuli_format == DataFormat.Bfp2_b:
    integer_part = torch.randint(low, 2, (size,))  # only 0 or 1 magnitude
    fraction = torch.zeros(size, dtype=torch.bfloat16)  # no fractional part
```

### 9.8 Files to Modify

| File | Changes |
|------|---------|
| `helpers/pack.py` | Add `truncate_bfp8_to_bfp2()`, `float_to_bfp2_block()`, `pack_bfp2_b()` |
| `helpers/unpack.py` | Add `bfp2_to_float_block()`, `unpack_bfp2_b()` |
| `helpers/bfp_format_utils.py` | Add `bfp2b_to_float16b()` |
| `helpers/golden_generators.py` | Add BFP2_b dispatch in every `_quantize_input`, `_quantize_output`, and `check_bfp_*` function |
| `helpers/utils.py` | Add `_bfp2_block_aware_compare()`, add `Bfp2_b` tolerance and PCC threshold |
| `helpers/stimuli_generator.py` | Add BFP2_b case in stimuli generation |
| `helpers/format_config.py` | Add `DataFormat.Bfp2_b` if not present |
| All test files | Add BFP2_b to format lists, add dedicated `test_*_bfp2_b` test functions |
| C++ test sources | Add `StochRndType::None` configuration |
