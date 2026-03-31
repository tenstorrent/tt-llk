# Analysis: trigonometry

## Kernel Type
sfpu

## Reference File
`tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_trigonometry.h`

## Purpose
Implements trigonometric and inverse hyperbolic functions: sine, cosine, acosh, asinh, and atanh. The sine/cosine functions use Maclaurin series polynomial approximation with range reduction. The inverse hyperbolic functions (acosh, asinh, atanh) compose existing sqrt, log, and reciprocal primitives.

## Algorithm Summary

### Sine (sin)
1. Range reduction: scale input by 1/pi to get number of half-periods
2. Extract integer part n (which half-period) and fractional part f
3. Scale fractional part back by pi to get radians in [-pi, pi]
4. Evaluate Maclaurin series: x - x^3/3! + x^5/5! - x^7/7! (+ x^9/9! - x^11/11! in non-approx mode)
5. If n is odd, negate the result (sign correction)

### Cosine (cos)
Same range reduction as sine, then:
1 - x^2/2! + x^4/4! - x^6/6! (+ x^8/8! - x^10/10! in non-approx mode)
If n is odd, negate the result.

### Acosh (inverse hyperbolic cosine)
acosh(x) = log(x + sqrt(x^2 - 1)) for x >= 1. Returns NaN for x < 1, returns 0 for x == 1.

### Asinh (inverse hyperbolic sine)
asinh(x) = sign(x) * log(|x| + sqrt(x^2 + 1))

### Atanh (inverse hyperbolic tangent)
atanh(x) = 0.5 * ln((1+x)/(1-x)) for |x| < 1. Returns NaN for |x| > 1, returns +/-inf for x == +/-1.

## Template Parameters
| Parameter | Purpose | Values |
|-----------|---------|--------|
| APPROXIMATION_MODE | Controls polynomial order (fewer terms = faster but less accurate) | true/false |
| ITERATIONS | Number of tile rows to process (compile-time for BH; runtime for Quasar) | typically 32 |
| is_fp32_dest_acc_en | Whether Dest accumulator is in FP32 mode (atanh only) | true/false |

## Functions Identified
| Function | Purpose | Complexity |
|----------|---------|------------|
| `_sfpu_sine_maclaurin_series_<APPROXIMATION_MODE>` | Helper: evaluate sine Maclaurin polynomial | Medium |
| `_sfpu_cosine_maclaurin_series_<APPROXIMATION_MODE>` | Helper: evaluate cosine Maclaurin polynomial | Medium |
| `_calculate_sine_<APPROXIMATION_MODE, ITERATIONS>` | Compute sin(x) for tile rows | High |
| `_calculate_cosine_<APPROXIMATION_MODE, ITERATIONS>` | Compute cos(x) for tile rows | High |
| `_calculate_acosh_<APPROXIMATION_MODE, ITERATIONS>` | Compute acosh(x) for tile rows | High |
| `_calculate_asinh_<APPROXIMATION_MODE, ITERATIONS>` | Compute asinh(x) for tile rows | High |
| `_calculate_atanh_<APPROXIMATION_MODE, is_fp32_dest_acc_en, ITERATIONS>` | Compute atanh(x) for tile rows | High |
| `_init_inverse_hyperbolic_<APPROXIMATION_MODE>` | Init for acosh/asinh (delegates to sqrt init) | Low |
| `_init_atanh_<APPROXIMATION_MODE>` | Init for atanh (delegates to reciprocal init) | Low |

## Key Constructs Used

### SFPI Vector Types and Operations (Blackhole-specific)
- `sfpi::vFloat`: 32-lane SIMD float vector type. Maps to LREG on Quasar (single lane per SFPU iteration, 2 rows processed per pass).
- `sfpi::vInt`: 32-lane SIMD integer vector type. Maps to integer interpretation of LREG.
- `sfpi::dst_reg[0]`: Read/write to current Dest register tile row. Maps to TTI_SFPLOAD/TTI_SFPSTORE on Quasar.
- `sfpi::dst_reg++`: Advance Dest register pointer. Maps to `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()` on Quasar.
- `sfpi::float_to_int16(v, 0)`: Convert float to 16-bit integer (truncate toward zero). Maps to TTI_SFPCAST on Quasar.
- `sfpi::int32_to_float(v, 0)`: Convert integer to float. Maps to TTI_SFPCAST on Quasar.
- `sfpi::abs(inp)`: Absolute value. Maps to TTI_SFPABS or similar on Quasar.
- `sfpi::setsgn(val, sign_src)`: Copy sign from one value to another. Maps to TTI_SFPSETSGN on Quasar.
- `sfpi::vConst0`, `sfpi::vConst1`: Constants 0.0 and 1.0. Map to p_sfpu::LCONST_0 and p_sfpu::LCONST_1 on Quasar.
- `v_if`/`v_elseif`/`v_else`/`v_endif`: Conditional execution per lane. Maps to TTI_SFPSETCC/TTI_SFPPUSHC/TTI_SFPCOMPC/TTI_SFPPOPC/TTI_SFPENCC on Quasar.

### Arithmetic Operations
- Scalar multiply: `val * constant` -> TTI_SFPMUL or TTI_SFPMAD
- Scalar add: `val + constant` -> TTI_SFPADD or TTI_SFPMAD
- Fused multiply-add used extensively in Maclaurin polynomial evaluation
- Integer AND: `whole_v & 0x1` -> TTI_SFPAND (available on Quasar)

### External Dependencies (acosh, asinh, atanh)
- `_calculate_sqrt_body_<APPROXIMATION_MODE>(tmp)`: Inline sqrt computation. On Quasar, the equivalent is TTI_SFPNONLINEAR with SQRT_MODE.
- `_calculate_log_body_no_init_(tmp)`: Inline log computation without init. On Quasar, this is the full log computation sequence (polynomial evaluation with SFPSETEXP/SFPEXEXP/SFPMAD/SFPCAST chain from ckernel_sfpu_log.h).
- `_sfpu_reciprocal_<N>(den)`: Iterative reciprocal. On Quasar, TTI_SFPNONLINEAR with RECIP_MODE gives FP16_B-precision approximation.
- `_init_sqrt_<APPROXIMATION_MODE>()`: Init for sqrt LUT (Quasar sqrt is via SFPNONLINEAR, no separate init needed).
- `_init_sfpu_reciprocal_<APPROXIMATION_MODE>()`: Init for reciprocal LUT (Quasar recip is via SFPNONLINEAR, no separate init needed).
- `float_to_fp16b(tmp, 0)`: Convert to FP16_b (used in atanh for non-fp32 non-approx mode). No direct Quasar equivalent needed since SFPU operates in FP32 internally and precision truncation happens at store.
- `std::numeric_limits<float>::quiet_NaN()` and `std::numeric_limits<float>::infinity()`: Special float constants used in boundary conditions.

## Dependencies

### Reference (Blackhole) Dependencies
- `ckernel_sfpu_log.h` — for `_calculate_log_body_no_init_()`
- `ckernel_sfpu_recip.h` — for `_sfpu_reciprocal_<N>()`
- `ckernel_sfpu_sqrt.h` — for `_calculate_sqrt_body_<APPROXIMATION_MODE>()`
- `sfpi.h` — SFPI programming interface (vFloat, vInt, dst_reg, etc.)
- `<limits>` — for NaN and infinity constants

### Quasar Dependencies (will need)
- `ckernel_trisc_common.h` — standard Quasar kernel infrastructure
- `cmath_common.h` — `_incr_counters_`, `SFP_ROWS`, `_sfpu_load_config32_`
- `ckernel_sfpu_log.h` — for `_init_log_()` and `_calculate_log_sfp_rows_()` (reusable for inverse hyperbolic functions, or inline the log polynomial)
- `ckernel_sfpu_recip.h` — for `_calculate_reciprocal_sfp_rows_()` (atanh needs reciprocal)
- `ckernel_sfpu_sqrt.h` — for `_calculate_sqrt_sfp_rows_()` (acosh/asinh need sqrt)

## Complexity Classification
**Complex**

This is a complex kernel because:
1. **Sine/cosine require multi-step polynomial evaluation** with range reduction that involves float-to-integer conversion (SFPCAST), integer-to-float conversion (SFPCAST), integer AND operation (SFPAND), and conditional sign flipping (SFPSETCC/SFPMOV with sign inversion).
2. **No single hardware instruction** for sin/cos exists on either Blackhole or Quasar -- must be implemented as multi-instruction software sequences.
3. **Inverse hyperbolic functions compose multiple primitives** (sqrt, log, recip) that each have their own multi-instruction implementations on Quasar. On Blackhole these are inline function calls (SFPI handles the abstraction), but on Quasar each must be TTI macro sequences.
4. **Conditional execution** is needed for boundary cases (acosh x<1, atanh |x|>1) and sign correction (sine/cosine odd period), requiring SFPSETCC/SFPPUSHC/SFPPOPC/SFPENCC chains.
5. **Register pressure is high**: sine/cosine needs LREGs for input, intermediate powers, accumulated output, integer part, and constants. With only 8 LREGs (LREG0-7), careful register allocation is required.

## Constructs Requiring Translation

| Blackhole Construct | What it Does | Quasar Equivalent |
|---------------------|-------------|-------------------|
| `sfpi::vFloat` | 32-lane vector float | LREGs (LREG0-7), 32-bit each |
| `sfpi::vInt` | 32-lane vector integer | LREGs in integer interpretation |
| `sfpi::dst_reg[0]` read | Load current Dest row to LREG | TTI_SFPLOAD(lreg, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0) |
| `sfpi::dst_reg[0]` write | Store LREG to current Dest row | TTI_SFPSTORE(lreg, 0, ADDR_MOD_7, 0, 0) |
| `sfpi::dst_reg++` | Advance Dest pointer | `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()` |
| `val * constant` | Scalar multiply | TTI_SFPMUL or TTI_SFPMAD |
| `val + constant` | Scalar add | TTI_SFPADD or TTI_SFPMAD |
| `float_to_int16(v, 0)` | Float to int conversion (truncate) | TTI_SFPCAST(src, dst, 4) (FP32->SMAG32) |
| `int32_to_float(v, 0)` | Int to float conversion | TTI_SFPCAST(src, dst, 0) (SMAG32->FP32) |
| `whole_v & 0x1` | Bitwise AND | TTI_SFPAND or TTI_SFPIADD-based approach |
| `v_if(whole_v != 0)` | Conditional execution (non-zero test) | TTI_SFPSETCC + TTI_SFPENCC/SFPPUSHC/SFPPOPC |
| `v * -1` (sign flip) | Negate value | TTI_SFPMOV(src, dst, 1) (copy with sign inversion) |
| `sfpi::abs(inp)` | Absolute value | TTI_SFPABS(src, dst) or use SFPSETSGN |
| `sfpi::setsgn(val, sign)` | Set sign bit from another value | TTI_SFPSETSGN |
| `sfpi::vConst0` / `sfpi::vConst1` | Constants 0.0 / 1.0 | p_sfpu::LCONST_0 / p_sfpu::LCONST_1 |
| `v_if` / `v_elseif` / `v_else` / `v_endif` | Nested conditional execution | SFPSETCC/SFPPUSHC/SFPCOMPC/SFPPOPC/SFPENCC |
| `_calculate_sqrt_body_<>(tmp)` | Inline sqrt of LREG value | TTI_SFPNONLINEAR(src, dst, SQRT_MODE) |
| `_sfpu_reciprocal_<N>(den)` | Iterative reciprocal | TTI_SFPNONLINEAR(src, dst, RECIP_MODE) |
| `_calculate_log_body_no_init_(tmp)` | Inline log without init | Full log sequence from ckernel_sfpu_log.h |
| `std::numeric_limits<float>::quiet_NaN()` | NaN constant (0x7FC00000) | TTI_SFPLOADI with 0x7FC0 (FP16_B) or split 32-bit load |
| `std::numeric_limits<float>::infinity()` | Infinity constant (0x7F800000) | TTI_SFPLOADI with 0x7F80 (FP16_B) |

## Target Expected API

### Function signatures the target test infrastructure expects
Based on the shared test harness (`tests/helpers/include/sfpu_operations.h`), the following calling conventions are used:

1. **Sine**: `_calculate_sine_<APPROX_MODE, ITERATIONS>(ITERATIONS)` — called with both template and runtime iterations parameter
2. **Cosine**: `_calculate_cosine_<APPROX_MODE, ITERATIONS>(ITERATIONS)` — same pattern
3. **Acosh**: `_init_inverse_hyperbolic_<APPROX_MODE>()` then `_calculate_acosh_<APPROX_MODE, ITERATIONS>()`
4. **Asinh**: `_init_inverse_hyperbolic_<APPROX_MODE>()` then `_calculate_asinh_<APPROX_MODE, ITERATIONS>()`
5. **Atanh**: `_init_atanh_<APPROX_MODE>()` then `_calculate_atanh_<APPROX_MODE, is_fp32_dest_acc_en, ITERATIONS>()`

However, the **Quasar pattern** (observed in all existing Quasar SFPU kernels) is different:
- No template parameters on the `_calculate_*_` functions (or only APPROXIMATION_MODE)
- `iterations` passed as `const int` runtime parameter
- Functions use `_sfp_rows_()` inner helper + outer loop with `_incr_counters_`
- Init functions may have no template parameters at all

### Template params to KEEP from reference
- `APPROXIMATION_MODE` (bool) — controls polynomial order for sine/cosine

### Template params to DROP (reference-only, not used on Quasar)
- `ITERATIONS` as template parameter — Quasar uses runtime `const int iterations` instead
- `is_fp32_dest_acc_en` — Quasar handles this at the test/infrastructure level, not in kernel code

### Template params to ADD (target-only)
- None identified

### Target-only features NOT in reference
- `_sfp_rows_()` inner function pattern (compute for 2 rows)
- `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()` for dest pointer advancement
- TTI macro instruction interface
- SFPNONLINEAR for sqrt/recip/exp (hardware approximation, not software iterative)

### Reference-only features to DROP
- SFPI library abstractions (vFloat, vInt, dst_reg, v_if, etc.) — not available on Quasar
- `float_to_fp16b()` conversion in atanh non-approx path — Quasar SFPU operates in FP32 internally, precision truncation is handled by the packer
- Software iterative reciprocal (`_sfpu_reciprocal_<N>`) — use hardware SFPNONLINEAR RECIP_MODE instead
- Software iterative sqrt (`_calculate_sqrt_body_`) — use hardware SFPNONLINEAR SQRT_MODE instead
- Inline log body (`_calculate_log_body_no_init_`) — need to call the existing Quasar log primitives or implement inline

**IMPORTANT NOTE on function signatures**: The shared test harness (sfpu_operations.h lines 46-62, 155-157) calls these functions with Blackhole-style signatures. However, the Quasar test infrastructure uses a **different test file pattern** (per-kernel C++ files like `sfpu_log_quasar_test.cpp`) that call functions directly with Quasar-style signatures. The trigonometry kernel MUST match the Quasar calling convention, NOT the Blackhole sfpu_operations.h convention.

Quasar-style signature pattern (from existing kernels):
- `_calculate_sine_(const int iterations)` (no template params, or only APPROXIMATION_MODE)
- `_calculate_cosine_(const int iterations)`
- `_init_log_()` (no template params)
- etc.

## Format Support

### Format Domain
**Float-only** — Sine, cosine, and inverse hyperbolic functions are transcendental floating-point operations. They are mathematically undefined for integer types.

### Applicable Formats for Testing
Based on the operation type, Quasar architecture support (from format_config.py QUASAR_DATA_FORMAT_ENUM_VALUES), and existing test patterns (test_sfpu_log_quasar.py):

| Format | Applicable | Rationale |
|--------|-----------|-----------|
| Float16 | Yes | Standard 16-bit float; unpacked to FP32 for SFPU math; primary test format |
| Float16_b | Yes | BFloat16; unpacked to FP32 for SFPU math; primary test format |
| Float32 | Yes | Direct FP32; requires dest_acc=Yes for 32-bit Dest mode |
| Int32 | No | Integer — transcendental trig is undefined for integer types |
| Int16 | No | Integer — not applicable |
| Int8 | No | Integer — not applicable |
| UInt8 | No | Integer — not applicable |
| MxFp8R | Yes (with care) | L1-only format; unpacked to Float16_b for math; requires implied_math_format=Yes; may have quantization error amplified by trig functions |
| MxFp8P | Possibly (exclude initially) | Same as MxFp8R but lower precision; log test excludes it due to quantization error amplification; trig functions have similar sensitivity |

### Format-Dependent Code Paths
The reference trigonometry kernel is essentially **format-agnostic** — there are no conditional branches based on data format in the sine/cosine/acosh/asinh functions. The SFPU always operates in FP32 internally regardless of input format.

The **one exception** is in `_calculate_atanh_`:
```cpp
if constexpr (is_fp32_dest_acc_en || APPROXIMATION_MODE)
{
    den = tmp;  // keep FP32 precision
}
else
{
    den = sfpi::reinterpret<sfpi::vFloat>(float_to_fp16b(tmp, 0));  // truncate to FP16_b
}
```
This path truncates the reciprocal result to FP16_b when not in FP32 mode and not in approximation mode. On Quasar, since SFPU operates in FP32 internally and SFPNONLINEAR RECIP_MODE produces FP16_B-precision output already, this distinction is less meaningful. The Quasar implementation can skip this `float_to_fp16b` call.

### Format Constraints
- **MX formats** (MxFp8R, MxFp8P) require `implied_math_format=Yes` in test configuration
- **Float32 input** requires `dest_acc=Yes` (32-bit Dest mode)
- **Float32 output from non-Float32 input** requires `dest_acc=Yes` (packer limitation)
- **Float32 input to Float16 output** requires `dest_acc=Yes` (cross-precision)
- **Integer and float formats cannot be mixed** in input->output
- **MxFp8 input -> non-MxFp8 output**: May cause golden mismatch due to MxFp8 quantization error amplified by trig functions (same issue as log test)

## Existing Target Implementations

### Existing Quasar SFPU Kernels (pattern reference)
The following Quasar SFPU kernels exist and inform the implementation pattern:

1. **Simple SFPNONLINEAR ops** (tanh, exp, recip, sqrt, relu): Load -> SFPNONLINEAR -> Store. Single instruction computation.
2. **LUT-based ops** (gelu): Init loads coefficients via `_sfpu_load_config32_`, compute uses SFPLUTFP32 + SFPMAD.
3. **Multi-step computation** (sigmoid, silu): Chain of SFPMOV/SFPNONLINEAR/SFPADD/SFPMUL operations.
4. **Conditional ops** (lrelu, threshold, elu, relu_max, relu_min): Use SFPSETCC + SFPMOV/SFPLOADI + SFPENCC for conditional per-lane execution.
5. **Polynomial + special cases** (log): SFPSETEXP + SFPMAD chain + SFPEXEXP + SFPCAST + SFPSETCC for boundary handling.

**The trigonometry kernel combines patterns from (3), (4), and (5)**: multi-step polynomial computation (like log), conditional execution (like threshold/lrelu), and composition of existing operations (like silu uses sigmoid).

### Key patterns from existing Quasar kernels
- All kernels use `_sfp_rows_()` inner function + outer loop with `_incr_counters_`
- Constants loaded via `TTI_SFPLOADI` with BF16 encoding (format=0) for simple constants
- Full 32-bit constants loaded via `TTI_SFPLOADI(lreg, 0x8, hi16)` then `TTI_SFPLOADI(lreg, 0xA, lo16)`
- Conditional execution pattern: `TTI_SFPENCC(1, 2)` to enable CC before loop, `TTI_SFPENCC(0, 2)` to disable after loop, with `TTI_SFPSETCC`/`TTI_SFPENCC(0, 0)` per-iteration
- No existing Quasar kernel uses SFPCAST for float-to-int or int-to-float conversion in a math context (only in typecast kernels). The log kernel uses SFPCAST(mode=2) for two's-complement-to-sign-magnitude and SFPCAST(mode=0) for sign-magnitude-to-FP32.

## Sub-Kernel Phases

The trigonometry kernel contains 5 distinct sub-kernels (sine, cosine, acosh, asinh, atanh) with two init functions. These should be grouped into 3 phases based on complexity and dependencies.

| Phase | Name | Functions | Dependencies |
|-------|------|-----------|--------------|
| 1 | sine_cosine | `_calculate_sine_(iterations)`, `_calculate_cosine_(iterations)` | none |
| 2 | acosh_asinh | `_init_inverse_hyperbolic_()`, `_calculate_acosh_(iterations)`, `_calculate_asinh_(iterations)` | Phase 1 (file must exist), depends on existing log.h and sqrt.h |
| 3 | atanh | `_init_atanh_()`, `_calculate_atanh_(iterations)` | Phase 2 (file must exist), depends on existing log.h and recip.h |

### Phase Rationale

**Phase 1 (sine_cosine)**: These two functions share the same algorithmic pattern (Maclaurin series + range reduction). They use only basic SFPU operations (SFPLOAD, SFPSTORE, SFPMAD, SFPMUL, SFPADD, SFPCAST, SFPIADD/SFPAND, SFPSETCC, SFPMOV, SFPENCC) and do not depend on other kernel files. This is the most independent and testable phase.

**Phase 2 (acosh_asinh)**: These two functions share the `_init_inverse_hyperbolic_()` initializer and both depend on sqrt and log primitives. They are simpler algorithmically (compose existing operations) but depend on including and calling functions from `ckernel_sfpu_log.h` and `ckernel_sfpu_sqrt.h`. The init function for inverse hyperbolic is shared between them.

**Phase 3 (atanh)**: This function has its own init (`_init_atanh_()`) and depends on reciprocal and log. It is separate because it has additional complexity (boundary condition handling for |x| >= 1 with NaN/inf, and the reciprocal composition) and a different init function. It also has the `is_fp32_dest_acc_en` template parameter in the reference (though on Quasar this may not be needed).

### Note on sine/cosine complexity
Sine and cosine are the most complex individual functions in this kernel due to the range reduction + polynomial evaluation + conditional sign correction. On Quasar, the range reduction requires:
1. Load 1/pi constant -> SFPMUL to scale
2. SFPCAST (FP32->SMAG32) to get integer part
3. SFPCAST (SMAG32->FP32) to convert back
4. SFPADD to get fractional part
5. SFPMUL to scale back by pi
6. Chain of SFPMAD for Maclaurin terms
7. SFPAND to check if integer part is odd (bit 0)
8. Conditional SFPMOV with sign inversion for sign correction

This requires careful register allocation across all 8 LREGs.
