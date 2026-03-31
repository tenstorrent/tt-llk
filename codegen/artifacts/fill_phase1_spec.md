# Specification: fill — Phase 1 (float_fill)

## Kernel Type
sfpu

## Target Architecture
quasar

## Overview
Based on analysis: `codegen/artifacts/fill_analysis.md`

Phase 1 implements the float fill sub-kernel (`_calculate_fill_`), which writes a floating-point constant value to every element in destination register tiles. This is the simplest fill variant: load an FP16_b immediate into an LREG, then store it to every Dest row with implied format, advancing the Dest pointer after each iteration.

This phase also establishes the file infrastructure (includes, namespace, copyright header).

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_fill.h`

## Reference Implementations Studied

- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_abs.h`: Standard Quasar SFPU kernel structure -- includes, namespace, `_sfp_rows_()` inner + `_calculate_*_(const int iterations)` outer pattern, `#pragma GCC unroll 8`, `_incr_counters_` loop, ADDR_MOD_7 usage.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_lrelu.h`: Loading a float constant via `TTI_SFPLOADI(lreg, 0 /*Float16_b*/, (value >> 16))` -- the exact pattern needed for fill's constant loading. Demonstrates passing `const std::uint32_t` parameter through the outer function.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_typecast_int32_fp32.h`: Demonstrates `p_sfpu::sfpmem::INT32` and `p_sfpu::sfpmem::FP32` SFPSTORE modes.
- `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_fill.h`: Reference semantics -- what the fill kernel does algorithmically.

## Algorithm in Target Architecture

### Pseudocode

**`_calculate_fill_sfp_rows_()`** (per-iteration inner function):
1. Store the fill value from LREG1 to Dest using implied format: `TTI_SFPSTORE(LREG1, DEFAULT, ADDR_MOD_7, 0, 0)`

**`_calculate_fill_(const int iterations, const std::uint32_t value)`** (outer function):
1. Load the float constant into LREG1: `TTI_SFPLOADI(LREG1, 0 /*FP16_B*/, (value >> 16))`
   - `value` is a uint32 with the FP16_b representation in the upper 16 bits (the standard Quasar convention for float params, as seen in lrelu.h)
   - `instr_mod0=0` means FP16_B mode, which auto-converts the FP16_b immediate to FP32 in the LREG
2. Loop `iterations` times:
   a. Call `_calculate_fill_sfp_rows_()`
   b. Increment Dest counter: `_incr_counters_<0, 0, SFP_ROWS, 0>()`

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|--------------------|-------------------|-----------------|
| `sfpi::vFloat fill_val = value` (load float constant) | `TTI_SFPLOADI(p_sfpu::LREG1, 0, (value >> 16))` | `ckernel_sfpu_lrelu.h` line 33 (identical pattern) |
| `sfpi::dst_reg[0] = fill_val` (store to dest) | `TTI_SFPSTORE(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` | `ckernel_sfpu_abs.h` line 23 (same SFPSTORE pattern with DEFAULT format) |
| `sfpi::dst_reg++` (advance dest pointer) | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` | `ckernel_sfpu_abs.h` line 33 (standard pattern) |
| `for d in ITERATIONS` (iteration loop) | `for (int d = 0; d < iterations; d++)` with `#pragma GCC unroll 8` | `ckernel_sfpu_abs.h` line 29-30 |

### Resource Allocation

| Resource | Purpose |
|----------|---------|
| `p_sfpu::LREG1` | Holds the fill value constant (loaded once before the loop) |
| `ADDR_MOD_7` | Address modifier for SFPSTORE, configured to {.srca=0, .srcb=0, .dest=0} by `_eltwise_unary_sfpu_configure_addrmod_()` |
| Dest RWC counter | Advanced by SFP_ROWS (2) per iteration via `_incr_counters_` |

**Why LREG1**: LREG0 is conventionally used as a scratch/load register in existing Quasar kernels. Using LREG1 for the constant keeps it consistent with the Blackhole reference (which uses LREG1 for `_calculate_fill_int_`) and avoids any accidental overwrite if future phases' inner functions use LREG0.

## Implementation Structure

### Includes
```cpp
#include <cstdint>

#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```
- `<cstdint>` for `std::uint32_t` parameter type (pattern from `ckernel_sfpu_lrelu.h`)
- `ckernel_trisc_common.h` for TTI_* macros
- `cmath_common.h` for `_incr_counters_`, `SFP_ROWS`, `p_sfpu` constants

### Namespace
```cpp
namespace ckernel
{
namespace sfpu
{
// ... functions ...
} // namespace sfpu
} // namespace ckernel
```
(C++11-style nested namespaces, matching all existing Quasar SFPU kernels)

### Functions

| Function | Template Params | Parameters | Purpose |
|----------|-----------------|------------|---------|
| `_calculate_fill_sfp_rows_()` | none | none | Store fill value (LREG1) to current Dest rows |
| `_calculate_fill_(const int iterations, const std::uint32_t value)` | none | `iterations`: number of row-pairs to fill; `value`: float constant as uint32 with FP16_b in upper 16 bits | Load constant, loop over iterations storing to all Dest rows |

**Note**: No init/uninit functions are needed for the float fill variant. The fill kernel does not modify any hardware state beyond what the standard SFPU setup provides. The `_eltwise_unary_sfpu_configure_addrmod_()` call in `_llk_math_eltwise_unary_sfpu_init_()` configures ADDR_MOD_7, which is all the init this kernel needs.

### Init/Uninit Symmetry
Not applicable -- this phase has no init/uninit pair. The float fill variant operates purely within the standard SFPU execution framework set up by `_llk_math_eltwise_unary_sfpu_init_()`.

## Instruction Sequence

### Inner Function: `_calculate_fill_sfp_rows_()`
```
TTI_SFPSTORE(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)
```
One instruction: store the pre-loaded LREG1 value to Dest using implied format. The implied format writes data according to the current Dest format configuration (set by the unpacker/math layer above).

**No SFPLOAD needed**: Unlike read-modify-write kernels (abs, relu), fill does not read from Dest. It writes a constant directly.

### Outer Function: `_calculate_fill_(const int iterations, const std::uint32_t value)`
```
TTI_SFPLOADI(p_sfpu::LREG1, 0 /*FP16_B mode*/, (value >> 16))
#pragma GCC unroll 8
for (int d = 0; d < iterations; d++) {
    _calculate_fill_sfp_rows_()
    _incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()
}
```

## Calling Convention

The function will be called via `_llk_math_eltwise_unary_sfpu_params_`:
```cpp
_llk_math_eltwise_unary_sfpu_params_<false>(
    ckernel::sfpu::_calculate_fill_,
    tile_index,
    num_sfpu_iterations,
    fill_value_as_uint32  // FP16_b representation in upper 16 bits
);
```

The `_llk_math_eltwise_unary_sfpu_params_` wrapper iterates over 4 faces, calling the function for each face. Each call processes `num_sfpu_iterations` row-pairs (typically 8 for a 16-row face, since SFP_ROWS=2).

## Writer Instructions

### File Creation
Create `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_fill.h` with:
1. Copyright header (SPDX, 2026)
2. `#pragma once`
3. Includes (see above)
4. Namespace block
5. `_calculate_fill_sfp_rows_()` function
6. `_calculate_fill_()` function

### Parent File Modification
Add `#include "sfpu/ckernel_sfpu_fill.h"` to `tt_llk_quasar/common/inc/ckernel_sfpu.h` (alphabetical order, after the exp include).

## Potential Issues

1. **Fill value encoding**: The caller must pass the float value as a uint32 with the FP16_b representation in the upper 16 bits. This is the standard Quasar convention (same as lrelu slope), but the test infrastructure must construct this encoding correctly.

2. **SFPLOADI FP16_B conversion precision**: SFPLOADI with instr_mod0=0 converts FP16_b to FP32 in the LREG. This means the fill value is limited to FP16_b precision (~7-bit mantissa, 8-bit exponent). For Float32 output with full precision, a different approach (bitcast fill or two SFPLOADI calls) would be needed -- but that is Phase 3's scope.

3. **Implied format store**: SFPSTORE with DEFAULT (0) stores data according to the register file's implied format. When Dest is in FP32 mode (dest_acc=Yes), this should correctly write the full FP32 value from the LREG. When Dest is in FP16_b mode, it converts the FP32 LREG value to FP16_b.

## Recommended Test Formats

Based on the analysis's "Format Support" section (universal domain, applicable to float and integer) and the architecture research's "Format Support Matrix". However, Phase 1 only tests the **float fill** variant, so only float formats should be tested.

### Format List
```python
# Phase 1: Float fill only -- float formats that use _calculate_fill_ with implied format store
FORMATS = input_output_formats([
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Float32,
])
```

**Rationale for Phase 1 format selection**:
- `Float16`, `Float16_b`, `Float32` are the core float formats that the float fill variant handles. The SFPLOADI(FP16_b) + SFPSTORE(DEFAULT) path covers all three.
- MX formats (`MxFp8R`, `MxFp8P`) are omitted from Phase 1 because they require `implied_math_format=Yes` configuration and the MX pack/unpack path adds test complexity. They can be added in a later phase or regression.
- Integer formats (`Int32`, `Int16`, `Int8`, `UInt8`) are handled by `_calculate_fill_int_` (Phase 2), not `_calculate_fill_`.

### Invalid Combination Rules
For `_is_invalid_quasar_combination()`:

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

These rules are identical to the abs test's rules, since fill has the same packer format constraints.

### MX Format Handling
Not applicable for Phase 1. MX formats are deferred to regression or a separate format-focused phase.

### Integer Format Handling
Not applicable for Phase 1. Integer formats use `_calculate_fill_int_` which is Phase 2.

## Testing Notes

1. **Fill is a write-only operation**: Unlike abs/relu which read-modify-write, fill ignores the input data and writes a constant. The test golden generator should produce a tile where every element equals the fill constant value, regardless of input.

2. **Input data for the test**: The test still needs valid input data to go through the unpack->datacopy->SFPU pipeline. The fill operation overwrites the data in Dest, so the input values do not matter -- but the format pipeline must be correctly configured.

3. **Value encoding in the test**: The fill constant must be passed as `std::uint32_t` with FP16_b representation in the upper 16 bits. For example, to fill with 5.0f: FP16_b(5.0) = 0x40A0, so the uint32 value = `0x40A00000`. The test C++ harness constructs this.

4. **Verification**: Compare every element in the output tile against the expected fill constant value (in the output format). Tolerance should be minimal since fill does no computation -- just format conversion.
