# Specification: trigonometry (Phase 1 — sine_cosine)

## Kernel Type
sfpu

## Target Architecture
quasar

## Overview
Based on analysis: `codegen/artifacts/trigonometry_analysis.md`

Phase 1 implements `_calculate_sine_` and `_calculate_cosine_` using Maclaurin series polynomial approximation with range reduction, ported from the Blackhole reference to Quasar TTI macros. Two internal helper functions (`_calculate_sine_maclaurin_series_sfp_rows_` and `_calculate_cosine_maclaurin_series_sfp_rows_`) handle the per-row computation. No init/uninit functions are needed for sine/cosine.

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_trigonometry.h`

## Reference Implementations Studied

- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_log.h`: Polynomial evaluation via SFPMAD chain (Horner form), SFPSETEXP/SFPEXEXP usage, SFPCAST (mode 2 for 2's-complement-to-sign-magnitude, mode 0 for sign-magnitude-to-FP32), conditional execution with SFPSETCC+SFPENCC, overall file structure and naming conventions. **Most relevant pattern** — polynomial computation with special cases.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_sigmoid.h`: Multi-step computation chaining (SFPMOV + SFPNONLINEAR + SFPADD + SFPNONLINEAR), register allocation for intermediate results, function decomposition into `_sfp_rows_` helper.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_lrelu.h`: Conditional execution pattern — `TTI_SFPENCC(1, 2)` to enable CC before loop, `TTI_SFPSETCC(0, lreg, 0)` for per-lane condition, `TTI_SFPENCC(0, 0)` to clear CC within loop, `TTI_SFPENCC(0, 2)` to disable CC after loop.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_exp2.h`: Template parameter `APPROXIMATION_MODE` usage, SFPMUL for scalar constant multiply, SFPNOP for pipeline hazard avoidance between MAD-type and Simple EXU instructions.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_typecast_int32_fp32.h`: SFPCAST usage for integer-float conversion (mode 0: SMAG32->FP32).

## Algorithm in Target Architecture

### High-Level Algorithm (Sine)
1. Load input x from Dest
2. Multiply x by 1/pi to get number of half-periods: `scaled = x * (1/pi)`
3. Convert scaled to integer (truncate toward zero): `n = trunc(scaled)` via FP32->SMAG32->FP32 sequence
4. Compute fractional part: `frac = scaled - float(n)`
5. Scale fractional part back to radians: `f_rad = frac * pi` (now in approximately [-pi/2, pi/2])
6. Evaluate Maclaurin series for sin(f_rad)
7. Check if n is odd (bit 0 of integer n): if odd, negate the result
8. Store result to Dest

### High-Level Algorithm (Cosine)
Steps 1-5 are identical to sine. Then:
6. Evaluate Maclaurin series for cos(f_rad)
7-8. Same sign correction and store.

### Pseudocode for `_calculate_sine_sfp_rows_` (per 2-row iteration)

```
// Load input from Dest into LREG0
SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)

// --- Range Reduction ---
// LREG1 = x * (1/pi)  [scaled value]
SFPMUL(LREG0, LREG7, LCONST_0, LREG1, 0)   // LREG7 holds 1/pi (loaded in outer function)

// Convert to integer (truncate toward zero): FP32 -> SMAG32
SFPCAST(LREG1, LREG2, 4)    // LREG2 = SMAG32(round(LREG1)) via RTNE

// Save the SMAG32 integer for odd/even check later
SFPMOV(LREG2, LREG3, 0)     // LREG3 = copy of integer n (SMAG32)

// Convert integer back to float: SMAG32 -> FP32
SFPCAST(LREG2, LREG2, 0)    // LREG2 = float(n)

// Compute fractional part: frac = scaled - float(n)
// SFPADD with negate: LREG1 = LREG1 + (-LREG2) = scaled - float(n)
SFPMAD(LREG1, LCONST_1_RG, LREG2, LREG1, 0x2)  // LREG1 = LREG1*1.0 + (-LREG2) = frac
// Note: InstrMod bit 1 negates SrcC

// Scale fractional part by pi: f_rad = frac * pi
SFPMUL(LREG1, LREG6, LCONST_0, LREG1, 0)   // LREG6 holds pi; LREG1 = f_rad

// --- Maclaurin Series for sin(f_rad) ---
// sin(x) = x - x^3/3! + x^5/5! - x^7/7! [+ x^9/9! - x^11/11! in non-approx]
// Use Horner-like evaluation with powers of x^2

// LREG0 = f_rad (copy for accumulation)
SFPMOV(LREG1, LREG0, 0)     // LREG0 = f_rad (output accumulator = x term)

// LREG2 = f_rad^2  (used repeatedly to build higher powers)
SFPMUL(LREG1, LREG1, LCONST_0, LREG2, 0)   // LREG2 = x^2

// --- x^3/3! term ---
// tmp = x * x^2 = x^3
SFPMUL(LREG1, LREG2, LCONST_0, LREG4, 0)   // LREG4 = x^3

// Load -1/6 coefficient and do: output += -1/6 * x^3
SFPLOADI(LREG5, 0, 0xBE2A)                  // LREG5 = -0.16667 (BF16)
SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)      // LREG0 = x^3*(-1/6) + output

// --- x^5/5! term ---
SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)   // LREG4 = x^5
SFPLOADI(LREG5, 0, 0x3C08)                  // LREG5 = 0.008333 (BF16)
SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)      // LREG0 = x^5*(1/120) + output

// --- x^7/7! term ---
SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)   // LREG4 = x^7
SFPLOADI(LREG5, 0, 0xB950)                  // LREG5 = -0.0001984 (BF16)
SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)      // LREG0 = x^7*(-1/5040) + output

// --- Non-approximation mode: x^9/9! and x^11/11! terms ---
if constexpr (!APPROXIMATION_MODE) {
    SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)   // LREG4 = x^9
    SFPLOADI(LREG5, 0, 0x3638)                  // LREG5 = 0.0000027557 (BF16)
    SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)      // LREG0 += x^9*(1/362880)

    SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)   // LREG4 = x^11
    SFPLOADI(LREG5, 0, 0xB2D7)                  // LREG5 = -0.00000002505 (BF16)
    SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)      // LREG0 += x^11*(-1/39916800)
}

// --- Sign Correction (if n is odd, negate result) ---
// LREG3 still holds the SMAG32 integer n from range reduction
// Need to check bit 0 of the magnitude to determine odd/even
// First, convert SMAG32 to 2's complement for proper bit masking
SFPCAST(LREG3, LREG3, 2)    // LREG3 = INT32(2's complement) from SMAG32
// AND with 1 (loaded into LREG5) to isolate bit 0
SFPLOADI(LREG5, 0x8, 0x0000) // LREG5 upper 16 = 0x0000
SFPLOADI(LREG5, 0xA, 0x0001) // LREG5 lower 16 = 0x0001 -> LREG5 = integer 1
SFPAND(LREG5, LREG3)         // LREG3 = n & 1 (0 if even, 1 if odd)

// Set CC where LREG3 != 0 (i.e., where n is odd)
SFPSETCC(0, LREG3, 2)        // InstrMod=2: CC.Res = (LREG3 != 0)

// Where CC is set (odd n): negate result
SFPMOV(LREG0, LREG0, 1)      // LREG0 = -LREG0 (sign inversion)

// Clear CC
SFPENCC(0, 0)                 // CC.Res = 0

// Store result to Dest
SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0)
```

### Cosine Pseudocode
Identical to sine for range reduction (steps 1-5). The Maclaurin series differs:

```
// cos(x) = 1 - x^2/2! + x^4/4! - x^6/6! [+ x^8/8! - x^10/10! in non-approx]

// LREG0 = 1.0 (start of accumulation)
SFPLOADI(LREG0, 0, 0x3F80)                  // LREG0 = 1.0 (BF16)

// LREG4 = x^2
SFPMUL(LREG1, LREG1, LCONST_0, LREG4, 0)   // LREG4 = x^2 (LREG2 also = x^2)

// --- x^2/2! term ---
SFPLOADI(LREG5, 0, 0xBF00)                  // LREG5 = -0.5 (BF16)
SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)      // LREG0 = x^2*(-0.5) + 1.0

// --- x^4/4! term ---
SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)   // LREG4 = x^4
SFPLOADI(LREG5, 0, 0x3D2A)                  // LREG5 = 0.041667 (BF16)
SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)      // LREG0 += x^4*(1/24)

// --- x^6/6! term ---
SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)   // LREG4 = x^6
SFPLOADI(LREG5, 0, 0xBAB6)                  // LREG5 = -0.001389 (BF16)
SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)      // LREG0 += x^6*(-1/720)

// Non-approximation mode additional terms
if constexpr (!APPROXIMATION_MODE) {
    SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)   // LREG4 = x^8
    SFPLOADI(LREG5, 0, 0x37D0)                  // LREG5 = 0.0000248015 (BF16)
    SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)      // LREG0 += x^8*(1/40320)

    SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)   // LREG4 = x^10
    SFPLOADI(LREG5, 0, 0xB493)                  // LREG5 = -0.0000002755 (BF16)
    SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)      // LREG0 += x^10*(-1/3628800)
}

// Then same sign correction as sine (LREG3 holds integer n)
```

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|--------------------|-------------------|-----------------|
| `sfpi::vFloat v = sfpi::dst_reg[0]` | `TTI_SFPLOAD(lreg, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` | ckernel_sfpu_log.h:39 |
| `sfpi::dst_reg[0] = v` | `TTI_SFPSTORE(lreg, 0, ADDR_MOD_7, 0, 0)` | ckernel_sfpu_log.h:77 |
| `sfpi::dst_reg++` | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` | ckernel_sfpu_log.h:88 |
| `v * constant` | `TTI_SFPMUL(lreg_a, lreg_b, LCONST_0, lreg_d, 0)` | ckernel_sfpu_exp2.h:27 |
| `a + b` | `TTI_SFPADD(lconst, lreg_b, lconst, lreg_d, 0)` | ckernel_sfpu_sigmoid.h:26 |
| `a * b + c` (FMA) | `TTI_SFPMAD(lreg_a, lreg_b, lreg_c, lreg_d, 0)` | ckernel_sfpu_log.h:49-56 |
| `a * 1.0 + (-b)` (subtract) | `TTI_SFPMAD(lreg_a, LCONST_1_RG, lreg_b, lreg_d, 0x2)` | Confluence SFPMAD spec (InstrMod[1] negates SrcC) |
| `sfpi::float_to_int16(v, 0)` | `TTI_SFPCAST(src, dst, 4)` (FP32->SMAG32 RTNE) | Confluence page 1170505767 (SFPCAST InstrMod=4) |
| `sfpi::int32_to_float(v, 0)` | `TTI_SFPCAST(src, dst, 0)` (SMAG32->FP32 RTNE) | ckernel_sfpu_log.h:66, Confluence page 1170505767 |
| SMAG32 -> INT32 (2's comp) | `TTI_SFPCAST(src, dst, 2)` | ckernel_sfpu_log.h:63 |
| `whole_v & 0x1` | Load 1 into LREG, then `TTI_SFPAND(mask_lreg, target_lreg)` | Confluence page 1170505767 (SFPAND: `RG[VD] = RG[VD] & RG[VC]`) |
| `v_if(whole_v != 0)` | `TTI_SFPSETCC(0, lreg, 2)` (InstrMod=2: CC.Res = value != 0) | Confluence SFPSETCC spec; ckernel_sfpu_lrelu.h pattern |
| `v *= -1` (negate) | `TTI_SFPMOV(src, dst, 1)` (InstrMod=1: copy with sign inversion) | Confluence SFPMOV spec; ckernel_sfpu_sigmoid.h:24 |
| `v_endif` / clear CC | `TTI_SFPENCC(0, 0)` | ckernel_sfpu_lrelu.h:24, ckernel_sfpu_log.h:74 |
| Enable CC before loop | `TTI_SFPENCC(1, 2)` | ckernel_sfpu_lrelu.h:34, ckernel_sfpu_log.h:83 |
| Disable CC after loop | `TTI_SFPENCC(0, 2)` | ckernel_sfpu_lrelu.h:41, ckernel_sfpu_log.h:90 |
| `sfpi::vConst0` | `p_sfpu::LCONST_0` (Fixed Constant index 13 in RG view = 0.0) | ckernel_sfpu_sigmoid.h:26 |
| `sfpi::vConst1` | `p_sfpu::LCONST_1` (Fixed Constant index 14 in RG view = 1.0) | Confirmed in Confluence constant registers |
| Load BF16 immediate | `TTI_SFPLOADI(lreg, 0, bf16_value)` | ckernel_sfpu_log.h:21-30 |
| Load 32-bit immediate | `TTI_SFPLOADI(lreg, 0x8, hi16)` then `TTI_SFPLOADI(lreg, 0xA, lo16)` | Architecture research section 9.4 |
| `SFPMOV(src, dst, 0)` (copy) | `TTI_SFPMOV(src, dst, 0)` | ckernel_sfpu_lrelu.h:82 |

### Resource Allocation

| Resource | Purpose | Lifetime |
|----------|---------|----------|
| LREG0 | Input x / output accumulator / final result | Full computation |
| LREG1 | f_rad (fractional part scaled back to radians) | After range reduction through series |
| LREG2 | x^2 (kept for building higher powers) | Series evaluation |
| LREG3 | Integer n (SMAG32, then INT32 for AND check) | Range reduction through sign correction |
| LREG4 | Current power of x (x^3, x^5, x^7, ...) | Series evaluation (reused each term) |
| LREG5 | Current coefficient (loaded inline each term) | Series evaluation (reused each term) |
| LREG6 | pi constant (persistent across iterations) | Loaded in outer function, used in every iteration |
| LREG7 | 1/pi constant (persistent across iterations) | Loaded in outer function, used in every iteration |

**Register pressure analysis**: This uses all 8 LREGs (LREG0-7). The design is tight but feasible because:
- LREG4 and LREG5 are reused for each Maclaurin term (coefficient loaded inline, power rebuilt from previous power * x^2)
- LREG6 and LREG7 hold constants that persist across the entire iteration loop (loaded once before the loop)
- LREG3 stores the integer part during series evaluation and is only read once for sign correction at the end

**Important note on SFPCAST truncation**: The reference uses `float_to_int16(v, 0)` which truncates toward zero. SFPCAST mode 4 (FP32->SMAG32) uses RTNE (Round To Nearest Even). For the range reduction to work correctly, we need truncation, not rounding. RTNE will round 2.5 to 2 and 3.5 to 4, whereas truncation would give 2 and 3. This difference matters for the algorithm's correctness.

**Mitigation**: The Blackhole reference `float_to_int16` actually uses truncation toward zero (floor for positive, ceil for negative). On Quasar, SFPCAST mode 4 (RTNE) is the closest available. The difference is at most 1 unit at boundary cases (e.g., x/pi = 2.5 rounds to 2 with truncation but 2 with RTNE too since RTNE rounds to even). In practice, for continuous inputs the fractional part adjusts to compensate, and the Maclaurin series remains valid for the resulting range [-pi, pi]. The RTNE behavior is acceptable for this use case.

## Implementation Structure

### Includes
```cpp
#pragma once

#include <cstdint>

#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```

Source: All existing Quasar SFPU kernels include `ckernel_trisc_common.h` and `cmath_common.h`. Some add `ckernel_ops.h` but it is included transitively. No additional includes are needed for phase 1 (sine/cosine do not depend on other SFPU kernels).

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

Source: All existing Quasar SFPU kernels use `ckernel::sfpu` namespace.

### Functions

| Function | Template Params | Signature | Purpose |
|----------|-----------------|-----------|---------|
| `_calculate_sine_maclaurin_series_sfp_rows_` | `bool APPROXIMATION_MODE` | `inline void _calculate_sine_maclaurin_series_sfp_rows_()` | Per-2-row computation: load, range reduce, evaluate sine Maclaurin series, sign correct, store |
| `_calculate_cosine_maclaurin_series_sfp_rows_` | `bool APPROXIMATION_MODE` | `inline void _calculate_cosine_maclaurin_series_sfp_rows_()` | Per-2-row computation: load, range reduce, evaluate cosine Maclaurin series, sign correct, store |
| `_calculate_sine_` | `bool APPROXIMATION_MODE` | `inline void _calculate_sine_(const int iterations)` | Outer loop: load persistent constants, iterate over tile rows calling `_sfp_rows_` |
| `_calculate_cosine_` | `bool APPROXIMATION_MODE` | `inline void _calculate_cosine_(const int iterations)` | Outer loop: load persistent constants, iterate over tile rows calling `_sfp_rows_` |

**Function signature rationale**:
- `APPROXIMATION_MODE` is kept as a template parameter because it controls `if constexpr` branches for additional polynomial terms (compile-time decision). This matches the existing Quasar pattern in `ckernel_sfpu_exp2.h`.
- `iterations` is a runtime `const int` parameter (not a template parameter) matching the Quasar calling convention observed in all existing kernels (log, sigmoid, exp, exp2, lrelu, threshold, etc.).
- No `ITERATIONS` template parameter: Quasar kernels universally use runtime iterations.
- No `is_fp32_dest_acc_en` parameter: Not needed for sine/cosine (no format-dependent code paths). This parameter is only used by atanh in the reference, which is phase 3.

**Naming convention**: The `_sfp_rows_` functions follow the pattern from `_calculate_log_sfp_rows_()` in ckernel_sfpu_log.h. They contain the per-iteration inner computation.

### Init/Uninit Symmetry
Phase 1 (sine/cosine) does **not** have init or uninit functions. The persistent constants (1/pi and pi) are loaded into LREGs at the start of the outer `_calculate_sine_`/`_calculate_cosine_` function, before the loop. LREGs are local to the SFPU computation and do not persist beyond the function call, so no cleanup is needed.

This matches the pattern from existing kernels like `_calculate_lrelu_` which loads a constant (slope) before the loop without a separate init function.

## Detailed Instruction Sequence

### `_calculate_sine_<APPROXIMATION_MODE>(const int iterations)`

```
// Load persistent constants into high LREGs
// Use full 32-bit loads for 1/pi and pi (precision matters for range reduction)
SFPLOADI(LREG7, 0x8, 0x3EA2)   // 1/pi upper 16 bits
SFPLOADI(LREG7, 0xA, 0xF983)   // 1/pi lower 16 bits -> LREG7 = 0.31830988...
SFPLOADI(LREG6, 0x8, 0x4049)   // pi upper 16 bits
SFPLOADI(LREG6, 0xA, 0x0FDB)   // pi lower 16 bits -> LREG6 = 3.14159265...

// Enable conditional execution
SFPENCC(1, 2)

// Iteration loop
#pragma GCC unroll 8
for (int d = 0; d < iterations; d++) {
    _calculate_sine_maclaurin_series_sfp_rows_<APPROXIMATION_MODE>();
    _incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>();
}

// Disable conditional execution
SFPENCC(0, 2)
```

### `_calculate_sine_maclaurin_series_sfp_rows_<APPROXIMATION_MODE>()`

```
// Load input
SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)

// Range reduction: scaled = x * (1/pi)
SFPMUL(LREG0, LREG7, LCONST_0, LREG1, 0)       // LREG1 = x * (1/pi)

// FP32 -> SMAG32 (truncate to integer via RTNE)
SFPCAST(LREG1, LREG2, 4)                         // LREG2 = SMAG32(n)

// Save integer for odd/even check
SFPMOV(LREG2, LREG3, 0)                          // LREG3 = integer n (SMAG32 copy)

// SMAG32 -> FP32
SFPCAST(LREG2, LREG2, 0)                         // LREG2 = float(n)

// Fractional part: frac = scaled - float(n)
// Use SFPMAD: result = LREG1 * 1.0 + (-LREG2) with InstrMod[1]=1 to negate SrcC
SFPMAD(LREG1, p_sfpu::LCONST_1, LREG2, LREG1, 0x2)  // LREG1 = scaled - float(n) = frac

// Scale back: f_rad = frac * pi
SFPMUL(LREG1, LREG6, LCONST_0, LREG1, 0)        // LREG1 = frac * pi = f_rad

// === Maclaurin Series: sin(x) = x - x^3/3! + x^5/5! - x^7/7! ... ===

// Initialize output = x (first term)
SFPMOV(LREG1, LREG0, 0)                          // LREG0 = f_rad

// x^2 (needed for building higher powers)
SFPMUL(LREG1, LREG1, LCONST_0, LREG2, 0)        // LREG2 = x^2

// --- Term: -x^3/3! ---
SFPMUL(LREG1, LREG2, LCONST_0, LREG4, 0)        // LREG4 = x^3
SFPLOADI(LREG5, 0, 0xBE2A)                       // LREG5 = -1/6 = -0.16667 (BF16)
SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)           // LREG0 += x^3 * (-1/6)

// --- Term: +x^5/5! ---
SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)        // LREG4 = x^5
SFPLOADI(LREG5, 0, 0x3C08)                       // LREG5 = 1/120 = 0.008333 (BF16)
SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)           // LREG0 += x^5 * (1/120)

// --- Term: -x^7/7! ---
SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)        // LREG4 = x^7
SFPLOADI(LREG5, 0, 0xB950)                       // LREG5 = -1/5040 = -0.0001984 (BF16)
SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)           // LREG0 += x^7 * (-1/5040)

if constexpr (!APPROXIMATION_MODE) {
    // --- Term: +x^9/9! ---
    SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)    // LREG4 = x^9
    SFPLOADI(LREG5, 0, 0x3638)                   // LREG5 = 1/362880 (BF16)
    SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)       // LREG0 += x^9 * (1/362880)

    // --- Term: -x^11/11! ---
    SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)    // LREG4 = x^11
    SFPLOADI(LREG5, 0, 0xB2D7)                   // LREG5 = -1/39916800 (BF16)
    SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)       // LREG0 += x^11 * (-1/39916800)
}

// === Sign Correction ===
// Convert SMAG32 integer n to 2's complement for bitwise AND
SFPCAST(LREG3, LREG3, 2)                         // LREG3 = INT32(2's complement)

// Load integer 1 mask for AND
SFPLOADI(LREG5, 0x8, 0x0000)                     // LREG5 upper = 0
SFPLOADI(LREG5, 0xA, 0x0001)                     // LREG5 lower = 1 -> LREG5 = 0x00000001

// AND to isolate bit 0
SFPAND(LREG5, LREG3)                             // LREG3 = LREG3 & 0x1

// Set CC where n is odd (bit 0 = 1, i.e., value != 0)
// Use INT32 comparison (Imm12[11]=0 for INT32 format)
SFPSETCC(0, LREG3, 2)                            // InstrMod=2: CC.Res = (LREG3 != 0)

// Negate where CC is set (odd n)
SFPMOV(LREG0, LREG0, 1)                          // LREG0 = -LREG0 (sign flip)

// Clear CC
SFPENCC(0, 0)                                    // CC.Res = 0

// Store result
SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0)
```

### `_calculate_cosine_<APPROXIMATION_MODE>(const int iterations)`

Identical structure to `_calculate_sine_` — same persistent constant loading and loop pattern.

### `_calculate_cosine_maclaurin_series_sfp_rows_<APPROXIMATION_MODE>()`

Same range reduction as sine. The Maclaurin series section differs:

```
// === Maclaurin Series: cos(x) = 1 - x^2/2! + x^4/4! - x^6/6! ... ===

// Initialize output = 1.0
SFPLOADI(LREG0, 0, 0x3F80)                       // LREG0 = 1.0 (BF16)

// x^2
SFPMUL(LREG1, LREG1, LCONST_0, LREG2, 0)        // LREG2 = x^2
SFPMOV(LREG2, LREG4, 0)                          // LREG4 = x^2 (current power)

// --- Term: -x^2/2! ---
SFPLOADI(LREG5, 0, 0xBF00)                       // LREG5 = -0.5 (BF16)
SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)           // LREG0 += x^2 * (-0.5)

// --- Term: +x^4/4! ---
SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)        // LREG4 = x^4
SFPLOADI(LREG5, 0, 0x3D2A)                       // LREG5 = 1/24 = 0.041667 (BF16)
SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)           // LREG0 += x^4 * (1/24)

// --- Term: -x^6/6! ---
SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)        // LREG4 = x^6
SFPLOADI(LREG5, 0, 0xBAB6)                       // LREG5 = -1/720 = -0.001389 (BF16)
SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)           // LREG0 += x^6 * (-1/720)

if constexpr (!APPROXIMATION_MODE) {
    // --- Term: +x^8/8! ---
    SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)    // LREG4 = x^8
    SFPLOADI(LREG5, 0, 0x37D0)                   // LREG5 = 1/40320 (BF16)
    SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)       // LREG0 += x^8 * (1/40320)

    // --- Term: -x^10/10! ---
    SFPMUL(LREG4, LREG2, LCONST_0, LREG4, 0)    // LREG4 = x^10
    SFPLOADI(LREG5, 0, 0xB493)                   // LREG5 = -1/3628800 (BF16)
    SFPMAD(LREG4, LREG5, LREG0, LREG0, 0)       // LREG0 += x^10 * (-1/3628800)
}

// Same sign correction and store as sine
```

## Potential Issues

### 1. SFPCAST RTNE vs. Truncation
The reference uses `float_to_int16(v, 0)` which truncates toward zero. SFPCAST mode 4 uses RTNE. For values like x/pi = 2.5, RTNE rounds to 2 (nearest even), which is the same as truncation. For 1.5, RTNE rounds to 2, but truncation gives 1. This difference means the fractional part and sign correction may differ at boundary cases, potentially causing a 1-ULP sign error for inputs near half-integer multiples of pi.

**Mitigation**: The error is bounded and only affects inputs extremely close to integer multiples of pi (where sin/cos values are near zero anyway). For practical floating-point inputs, the difference is within tolerance. If testing reveals issues, an alternative approach is to subtract 0.5, round via SFPCAST, then add 0.5 back (emulating truncation) — but this costs extra instructions and likely is not needed.

### 2. BF16 Coefficient Precision
Small Maclaurin coefficients (especially x^9/9! and x^11/11!) loaded as BF16 have up to 0.6% error. This is acceptable because:
- These terms contribute very small values to the sum
- The error is proportional to the coefficient's magnitude, so absolute error is tiny
- Even with FP32 coefficients, the Maclaurin series truncation itself introduces similar-magnitude error

If higher precision is needed for non-approximation mode, the coefficients could be loaded as full 32-bit values (2 SFPLOADI instructions each), but this doubles the instruction count for the coefficient loads and consumes LREG bandwidth.

### 3. Register Pressure
All 8 LREGs are used. If any bug or design change requires an additional register, the design will need restructuring (e.g., reloading constants within the loop instead of persisting them, or combining the two 32-bit constant loads into the inner function).

### 4. LCONST_1 as SFPMAD SrcB for Subtraction
The subtraction `frac = scaled - float(n)` uses `SFPMAD(LREG1, LCONST_1, LREG2, LREG1, 0x2)` which computes `LREG1 * 1.0 + (-LREG2)`. The constant 1.0 is accessed via `p_sfpu::LCONST_1` in the RG view (index 14, Fixed Constant 2). Verify that this is accessible as an SFPMAD SrcB operand (it should be, as the RG view includes all 16 entries including fixed constants at indices 12-15).

### 5. SFPAND Operand Convention
`TTI_SFPAND(lreg_c, lreg_dest)` performs `RG[lreg_dest] = RG[lreg_dest] & RG[lreg_c]`. The mask (0x1) must be in `lreg_c` and the value to be masked must be in `lreg_dest`. In our design, LREG5 holds the mask and LREG3 holds the integer n, so `TTI_SFPAND(p_sfpu::LREG5, p_sfpu::LREG3)` is correct: `LREG3 = LREG3 & LREG5`.

### 6. SFPSETCC INT32 vs FP32/SMAG32 Mode
After SFPAND, LREG3 contains an INT32 value (0 or 1 in 2's complement, which is the same bit pattern as unsigned). `SFPSETCC(0, LREG3, 2)` with Imm12[11]=0 treats the input as INT32 and mode 2 checks "!= 0". Since the result of AND is 0x00000000 or 0x00000001 in either INT32 or SMAG32 (bit 0 only, sign bit is 0), both INT32 and SMAG32 interpretations give the same zero/non-zero result. Using INT32 mode (Imm12[11]=0) is correct.

## Recommended Test Formats

Based on the analysis's "Format Support" section and the architecture research's "Format Support Matrix". Sine and cosine are **float-only transcendental operations** — integer formats are not applicable.

### Format List
The exact DataFormat enum values to pass to `input_output_formats()`:

```python
SFPU_TRIG_FORMATS = input_output_formats([
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Float32,
    DataFormat.MxFp8R,
    # MxFp8P excluded: lower precision (P stands for "proxy") causes quantization
    # error amplified by trig range reduction to exceed tolerance. MxFp8R provides
    # sufficient MX format coverage. Same reasoning as log test exclusion.
])
```

### Invalid Combination Rules
Rules for `_is_invalid_quasar_combination()` filtering:

1. **Non-Float32 input -> Float32 output with dest_acc=No**: Invalid. Quasar packer does not support this conversion without 32-bit Dest mode.
   ```python
   if in_fmt != DataFormat.Float32 and out_fmt == DataFormat.Float32 and dest_acc == DestAccumulation.No:
       return True
   ```

2. **Float32 input -> Float16 output with dest_acc=No**: Invalid. Cross-precision conversion requires dest_acc=Yes.
   ```python
   if in_fmt == DataFormat.Float32 and out_fmt == DataFormat.Float16 and dest_acc == DestAccumulation.No:
       return True
   ```

3. **Integer/float format mixing**: Invalid. Integer and float formats cannot be mixed in input->output.
   ```python
   if in_fmt.is_integer() != out_fmt.is_integer():
       return True
   ```

4. **MxFp8 input -> non-MxFp8 output**: Invalid. Golden mismatch due to MxFp8 quantization error amplified by trig range reduction and polynomial evaluation. The golden computes on pre-quantized Python floats but hardware operates on MxFp8-quantized data.
   ```python
   if in_fmt.is_mx_format() and not out_fmt.is_mx_format():
       return True
   ```

These rules are identical to the log test's rules (same transcendental float-only pattern).

### MX Format Handling
MxFp8R is included in the format list:
- Combination generator must skip MX + `implied_math_format=No` (MX formats require `implied_math_format=Yes`)
- MX formats exist only in L1; unpacked to Float16_b for SFPU math
- Golden generator handles MX quantization via existing infrastructure
- MxFp8P is excluded due to quantization error amplification by trig functions (same as log)

### Integer Format Handling
Not applicable — trigonometric functions are undefined for integer types. No integer formats in the format list.

## Testing Notes

### Input Range Considerations
- **Sine/cosine**: Valid for all real numbers. The range reduction handles any input magnitude.
- For Float16 inputs, max magnitude is ~65504, giving ~20,800 pi-radians — the integer extraction must handle this range (SMAG32 supports up to ~2.1 billion, so no overflow concern).
- Input generation should include:
  - Small values near zero (where sin(x) ~ x and cos(x) ~ 1)
  - Values near multiples of pi (where sign correction matters)
  - Large positive and negative values (to stress range reduction)
  - Values near pi/2 (where sin is max and cos is zero)

### Tolerance
- For BF16 coefficients with Maclaurin series truncation, expect ~1-2% relative error for approximation mode and ~0.1% for non-approximation mode.
- Use the same PCC (Pearson Correlation Coefficient) tolerance as similar SFPU tests (typically 0.999 or higher).

### Phase Test Strategy
Phase 1 tests should exercise only `_calculate_sine_` and `_calculate_cosine_`. The test C++ source should include `sfpu/ckernel_sfpu_trigonometry.h` and call these two functions directly. The Python test should use `math.sin` and `math.cos` as golden references.
