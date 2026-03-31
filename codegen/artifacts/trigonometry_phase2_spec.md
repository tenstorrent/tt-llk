# Specification: trigonometry (Phase 2 -- acosh_asinh)

## Kernel Type
sfpu

## Target Architecture
quasar

## Overview
Based on analysis: `codegen/artifacts/trigonometry_analysis.md`

Phase 2 implements `_init_inverse_hyperbolic_()`, `_calculate_acosh_(iterations)`, and `_calculate_asinh_(iterations)`. These functions compute the inverse hyperbolic cosine and sine using the mathematical identities:
- acosh(x) = log(x + sqrt(x^2 - 1)) for x >= 1, NaN for x < 1, 0 for x == 1
- asinh(x) = sign(x) * log(|x| + sqrt(x^2 + 1))

Phase 1 (sine/cosine) is already written and tested in `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_trigonometry.h`. Phase 2 functions must be **appended** after the phase 1 functions, within the same namespace.

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_trigonometry.h`

## Reference Implementations Studied

### Existing Quasar Kernels (pattern source)
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_log.h`: Key pattern for the inline log computation. Uses `_init_log_()` to preload polynomial coefficients into LREGs, then `_calculate_log_sfp_rows_()` performs the full log computation (SFPSETEXP for mantissa normalization, SFPMAD chain for Horner polynomial, SFPEXEXP + SFPCAST for exponent extraction, SFPSETCC for ln(0)=-inf special case). **This kernel operates on Dest rows** via SFPLOAD/SFPSTORE. For phase 2, we need to inline a similar log computation that operates on an LREG intermediate value, not from Dest.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_sqrt.h`: Uses `TTI_SFPNONLINEAR(src, dst, SQRT_MODE)` for hardware-accelerated sqrt approximation. Single instruction, FP16_B precision (~7 mantissa bits). This is the Quasar sqrt primitive that replaces the Blackhole software `_calculate_sqrt_body_()`.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_sigmoid.h`: Pattern for composing multiple SFPU primitives in a helper function (`_calculate_sigmoid_regs_()` takes register arguments). Demonstrates how to chain SFPNONLINEAR operations on LREG values.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_elu.h`: Pattern for conditional execution using SFPSETCC + SFPENCC. Shows how to apply operations only to lanes where a condition holds, without needing SFPPUSHC/SFPPOPC (no nesting).
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_lrelu.h`: Confirms the `TTI_SFPENCC(1, 2)` / `TTI_SFPENCC(0, 2)` enable/disable pattern at the outer loop level, with `TTI_SFPSETCC` + `TTI_SFPENCC(0, 0)` for per-iteration conditional.

### Blackhole Reference (semantics source)
- `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_trigonometry.h`: Defines the mathematical algorithms for acosh and asinh. Key insight: both functions call `_calculate_sqrt_body_()` and `_calculate_log_body_no_init_()` as inline helpers that operate on `vFloat` LREG values, not on Dest rows. The Blackhole acosh has three branches (x < 1 -> NaN, x == 1 -> 0, x > 1 -> compute), while asinh is unconditional except for the final sign correction.
- `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_log.h`: The `_calculate_log_body_no_init_()` function shows the full log algorithm: setexp(base, 127) to normalize mantissa, polynomial approximation, exexp() to extract exponent, convert to float, combine. This function operates on a vFloat value and returns a vFloat -- it does NOT load/store from Dest.
- `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_sqrt.h`: The `_calculate_sqrt_body_()` is a complex software algorithm (Newton-Raphson refinement of an initial fast inverse sqrt). On Quasar, this is replaced by the single-instruction `TTI_SFPNONLINEAR(src, dst, SQRT_MODE)`.

## Critical Porting Challenge: Composing Log and Sqrt on LREG Values

On Blackhole, acosh/asinh call inline helpers (`_calculate_sqrt_body_`, `_calculate_log_body_no_init_`) that operate directly on `vFloat` (LREG) values. On Quasar, the existing `ckernel_sfpu_log.h` and `ckernel_sfpu_sqrt.h` operate on **Dest rows** via SFPLOAD/SFPSTORE.

### Strategy: Inline LREG-Based Operations

**For sqrt**: Use `TTI_SFPNONLINEAR(src_lreg, dst_lreg, SQRT_MODE)` directly. This instruction operates on LREG values -- no Dest interaction needed. This is a single instruction that replaces the entire Blackhole `_calculate_sqrt_body_()`.

**For log**: Inline the log computation from `ckernel_sfpu_log.h` but adapted to operate on an LREG value instead of loading from Dest. The algorithm:
1. `SFPSETEXP(127, input_lreg, norm_lreg, 1)` -- normalize mantissa to [1, 2)
2. `SFPLOADI` + SFPMAD chain -- Horner polynomial: x*(x*(x*A + B) + C) + D
3. `SFPEXEXP(input_lreg, exp_lreg, 0)` -- extract unbiased exponent
4. `SFPCAST(exp_lreg, exp_lreg, 2)` -- two's complement to sign-magnitude
5. `SFPCAST(exp_lreg, exp_lreg, 0)` -- sign-magnitude to FP32
6. `SFPMAD(exp_lreg, ln2_lreg, series_lreg, result_lreg, 0)` -- combine
7. Handle ln(0)=-inf special case via SFPSETCC

The log polynomial coefficients are the same as in `ckernel_sfpu_log.h`:
- A = 0.1058 (BF16: 0x3DD8)
- B = -0.7116 (BF16: 0xBF36)
- C = 2.0871 (BF16: 0x4005)
- D = -1.4753 (BF16: 0xBFBC)
- ln(2) = 0.6929 (BF16: 0x3F31)

### Why Not Store-to-Dest-and-Call?

An alternative strategy would be to SFPSTORE the intermediate LREG to Dest, call `_calculate_log_sfp_rows_()`, then SFPLOAD the result back. This is rejected because:
1. SFPSTORE to Dest has 3-cycle latency, and storing/loading corrupts the current Dest row being processed.
2. The `_calculate_log_sfp_rows_()` function does SFPLOAD/SFPSTORE internally, so it would clobber the Dest row that acosh/asinh is working on.
3. Register allocation would be extremely tight -- we'd need to save/restore all intermediate LREGs around the call.

Inlining the log computation is cleaner and more efficient.

## Algorithm in Target Architecture

### Function: `_init_inverse_hyperbolic_()`

No-op on Quasar. On Blackhole, this calls `_init_sqrt_()` to set up programmable constant registers for the software sqrt algorithm. On Quasar, sqrt uses the hardware `SFPNONLINEAR SQRT_MODE` instruction which needs no initialization. The log polynomial coefficients will be loaded inline within each `_sfp_rows_` function to avoid occupying persistent LREGs across iterations.

The function must still exist (the test harness may call it), but it can be empty.

### Function: `_calculate_acosh_(iterations)`

Per-row algorithm (inside `_calculate_acosh_sfp_rows_()`):

```
1. Load input x from Dest -> LREG0
2. BOUNDARY CHECK: if x < 1.0 -> output NaN
3. BOUNDARY CHECK: if x == 1.0 -> output 0.0
4. MAIN COMPUTATION (x > 1.0):
   a. tmp = x * x          [SFPMUL] -> LREG1
   b. tmp = tmp - 1.0      [SFPMAD with negate C] -> LREG1  (x^2 - 1)
   c. tmp = sqrt(tmp)      [SFPNONLINEAR SQRT_MODE] -> LREG2
   d. tmp = tmp + x        [SFPADD] -> LREG1  (x + sqrt(x^2 - 1))
   e. result = log(tmp)    [inline log computation] -> LREG2
5. Store result to Dest
```

#### Boundary Handling Strategy

The Blackhole reference uses `v_if` / `v_elseif` / `v_else` / `v_endif` for three-way branching. On Quasar, no existing kernel uses nested SFPPUSHC/SFPPOPC. Instead, use a **sequential approach with SFPSETCC + SFPMOV/SFPLOADI + SFPENCC** that avoids nesting:

Strategy: Compute the main result for all lanes unconditionally, then overwrite boundary lanes.

```
1. Load x from Dest -> LREG0
2. Compute result for all lanes (including invalid ones -- garbage values for x<1 will be overwritten):
   a. SFPMUL(LREG0, LREG0, ..., LREG1, 0)     // LREG1 = x^2
   b. SFPMAD(LREG1, LCONST_1, LCONST_1, LREG1, 0x2)  // LREG1 = x^2 - 1.0
   c. SFPNONLINEAR(LREG1, LREG2, SQRT_MODE)    // LREG2 = sqrt(x^2 - 1)
   d. SFPADD(LCONST_1, LREG2, LREG0, LREG1, 0) // LREG1 = sqrt(x^2-1) + x
      Note: SFPADD(A, B, C, D, 0) = A*B + C -> 1.0*LREG2 + LREG0 = sqrt + x
   e. [inline log of LREG1 -> result in LREG2]
3. Overwrite lanes where x == 1.0 with 0.0:
   a. Load 1.0 for comparison -> use LCONST_1 (fixed constant, index 14 in RG view)
   b. SFPSETCC to compare LREG0 with 1.0 -- but SFPSETCC only tests sign/zero, not equality with arbitrary values.

   REVISED: Use SFPMAD to compute (x - 1.0) and test if result is zero:
   a. SFPMAD(LREG0, LCONST_1, LCONST_1, LREG3, 0x2)  // LREG3 = x - 1.0
   b. SFPSETCC(0, LREG3, 0x6)  // CC = 1 where LREG3 is zero (x == 1.0)
   c. SFPLOADI(LREG2, 0, 0x0000)  // result = 0.0 (BF16 encoding of 0.0)
   d. SFPENCC(0, 0)  // clear CC

4. Overwrite lanes where x < 1.0 with NaN:
   a. SFPSETCC(0, LREG0, 0)  // CC = 1 where LREG0 is negative (x < 0 -- guaranteed < 1)
   b. SFPLOADI(LREG2, 0, 0x7FC0)  // NaN (BF16: 0x7FC0 = quiet NaN)
   c. SFPENCC(0, 0)  // clear CC

   Also handle 0 <= x < 1: compute (x - 1.0) and check if negative:
   a. SFPSETCC(0, LREG3, 0)  // CC = 1 where (x - 1.0) is negative -> x < 1
   b. SFPLOADI(LREG2, 0, 0x7FC0)  // NaN
   c. SFPENCC(0, 0)

5. Store LREG2 (result) to Dest
```

**IMPORTANT**: The sqrt of a negative number (x^2 - 1 when x < 1) through SFPNONLINEAR produces garbage, but we overwrite those lanes with NaN afterwards, so it does not matter.

#### Revised Acosh Algorithm (Final Design)

To minimize register pressure and use consistent patterns, the final design computes `(x - 1.0)` once and uses it for both boundary checks:

```
// Load input x from Dest
SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)          // LREG0 = x

// Compute x-1 for boundary detection (will also be useful for main path)
SFPMAD(LREG0, LCONST_1, LCONST_1, LREG3, 0x2)      // LREG3 = x*1.0 + (-1.0) = x-1

// === Main computation (compute for ALL lanes, boundary lanes will be overwritten) ===
// tmp = x^2
SFPMUL(LREG0, LREG0, LCONST_0, LREG1, 0)           // LREG1 = x^2
// tmp = x^2 - 1.0
SFPMAD(LREG1, LCONST_1, LCONST_1, LREG1, 0x2)      // LREG1 = x^2 - 1 (via LREG1*1 + (-1))
// tmp = sqrt(x^2 - 1)
SFPNONLINEAR(LREG1, LREG2, SQRT_MODE)               // LREG2 = sqrt(x^2 - 1)
// tmp = sqrt(x^2 - 1) + x
SFPADD(LCONST_1, LREG2, LREG0, LREG1, 0)           // LREG1 = 1.0*LREG2 + LREG0 = sqrt + x

// === Inline log(LREG1) -> LREG2 (result) ===
// [See "Inline Log Computation" section below]
// Result lands in LREG2.

// === Boundary: overwrite where x == 1.0 (LREG3 == 0) ===
SFPSETCC(0, LREG3, 0x6)                              // CC = 1 where (x-1) == 0
SFPLOADI(LREG2, 0, 0x0000)                           // result = 0.0 for x==1 lanes
SFPENCC(0, 0)                                        // clear CC

// === Boundary: overwrite where x < 1.0 (LREG3 < 0) ===
SFPSETCC(0, LREG3, 0)                                // CC = 1 where (x-1) < 0
SFPLOADI(LREG2, 0, 0x7FC0)                           // result = NaN for x<1 lanes
SFPENCC(0, 0)                                        // clear CC

// Store result
SFPSTORE(LREG2, 0, ADDR_MOD_7, 0, 0)
```

### Function: `_calculate_asinh_(iterations)`

Per-row algorithm (inside `_calculate_asinh_sfp_rows_()`):

```
1. Load input x from Dest -> LREG0
2. Save sign of x: abs_x = |x| via SFPABS
3. Compute: tmp = abs_x^2 + 1.0
4. tmp = sqrt(tmp)     [SFPNONLINEAR SQRT_MODE]
5. tmp = tmp + abs_x   [SFPADD]
6. result = log(tmp)   [inline log]
7. Restore sign: if original x < 0, negate result
8. Store result to Dest
```

Detailed pseudocode:
```
// Load input x from Dest
SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)           // LREG0 = x

// Save original x for sign restoration later
SFPMOV(LREG0, LREG3, 0)                              // LREG3 = x (copy for sign check)

// |x|
SFPABS(LREG0, LREG0, 0)                              // LREG0 = |x|

// tmp = |x|^2 + 1.0
SFPMUL(LREG0, LREG0, LCONST_0, LREG1, 0)            // LREG1 = |x|^2
SFPADD(LCONST_1, LREG1, LCONST_1, LREG1, 0)         // LREG1 = 1.0*|x|^2 + 1.0 = |x|^2 + 1

// sqrt(|x|^2 + 1)
SFPNONLINEAR(LREG1, LREG2, SQRT_MODE)                // LREG2 = sqrt(|x|^2 + 1)

// sqrt + |x|
SFPADD(LCONST_1, LREG2, LREG0, LREG1, 0)            // LREG1 = sqrt(...) + |x|

// === Inline log(LREG1) -> LREG2 (result) ===
// [See "Inline Log Computation" section below]

// === Sign correction: negate result where original x was negative ===
SFPSETCC(0, LREG3, 0)                                // CC = 1 where original x < 0
SFPMOV(LREG2, LREG2, 1)                              // negate LREG2 where CC is set
SFPENCC(0, 0)                                        // clear CC

// Store result
SFPSTORE(LREG2, 0, ADDR_MOD_7, 0, 0)
```

### Inline Log Computation (Shared by acosh and asinh)

This is the critical shared subroutine. It computes `log(LREG1)` and stores the result in `LREG2`. It is inlined into both `_calculate_acosh_sfp_rows_()` and `_calculate_asinh_sfp_rows_()`.

The algorithm matches `_calculate_log_sfp_rows_()` from `ckernel_sfpu_log.h` exactly, but operates on LREG1 as input instead of loading from Dest.

**Register usage during inline log** (LREG0 holds input x or |x| from the calling function and may be clobbered; LREG3 holds saved sign for asinh or boundary check for acosh):

```
// Save input for exponent extraction
// LREG1 already holds the value to take log of

// Normalize mantissa to [1, 2) by setting exponent to 127
SFPSETEXP(127, LREG1, LREG4, 1)                     // LREG4 = mantissa in [1,2)

// Load coefficient A inline
SFPLOADI(LREG5, 0, 0x3DD8)                           // LREG5 = A = 0.1058 (BF16)

// Load coefficient B
SFPLOADI(LREG0, 0, 0xBF36)                           // LREG0 = B = -0.7116 (BF16)

// Horner polynomial: x*(x*(x*A + B) + C) + D
// Step 1: x*A + B
SFPMAD(LREG4, LREG5, LREG0, LREG5, 0)              // LREG5 = x*A + B

// Load coefficient C
SFPLOADI(LREG0, 0, 0x4005)                           // LREG0 = C = 2.0871 (BF16)

// Step 2: x*(x*A+B) + C
SFPMAD(LREG4, LREG5, LREG0, LREG5, 0)              // LREG5 = x*(x*A+B) + C

// Load coefficient D
SFPLOADI(LREG0, 0, 0xBFBC)                           // LREG0 = D = -1.4753 (BF16)

// Step 3: x*(x*(x*A+B)+C) + D = series_result
SFPMAD(LREG4, LREG5, LREG0, LREG5, 0)              // LREG5 = series_result

// Extract unbiased exponent as two's complement integer
SFPEXEXP(LREG1, LREG4, 0)                            // LREG4 = exponent (2's complement)

// Convert two's complement -> sign-magnitude
SFPCAST(LREG4, LREG4, 2)                             // LREG4 = sign-magnitude int32

// Convert sign-magnitude -> FP32
SFPCAST(LREG4, LREG4, 0)                             // LREG4 = float(exponent)

// Load ln(2)
SFPLOADI(LREG0, 0, 0x3F31)                           // LREG0 = ln(2) = 0.6929 (BF16)

// Combine: result = exponent * ln(2) + series_result
SFPMAD(LREG4, LREG0, LREG5, LREG2, 0)              // LREG2 = exp*ln2 + series

// Handle log(0) = -inf: check if input was zero
SFPSETCC(0, LREG1, 0x6)                              // CC = 1 where input is zero
SFPLOADI(LREG2, 0, 0xFF80)                           // -infinity (BF16)
SFPENCC(0, 0)                                        // clear CC
```

**Register allocation during inline log**:
| Register | Purpose |
|----------|---------|
| LREG0 | Clobbered: holds coefficients B, C, D, ln(2) sequentially |
| LREG1 | Input to log (preserved for SFPEXEXP) |
| LREG2 | Output: log result |
| LREG3 | Preserved: holds sign (asinh) or boundary check value (acosh) |
| LREG4 | Normalized mantissa, then exponent float |
| LREG5 | Polynomial accumulator (series_result) |
| LREG6 | Available (unused during log) |
| LREG7 | Available (unused during log) |

### SFPSETCC InstrMod Values Reference

From the architecture research and existing kernel usage:
- `InstrMod=0`: CC = 1 where value is negative (sign bit set)
- `InstrMod=2`: CC = 1 where value is non-zero
- `InstrMod=0x6`: CC = 1 where value is zero

These are used for boundary checking in acosh and sign correction in asinh.

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|--------------------|-------------------|-----------------|
| `_calculate_sqrt_body_<APPROX>(tmp)` | `TTI_SFPNONLINEAR(src, dst, SQRT_MODE)` | ckernel_sfpu_sqrt.h line 22 |
| `_calculate_log_body_no_init_(tmp)` | Inline log sequence (see above) | ckernel_sfpu_log.h lines 38-77 adapted |
| `_init_sqrt_<APPROX>()` | No-op (hardware sqrt) | SFPNONLINEAR needs no init |
| `sfpi::abs(inp)` | `TTI_SFPABS(src, dst, 0)` | ckernel_ops.h line 601 |
| `v_if (inp < vConst0)` | `TTI_SFPSETCC(0, lreg, 0)` | ckernel_sfpu_elu.h line 22, InstrMod=0 = negative |
| `v_if (inp < vConst1)` | `TTI_SFPSETCC(0, diff_lreg, 0)` where diff = x - 1 | Derived: compute x-1, test negative |
| `v_if (inp == vConst1)` | `TTI_SFPSETCC(0, diff_lreg, 0x6)` where diff = x - 1 | Derived: compute x-1, test zero |
| `sfpi::dst_reg[0] = NaN` | `TTI_SFPLOADI(result_lreg, 0, 0x7FC0)` (conditional) | BF16 encoding of quiet NaN |
| `sfpi::dst_reg[0] = 0` | `TTI_SFPLOADI(result_lreg, 0, 0x0000)` (conditional) | BF16 encoding of 0.0 |
| `result *= -1` | `TTI_SFPMOV(src, dst, 1)` (with CC) | ckernel_sfpu_trigonometry.h line 98 |
| `sfpi::setexp(in, 127)` | `TTI_SFPSETEXP(127, src, dst, 1)` | ckernel_sfpu_log.h line 42 |
| `sfpi::exexp(in)` | `TTI_SFPEXEXP(src, dst, 0)` | ckernel_sfpu_log.h line 58 |

### Resource Allocation

**LREG usage for acosh `_sfp_rows_`** (tightest register pressure point):
| Register | Usage |
|----------|-------|
| LREG0 | Input x from Dest; clobbered by inline log coefficients |
| LREG1 | x^2, then x^2-1, then sqrt+x (log input); preserved through log for SFPEXEXP |
| LREG2 | sqrt result; then final log result (output) |
| LREG3 | x-1.0 (boundary check value, preserved through computation) |
| LREG4 | Inline log: normalized mantissa, then exponent float |
| LREG5 | Inline log: polynomial accumulator |
| LREG6 | Available |
| LREG7 | Available |

**LREG usage for asinh `_sfp_rows_`**:
| Register | Usage |
|----------|-------|
| LREG0 | Input x, then |x|; clobbered by inline log coefficients |
| LREG1 | |x|^2, then |x|^2+1, then sqrt+|x| (log input); preserved through log |
| LREG2 | sqrt result; then final log result (output) |
| LREG3 | Copy of original x (for sign restoration) |
| LREG4 | Inline log: normalized mantissa, then exponent float |
| LREG5 | Inline log: polynomial accumulator |
| LREG6 | Available |
| LREG7 | Available |

## Implementation Structure

### Includes
The file already has the required includes from phase 1:
```cpp
#include <cstdint>
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```

No additional includes needed. The log and sqrt computations are inlined, not called from other headers.

### Namespace
Same as phase 1: `ckernel::sfpu`

### Functions

| Function | Template Params | Signature | Purpose |
|----------|-----------------|-----------|---------|
| `_init_inverse_hyperbolic_` | none | `inline void _init_inverse_hyperbolic_()` | No-op init (hardware sqrt needs no setup) |
| `_calculate_acosh_sfp_rows_` | none | `inline void _calculate_acosh_sfp_rows_()` | Inner per-row: acosh with boundary checks + inline log |
| `_calculate_acosh_` | none | `inline void _calculate_acosh_(const int iterations)` | Outer loop over iterations |
| `_calculate_asinh_sfp_rows_` | none | `inline void _calculate_asinh_sfp_rows_()` | Inner per-row: asinh with abs + inline log + sign restore |
| `_calculate_asinh_` | none | `inline void _calculate_asinh_(const int iterations)` | Outer loop over iterations |

**No template parameters** on the phase 2 functions. This matches the Quasar pattern (see existing Quasar kernels -- most have no template params or only APPROXIMATION_MODE). The Blackhole `APPROXIMATION_MODE` and `ITERATIONS` template params are dropped per the Quasar convention.

**However**: The test C++ file dispatches based on `SfpuType` enum. Currently it only handles `sine` and `cosine`. The test file needs to be updated to also handle `acosh` and `asinh`, calling `_init_inverse_hyperbolic_()` then `_calculate_acosh_()` or `_calculate_asinh_()`.

## Instruction Sequence

### `_init_inverse_hyperbolic_()`
```cpp
inline void _init_inverse_hyperbolic_()
{
    // No initialization needed on Quasar.
    // Blackhole initializes software sqrt constants, but Quasar uses
    // hardware SFPNONLINEAR SQRT_MODE which requires no setup.
    // Log coefficients are loaded inline per-iteration to avoid
    // consuming persistent LREGs.
}
```

### `_calculate_acosh_sfp_rows_()`
See "Revised Acosh Algorithm" pseudocode above, translated to TTI macros.

### `_calculate_acosh_(iterations)`
```cpp
inline void _calculate_acosh_(const int iterations)
{
    TTI_SFPENCC(1, 2); // enable conditional execution
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_acosh_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
    TTI_SFPENCC(0, 2); // disable conditional execution
}
```

### `_calculate_asinh_sfp_rows_()`
See "Asinh Algorithm" pseudocode above, translated to TTI macros.

### `_calculate_asinh_(iterations)`
```cpp
inline void _calculate_asinh_(const int iterations)
{
    TTI_SFPENCC(1, 2); // enable conditional execution
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_asinh_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
    TTI_SFPENCC(0, 2); // disable conditional execution
}
```

## Init/Uninit Symmetry

| Init Action | Uninit Reversal |
|-------------|-----------------|
| None (`_init_inverse_hyperbolic_` is no-op) | N/A |

There is no uninit function for inverse hyperbolic. The init is empty, so no hardware state change needs to be reversed.

## SFPSETCC Mode Reference

For the writer's reference, SFPSETCC InstrMod encoding (from architecture research and ckernel_ops.h):
- `TTI_SFPSETCC(0, lreg, 0)` -- CC = 1 where `lreg` is negative (sign bit set)
- `TTI_SFPSETCC(0, lreg, 2)` -- CC = 1 where `lreg` is non-zero
- `TTI_SFPSETCC(0, lreg, 0x6)` -- CC = 1 where `lreg` is zero

Existing usage confirms:
- `ckernel_sfpu_log.h` line 72: `SFPSETCC(0, LREG0, 0x6)` for "input is zero"
- `ckernel_sfpu_elu.h` line 22: `SFPSETCC(0, LREG0, 0)` for "value is negative"
- `ckernel_sfpu_trigonometry.h` line 95: `SFPSETCC(0, LREG3, 2)` for "value is non-zero"

## Test Infrastructure Changes Required

### SfpuType Enum
`tt_llk_quasar/llk_lib/llk_defs.h` must add `acosh` and `asinh` to the `SfpuType` enum. The test infrastructure's `llk_sfpu_types.h` already has these values, so the Quasar-local enum must match.

### C++ Test File Update
`tests/sources/quasar/sfpu_trigonometry_quasar_test.cpp` in the `LLK_TRISC_MATH` section must add dispatch cases:
```cpp
else if constexpr (op == SfpuType::acosh)
{
    _init_inverse_hyperbolic_();
    _llk_math_eltwise_unary_sfpu_params_<false>(ckernel::sfpu::_calculate_acosh_, i, num_sfpu_iterations);
}
else if constexpr (op == SfpuType::asinh)
{
    _init_inverse_hyperbolic_();
    _llk_math_eltwise_unary_sfpu_params_<false>(ckernel::sfpu::_calculate_asinh_, i, num_sfpu_iterations);
}
```

**Note**: The `_init_inverse_hyperbolic_()` should be called once before the tile loop, not per-tile. Since it is a no-op on Quasar, this does not matter functionally, but for correctness pattern:
```cpp
if constexpr (op == SfpuType::acosh || op == SfpuType::asinh)
{
    _init_inverse_hyperbolic_();
}
// then in the tile loop:
if constexpr (op == SfpuType::acosh)
{
    _llk_math_eltwise_unary_sfpu_params_<false>(ckernel::sfpu::_calculate_acosh_, i, num_sfpu_iterations);
}
```

### Python Test Update
`tests/python_tests/quasar/test_sfpu_trigonometry_quasar.py` must add `MathOperation.Acosh` and `MathOperation.Asinh` to the `TRIG_OPS` list, along with appropriate input preparation functions for the different value domains.

## Potential Issues

### 1. SFPNONLINEAR sqrt on negative input
When computing acosh for x < 1, the value `x^2 - 1` is negative. `SFPNONLINEAR SQRT_MODE` on a negative input produces undefined/garbage output. This is acceptable because we unconditionally overwrite those lanes with NaN in the boundary handling section.

### 2. Inline log precision
The BF16-encoded polynomial coefficients (loaded via SFPLOADI format 0) have only ~7 bits of mantissa precision. This matches the existing `ckernel_sfpu_log.h` which uses the same BF16 coefficients. The log result precision is approximately FP16_B level.

### 3. SFPNONLINEAR sqrt precision
Hardware sqrt via SFPNONLINEAR provides FP16_B precision (~7 mantissa bits). For acosh(x) where x is close to 1.0, the value `x^2 - 1` is small and `sqrt(x^2-1)` is very small, so the relative error in the final result could be amplified. This is inherent to the hardware approximation and matches what other Quasar kernels deliver.

### 4. asinh sign restoration
The sign correction for asinh uses SFPSETCC to test original x's sign and SFPMOV with sign inversion to negate. This is the same pattern used in phase 1 sine/cosine for the odd-n sign flip and is well-tested.

### 5. Log of zero in asinh
For asinh(x), the argument to log is `|x| + sqrt(x^2 + 1)`. Since `sqrt(x^2 + 1) >= 1` for all real x, the argument is always >= 1 > 0, so log(0) = -inf never occurs. No special handling needed.

### 6. Log of zero in acosh
For acosh(x) when x == 1, the argument to log is `1 + sqrt(1^2 - 1) = 1 + 0 = 1`, so `log(1) = 0`. However, since we overwrite x==1 lanes with 0.0 directly (bypassing the log computation), this is handled correctly regardless of what log(1) computes.

### 7. CC state interaction between inline log and boundary checks
The inline log computation uses SFPSETCC for the log(0)=-inf special case, then clears CC with SFPENCC(0,0). The subsequent boundary checks (acosh) also use SFPSETCC + SFPENCC(0,0). Since each section clears CC before the next section uses it, there is no CC state leakage.

## Recommended Test Formats

### Format List
Float-only SFPU operations -- same as phase 1 (transcendental floating-point operations undefined for integer types):

```python
SFPU_TRIG_FORMATS = input_output_formats([
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Float32,
    DataFormat.MxFp8R,
    # MxFp8P excluded: lower precision causes quantization error amplified
    # by sqrt/log chain to exceed tolerance
])
```

### Invalid Combination Rules
Same rules as phase 1 in `_is_invalid_quasar_combination()`:
- Quasar packer does not support non-Float32 to Float32 when dest_acc=No
- Float32 input to Float16 output requires dest_acc=Yes
- Integer and float formats cannot be mixed
- MX input to non-MX output: golden mismatch due to MxFp8 quantization error amplified by sqrt+log chain (same pattern as log test)
- MX formats require implied_math_format=Yes

### MX Format Handling
MxFp8R is included. Combination generator must skip MX + implied_math_format=No. MX formats exist only in L1; unpacked to Float16_b for math.

### Integer Format Handling
Not applicable -- trig/inverse-hyperbolic are float-only operations.

## Testing Notes

### Acosh Input Domain
- Requires x >= 1.0 (returns NaN for x < 1)
- Test must generate inputs with x >= 1.0 for the main test cases
- Should also include some values < 1.0 to verify NaN boundary handling
- Should include x == 1.0 exactly to verify the zero output
- Recommended input range: [0.5, 10.0] with a mix (some below 1.0 for boundary testing)

### Asinh Input Domain
- Defined for all real x
- Unlike acosh, no boundary restrictions
- Test inputs in [-10.0, 10.0] (same range as sin/cos) should work well

### Golden Generator
The test infrastructure already has `MathOperation.Acosh` and `MathOperation.Asinh` with golden generators (`_acosh` and `_asinh` in `golden_generators.py`) that use `math.acosh()` and `math.asinh()`.

### Tolerance
The hardware sqrt + inline log chain introduces cumulative approximation error:
1. SFPNONLINEAR sqrt: ~FP16_B precision (7 mantissa bits, 1-3 ULP)
2. Log polynomial: ~FP16_B precision (BF16 coefficients)
3. Combined: expect ~FP16_B precision in the final result

The standard `passed_test()` tolerance should handle this for Float16/Float16_b outputs. Float32 output may show more absolute error but within relative tolerance.
