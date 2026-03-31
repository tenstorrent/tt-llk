# Specification: abs (Phase 1 — abs_core)

## Kernel Type
SFPU

## Target Architecture
quasar

## Overview
Based on analysis: `codegen/artifacts/abs_analysis.md`

Phase 1 implements the complete abs kernel for Quasar. The kernel computes element-wise absolute value by clearing the sign bit of floating-point data using the dedicated `SFPABS` hardware instruction. This is the simplest possible SFPU kernel — a single-instruction core operation with a standard load/compute/store wrapper.

There is only one phase because the abs kernel has exactly one logical sub-kernel: no init/uninit pair, no integer variant, and no approximation modes.

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_abs.h`

## Reference Implementations Studied

- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_relu.h`: Closest simple SFPU kernel. Established the two-function pattern (`_sfp_rows_` helper + `_calculate_` loop), ADDR_MOD_7 usage, SFPSTORE with `0` for instr_mod, no template parameters on the main function. Relu uses SFPNONLINEAR instead of SFPABS but the structural pattern is identical.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_square.h`: Confirmed the same two-function pattern. Uses `<bool APPROXIMATION_MODE>` template on the `_sfp_rows_` helper only (called with `<false>`). Uses `p_sfpu::sfpmem::DEFAULT` for SFPSTORE instr_mod (equivalent to `0`). Includes `ckernel_ops.h` explicitly.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_silu.h`: Confirmed pattern consistency. Uses `0` for SFPSTORE instr_mod. No templates.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_rsqrt.h`: Confirmed pattern consistency. Uses `p_sfpu::sfpmem::DEFAULT` for SFPSTORE instr_mod. No templates. Includes `ckernel_ops.h` explicitly.

## Algorithm in Target Architecture

### Pseudocode
For each iteration (processing SFP_ROWS=2 rows of data):
1. Load data from Dest register into LREG0 (auto-converts any float format to FP32)
2. Apply absolute value: clear the sign bit of the FP32 value in LREG0
3. Store result from LREG0 back to Dest register (auto-converts FP32 back to original format)
4. Advance Dest register pointer by SFP_ROWS (2 rows)

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|----|----|----|
| `sfpi::vFloat v = sfpi::dst_reg[0]` (load from Dest) | `TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` | All existing Quasar SFPU kernels (relu.h, square.h, rsqrt.h, silu.h) |
| `sfpi::abs(v)` (clear sign bit for FP32) | `TTI_SFPABS(p_sfpu::LREG0, p_sfpu::LREG0, 1)` | Confluence page 1612186129 (SFPABS ISA), assembly.yaml opcode 0x7D, ckernel_ops.h line 601 |
| `sfpi::dst_reg[0] = result` (store to Dest) | `TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0)` | relu.h, silu.h, sigmoid.h, exp.h (most common SFPSTORE pattern) |
| `sfpi::dst_reg++` (advance pointer) | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` | All existing Quasar SFPU kernels |

### SFPABS Parameter Details

`TTI_SFPABS(lreg_c, lreg_dest, instr_mod1)` — 3 parameters on Quasar (NOT 4 like Blackhole):
- `lreg_c`: Source LREG (LREG0)
- `lreg_dest`: Destination LREG (LREG0 — same register, in-place)
- `instr_mod1`: `1` for FP32 mode (clears sign bit). `0` would be INT32 mode (not used here).

Verified in assembly.yaml: SFPABS opcode 0x7D, uses SFPU_MATHI12 encoding `{lreg_c[11:8], lreg_dest[7:4], instr_mod1[3:0]}`.

### Resource Allocation

| Resource | Purpose |
|----------|---------|
| LREG0 | Load data from Dest, compute abs in-place, store result back |
| ADDR_MOD_7 | Address modifier for SFPLOAD/SFPSTORE (all zeroes) |

Only one LREG is needed because SFPABS can operate in-place (src == dest LREG).

## Implementation Structure

### Includes
```cpp
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```

These two headers are the minimum required set for simple SFPU kernels. `ckernel_ops.h` provides the TTI macros (TTI_SFPLOAD, TTI_SFPABS, TTI_SFPSTORE) and is transitively included via `ckernel_trisc_common.h`. Some kernels include it explicitly (square.h, rsqrt.h), but relu.h does not and compiles fine. Following relu.h's minimal include pattern since abs is equally simple.

### Namespace
```cpp
namespace ckernel {
namespace sfpu {
  // ... functions ...
} // namespace sfpu
} // namespace ckernel
```
Matches all existing Quasar SFPU kernels.

### Functions

| Function | Template Params | Signature | Purpose |
|----------|-----------------|-----------|---------|
| `_calculate_abs_sfp_rows_` | None | `inline void _calculate_abs_sfp_rows_()` | Process SFP_ROWS (2) rows: SFPLOAD, SFPABS, SFPSTORE |
| `_calculate_abs_` | None | `inline void _calculate_abs_(const int iterations)` | Iteration loop calling `_sfp_rows_` then `_incr_counters_` |

No template parameters on either function. This matches the pattern of relu.h and silu.h (the simplest Quasar SFPU kernels). Square.h uses `<bool APPROXIMATION_MODE>` on its `_sfp_rows_` helper, but that is because square uses SFPMUL which could have approximation variants. Abs has no approximation mode — it is a single bit-clear operation.

### Function Signatures Verified Against Target Sources

1. **Test harness** (`tests/sources/quasar/sfpu_square_quasar_test.cpp`): Calls `_llk_math_eltwise_unary_sfpu_params_<false>(ckernel::sfpu::_calculate_square_, i, num_sfpu_iterations)`. The `_calculate_abs_` function will be called identically: `_llk_math_eltwise_unary_sfpu_params_<false>(ckernel::sfpu::_calculate_abs_, i, num_sfpu_iterations)`. This requires signature `inline void _calculate_abs_(const int iterations)`.

2. **Parent caller** (`tt_llk_quasar/llk_lib/llk_math_eltwise_unary_sfpu_common.h`): `_llk_math_eltwise_unary_sfpu_params_` accepts `F&& sfpu_func` as a callable with variadic args. The first variadic arg after `dst_tile_index` is `num_sfpu_iterations` which maps to `const int iterations`.

3. **Closest existing kernel** (relu.h): `inline void _calculate_relu_(const int iterations)` — no templates, `const int iterations` parameter. Exact same pattern.

## Instruction Sequence

### _calculate_abs_sfp_rows_()
```
// Load current row from Dest into LREG0 (format auto-detected: FP16a/FP16b/FP32 -> FP32 in LREG)
TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

// Absolute value: clear sign bit of FP32 value in LREG0 (in-place)
// instr_mod1=1 selects FP32 mode (sign-magnitude abs, clears bit 31)
// NaN passes through unchanged, -Inf becomes +Inf, subnormals NOT flushed
TTI_SFPABS(p_sfpu::LREG0, p_sfpu::LREG0, 1);

// Store result from LREG0 back to Dest (FP32 in LREG -> auto-converted to original format)
TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0);
```

### _calculate_abs_()
```
#pragma GCC unroll 8
for (int d = 0; d < iterations; d++) {
    _calculate_abs_sfp_rows_();
    ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
}
```

## Init/Uninit Symmetry
Not applicable. The abs kernel has no init or uninit functions. It modifies no hardware state beyond the Dest register contents (which are the intended output). This matches relu.h, which also has no init/uninit pair.

## Potential Issues

1. **SFPABS is new**: No existing Quasar kernel uses SFPABS. The instruction is verified in assembly.yaml (opcode 0x7D) and Confluence (page 1612186129), and the TTI macro exists in ckernel_ops.h (line 601). However, this will be the first real usage, so any ISA encoding bugs would surface here.

2. **In-place operation**: Using LREG0 as both source and destination for SFPABS. This is explicitly supported (Confluence shows src/dest can be the same LREG) and is analogous to how square.h uses `TTI_SFPMUL(LREG0, LREG0, LCONST_0, LREG0, 0)`.

3. **NaN handling**: SFPABS passes NaN through unchanged (documented in Confluence page 1612186129). The abs of NaN is NaN — this is correct IEEE 754 behavior and matches what PyTorch produces for the golden reference.

## Recommended Test Formats

Based on the analysis's "Format Support" section and the architecture research's "Format Support Matrix", and verified against ALL existing Quasar SFPU tests which use exactly this format set.

### Format List

```python
SFPU_ABS_FORMATS = input_output_formats([
    DataFormat.Float16,
    DataFormat.Float32,
    DataFormat.Float16_b,
])
```

This matches the exact format list used by all existing Quasar SFPU tests (`test_sfpu_square_quasar.py`, `test_sfpu_rsqrt_quasar.py`, `test_sfpu_nonlinear_quasar.py`). While abs is a universal operation (valid for integers too), the initial implementation uses FP32 mode only (instr_mod1=1), and existing SFPU tests do not include MxFp8 or integer formats.

**Rationale for excluding other formats**:
- `MxFp8R`, `MxFp8P`: Not tested in any existing SFPU test. Could work (MX formats unpack to Float16_b, then abs operates on FP32 in LREG), but adding untested MX format handling would increase risk for the initial implementation.
- `Int32`: Would require a different code path (instr_mod1=0 for integer abs, SMAG32 load mode). Not in scope for this phase.
- `Int8`, `Int16`: Limited SFPU support, not in reference implementation.
- `UInt8`, `UInt16`: Unsigned types where abs is identity — pointless to test.
- `Tf32`: Not in the standard SFPU test format list (QUASAR_DATA_FORMAT_ENUM_VALUES includes it, but no SFPU test uses it).

### Invalid Combination Rules

The `_is_invalid_quasar_combination()` function should filter these invalid configurations:

1. **Non-Float32 input to Float32 output with dest_acc=No**: Quasar packer does not support this conversion without 32-bit Dest mode.
   ```python
   if in_fmt != DataFormat.Float32 and out_fmt == DataFormat.Float32 and dest_acc == DestAccumulation.No:
       return True
   ```

2. **Float32 input to Float16 output with dest_acc=No**: Quasar requires dest_acc=Yes for this conversion path.
   ```python
   if in_fmt == DataFormat.Float32 and out_fmt == DataFormat.Float16 and dest_acc == DestAccumulation.No:
       return True
   ```

These are the exact same rules used by `test_sfpu_square_quasar.py`. No additional operation-specific rules are needed for abs (it has no range expansion like square does, so no extra overflow constraints).

### MX Format Handling
Not applicable for this phase. MxFp8R/MxFp8P are excluded from the format list.

### Integer Format Handling
Not applicable for this phase. Integer formats are excluded from the format list.

## Testing Notes

### Input Preparation
Abs does not expand or contract value ranges — `|x|` has the same magnitude as `x`. Unlike square (where x^2 can overflow), abs is safe for any input value. The test can use the default stimuli generator without custom range clamping. A simple `prepare_abs_inputs` function is optional but not strictly needed — just ensuring no inf/nan in random inputs is sufficient for basic correctness testing.

### Golden Reference
Use `torch.abs(src_A)` as the golden reference. This handles:
- Positive values: unchanged
- Negative values: negated
- Zero: unchanged (both +0 and -0 map to +0)
- NaN: preserved (abs(NaN) = NaN in IEEE 754)
- Inf: abs(-Inf) = +Inf, abs(+Inf) = +Inf

### dest_acc Modes
- `dest_acc=No`: 16-bit Dest mode. Used for Float16 and Float16_b input/output (unless the combination would be invalid per the rules above).
- `dest_acc=Yes`: 32-bit Dest mode. Required for Float32 input. Also valid for Float16/Float16_b (provides higher intermediate precision but same result for abs since it's a bit operation).

### 32-bit input path (Float32 with dest_acc=Yes)
When `formats.input_format.is_32_bit() and dest_acc == DestAccumulation.Yes`, the test uses `unpack_to_dest=True` and `UnpackerEngine.UnpDest` instead of `UnpackerEngine.UnpA`. This matches the square test pattern exactly.
