# Specification: fill -- Phase 3 (bitcast_fill)

## Kernel Type
sfpu

## Target Architecture
quasar

## Overview
Based on analysis: `codegen/artifacts/fill_analysis.md`

Phase 3 implements the bitcast fill sub-kernel (`_calculate_fill_bitcast_`), which writes an arbitrary 32-bit bit pattern to every element in destination register tiles, treating the result as a float via implied format store. Unlike the float fill (Phase 1), which interprets the value as FP16_b (loading only the upper 16 bits), the bitcast fill loads the full 32-bit value using two SFPLOADI operations (LO16_ONLY + HI16_ONLY) and stores with implied format (DEFAULT). This preserves all 32 bits of the bit pattern in the destination.

Use case: writing specific IEEE 754 bit patterns (e.g., NaN, infinity, denormals, or precise Float32 values that would lose precision through FP16_b conversion) directly into float tiles.

This phase appends to the file created in Phase 1 and extended in Phase 2. The existing `_calculate_fill_`, `_calculate_fill_sfp_rows_`, and `_calculate_fill_int_` functions must NOT be modified.

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_fill.h`

## Reference Implementations Studied

- `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_fill.h`: Blackhole's `_calculate_fill_bitcast_<APPROX, ITERATIONS>(uint32_t value_bit_mask)` -- uses `Converter::as_float(value_bit_mask)` to reinterpret the uint32 as a float, then assigns to `sfpi::vFloat` and stores via `sfpi::dst_reg[0]`. The semantics: load the full 32-bit pattern as-is and store it to Dest with implied format.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_fill.h` (current file): Phase 1's `_calculate_fill_` and Phase 2's `_calculate_fill_int_` provide the established patterns for this file. Phase 2's INT32 path (`TTI_SFPLOADI(LREG1, 10, lo16)` + `TTI_SFPLOADI(LREG1, 8, hi16)`) demonstrates the exact two-step 32-bit load needed here.
- `tt_llk_quasar/common/inc/cmath_common.h` (lines 115-116): `_sfpu_load_config32_` uses `TTI_SFPLOADI(LREG0, 10, lower16)` then `TTI_SFPLOADI(LREG0, 8, upper16)` -- confirming the mode 10 (LO16_ONLY) + mode 8 (HI16_ONLY) pattern for assembling a 32-bit value in an LREG.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_abs.h`: Standard iteration pattern with `_incr_counters_`.
- `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_load_config.h`: Blackhole's `_sfpu_load_imm32_` helper -- functionally identical to what the bitcast fill needs (two SFPLOADI calls: mode 10 for lower bits, mode 8 for upper bits).

## Algorithm in Target Architecture

### Pseudocode

**`_calculate_fill_bitcast_(const int iterations, const std::uint32_t value_bit_mask)`**:
1. Load the full 32-bit bit pattern into LREG1 via two partial writes:
   - `TTI_SFPLOADI(LREG1, 10, value_bit_mask & 0xFFFF)` -- LO16_ONLY: write lower 16 bits
   - `TTI_SFPLOADI(LREG1, 8, (value_bit_mask >> 16) & 0xFFFF)` -- HI16_ONLY: write upper 16 bits
2. Loop `iterations` times:
   a. `TTI_SFPSTORE(LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` -- store with implied format
   b. `_incr_counters_<0, 0, SFP_ROWS, 0>()` -- advance Dest pointer

### Design Rationale

**Why two SFPLOADI instead of one**: Phase 1's float fill uses a single `SFPLOADI(lreg, 0, value >> 16)` with FP16_B mode (instr_mod0=0), which interprets the 16-bit immediate as FP16_b and auto-converts to FP32 in the LREG. This loses the lower 16 bits of the original FP32 value. The bitcast fill needs all 32 bits preserved exactly, so it uses the LO16_ONLY + HI16_ONLY pair to write both halves of the LREG directly without any format conversion.

**Why DEFAULT (implied format) store, not FP32**: The bitcast fill writes a raw bit pattern that should be interpreted according to the current Dest configuration. Using `p_sfpu::sfpmem::DEFAULT` (0) lets the hardware interpret the stored value based on the Dest format settings -- the same pattern as Phase 1's float fill. This works because the LREG holds a 32-bit value that represents the desired bit pattern, and the implied format store maps it correctly to the Dest format.

**Difference from Phase 2's INT32 path**: Phase 2 uses the same two-SFPLOADI loading pattern but stores with `sfpmem::INT32` (sign+magnitude integer format). The bitcast fill stores with `sfpmem::DEFAULT` (implied float format). This is the key semantic difference: Phase 2 writes integer data, Phase 3 writes float data from a raw bit pattern.

**Why no inner `_sfp_rows_` function**: The per-row operation is a single `TTI_SFPSTORE` call. Following Phase 2's pattern, which also inlines the single-instruction store in the loop body, there is no need for a separate inner function.

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|--------------------|-------------------|-----------------|
| `Converter::as_float(value_bit_mask)` + `sfpi::vFloat fill_val = ...` (bitcast uint32 to float and load) | `TTI_SFPLOADI(p_sfpu::LREG1, 10, (value_bit_mask & 0xFFFF))` then `TTI_SFPLOADI(p_sfpu::LREG1, 8, ((value_bit_mask >> 16) & 0xFFFF))` | `ckernel_sfpu_fill.h` Phase 2 INT32 load path (lines 43-44); `cmath_common.h` `_sfpu_load_config32_` (lines 115-116); Blackhole `_sfpu_load_imm32_` |
| `sfpi::dst_reg[0] = fill_val` (store to dest with implied format) | `TTI_SFPSTORE(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` | `ckernel_sfpu_fill.h` Phase 1 `_calculate_fill_sfp_rows_` (line 18) |
| `sfpi::dst_reg++` (advance dest pointer) | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` | `ckernel_sfpu_abs.h` line 33, Phase 1 and Phase 2 of this file |
| `for d in ITERATIONS` (iteration loop) | `for (int d = 0; d < iterations; d++)` with `#pragma GCC unroll 8` | Standard Quasar SFPU pattern from abs.h, and Phase 1/2 of this file |

### Resource Allocation

| Resource | Purpose |
|----------|---------|
| `p_sfpu::LREG1` | Holds the 32-bit bit pattern (loaded once before the loop, in two 16-bit halves) |
| `ADDR_MOD_7` | Address modifier for SFPSTORE, configured to {.srca=0, .srcb=0, .dest=0} by `_eltwise_unary_sfpu_configure_addrmod_()` |
| Dest RWC counter | Advanced by SFP_ROWS (2) per iteration via `_incr_counters_` |

**Why LREG1**: Consistent with Phase 1's float fill and Phase 2's integer fill, which both use LREG1 for the fill constant. LREG0 is conventionally used as scratch in existing Quasar kernels.

## Implementation Structure

### Includes
No additional includes needed beyond what Phase 1 already provides:
```cpp
#include <cstdint>
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```

### Namespace
Same as Phase 1 and Phase 2 -- append within the existing `namespace ckernel { namespace sfpu {` block.

### Functions

| Function | Template Params | Parameters | Purpose |
|----------|-----------------|------------|---------|
| `_calculate_fill_bitcast_(const int iterations, const std::uint32_t value_bit_mask)` | none | `iterations`: number of row-pairs to fill; `value_bit_mask`: raw 32-bit bit pattern to write | Load raw 32-bit value into LREG1, store to all Dest rows with implied format |

### Init/Uninit Symmetry
Not applicable -- the bitcast fill variant has no init/uninit pair. It operates within the standard SFPU execution framework.

## Instruction Sequence

### Main Function: `_calculate_fill_bitcast_`

```
// Load the full 32-bit bit pattern into LREG1 via two partial writes
TTI_SFPLOADI(p_sfpu::LREG1, 10, (value_bit_mask & 0xFFFF))         // LO16_ONLY: write lower 16 bits
TTI_SFPLOADI(p_sfpu::LREG1, 8, ((value_bit_mask >> 16) & 0xFFFF))  // HI16_ONLY: write upper 16 bits

// Store to all Dest rows with implied format
#pragma GCC unroll 8
for (int d = 0; d < iterations; d++) {
    TTI_SFPSTORE(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)  // Implied format store
    _incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()                               // Advance Dest pointer
}
```

**SFPLOADI mode details** (from Confluence page 1170505767):
- Mode 10 (0xA = LO16_ONLY): Writes only the lower 16 bits of the LREG, preserving the upper 16 bits. Treats the 16-bit immediate as raw bits (no format conversion).
- Mode 8 (0x8 = HI16_ONLY): Writes only the upper 16 bits of the LREG, preserving the lower 16 bits. Treats the 16-bit immediate as raw bits (no format conversion).

**Order of LO16_ONLY then HI16_ONLY**: Following the Blackhole `_sfpu_load_imm32_` convention and the Quasar `_sfpu_load_config32_` convention. LO16_ONLY is first because it does not affect the upper 16 bits; HI16_ONLY is second because it does not affect the lower 16 bits. Both orderings produce the same result since neither mode affects the other half, but LO-then-HI matches established convention.

**SFPSTORE DEFAULT mode** (from `ckernel_instr_params.h` line 324):
- `p_sfpu::sfpmem::DEFAULT = 0b0000`: Format is determined by the combination of SrcB exponent width in ALU_FORMAT_SPEC_REG and ACC_CTRL_SFPU_Fp32. This stores the LREG value in whatever format the Dest register file is currently configured for.

## Calling Convention

The function will be called from the test harness via `_llk_math_eltwise_unary_sfpu_params_`:
```cpp
// Fill with a specific 32-bit float bit pattern (e.g., 3.14159f = 0x40490FDB)
constexpr std::uint32_t fill_value_bits = 0x40490FDBu;  // IEEE 754 float32 for pi

_llk_math_eltwise_unary_sfpu_params_<false>(
    ckernel::sfpu::_calculate_fill_bitcast_,
    tile_index,
    num_sfpu_iterations,
    fill_value_bits  // Raw 32-bit bit pattern
);
```

The `_llk_math_eltwise_unary_sfpu_params_` wrapper iterates over 4 faces, calling the function for each face. Each call processes `num_sfpu_iterations` row-pairs (typically 8 for a 16-row face, since SFP_ROWS=2).

## Writer Instructions

### Code Placement
Append the `_calculate_fill_bitcast_` function AFTER the existing `_calculate_fill_int_` function but BEFORE the closing namespace braces. The file currently ends with:
```cpp
} // namespace sfpu
} // namespace ckernel
```
Insert the new function between `_calculate_fill_int_` and these closing braces.

### Do NOT Modify
- The `_calculate_fill_sfp_rows_()` function (Phase 1)
- The `_calculate_fill_()` function (Phase 1)
- The `_calculate_fill_int_()` function (Phase 2)
- The includes, copyright header, or pragma once (Phase 1)

## Potential Issues

1. **LREG initial state for LO16_ONLY**: The LO16_ONLY SFPLOADI mode (instr_mod0=10) preserves the upper 16 bits of the LREG. Since LREG1 may contain residual data from previous operations, we must write both halves. The HI16_ONLY write (mode 8) will overwrite the upper half. As long as both SFPLOADI calls execute (they always do -- unconditional), the final LREG1 value will be exactly `value_bit_mask`. This is safe.

2. **Implied format and 32-bit values**: When Dest is in 16-bit mode (FP16_b), SFPSTORE with DEFAULT format will convert the 32-bit LREG value to 16 bits, potentially losing precision from the lower mantissa bits. This is expected behavior -- the bitcast fill writes the full 32-bit pattern, and the Dest format determines how it is stored. For Float32 Dest (dest_acc=Yes), all 32 bits are preserved.

3. **Relationship to Phase 1 float fill**: For values that are exactly representable in FP16_b, Phase 1's `_calculate_fill_` and Phase 3's `_calculate_fill_bitcast_` produce the same result. The bitcast variant is useful when:
   - The value needs full FP32 precision (e.g., pi = 0x40490FDB has mantissa bits that FP16_b would truncate)
   - The value is a special IEEE 754 pattern (NaN, infinity, specific denormals)
   - The caller already has the value as a uint32 bit pattern rather than an FP16_b encoding

4. **No SFPNOP needed between SFPLOADI calls**: SFPLOADI has 1-cycle latency and the two writes target different halves of the same LREG. There is no WAR hazard on LREGs across consecutive instructions (architecture research Section 6). The two SFPLOADI calls can be issued back-to-back.

## Recommended Test Formats

Based on the analysis's "Format Support" section and the architecture research's "Format Support Matrix". Phase 3 tests the bitcast fill variant, which stores with implied format -- applicable to float formats.

### Format List
```python
# Phase 3: Bitcast fill -- float formats that use _calculate_fill_bitcast_ with implied format store
FORMATS = input_output_formats([
    DataFormat.Float16_b,
    DataFormat.Float32,
])
```

**Rationale for Phase 3 format selection**:
- `Float16_b`: Primary consumer -- the most common Dest format for float SFPU operations. The bitcast fill writes a 32-bit pattern that gets stored according to implied format (truncated to FP16_b precision in Dest).
- `Float32`: Tests full 32-bit precision preservation. With `dest_acc=Yes`, all 32 bits of the bit pattern are stored in Dest. This is the key format where bitcast fill differs from Phase 1's float fill.
- `Float16` is omitted because bitcast fill's primary value is full-precision 32-bit writes. Float16 (FP16_a) is rarely used as a Dest format and the implied format behavior would be the same as Float16_b (16-bit truncation). Can be added in regression.
- MX formats (`MxFp8R`, `MxFp8P`) are omitted for the same reason as Phase 1 -- they add test infrastructure complexity and the Dest representation is Float16_b, making them equivalent to Float16_b from the SFPU's perspective.
- Integer formats use `_calculate_fill_int_` (Phase 2), not `_calculate_fill_bitcast_`.

### Invalid Combination Rules

```python
def _is_invalid_quasar_combination(fmt: FormatConfig, dest_acc: DestAccumulation) -> bool:
    in_fmt = fmt.input_format
    out_fmt = fmt.output_format

    # Quasar packer does not support non-Float32 to Float32 conversion when dest_acc=No
    if (in_fmt != DataFormat.Float32
        and out_fmt == DataFormat.Float32
        and dest_acc == DestAccumulation.No):
        return True

    # Float32 input -> Float16 output requires dest_acc=Yes on Quasar
    if (in_fmt == DataFormat.Float32
        and out_fmt == DataFormat.Float16
        and dest_acc == DestAccumulation.No):
        return True

    return False
```

These rules are identical to Phase 1's rules, since bitcast fill has the same packer format constraints as float fill (both use implied format store to float Dest).

### MX Format Handling
Not applicable for Phase 3. MX formats are deferred to regression.

### Integer Format Handling
Not applicable for Phase 3. Integer formats use `_calculate_fill_int_` (Phase 2).

## Testing Notes

1. **Test value selection**: Choose a Float32 value with full mantissa precision that would lose bits through FP16_b conversion. Good test values:
   - `0x40490FDB` (pi = 3.14159265...) -- has mantissa bits beyond FP16_b precision
   - `0x40A00000` (5.0f) -- exactly representable in FP16_b, useful as a sanity check
   The test should use a value like pi to verify that the bitcast path preserves bits that the float fill path would lose.

2. **Golden generator**: The golden output should interpret the bit pattern according to the output format:
   - For Float32 output: the exact IEEE 754 float value of the bit pattern
   - For Float16_b output: the bit pattern converted from Float32 to FP16_b (truncation of lower mantissa bits)

3. **Verification**: Compare every element in the output tile against the expected value. For Float32 output with a known bit pattern, the comparison should be exact (or within 1 ULP if the packer does rounding). For Float16_b output, the tolerance should account for the truncation from FP32 to FP16_b.

4. **Difference from Phase 1 float fill**: To demonstrate that bitcast fill provides value beyond float fill, the test could use a value where `(float)(BF16(value))` differs from `bitcast_to_float(value)`. For example:
   - Float fill with 0x40490FDB: loads `(0x40490FDB >> 16) = 0x4049` as FP16_b, which converts to `0x40490000` in FP32 (pi truncated to ~3.140625)
   - Bitcast fill with 0x40490FDB: loads all 32 bits, so the Dest gets `0x40490FDB` (pi = 3.14159265...)
   The difference is visible in Float32 Dest mode.

5. **Test harness dispatch**: The phase test's C++ harness calls `_calculate_fill_bitcast_` similarly to Phase 1's `_calculate_fill_`:
   ```cpp
   _llk_math_eltwise_unary_sfpu_params_<false>(
       ckernel::sfpu::_calculate_fill_bitcast_,
       tile_index,
       num_sfpu_iterations,
       fill_value_bits  // Raw uint32 bit pattern
   );
   ```
