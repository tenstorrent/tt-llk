# Specification: fill -- Phase 2 (int_fill)

## Kernel Type
sfpu

## Target Architecture
quasar

## Overview
Based on analysis: `codegen/artifacts/fill_analysis.md`

Phase 2 implements the integer fill sub-kernel (`_calculate_fill_int_`), which writes an integer constant value to every element in destination register tiles using an explicit SFPSTORE format mode. Unlike the float fill (Phase 1), which uses implied format and loads via FP16_B mode, the integer fill must:
1. Select the correct SFPLOADI mode to load the integer value (UINT16 zero-extend for 16-bit values, or two partial writes for 32-bit values).
2. Use an explicit SFPSTORE format mode (`sfpmem::INT32`, `sfpmem::UINT16`, or `sfpmem::UINT8`) rather than the implied default.

This phase appends to the file created in Phase 1. The existing `_calculate_fill_` and `_calculate_fill_sfp_rows_` functions must NOT be modified.

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_fill.h`

## Reference Implementations Studied

- `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_fill.h`: Blackhole's `_calculate_fill_int_<APPROX, INSTRUCTION_MODE, ITERATIONS>` -- uses `_sfpu_load_imm32_` for INT32 mode and `_sfpu_load_imm16_` for LO16 mode, then loops with `TTI_SFPSTORE(LREG1, INSTRUCTION_MODE, ADDR_MOD_3, 0)`.
- `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_load_config.h`: Defines `_sfpu_load_imm32_()` (two SFPLOADI calls: LO16_ONLY then HI16_ONLY) and `_sfpu_load_imm16_()` (one SFPLOADI with UINT16 mode).
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_typecast_int32_fp32.h`: Demonstrates `p_sfpu::sfpmem::INT32` usage for SFPLOAD on Quasar. Confirms `INT32 = 0b0100 = 4`.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_typecast_fp16b_uint16.h`: Demonstrates `p_sfpu::sfpmem::UINT16` usage for SFPSTORE on Quasar. Confirms `UINT16 = 0b0110 = 6`.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_abs.h`: Standard Quasar SFPU iteration pattern (loop + `_incr_counters_`).
- `tt_llk_quasar/common/inc/ckernel_instr_params.h` (lines 322-333): Defines all `p_sfpu::sfpmem::*` constants -- `INT32 = 0b0100`, `UINT8 = 0b0101`, `UINT16 = 0b0110`.

## Algorithm in Target Architecture

### Pseudocode

**`_calculate_fill_int_(const int iterations, const std::uint32_t value, const std::uint32_t store_mode)`**:
1. Load the integer constant into LREG1 based on `store_mode`:
   - If `store_mode == p_sfpu::sfpmem::INT32` (32-bit integer):
     - `TTI_SFPLOADI(LREG1, 10, value & 0xFFFF)` -- write lower 16 bits (LO16_ONLY mode)
     - `TTI_SFPLOADI(LREG1, 8, (value >> 16) & 0xFFFF)` -- write upper 16 bits (HI16_ONLY mode)
   - Else (16-bit or 8-bit integer: `UINT16` or `UINT8` store modes):
     - `TTI_SFPLOADI(LREG1, 2, value & 0xFFFF)` -- UINT16 zero-extend mode
2. Loop `iterations` times:
   a. `TTI_SFPSTORE(LREG1, store_mode, ADDR_MOD_7, 0, 0)` -- store with explicit integer format
   b. `_incr_counters_<0, 0, SFP_ROWS, 0>()` -- advance Dest pointer

### Design Rationale

**Why runtime `store_mode` instead of template**: Blackhole uses `InstrModLoadStore` as a template parameter for compile-time dispatch. This enum does NOT exist on Quasar. On Quasar, the SFPSTORE instr_mod0 is just a `std::uint32_t` passed directly. The Quasar convention (observed in all existing kernels) uses runtime parameters, not templates for SFPU functions. Using a runtime `store_mode` parameter follows this convention.

**Why `if` on `store_mode` for load logic**: The 32-bit load path (two SFPLOADI calls for HI16_ONLY + LO16_ONLY) differs from the 16/8-bit load path (single SFPLOADI UINT16 mode). The store mode tells us which path to use: `sfpmem::INT32` requires the 32-bit load; all others use the 16-bit load.

**Why LREG1**: Consistent with Phase 1's float fill and the Blackhole reference. LREG0 is conventionally used as scratch in existing Quasar kernels; LREG1 avoids collisions.

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|--------------------|-------------------|-----------------|
| `_sfpu_load_imm32_(LREG1, value)` (load 32-bit int) | `TTI_SFPLOADI(p_sfpu::LREG1, 10, value & 0xFFFF)` then `TTI_SFPLOADI(p_sfpu::LREG1, 8, (value >> 16) & 0xFFFF)` | `ckernel_sfpu_load_config.h` lines 17-21 (Blackhole helper) + `cmath_common.h` `_sfpu_load_config32_` (Quasar uses same SFPLOADI modes 10/8) |
| `_sfpu_load_imm16_(LREG1, value)` (load 16-bit uint) | `TTI_SFPLOADI(p_sfpu::LREG1, 2, value & 0xFFFF)` | `ckernel_sfpu_load_config.h` line 25 (Blackhole helper) -- instr_mod0=2 = UINT16 zero-extend mode |
| `TTI_SFPSTORE(LREG1, INSTRUCTION_MODE, ADDR_MOD_3, 0)` (BH 4-arg) | `TTI_SFPSTORE(p_sfpu::LREG1, store_mode, ADDR_MOD_7, 0, 0)` (QS 5-arg) | `ckernel_sfpu_typecast_int32_fp32.h` line 22 (Quasar SFPSTORE pattern) |
| `InstrModLoadStore::INT32` (= 4) | `p_sfpu::sfpmem::INT32` (= 0b0100 = 4) | `ckernel_instr_params.h` line 329 |
| `InstrModLoadStore::LO16` (= 6) | `p_sfpu::sfpmem::UINT16` (= 0b0110 = 6) | `ckernel_instr_params.h` line 331 |
| `InstrModLoadStore::INT8` (= 5) | `p_sfpu::sfpmem::UINT8` (= 0b0101 = 5) | `ckernel_instr_params.h` line 330 |
| `sfpi::dst_reg++` | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` | `ckernel_sfpu_abs.h` line 33 |

### Resource Allocation

| Resource | Purpose |
|----------|---------|
| `p_sfpu::LREG1` | Holds the integer fill constant (loaded before the loop) |
| `ADDR_MOD_7` | Address modifier for SFPSTORE, configured to {.srca=0, .srcb=0, .dest=0} |
| Dest RWC counter | Advanced by SFP_ROWS (2) per iteration via `_incr_counters_` |

## Implementation Structure

### Includes
No additional includes needed beyond what Phase 1 already provides:
```cpp
#include <cstdint>
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```

### Namespace
Same as Phase 1 -- append within the existing `namespace ckernel { namespace sfpu {` block.

### Functions

| Function | Template Params | Parameters | Purpose |
|----------|-----------------|------------|---------|
| `_calculate_fill_int_(const int iterations, const std::uint32_t value, const std::uint32_t store_mode)` | none | `iterations`: number of row-pairs to fill; `value`: integer constant; `store_mode`: SFPSTORE format mode | Load integer constant into LREG1, store to all Dest rows with explicit format |

**Note on `sfpu_fill_int` wrapper**: The analysis lists `sfpu_fill_int` as a "top-level wrapper" for Phase 2. However, on Quasar there is no separate wrapper function pattern for SFPU kernels -- the `_calculate_*_` functions are called directly via `_llk_math_eltwise_unary_sfpu_params_<false>(ckernel::sfpu::_calculate_fill_int_, ...)`. There is no need for an additional `sfpu_fill_int` wrapper. The `_calculate_fill_int_` function itself IS the callable entry point.

### Init/Uninit Symmetry
Not applicable -- the integer fill variant has no init/uninit pair. It operates within the standard SFPU execution framework.

## Instruction Sequence

### Main Function: `_calculate_fill_int_`

```
// Load the integer constant into LREG1
if (store_mode == p_sfpu::sfpmem::INT32) {
    // 32-bit integer: load in two halves
    TTI_SFPLOADI(p_sfpu::LREG1, 10, (value & 0xFFFF))         // LO16_ONLY: write lower 16 bits
    TTI_SFPLOADI(p_sfpu::LREG1, 8, ((value >> 16) & 0xFFFF))  // HI16_ONLY: write upper 16 bits
} else {
    // 16-bit or 8-bit integer: load as UINT16 (zero-extended to 32 bits)
    TTI_SFPLOADI(p_sfpu::LREG1, 2, (value & 0xFFFF))          // UINT16: zero-extend to UINT32
}

// Store to all Dest rows
#pragma GCC unroll 8
for (int d = 0; d < iterations; d++) {
    TTI_SFPSTORE(p_sfpu::LREG1, store_mode, ADDR_MOD_7, 0, 0)  // Store with explicit integer format
    _incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()                 // Advance Dest pointer
}
```

**SFPLOADI mode details** (from Confluence page 1170505767):
- Mode 10 (0xA = LO16_ONLY): Writes only the lower 16 bits of the LREG, preserving the upper 16 bits. Used as the first step of a 32-bit load.
- Mode 8 (0x8 = HI16_ONLY): Writes only the upper 16 bits of the LREG, preserving the lower 16 bits. Used as the second step of a 32-bit load.
- Mode 2 (0x2 = UINT16): Zero-extends the 16-bit immediate to a 32-bit unsigned integer in the LREG. Suitable for 16-bit and 8-bit integer values.

**SFPSTORE mode details** (from assembly.yaml SFPU_MEM instr_mod0 field):
- INT32 (0b0100 = 4): Store LREG as sign+magnitude 32-bit integer to Dest, using 32-bit addressing.
- UINT16 (0b0110 = 6): Store lower 16 bits of LREG as unsigned 16-bit integer to Dest, using 16-bit addressing.
- UINT8 (0b0101 = 5): Store lowest 8 bits of LREG as unsigned 8-bit integer to Dest, using 16-bit addressing.

## Calling Convention

The function will be called from the test harness via `_llk_math_eltwise_unary_sfpu_params_`:
```cpp
// For Int32 format:
_llk_math_eltwise_unary_sfpu_params_<false>(
    ckernel::sfpu::_calculate_fill_int_,
    tile_index,
    num_sfpu_iterations,
    fill_value_as_uint32,       // The 32-bit integer value to fill
    p_sfpu::sfpmem::INT32       // store_mode = 4
);

// For Int16/UInt16 format:
_llk_math_eltwise_unary_sfpu_params_<false>(
    ckernel::sfpu::_calculate_fill_int_,
    tile_index,
    num_sfpu_iterations,
    fill_value_as_uint32,       // Value (lower 16 bits used)
    p_sfpu::sfpmem::UINT16      // store_mode = 6
);

// For Int8/UInt8 format:
_llk_math_eltwise_unary_sfpu_params_<false>(
    ckernel::sfpu::_calculate_fill_int_,
    tile_index,
    num_sfpu_iterations,
    fill_value_as_uint32,       // Value (lower 8 bits used)
    p_sfpu::sfpmem::UINT8       // store_mode = 5
);
```

## Writer Instructions

### Code Placement
Append the `_calculate_fill_int_` function AFTER the existing `_calculate_fill_` function but BEFORE the closing namespace braces. The file currently ends with:
```cpp
} // namespace sfpu
} // namespace ckernel
```
Insert the new function between `_calculate_fill_` and these closing braces.

### Do NOT Modify
- The `_calculate_fill_sfp_rows_()` function (Phase 1)
- The `_calculate_fill_()` function (Phase 1)
- The includes, copyright header, or pragma once (Phase 1)

## Potential Issues

1. **INT32 sign+magnitude**: The SFPSTORE with INT32 mode stores in sign+magnitude format, not two's complement. The caller/test must be aware that the value parameter for negative integers must already be in sign+magnitude representation as expected by the hardware, OR the value should be loaded as a raw bit pattern. For the fill test with a simple positive integer constant, this is not an issue.

2. **UINT8 truncation**: SFPSTORE with UINT8 mode takes the lowest 8 bits of the LREG value. If the fill value exceeds 255, it will be silently truncated. The test must use values within the valid range for each format.

3. **No inner `_sfp_rows_` function**: Unlike Phase 1's float fill which has a separate `_calculate_fill_sfp_rows_()` for the per-row operation, the integer fill's per-row operation is a single `TTI_SFPSTORE` call. Adding a separate inner function for one instruction would add unnecessary indirection. The store is inlined directly in the loop body.

4. **SFPSTORE does not round**: Per the architecture research (Confluence page 1170505767), SFPSTORE does NOT perform rounding or saturation when converting to 8/16-bit formats. For faithful integer fill, the caller should ensure the value fits in the target format's range. `SFP_STOCH_RND` is available for rounding but is not needed for fill since we are writing a known constant.

## Recommended Test Formats

Based on the analysis's "Format Support" section (universal domain) and the architecture research's "Format Support Matrix". Phase 2 tests ONLY the integer fill variant.

### Format List
```python
# Phase 2: Integer fill only -- integer formats that use _calculate_fill_int_
FORMATS = input_output_formats([
    DataFormat.Int32,
    DataFormat.Int16,
])
```

**Rationale for Phase 2 format selection**:
- `Int32` tests the 32-bit integer path (two SFPLOADI calls + `sfpmem::INT32` store mode).
- `Int16` tests the 16-bit integer path (single SFPLOADI UINT16 mode + `sfpmem::UINT16` store mode).
- `Int8` and `UInt8` are omitted from Phase 2 because they require `sfpmem::UINT8` store mode and may need special sign+magnitude handling for Int8. They share the same SFPLOADI path as Int16 (UINT16 mode), so the risk profile is similar. They can be added in regression testing after the core paths are validated.

### Invalid Combination Rules

```python
def _is_invalid_quasar_combination(fmt: FormatConfig, dest_acc: DestAccumulation) -> bool:
    in_fmt = fmt.input_format
    out_fmt = fmt.output_format

    # Integer and float formats cannot be mixed
    if in_fmt.is_integer() != out_fmt.is_integer():
        return True

    # Int32 requires dest_acc=Yes (32-bit Dest)
    if in_fmt == DataFormat.Int32 and dest_acc == DestAccumulation.No:
        return True
    if out_fmt == DataFormat.Int32 and dest_acc == DestAccumulation.No:
        return True

    # Non-integer-to-integer and integer-to-non-integer are invalid
    # (handled by the first rule above)

    return False
```

**Key rules**:
- Integer formats must not be mixed with float formats in input->output.
- `Int32` requires `dest_acc=Yes` because it needs 32-bit Dest mode.
- `Int16` works with both `dest_acc=No` and `dest_acc=Yes`.
- `implied_math_format` should be `ImpliedMathFormat.No` for integer formats (the implied format mechanism is for float formats).

### MX Format Handling
Not applicable for Phase 2. MX formats use the float fill variant (Phase 1).

### Integer Format Handling
- **Input preparation**: The test must generate integer-range values. For `Int32`, values should be within the signed 32-bit range. For `Int16`, values should be within the unsigned 16-bit range (0 to 65535) or signed 16-bit range depending on the format.
- **Golden generator**: Must use integer arithmetic. The golden output is a tile where every element equals the fill constant value (as an integer).
- **Fill constant for integer test**: Use a simple integer value like `42` (instead of `5.0f` for float). The test C++ harness should pass this as `std::uint32_t`. For Int32, the full 32 bits are used. For Int16, only the lower 16 bits.
- **`format_dict` mapping**: Verify that `helpers/llk_params.py` maps `DataFormat.Int32` and `DataFormat.Int16` to appropriate torch dtypes for golden comparison. Int32 maps to `torch.int32`, Int16 maps to `torch.int16`.

## Testing Notes

1. **Separate test dispatch**: The phase test's C++ harness must dispatch to `_calculate_fill_int_` based on the math format. The pattern from the Blackhole `sfpu_operations.h` provides guidance:
   - If `math_format == Int32`: call `_calculate_fill_int_(iterations, value, p_sfpu::sfpmem::INT32)`
   - If `math_format == Int16`: call `_calculate_fill_int_(iterations, value, p_sfpu::sfpmem::UINT16)`
   - If `math_format == UInt8` or `Int8`: call `_calculate_fill_int_(iterations, value, p_sfpu::sfpmem::UINT8)`

2. **Value encoding**: Unlike float fill where the value is an FP16_b encoding in the upper 16 bits, integer fill takes the raw integer value directly. For Int32, the full uint32 is the value. For Int16, the lower 16 bits of the uint32 parameter are the value.

3. **Verification**: Compare every element in the output tile against the expected fill constant (in the output integer format). The comparison should be exact for integers -- no tolerance needed.

4. **`is_int_fpu_en`**: The test harness sets `is_int_fpu_en = false` (inherited from Phase 1). For integer SFPU operations, this flag may need to be set differently. Check existing integer typecast tests for the correct setting. Based on `ckernel_sfpu_typecast_int32_fp32.h` tests, `is_int_fpu_en = false` is correct for SFPU-only integer operations.
