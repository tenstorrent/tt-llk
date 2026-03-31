# Specification: exp2 — Phase 1 (exp2 core)

## Kernel Type
sfpu

## Target Architecture
quasar

## Overview
Based on analysis: `codegen/artifacts/exp2_analysis.md`

This phase implements the complete exp2 kernel for Quasar. The operation computes 2^x for each element in the destination register tile by converting it to e^(x * ln(2)) and using the hardware-accelerated SFPNONLINEAR EXP_MODE instruction. This is a single-phase kernel because exp2 is simple enough to have only two functions (a row helper and the iteration wrapper), no init function, and no variant sub-kernels.

## Phase Scope
- **Phase**: 1 of 1 ("exp2 core")
- **Functions**: `_calculate_exp2_sfp_rows_`, `_calculate_exp2_`
- **Previously completed phases**: none

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_exp2.h`

## Reference Implementations Studied

### Existing Target (Quasar) Implementations
- **`ckernel_sfpu_exp.h`**: Primary pattern source. Identical structure — SFPLOAD + SFPNONLINEAR(EXP_MODE) + SFPSTORE. exp2 adds one SFPMULI step between SFPLOAD and SFPNONLINEAR. Confirmed: `template <bool APPROXIMATION_MODE>` on both `_sfp_rows_` and `_calculate_` functions. Confirmed: 5-argument SFPLOAD/SFPSTORE, ADDR_MOD_7, `(0, 0)` for done+dest_reg_addr.
- **`ckernel_sfpu_tanh.h`**: Same pattern as exp but with TANH_MODE. Confirms the template structure.
- **`ckernel_sfpu_sigmoid.h`**: Shows non-templated functions (no APPROXIMATION_MODE). Uses `ckernel_ops.h` include. Confirms the function naming convention `_calculate_{op}_sfp_rows_` and `_calculate_{op}_`.
- **`ckernel_sfpu_silu.h`**: Shows SFPMUL usage pattern (4-argument: `lreg_a, lreg_b, lconst_0, lreg_dest, 0`). Confirms namespace pattern.

### Blackhole Reference
- **`ckernel_sfpu_exp2.h`**: Shows the algorithm: `v * 0.6931471805f` followed by exponential. On Blackhole this is complex (calls `_calculate_exponential_piecewise_`). On Quasar, replaced entirely by SFPNONLINEAR.

## Algorithm in Target Architecture

### Mathematical Operation
```
output = 2^x = e^(x * ln(2))
```
Where ln(2) = 0.6931471805599453

### Pseudocode (per SFP_ROWS iteration)
1. **SFPLOAD**: Load value x from Dest register into LREG0
2. **SFPMULI**: Multiply LREG0 by ln(2) in FP16_B immediate format (result stays in LREG0)
3. **SFPNONLINEAR**: Compute e^(LREG0), store result in LREG1 (EXP_MODE)
4. **SFPSTORE**: Store LREG1 back to Dest register
5. **_incr_counters_**: Advance Dest address by SFP_ROWS (=2 rows)

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|--------------------|-------------------|-----------------|
| `sfpi::dst_reg[0]` (load) | `TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` | `ckernel_sfpu_exp.h` line 17 |
| `v * 0.6931471805f` | `TTI_SFPMULI(0x3F31, p_sfpu::LREG0, 0)` | `ckernel_ops.h` SFPMULI definition; FP16_B encoding verified below |
| `_calculate_exponential_piecewise_()` | `TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE)` | `ckernel_sfpu_exp.h` line 22 |
| `sfpi::dst_reg[0] = exp` (store) | `TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0)` | `ckernel_sfpu_exp.h` line 26 |
| `sfpi::dst_reg++` | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` | `ckernel_sfpu_exp.h` line 37 |

### FP16_B Encoding of ln(2)

ln(2) = 0.6931471805599453
- IEEE 754 FP32: `0x3F317218`
- FP16_B (bfloat16) = upper 16 bits of FP32: `0x3F31`
- Reconstructed value: 0.69140625 (truncation error ~0.0017, acceptable for SFPU approximation which already has 2 ULP error for exp)

The `TTI_SFPMULI(imm16_math, lreg_dest, instr_mod1)` takes `imm16_math` as the raw 16-bit FP16_B constant value. So `imm16_math = 0x3F31` for ln(2).

### Resource Allocation

| Resource | Purpose |
|----------|---------|
| LREG0 | Input value x, then x * ln(2) after SFPMULI |
| LREG1 | Output value e^(x * ln(2)) = 2^x from SFPNONLINEAR |
| ADDR_MOD_7 | Address modifier (all zeroes, standard for SFPLOAD/SFPSTORE) |

No additional LREGs, constants, or config registers needed. This is a minimal resource usage kernel.

### Pipeline Considerations
- **SFPMULI latency**: 2 cycles, fully pipelined (IPC=1). Hardware auto-stalls if SFPNONLINEAR reads LREG0 before SFPMULI completes. No explicit NOP needed (confirmed by sigmoid.h comment: "hardware implicitly stalls if the next instruction depends on results").
- **SFPNONLINEAR latency**: 1 cycle. Result in LREG1 available next cycle for SFPSTORE.
- **No pipeline hazards** in this 4-instruction sequence.

## Implementation Structure

### Includes
```cpp
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```
Discovered from `ckernel_sfpu_exp.h` (the closest existing kernel). No additional includes needed — SFPMULI is defined in `ckernel_ops.h` which is transitively included via `ckernel_trisc_common.h`.

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
Discovered from all existing Quasar SFPU kernels.

### Functions

| Function | Template Params | Signature | Purpose |
|----------|-----------------|-----------|---------|
| `_calculate_exp2_sfp_rows_` | `<bool APPROXIMATION_MODE>` | `inline void _calculate_exp2_sfp_rows_()` | Process SFP_ROWS=2 rows: load, multiply by ln(2), exp, store |
| `_calculate_exp2_` | `<bool APPROXIMATION_MODE>` | `inline void _calculate_exp2_(const int iterations)` | Iterate over all rows, calling `_sfp_rows_` + `_incr_counters_` |

**Why APPROXIMATION_MODE template?**: Following the exp/tanh pattern exactly. The `if constexpr (APPROXIMATION_MODE)` guard around SFPNONLINEAR is the established Quasar pattern. The test harness will call `_calculate_exp2_<true>`.

**Why no init function?**: SFPNONLINEAR is hardware-internal (no LUT setup needed). SFPMULI uses an inline immediate (no LREG pre-loading needed). The Blackhole `_init_exp2_` only exists to initialize `_init_exponential_` which is not needed on Quasar. This matches the existing exp kernel which has no init.

### Init/Uninit Symmetry
Not applicable. This kernel has no `_init_` or `_uninit_` functions. SFPNONLINEAR requires no setup, and the ln(2) constant is encoded as an inline immediate in SFPMULI.

## Instruction Sequence

### `_calculate_exp2_sfp_rows_<APPROXIMATION_MODE>()`

```
SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)     // Load x from Dest into LREG0
if constexpr (APPROXIMATION_MODE):
    SFPMULI(0x3F31, LREG0, 0)                  // LREG0 = LREG0 * ln(2) [FP16_B imm]
    SFPNONLINEAR(LREG0, LREG1, EXP_MODE)       // LREG1 = e^(LREG0) = 2^x
SFPSTORE(LREG1, 0, ADDR_MOD_7, 0, 0)          // Store LREG1 back to Dest
```

**Critical design decision**: The SFPMULI is inside the `if constexpr (APPROXIMATION_MODE)` block along with SFPNONLINEAR. This follows the exp/tanh pattern where the entire computation is guarded. When `APPROXIMATION_MODE=false`, the store would write LREG1 which is whatever was there before (undefined behavior, but matches the exp pattern).

### `_calculate_exp2_<APPROXIMATION_MODE>(iterations)`

```
#pragma GCC unroll 8
for d = 0 to iterations-1:
    _calculate_exp2_sfp_rows_<APPROXIMATION_MODE>()
    _incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()
```

## Function Signatures Verified Against Target

### Test Harness
No existing test file for exp2. The expected call pattern (based on the exp pattern in `sfpu_nonlinear_quasar_test.cpp` line 108):
```cpp
_llk_math_eltwise_unary_sfpu_params_<false>(_calculate_exp2_<true>, tile_idx, num_sfpu_iterations);
```
This means `_calculate_exp2_<true>` is called with `(num_sfpu_iterations)` which matches `void _calculate_exp2_(const int iterations)`.

### Parent File
`tt_llk_quasar/common/inc/ckernel_sfpu.h` does not currently include exp2. It will need:
```cpp
#include "sfpu/ckernel_sfpu_exp2.h"
```

### SfpuType Enum
`tt_llk_quasar/llk_lib/llk_defs.h` needs `exp2` added to the `SfpuType` enum. Current entries end with `threshold`. The new entry should be added after the existing entries.

## Potential Issues

1. **SFPMULI FP16_B truncation**: ln(2) = 0.6931... truncates to 0.6914... in FP16_B. This introduces ~0.25% relative error in the multiplication step, on top of the SFPNONLINEAR approximation error (2 ULP for exp). For most ML use cases this is acceptable, and it matches the Blackhole approach which also uses truncated FP16_B constants via `sfpi::vFloat`.

2. **SFPMULI writing back to same register**: `TTI_SFPMULI(0x3F31, p_sfpu::LREG0, 0)` reads from and writes to LREG0. This is the defined behavior per the ISA spec: "Multiplies RG[VD] by an FP16_B immediate and stores back to RG[VD]."

3. **Non-APPROXIMATION_MODE path**: When `APPROXIMATION_MODE=false`, the function loads from Dest but stores LREG1 without computing anything into it. This matches the exp/tanh pattern exactly and is effectively a no-op path. The test always calls with `<true>`.

## Recommended Test Formats

### Format Domain
**Float-only** -- exp2 is a transcendental floating-point operation.

### Format List
The exact DataFormat enum values for `input_output_formats()`:

```python
FORMATS = input_output_formats([
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Float32,
])
```

This matches the existing `SFPU_NONLINEAR_FORMATS` list in `test_sfpu_nonlinear_quasar.py` (which tests exp, gelu, relu, reciprocal, sqrt, tanh, sigmoid, silu with exactly these three formats). MX formats (MxFp8R, MxFp8P) are excluded because:
- Existing nonlinear tests do not include them
- They would work in principle (unpacked to Float16_b for math) but add test complexity without meaningful coverage benefit for a transcendental operation

### Invalid Combination Rules
The `_is_invalid_quasar_combination()` function should implement:

```python
def _is_invalid_quasar_combination(
    fmt: FormatConfig, dest_acc: DestAccumulation
) -> bool:
    in_fmt = fmt.input_format
    out_fmt = fmt.output_format

    # Quasar packer does not support non-Float32 to Float32 conversion when dest_acc=No
    if (
        in_fmt != DataFormat.Float32
        and out_fmt == DataFormat.Float32
        and dest_acc == DestAccumulation.No
    ):
        return True

    # Quasar SFPU with Float32 input and Float16 output requires dest_acc=Yes
    if (
        in_fmt == DataFormat.Float32
        and out_fmt == DataFormat.Float16
        and dest_acc == DestAccumulation.No
    ):
        return True

    return False
```

This is identical to the existing `test_sfpu_nonlinear_quasar.py` filtering rules.

### Dest Accumulation Rules
- Float32 input format requires `dest_acc=Yes` (32-bit Dest mode required)
- Float16/Float16_b can use either `dest_acc=No` or `dest_acc=Yes`

### MX Format Handling
Not applicable -- MX formats excluded from test list.

### Integer Format Handling
Not applicable -- exp2 is float-only.

## Testing Notes

### Input Range
exp2(x) grows rapidly: 2^10 = 1024, 2^127 overflows FP32. Recommended test input range: [-10, 10] to keep results well within representable range while exercising both negative (small fractional results) and positive (large results) inputs. For very negative inputs (x < -126), 2^x underflows to subnormal which SFPU flushes to zero.

### Golden Generator
The golden computation is simply `2^x` which can be computed as `torch.pow(2.0, x)` or equivalently `torch.exp2(x)`.

### Tolerance
Given SFPNONLINEAR has 2 ULP error for exp and the SFPMULI truncation adds ~0.25% relative error, the test should use the same tolerance as existing nonlinear tests (typically `atol=0.01, rtol=0.02` or similar).
