# Analysis: sign

## Kernel Type
sfpu

## Reference File
`tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_sign.h`

## Purpose
Computes the element-wise sign function: sign(x) = +1.0 if x > 0, -1.0 if x < 0, 0.0 if x == 0.
The output is always a floating-point value from the set {-1.0, 0.0, +1.0}.

## Algorithm Summary
1. Load each element from Dest register
2. Default: assume result is +1.0
3. If element is negative, replace result with -1.0
4. If element is exactly zero, replace result with 0.0
5. Store result back to Dest register

This is a three-way conditional classification using the sign and magnitude of the input.

## Template Parameters
| Parameter | Purpose | Values | Target Status |
|-----------|---------|--------|---------------|
| `APPROXIMATION_MODE` | Controls accuracy vs performance tradeoff | `bool` | **DROP** - Quasar kernels do not use template params |
| `ITERATIONS` | Number of loop iterations (compile-time) | `int` (default 8) | **DROP** - Quasar uses runtime `iterations` param |

## Functions Identified
| Function | Purpose | Complexity |
|----------|---------|------------|
| `_calculate_sign_` | Main sign function: iterates over rows, applies sign logic to each element | Medium |

Note: The blackhole reference has a single function. The Quasar implementation will follow the standard two-function pattern: `_calculate_sign_sfp_rows_()` (inner loop body) and `_calculate_sign_(const int iterations)` (outer loop with setup/teardown).

## Key Constructs Used
- **sfpi::vFloat / sfpi::dst_reg[0]**: SFPI vector type and register access abstraction (Blackhole-only, not available on Quasar)
- **sfpi::vConst1, sfpi::vConstNeg1, sfpi::vConst0**: Named constant registers for +1.0, -1.0, 0.0
- **v_if / v_endif**: SFPI conditional execution macros (Blackhole-only)
- **_sfpu_is_fp16_zero_()**: Helper function for zero detection with FP16 bias handling (Blackhole-only, not present on Quasar)
- **`#pragma GCC unroll 0`**: Prevents loop unrolling (Blackhole-specific; Quasar uses `#pragma GCC unroll 8`)
- **`sfpi::dst_reg++`**: Advances to next row (Blackhole processes 1 row/iteration)
- **`exponent_size_8` parameter**: Controls FP16a vs FP16b zero detection (Blackhole-specific; Quasar handles formats via SFPLOAD implied mode)

## Dependencies
### Reference (Blackhole):
- `ckernel_sfpu_is_fp16_zero.h` - FP16 zero detection helper
- `sfpi.h` - SFPI vector programming abstraction
- `<cstdint>` - Standard integer types

### Target (Quasar) - expected:
- `ckernel_trisc_common.h` - Quasar common definitions (TTI macros, p_sfpu constants)
- `cmath_common.h` - Quasar math common (SFP_ROWS, _incr_counters_)
- `<cstdint>` - Standard integer types

## Complexity Classification
**Simple**

The sign function is a straightforward three-way conditional (negative, positive, zero) with no complex math.
Quasar has all required instructions: SFPLOAD, SFPSTORE, SFPSETCC (with mode 0 for negative check, mode 6 for zero check), SFPCOMPC (for complement/else branch), SFPLOADI (for constants), SFPMOV (for conditional copy), SFPENCC (for CC management).

The closest existing Quasar patterns (ELU, threshold, lrelu) already demonstrate all the necessary CC-based conditional execution patterns. The sign kernel is simpler than ELU (no exp/mul needed) and comparable to threshold in structure.

## Constructs Requiring Translation
| Reference Construct | Logical Purpose | Quasar Equivalent |
|---|---|---|
| `sfpi::vFloat v = sfpi::dst_reg[0]` | Load element from Dest | `TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` |
| `sfpi::dst_reg[0] = sfpi::vConst1` | Store +1.0 to Dest | Load +1.0 into LREG via SFPLOADI, then SFPSTORE |
| `sfpi::vConst1` / `sfpi::vConstNeg1` / `sfpi::vConst0` | Named FP constants | `TTI_SFPLOADI(LREG, 0, immediate)` for BF16 constants |
| `v_if (v < 0.0F) { ... } v_endif;` | Conditional on negative | `TTI_SFPSETCC(0, LREG0, 0)` (mode 0 = set if negative) + conditional operations + `TTI_SFPENCC(0, 0)` |
| `v_if (_sfpu_is_fp16_zero_(...)) { ... } v_endif;` | Conditional on zero | `TTI_SFPSETCC(0x800, LREG0, 6)` (mode 6 = set if zero, Imm12[11]=1 for FP mode) |
| `sfpi::dst_reg++` | Advance row pointer by 1 | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` (advance by 2 rows) |
| `_sfpu_is_fp16_zero_(v, exponent_size_8)` | FP16 format-aware zero check | Not needed on Quasar; SFPSETCC mode 6 handles FP32 zero check natively in LREG space |

## Target Expected API

### Function Signatures the Target Expects
Based on the established Quasar SFPU kernel patterns (from abs, threshold, elu, fill, lrelu):

**Inner function (row processing):**
```cpp
inline void _calculate_sign_sfp_rows_()
```
- No parameters needed (input loaded from Dest, result stored to Dest)
- Constants are pre-loaded in persistent LREGs by the outer function

**Outer function (iteration loop with setup/teardown):**
```cpp
inline void _calculate_sign_(const int iterations)
```
- `iterations` = number of row-pairs to process (passed at runtime as `TEST_FACE_R_DIM / SFP_ROWS`)
- No additional parameters needed (the reference's `exponent_size_8` is Blackhole-specific and not used on Quasar)

**Call site pattern (from test harness):**
```cpp
_llk_math_eltwise_unary_sfpu_params_<false>(ckernel::sfpu::_calculate_sign_, i, num_sfpu_iterations);
```

### Template params to KEEP from reference
- None. Quasar SFPU kernels do not use template parameters.

### Template params to DROP (reference-only)
- `APPROXIMATION_MODE` - Not used on Quasar
- `ITERATIONS` - Quasar uses runtime iteration count

### Template params to ADD (target-only)
- None.

### Target features NOT present in the reference
- Two-function pattern: `_calculate_sign_sfp_rows_()` inner + `_calculate_sign_()` outer
- `#pragma GCC unroll 8` (instead of blackhole's `#pragma GCC unroll 0`)
- `_incr_counters_` for row advancement (instead of `dst_reg++`)
- CC enable/disable bookkeeping: `TTI_SFPENCC(1, 2)` before loop, `TTI_SFPENCC(0, 2)` after loop
- `p_sfpu::` constants for register and mode identifiers

### Reference features NOT present in the target (DROP these)
- `exponent_size_8` parameter - Quasar does not need FP16 bias adjustment for zero check
- `_sfpu_is_fp16_zero_()` dependency - Not present on Quasar
- SFPI types and abstractions (`vFloat`, `dst_reg`, `vConst1`, `v_if`/`v_endif`) - Quasar uses TTI macros directly
- `sfpi.h` include - Not used on Quasar

## Format Support

### Format Domain
**float-only**

The sign function maps floating-point values to {-1.0, 0.0, +1.0}. The operation is mathematically defined for floating-point numbers. Integer sign can be computed but the output is always float, and the SFPU operates on FP32 internally.

### Applicable Formats for Testing
Based on the operation type, target architecture support, and existing test patterns (abs, threshold, elu all use Float16/Float16_b/Float32):

| Format | Applicable | Rationale |
|--------|-----------|-----------|
| Float16 | Yes | Standard FP16 (E5M10), primary test format per existing Quasar SFPU patterns |
| Float16_b | Yes | BFloat16 (E8M7), most common Quasar format, primary test format |
| Float32 | Yes | Full precision FP32, requires dest_acc=Yes for 32-bit Dest mode |
| Int32 | No | Sign is a float-domain operation; output is always float |
| Int16 | No | Same as Int32 |
| Int8 | No | Same as Int32 |
| UInt8 | No | Same as Int32; unsigned values don't have negative sign |
| MxFp8R | No | While technically MX formats unpack to Float16_b and could work, existing Quasar SFPU tests for similar operations (abs, threshold, elu) do not include MX formats. Keeping format list consistent with established patterns. |
| MxFp8P | No | Same as MxFp8R |

### Format-Dependent Code Paths
The reference kernel has one format-dependent code path:
- `_sfpu_is_fp16_zero_(v, exponent_size_8)`: The `exponent_size_8` parameter selects between FP16a and FP16b zero detection logic. On Blackhole, FP16a inputs have an unconditional bias added to the exponent, so a special check is needed.

**This format dependency does NOT apply to Quasar.** On Quasar, the SFPU operates on FP32 values in LREGs (loaded via SFPLOAD with implied format conversion). The zero check via `TTI_SFPSETCC(mode=6)` works on the FP32 representation directly, regardless of the original L1 format. The kernel is therefore format-agnostic on Quasar.

### Format Constraints
- **Float32** requires `dest_acc=Yes` (32-bit Dest mode) for both input and output
- **Cross-exponent-family** conversions (Float16_b input -> Float16 output) require `dest_acc=Yes`
- **Float16_b -> Float16** output without dest_acc is an outlier case (requires Float32 intermediate in Dest)
- **Same-family** formats (Float16->Float16, Float16_b->Float16_b) work with `dest_acc=No`
- **Integer formats** are excluded entirely (float-only operation)

## Existing Target Implementations
The following Quasar SFPU kernels exist and provide pattern guidance:

| Kernel | Pattern Relevance | Key Similarity |
|--------|-------------------|----------------|
| `ckernel_sfpu_abs.h` | Simple load-transform-store, no CC needed | Basic SFPLOAD/SFPABS/SFPSTORE pattern |
| `ckernel_sfpu_threshold.h` | Uses CC with SFPGT + conditional SFPMOV | Conditional execution with comparison |
| `ckernel_sfpu_elu.h` | Uses SFPSETCC(mode=0) for negative check + CC | Negative check pattern (closest match) |
| `ckernel_sfpu_lrelu.h` | Uses SFPSETCC(mode=0) for negative + conditional MAD | Negative check + conditional operation |
| `ckernel_sfpu_activations.h` (hardsigmoid) | Uses SFPSETCC(mode=0) + SFPLOADI for zero + SFPENCC resets | Multiple CC checks in sequence |
| `ckernel_sfpu_fill.h` | Uses SFPLOADI for constant loading | Constant loading pattern |

**Most relevant pattern**: ELU (negative check with SFPSETCC mode=0) combined with hardsigmoid (multiple sequential CC checks with SFPENCC resets). The sign kernel needs:
1. SFPSETCC mode=0 for negative check (like ELU)
2. SFPCOMPC for "else" (non-negative) branch (new pattern - SFPCOMPC exists in ISA but not used in existing kernels)
3. SFPSETCC mode=6 for zero check (seen in trigonometry.h and log.h)

**Alternative approach** (avoids SFPCOMPC): Use two sequential SFPSETCC checks with SFPENCC resets between them, similar to hardsigmoid's pattern. This is safer since SFPCOMPC has not been proven in existing Quasar kernels.

## Sub-Kernel Phases

| Phase | Name | Functions | Dependencies |
|-------|------|-----------|--------------|
| 1 | sign | `_calculate_sign_sfp_rows_`, `_calculate_sign_` | none |

**Ordering rationale**: This is a simple SFPU kernel with a single sub-kernel. The inner function `_calculate_sign_sfp_rows_()` processes one iteration of rows, and the outer function `_calculate_sign_(const int iterations)` handles setup (constant loading, CC enable), the iteration loop, and teardown (CC disable). Both functions form a single logical unit and must be written together.

There is no init function needed (unlike hardsigmoid which has `_init_hardsigmoid_()`) because the sign constants can be loaded inside the outer `_calculate_sign_` function before the loop, following the pattern of elu, lrelu, threshold, and fill.
