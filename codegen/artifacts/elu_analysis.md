# Analysis: elu

## Kernel Type
sfpu

## Reference File
`tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_elu.h`

## Purpose
Computes the Exponential Linear Unit (ELU) activation function elementwise on tile data in the Dest register:
- For x >= 0: output = x (identity, value is left untouched in Dest)
- For x < 0: output = alpha * (exp(x) - 1)

The `alpha` (called `slope` in the Blackhole code) parameter controls the negative saturation value. ELU is a smooth alternative to ReLU that avoids the "dying neuron" problem by producing non-zero gradients for negative inputs.

## Algorithm Summary
1. Load alpha parameter once before the iteration loop
2. For each row group (SFP_ROWS iterations):
   a. Load value x from Dest register
   b. Check if x < 0 (conditional execution)
   c. If x < 0:
      - Compute exp(x)
      - Subtract 1 to get exp(x) - 1
      - Multiply by alpha to get alpha * (exp(x) - 1)
      - Store result back to Dest
   d. If x >= 0: do nothing (identity preserved in Dest)
   e. Clear conditional execution state
   f. Advance to next row group

## Template Parameters

### Blackhole (Reference)
| Parameter | Purpose | Values |
|-----------|---------|--------|
| APPROXIMATION_MODE | Controls exp() implementation accuracy | true/false |
| is_fp32_dest_acc_en | Whether Dest uses 32-bit accumulation mode | true/false (default: false) |
| ITERATIONS | Number of row groups to process per face | int (default: 8) |

### Quasar (Target) -- Expected
| Parameter | Purpose | Values |
|-----------|---------|--------|
| (none) | Quasar SFPU kernels use runtime `iterations` parameter, not template params | N/A |

## Functions Identified
| Function | Purpose | Complexity |
|----------|---------|------------|
| `_calculate_elu_(std::uint32_t slope)` | Blackhole: Full ELU computation with loop over ITERATIONS | Medium |
| `_calculate_elu_(const int iterations, const std::uint32_t slope)` | Quasar target: ELU with runtime iteration count and slope parameter | Medium |

The Blackhole has a single function `_calculate_elu_` with template parameters. For Quasar, the target API should follow the established pattern: a `_calculate_elu_sfp_rows_()` inner helper plus a `_calculate_elu_(const int iterations, const std::uint32_t slope)` outer loop function.

## Key Constructs Used

### From Blackhole Reference
- **`sfpi::vFloat`**: SFPI vector float type -- wraps SFPU LREG operations (Quasar equivalent: raw LREG access via TTI macros)
- **`sfpi::dst_reg[0]`**: Reads/writes Dest register (Quasar equivalent: TTI_SFPLOAD / TTI_SFPSTORE)
- **`sfpi::dst_reg++`**: Advances Dest pointer (Quasar equivalent: `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()`)
- **`Converter::as_float(slope)`**: Reinterprets uint32 slope as float (NOT available on Quasar; use SFPLOADI with upper 16 bits)
- **`v_if (v < 0.0f) { ... } v_endif`**: Conditional execution (Quasar equivalent: SFPENCC/SFPSETCC/SFPENCC pattern)
- **`_sfpu_exp_21f_bf16_<true>(v)`**: High-precision polynomial exp implementation (NOT available on Quasar; use TTI_SFPNONLINEAR with EXP_MODE)
- **`sfpi::vConst1`**: Constant 1.0 (Quasar equivalent: `p_sfpu::LCONST_1`)
- **`sfpi::float_to_fp16b(result, 0)`**: Explicit FP32-to-BF16 rounding (NOT available on Quasar; SFPSTORE handles truncation)
- **`sfpi::reinterpret<sfpi::vFloat>(...)`**: Type reinterpretation (NOT available on Quasar)

### Quasar Patterns (from existing implementations)
- **TTI_SFPLOAD/TTI_SFPSTORE**: Load from / store to Dest register with format mode and ADDR_MOD_7
- **TTI_SFPSETCC(0, lreg, 0)**: Set CC.Res based on sign of lreg (mode 0 = negative check)
- **TTI_SFPENCC(1, 2)**: Enable conditional execution globally
- **TTI_SFPENCC(0, 0)**: Reset CC.Res to 1 for all lanes (clear condition within loop)
- **TTI_SFPENCC(0, 2)**: Disable conditional execution globally
- **TTI_SFPNONLINEAR(src, dst, mode)**: Hardware approximation for nonlinear functions (exp, recip, sqrt, tanh, relu)
- **TTI_SFPLOADI(lreg, fmt, imm16)**: Load 16-bit immediate into LREG
- **TTI_SFPMAD/TTI_SFPADD/TTI_SFPMUL**: FP multiply-add / add / multiply instructions
- **`_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()`**: Advance Dest pointer by SFP_ROWS (2 rows)

## Dependencies

### Blackhole Reference Dependencies
- `ckernel_sfpu_converter.h` -- provides `Converter::as_float()`
- `ckernel_sfpu_exp.h` -- provides `_sfpu_exp_21f_bf16_<>()`
- `sfpi.h` -- provides vFloat, dst_reg, v_if/v_endif, vConst1, float_to_fp16b

### Quasar Target Dependencies (Expected)
- `ckernel_trisc_common.h` -- provides TTI macros, ADDR_MOD_7
- `cmath_common.h` -- provides `_incr_counters_`, `SFP_ROWS`
- `ckernel_ops.h` -- provides `TTI_SFPNONLINEAR` and other TTI instruction macros

Note: Quasar does NOT need `ckernel_sfpu_exp.h` or `ckernel_sfpu_converter.h` since it uses the hardware SFPNONLINEAR(EXP_MODE) approximation instead of the software polynomial.

## Complexity Classification
**Medium**

Rationale:
- The core mathematical operation (exp, subtract, multiply) maps cleanly to Quasar SFPNONLINEAR + SFPADD + SFPMUL instructions
- Conditional execution (if x < 0) maps directly to the SFPSETCC/SFPENCC pattern already used in Quasar lrelu.h
- The main complexity comes from correctly combining conditional execution with multi-instruction math (exp, subtract 1, multiply by alpha)
- No algorithm redesign needed; the same logic flow works, just with different primitives
- The Blackhole `_sfpu_exp_21f_bf16_` software polynomial is replaced by the simpler hardware `SFPNONLINEAR(EXP_MODE)` one-instruction approximation

## Constructs Requiring Translation

| Blackhole Construct | What it does | Quasar Approach |
|---------------------|-------------|-----------------|
| `Converter::as_float(slope)` | Reinterprets uint32 parameter as float for vector use | `TTI_SFPLOADI(LREG2, 0, (slope >> 16))` -- loads upper 16 bits as Float16_b immediate |
| `sfpi::vFloat v = sfpi::dst_reg[0]` | Load from Dest into local vector register | `TTI_SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)` |
| `v_if (v < 0.0f) { ... } v_endif` | Conditional execution for negative values | `TTI_SFPENCC(1, 2)` + `TTI_SFPSETCC(0, LREG0, 0)` + `...` + `TTI_SFPENCC(0, 0)` + `TTI_SFPENCC(0, 2)` |
| `_sfpu_exp_21f_bf16_<true>(v)` | Software polynomial exp approximation | `TTI_SFPNONLINEAR(src, dst, EXP_MODE)` -- hardware exp approximation |
| `- sfpi::vConst1` | Subtract 1.0 from exp result | `TTI_SFPADD(LCONST_1, exp_reg, LCONST_neg1, dst_reg, 0)` -- computes 1.0 * exp_reg + (-1.0) |
| `s * v_exp` | Multiply alpha by (exp(x)-1) | `TTI_SFPMUL(alpha_reg, sub_result_reg, LCONST_0, dst_reg, 0)` -- computes alpha * result + 0 |
| `float_to_fp16b(result, 0)` | Round FP32 to BF16 explicitly | Not needed on Quasar -- SFPSTORE truncates automatically |
| `sfpi::dst_reg[0] = result` | Store result to Dest | `TTI_SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0)` |
| `sfpi::dst_reg++` | Advance Dest pointer | `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()` |

## Target Expected API

### From Test Harness (sfpu_nonlinear_quasar_test.cpp)
The test harness at `tests/sources/quasar/sfpu_nonlinear_quasar_test.cpp` does NOT currently include ELU. It exercises exp, gelu, relu, reciprocal, sqrt, tanh, sigmoid, and silu via the `sfpu_op_dispatcher` pattern. ELU would need its own test file since it requires a `slope` parameter that the nonlinear test framework does not support.

Key observations from the test harness:
- SFPU functions are called via `_llk_math_eltwise_unary_sfpu_params_<false>(function_ptr, tile_idx, num_sfpu_iterations)`
- The `num_sfpu_iterations` is computed as `FACE_R_DIM / SFP_ROWS`
- Operations with init functions call `init_sfpu_operation_quasar()` before the loop
- The test uses `SfpuType` enum for dispatch

### From Parent File (ckernel_sfpu.h)
The parent file `tt_llk_quasar/common/inc/ckernel_sfpu.h` currently does NOT include `ckernel_sfpu_elu.h`. It will need to be added.

### Function Signatures (Target Expected)
Based on the pattern established by existing Quasar SFPU kernels (lrelu, sigmoid, exp, etc.):

```cpp
// Inner helper -- processes SFP_ROWS (2) rows per call
inline void _calculate_elu_sfp_rows_()

// Outer loop -- iterates over all row groups, takes runtime iterations + slope param
inline void _calculate_elu_(const int iterations, const std::uint32_t slope)
```

### Template Parameters
- **KEEP from reference**: None. Quasar SFPU kernels do not use template parameters for APPROXIMATION_MODE, is_fp32_dest_acc_en, or ITERATIONS.
- **DROP (reference-only)**:
  - `APPROXIMATION_MODE` -- Quasar always uses SFPNONLINEAR hardware approximation
  - `is_fp32_dest_acc_en` -- Not needed because Quasar SFPSTORE handles truncation automatically; there is no manual `float_to_fp16b` path
  - `ITERATIONS` -- Replaced by runtime `const int iterations` parameter
- **ADD (target-only)**: None -- the pattern matches existing Quasar kernels exactly

### SfpuType Enum
The `SfpuType` enum in `tt_llk_quasar/llk_lib/llk_defs.h` does NOT currently contain an `elu` entry. If tests need SfpuType-based dispatch, an `elu` entry would need to be added. However, since ELU takes a parameter (slope), it may use direct function calls rather than SfpuType dispatch (similar to how lrelu takes a slope parameter).

## Format Support

### Format Domain
**Float-only**: ELU requires `exp()` which is a floating-point operation mathematically undefined for integers. The output range includes negative values approaching -alpha, making integer outputs impractical.

### Applicable Formats for Testing
Based on the operation type, target architecture support, and existing test patterns:

| Format | Applicable | Rationale |
|--------|-----------|-----------|
| Float16 | Yes | Standard 16-bit float; SFPU loads to FP32, computes, stores back. Used by all Quasar SFPU tests. |
| Float16_b | Yes | Primary BF16 format; directly supported in Dest. Slope is loaded as BF16 via SFPLOADI. |
| Float32 | Yes | Full precision; requires dest_acc=Yes for 32-bit Dest mode. |
| Int32 | No | ELU requires exp() which is a float-only operation. |
| Int16 | No | ELU requires exp() which is a float-only operation. |
| Int8 | No | ELU requires exp() which is a float-only operation. |
| UInt8 | No | ELU requires exp() which is a float-only operation. |
| MxFp8R | Yes (L1 only) | Unpacked to Float16_b by hardware before reaching SFPU. Requires implied_math_format=Yes. |
| MxFp8P | Yes (L1 only) | Unpacked to Float16_b by hardware before reaching SFPU. Requires implied_math_format=Yes. |

### Format-Dependent Code Paths
The Blackhole reference has one format-dependent code path:
```cpp
if constexpr (!is_fp32_dest_acc_en)
{
    result = sfpi::reinterpret<sfpi::vFloat>(sfpi::float_to_fp16b(result, 0));
}
```
This performs explicit FP32-to-BF16 rounding when NOT in 32-bit Dest mode. On Quasar, this is unnecessary because:
1. SFPSTORE handles truncation from FP32 (LREG internal format) to the Dest format automatically
2. All existing Quasar SFPU kernels (lrelu, sigmoid, silu, etc.) do NOT have format-dependent code paths
3. The kernel will be format-agnostic -- the same instruction sequence works for all supported float formats

### Format Constraints
Hardware constraints that affect format combinations for ELU:

1. **Float32 input/output requires dest_acc=Yes**: 32-bit data requires 32-bit Dest register mode
2. **Non-Float32 input to Float32 output requires dest_acc=Yes**: Quasar packer does not support 16-bit Dest to 32-bit L1 conversion without 32-bit Dest mode
3. **Float32 input to Float16 output requires dest_acc=Yes**: Cross-precision conversion needs 32-bit intermediate
4. **MX formats (MxFp8R, MxFp8P) require implied_math_format=Yes**: L1-only formats unpacked to Float16_b by hardware
5. **Integer and float formats cannot be mixed**: ELU is float-only, so this is not an issue
6. **Cross-exponent-family (expB input to Float16 output) requires dest_acc=Yes**: e.g., Float16_b input, Float16 output without dest_acc is an unsupported hardware path

Invalid combination rules (from existing Quasar SFPU tests):
- `in_fmt != Float32 && out_fmt == Float32 && dest_acc == No` -> INVALID
- `in_fmt == Float32 && out_fmt == Float16 && dest_acc == No` -> INVALID

## Existing Target Implementations

### Closest Existing Kernel: lrelu (ckernel_sfpu_lrelu.h)
The leaky ReLU kernel is the closest analog to ELU on Quasar:
- Both take a `slope`/`alpha` parameter as `std::uint32_t`
- Both use conditional execution (SFPSETCC for negative check)
- Both apply a transformation only to negative values
- Both load the parameter via `TTI_SFPLOADI(LREG2, 0, (slope >> 16))`

Key differences from lrelu:
- lrelu only multiplies by slope: `x * slope`
- ELU computes `alpha * (exp(x) - 1)` which requires exp + subtract + multiply

### Sigmoid (ckernel_sfpu_sigmoid.h)
Uses SFPNONLINEAR(EXP_MODE) as part of its computation, demonstrating the pattern for hardware exponential on Quasar.

### Silu (ckernel_sfpu_silu.h)
Builds on sigmoid, showing how to compose multiple SFPU operations (sigmoid + multiply).

### Pattern Summary
All existing Quasar SFPU kernels follow this pattern:
```cpp
#pragma once
#include "ckernel_trisc_common.h"  // (or ckernel_ops.h)
#include "cmath_common.h"

namespace ckernel {
namespace sfpu {

inline void _calculate_{op}_sfp_rows_() {
    TTI_SFPLOAD(...);  // Load from Dest
    // ... compute ...
    TTI_SFPSTORE(...); // Store to Dest
}

inline void _calculate_{op}_(const int iterations, ...) {
    // Optional: load parameters before loop
    #pragma GCC unroll 8
    for (int d = 0; d < iterations; d++) {
        _calculate_{op}_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
    // Optional: cleanup (e.g., disable CC)
}

} // namespace sfpu
} // namespace ckernel
```

## Sub-Kernel Phases

ELU has a single `_calculate_elu_` function (plus its `_calculate_elu_sfp_rows_` inner helper). There is no init function needed (unlike gelu which requires LUT initialization). The alpha parameter is loaded inline at the start of `_calculate_elu_`.

| Phase | Name | Functions | Dependencies |
|-------|------|-----------|--------------|
| 1 | elu_calculate | `_calculate_elu_sfp_rows_()`, `_calculate_elu_(const int iterations, const std::uint32_t slope)` | none |

**Ordering rationale**: This is a simple SFPU kernel with just one logical sub-kernel (calculate). There is no separate init function since the alpha parameter load happens inside the calculate function (before the loop), following the same pattern as lrelu. Only 1 phase is needed.
