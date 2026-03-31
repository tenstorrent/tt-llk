# Analysis: threshold

## Kernel Type
SFPU

## Reference File
`tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_threshold.h`

## Purpose
The threshold kernel implements element-wise conditional replacement: for each element `x` in the tile, if `x <= threshold` then replace `x` with `value`, otherwise leave `x` unchanged. This is equivalent to PyTorch's `torch.nn.functional.threshold(input, threshold, value)`.

## Algorithm Summary
1. Load threshold and replacement value constants.
2. For each tile row group (2 rows per iteration on Quasar):
   a. Load the input element from the Dest register.
   b. Compare: is input <= threshold?
   c. If yes, replace with the specified value.
   d. Store the result back to Dest.
   e. Advance to the next row group.

The operation is a pure comparison-and-replace -- no arithmetic transformations. The comparison uses FP32 total ordering in the SFPU.

## Template Parameters (Reference - Blackhole)
| Parameter | Purpose | Values |
|-----------|---------|--------|
| `APPROXIMATION_MODE` | Template bool for accuracy mode (unused in threshold) | `true`/`false` |
| `ITERATIONS` | Number of row-group iterations per face | Typically `num_sfpu_iterations` |
| `T` | Type of threshold/value params | `float` or `std::uint32_t` |

## Functions Identified
| Function | Purpose | Complexity |
|----------|---------|------------|
| `_calculate_threshold_<APPROX, ITERATIONS, T>(threshold, value)` (Blackhole) | Main threshold computation | Low |

On Blackhole, this is a single function with template parameters. On Quasar, it should be:
| Function | Purpose | Complexity |
|----------|---------|------------|
| `_calculate_threshold_(iterations, threshold, value)` | Main threshold computation | Low |

## Key Constructs Used (Reference - Blackhole)
- **`sfpi::vFloat`**: SFPI vector float type -- each lane holds one FP32 value. **Not available on Quasar.**
- **`sfpi::dst_reg[0]`**: Load from current Dest register position. **Not available on Quasar** -- use `TTI_SFPLOAD`.
- **`sfpi::dst_reg[0] = v_value`**: Store to Dest. **Not available on Quasar** -- use `TTI_SFPSTORE`.
- **`sfpi::dst_reg++`**: Increment Dest pointer. **Not available on Quasar** -- use `_incr_counters_`.
- **`v_if (in <= v_threshold) { ... } v_endif`**: Conditional per-lane execution. **Not available on Quasar** -- use `TTI_SFPENCC` + `TTI_SFPGT` + `TTI_SFPENCC` pattern.
- **`Converter::as_float(uint32_t)`**: Reinterpret-cast uint32_t to float for SFPI. **Not available on Quasar.**
- **`is_supported_threshold_type_v<T>`**: Type trait for compile-time validation. **Not needed on Quasar** -- Quasar uses `uint32_t` directly.
- **`#pragma GCC unroll 8`**: Loop unroll hint -- **same on Quasar**.

## Dependencies
### Blackhole Reference
- `<cstdint>` -- standard integer types
- `<type_traits>` -- for `std::is_same_v` (not needed on Quasar)
- `"ckernel_defs.h"` -- architecture defines
- `"sfpi.h"` -- SFPI vector abstraction library (**not on Quasar**)
- `"sfpu/ckernel_sfpu_converter.h"` -- `Converter::as_float` (**not on Quasar**)

### Quasar Target (Expected)
- `<cstdint>` -- standard integer types
- `"ckernel_trisc_common.h"` -- Quasar SFPU TTI instruction macros, `p_sfpu::*` constants
- `"cmath_common.h"` -- `ckernel::math::_incr_counters_`, `SFP_ROWS`, `ADDR_MOD_7`

## Complexity Classification
**Simple**

The threshold kernel is one of the simplest SFPU operations -- a single comparison and conditional replace. The Quasar target has `SFPGT` instruction which directly provides the comparison, and the `_calculate_relu_min_` function in `ckernel_sfpu_lrelu.h` is an almost identical pattern (compare input against threshold, conditionally replace). The Quasar port is essentially adapting the relu_min pattern with a different replacement value.

## Constructs Requiring Translation

| Reference Construct | What it does | Quasar Equivalent |
|---------------------|-------------|-------------------|
| `sfpi::vFloat in = sfpi::dst_reg[0]` | Load from Dest into vector register | `TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` |
| `v_if (in <= v_threshold) { ... } v_endif` | Conditional execution per lane | `TTI_SFPENCC(1,2)` + `TTI_SFPGT` + conditional ops + `TTI_SFPENCC(0,0)` |
| `sfpi::dst_reg[0] = v_value` | Store to Dest | Conditional `TTI_SFPMOV` or `TTI_SFPLOADI` + `TTI_SFPSTORE` |
| `sfpi::dst_reg++` | Advance Dest pointer | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` |
| `Converter::as_float(threshold)` | uint32_t-to-float reinterpret | `TTI_SFPLOADI(LREG, 0, (value >> 16))` -- loads as BF16 upper bits |
| Template `<T>` for float/uint32_t | Type dispatch for parameter loading | Single `uint32_t` parameter, loaded via `TTI_SFPLOADI(LREG, 0, (param >> 16))` |

## Target Expected API

### From the Test Harness (`tests/helpers/include/sfpu_operations.h`)
The shared test infrastructure calls threshold as:
```cpp
case SfpuType::threshold:
    _calculate_threshold_<APPROX_MODE, ITERATIONS>(5.0f, 10.0f);
    break;
```
However, this is the **Blackhole** path. On Quasar, the function is invoked differently via `_llk_math_eltwise_unary_sfpu_params_<false>(function_ptr, tile_index, iterations, ...)`.

### From the Quasar Parent File (`tt_llk_quasar/common/inc/ckernel_sfpu.h`)
Currently does NOT include `ckernel_sfpu_threshold.h`. An `#include` line must be added.

### From Quasar Test C++ Source Pattern (e.g., `sfpu_abs_quasar_test.cpp`)
The test calls:
```cpp
_llk_math_eltwise_unary_sfpu_params_<false>(ckernel::sfpu::_calculate_threshold_, i, num_sfpu_iterations, threshold_param, value_param);
```

### Expected Quasar Function Signature
Based on existing Quasar SFPU patterns (abs, fill, lrelu):
```cpp
inline void _calculate_threshold_(const int iterations, const std::uint32_t threshold, const std::uint32_t value)
```

Parameters:
- `iterations`: number of row-group iterations (passed by the test framework)
- `threshold`: threshold value as uint32_t (upper 16 bits = BF16 representation)
- `value`: replacement value as uint32_t (upper 16 bits = BF16 representation)

### Template params to KEEP from reference: NONE
All template params are dropped on Quasar. `APPROXIMATION_MODE` and `ITERATIONS` become unused or are handled differently. The `T` type dispatch is eliminated -- Quasar always uses `uint32_t` and loads via `SFPLOADI`.

### Template params to DROP (reference-only):
- `APPROXIMATION_MODE` -- not used in threshold logic
- `ITERATIONS` -- becomes a regular function parameter
- `T` -- type dispatch eliminated; always uint32_t on Quasar

### Template params to ADD (target-only): NONE

### Reference features NOT present in target (should be DROPPED):
- `Converter::as_float` -- not available, not needed (uint32_t loaded directly via SFPLOADI)
- `sfpi::vFloat` type -- not available, use TTI LREG operations
- `is_supported_threshold_type_v` type trait -- not needed with single uint32_t param
- `std::is_same_v` / `<type_traits>` include -- not needed

## Format Support

### Format Domain
**Universal** (float comparison operation, but mathematically valid for any format that converts to FP32 in the SFPU)

Threshold is a comparison-and-replace operation. The SFPU operates entirely in FP32 internally -- SFPLOAD converts from Dest format to FP32, the comparison happens in FP32, and SFPSTORE converts back. This means any format that can be loaded into the SFPU via SFPLOAD is valid.

However, the threshold and value parameters are loaded as BF16 immediates via `SFPLOADI(LREG, 0, upper_16_bits)`, which means the constants have BF16 precision regardless of the data format. This is consistent with how lrelu, relu_min, and relu_max handle their constants on Quasar.

### Applicable Formats for Testing

| Format | Applicable | Rationale |
|--------|-----------|-----------|
| Float16 | Yes | Standard FP16 format, converted to FP32 in SFPU. Comparison works correctly. |
| Float16_b | Yes | Standard BF16 format, converted to FP32 in SFPU. Most common test format. |
| Float32 | Yes | Native FP32 format. No conversion needed in SFPU. Requires dest_acc=Yes when used as input or output with non-Float32 counterpart. |
| Int32 | No | While SFPGT supports sign-magnitude comparison, the threshold/value constants are loaded as BF16 floats via SFPLOADI mode 0. Integer comparison would need different constant loading (SFPLOADI mode 2/4) and different SFPLOAD/SFPSTORE modes. Not practical without format-dependent code paths. |
| Int16 | No | Limited SFPU support for Int16. Not meaningful for threshold operation. |
| Int8 | No | Very limited range. Not meaningful for threshold comparison. |
| UInt8 | No | Unsigned 8-bit, very limited range. Not meaningful for threshold. |
| MxFp8R | Yes | L1-only format; unpacked to Float16_b for registers, then FP32 in SFPU. Comparison works correctly. Requires implied_math_format=Yes. |
| MxFp8P | Yes | L1-only format; unpacked to Float16_b for registers, then FP32 in SFPU. Comparison works correctly. Requires implied_math_format=Yes. |

### Format-Dependent Code Paths
The reference Blackhole implementation has a type dispatch (`std::is_same_v<T, float>` vs `std::is_same_v<T, uint32_t>`) for how to load threshold/value constants. On Quasar, this is eliminated -- only `uint32_t` is used, loaded via `SFPLOADI(LREG, 0, param >> 16)` as BF16.

The Quasar implementation is **format-agnostic** at the SFPU level. All format conversion happens at the SFPLOAD/SFPSTORE boundary. The kernel code itself does not branch on data format.

### Format Constraints
- **MX formats** (MxFp8R, MxFp8P): Require `implied_math_format=Yes` in unpacker config. Stored in L1 with block exponents; unpacked to Float16_b for register storage.
- **Float32 input, non-Float32 output**: Quasar packer does not support non-Float32 to Float32 conversion when `dest_acc=No`. If input is Float32, `dest_acc=Yes` is required.
- **Non-Float32 input, Float32 output**: Requires `dest_acc=Yes`.
- **Float32 input, Float16 output**: Requires `dest_acc=Yes` (cross-exponent-family conversion).
- **Cross-exponent-family** (e.g., Float16_b input -> Float16 output with `dest_acc=No`): This is an outlier case where the intermediate Dest format needs to be Float32.

## Existing Target Implementations

### Closest Existing Quasar Kernel: `ckernel_sfpu_lrelu.h`
This file contains three functions that are structurally almost identical to what threshold needs:

1. **`_calculate_lrelu_`**: Conditional multiply (if negative, multiply by slope). Uses `SFPSETCC` for sign check.
2. **`_calculate_relu_min_`**: If `input <= threshold`, replace with 0. Uses `SFPGT` for comparison. **This is the closest pattern to threshold** -- the only difference is threshold replaces with 0, while our kernel replaces with an arbitrary value.
3. **`_calculate_relu_max_`**: If `input > threshold`, replace with threshold, then if negative, replace with 0. Uses `SFPGT` + `SFPMOV` + `SFPSETCC`.

### Pattern from `_calculate_relu_min_` (most relevant):
```cpp
inline void _calculate_relu_min_(const int iterations, const std::uint32_t threshold)
{
    TTI_SFPLOADI(p_sfpu::LREG2, 0 /*Float16_b*/, (threshold >> 16));
    TTI_SFPENCC(1, 2);  // enable CC
    for (int d = 0; d < iterations; d++)
    {
        TTI_SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0);
        TTI_SFPGT(0, LREG0, LREG2, 0x1);  // CC set where LREG2 > LREG0 (input <= threshold)
        TTI_SFPLOADI(LREG0, 0, 0);          // load 0 where CC is set
        TTI_SFPENCC(0, 0);                   // clear CC
        TTI_SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0);
        _incr_counters_<...>();
    }
    TTI_SFPENCC(0, 2);  // disable CC
}
```

For threshold, we change `TTI_SFPLOADI(LREG0, 0, 0)` (load zero) to `TTI_SFPMOV(LREG3, LREG0, 0)` (copy pre-loaded replacement value from LREG3 to LREG0).

### Other Quasar SFPU patterns:
- `ckernel_sfpu_abs.h`: Simplest pattern (no conditional, single instruction per row).
- `ckernel_sfpu_fill.h`: Fill with constant. Shows `SFPLOADI` + `SFPSTORE` loop pattern.

## Quasar SfpuType Enum Status
The Quasar `SfpuType` enum in `tt_llk_quasar/llk_lib/llk_defs.h` does **NOT** include `threshold`. It must be added for the test infrastructure to work. The enum currently has: tanh, gelu, exponential, reciprocal, sqrt, rsqrt, relu, lrelu, relumin, relumax, stochround, typecast, add, square, sigmoid, silu, abs, fill.

Note: The test infrastructure's `llk_sfpu_types.h` guards its own SfpuType enum with `#ifndef ARCH_QUASAR`, meaning on Quasar the enum from `llk_defs.h` is used. So `threshold` must be added to the Quasar enum.

## Sub-Kernel Phases

This is a simple single-function SFPU kernel. There is only 1 phase.

| Phase | Name | Functions | Dependencies |
|-------|------|-----------|--------------|
| 1 | threshold | `_calculate_threshold_` | none |

The kernel has a single function that implements the complete threshold operation. There are no init/uninit functions needed (unlike some other SFPU kernels), no multi-variant complexity, and no separate helper rows functions needed (the row processing is simple enough to inline in the loop body).

**Ordering**: Only 1 phase, so ordering is trivial.
