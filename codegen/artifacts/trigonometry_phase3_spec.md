# Specification: trigonometry (Phase 3 -- atanh)

## Kernel Type
sfpu

## Target Architecture
quasar

## Overview
Based on analysis: `codegen/artifacts/trigonometry_analysis.md`

Phase 3 implements `_init_atanh_()` and `_calculate_atanh_(iterations)`. The function computes the inverse hyperbolic tangent using the identity:

    atanh(x) = 0.5 * ln((1 + x) / (1 - x))

with boundary conditions:
- |x| > 1: NaN (undefined)
- x == 1: +infinity
- x == -1: -infinity
- |x| < 1: computed result

Phases 1 (sine/cosine) and 2 (acosh/asinh) are already written and tested in `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_trigonometry.h`. Phase 3 functions must be **appended** after the phase 2 functions, within the same namespace, before the closing `} // namespace sfpu` and `} // namespace ckernel`.

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_trigonometry.h`

## Reference Implementations Studied

### Existing Quasar Kernels (pattern source)
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_recip.h`: Uses `TTI_SFPNONLINEAR(src, dst, p_sfpnonlinear::RECIP_MODE)` for hardware-accelerated reciprocal approximation. Single instruction, FP16_B precision (~7 mantissa bits). This replaces the Blackhole software `_sfpu_reciprocal_<N>()` function.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_log.h`: Provides the inline log polynomial computation pattern. Uses SFPSETEXP to normalize mantissa, SFPMAD chain for Horner polynomial with coefficients A=0x3DD8, B=0xBF36, C=0x4005, D=0xBFBC, then SFPEXEXP + two SFPCASTs for exponent extraction, final SFPMAD to combine with ln(2)=0x3F31.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_trigonometry.h` (phase 2): Contains the inline log pattern already used by `_calculate_acosh_sfp_rows_()` and `_calculate_asinh_sfp_rows_()`. The atanh function MUST reuse the exact same inline log approach.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_elu.h`: Pattern for conditional execution using `TTI_SFPSETCC` + `TTI_SFPENCC(0, 0)` to apply operations only to lanes matching a condition. No nested SFPPUSHC/SFPPOPC.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_threshold.h`: Uses `TTI_SFPGT(0, lreg_a, lreg_b, 0x1)` for floating-point greater-than comparison, setting CC where lreg_b > lreg_a.

### Blackhole Reference (semantics source)
- `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_trigonometry.h` lines 185-224: Defines the atanh algorithm. Key observations:
  1. Computes `abs_inp = abs(inp)` for boundary checks
  2. Three-way branch: `abs_inp > 1` -> NaN, `abs_inp == 1` -> setsgn(inf, inp), else compute
  3. In the compute path: `num = 1 + inp`, `den = 1 - inp`, `tmp = reciprocal(den)`, `tmp = setsgn(tmp, den)` (sign correction), then `num = num * tmp`, `result = 0.5 * log(num)`
  4. The `setsgn(tmp, den)` call is a sign correction to ensure the reciprocal result has the correct sign (SFPNONLINEAR RECIP_MODE may produce an incorrect sign for negative inputs)
  5. The `is_fp32_dest_acc_en` conditional is a precision path that can be dropped on Quasar (SFPU operates in FP32 internally)

## Critical Porting Decisions

### 1. Reciprocal: Hardware SFPNONLINEAR vs Software Iterative
Blackhole uses `_sfpu_reciprocal_<N>(den)` which is a multi-iteration Newton-Raphson software reciprocal. On Quasar, use the single-instruction `TTI_SFPNONLINEAR(src, dst, p_sfpnonlinear::RECIP_MODE)` which provides FP16_B-precision (~7 mantissa bit) approximation in 1 cycle.

### 2. Sign Correction After Reciprocal
The Blackhole reference does `tmp = sfpi::setsgn(tmp, den)` after reciprocal. This is necessary because the hardware reciprocal instruction may not preserve the correct sign for all input values. On Quasar, use `TTI_SFPSETSGN(0, lreg_magnitude, lreg_sign_and_dest, 0)` which sets the sign of RG[VC] (magnitude register) to the sign of RG[VD] (sign source register), writing the result to RG[VD].

### 3. Drop `is_fp32_dest_acc_en` Template Parameter
The Blackhole reference has a `float_to_fp16b` precision truncation path controlled by `is_fp32_dest_acc_en`. On Quasar, the SFPU operates in FP32 internally and `SFPNONLINEAR RECIP_MODE` already produces FP16_B-precision output, so the precision truncation path is unnecessary. The Quasar function takes only `const int iterations` with no template parameters.

### 4. Drop `APPROXIMATION_MODE` Template Parameter
The Blackhole reference uses APPROXIMATION_MODE to control the number of Newton-Raphson iterations in the software reciprocal (0 vs 2 iterations). On Quasar, the hardware reciprocal has fixed precision, so APPROXIMATION_MODE has no effect on atanh. The function does not need this template parameter.

### 5. Boundary Handling Strategy
Follow the same sequential overwrite pattern used in phase 2's `_calculate_acosh_sfp_rows_()`:
- Compute the main result for ALL lanes unconditionally
- Then overwrite boundary lanes using SFPSETCC + SFPLOADI + SFPENCC

This avoids nested SFPPUSHC/SFPPOPC which no existing Quasar kernel uses.

### 6. Init Function: No-op
The Blackhole `_init_atanh_()` calls `_init_sfpu_reciprocal_()` to set up software reciprocal constants. On Quasar, `SFPNONLINEAR RECIP_MODE` needs no initialization. The init function must exist (test harness calls it) but can be empty.

## Infrastructure Changes Required

### SfpuType Enum
The `SfpuType` enum in `tt_llk_quasar/llk_lib/llk_defs.h` currently ends with `asinh`. The writer must add `atanh` to this enum so the test harness can dispatch to it.

### Test Harness Dispatch
The C++ test file `tests/sources/quasar/sfpu_trigonometry_quasar_test.cpp` currently dispatches for sine, cosine, acosh, and asinh. The writer must add an `else if constexpr (op == SfpuType::atanh)` branch that calls `_init_atanh_()` and then dispatches `_calculate_atanh_` via `_llk_math_eltwise_unary_sfpu_params_`.

## Algorithm in Target Architecture

### Function: `_init_atanh_()`

No-op on Quasar. On Blackhole, this calls `_init_sfpu_reciprocal_()` to set up LUT constants for the software reciprocal. On Quasar, reciprocal uses hardware `SFPNONLINEAR RECIP_MODE` which needs no initialization.

The function must exist with an empty body (the test harness calls it).

```
inline void _init_atanh_()
{
    // No-op: Quasar uses hardware SFPNONLINEAR RECIP_MODE, no init needed.
}
```

### Function: `_calculate_atanh_sfp_rows_()`

Per-row algorithm (the inner function called per 2-row iteration):

```
1. Load input x from Dest -> LREG0
2. Save copy of x for sign extraction later -> LREG3
3. Compute |x| = abs(x) -> LREG0
4. Save |x| for boundary checks later -> LREG7

--- MAIN COMPUTATION (all lanes, boundary lanes overwritten later) ---
5. Compute num = 1.0 + x (using saved copy in LREG3) -> LREG1
   SFPADD(LCONST_1, LREG3, LCONST_1, LREG1, 0)  // 1.0 * LREG3 + 1.0 = x + 1
6. Compute den = 1.0 - x -> LREG2
   SFPMAD(LREG3, LCONST_1, LCONST_1, LREG2, 0x2)  // LREG3 * 1.0 + (-1.0) = x - 1
   SFPMOV(LREG2, LREG2, 1)                          // negate: LREG2 = -(x-1) = 1-x
7. Save den for sign correction -> LREG6
   SFPMOV(LREG2, LREG6, 0)
8. Compute recip(den) -> LREG4
   SFPNONLINEAR(LREG2, LREG4, RECIP_MODE)
9. Sign correction: set sign of recip to sign of den
   TTI_SFPSETSGN(0, LREG4, LREG6, 0)
   // Now LREG6 = abs(LREG4) with sign of LREG6 = |recip| * sign(den)
   SFPMOV(LREG6, LREG4, 0)  // move corrected value back to LREG4
10. Compute num * (1/den) = (1+x)/(1-x) -> LREG1
    SFPMUL(LREG1, LREG4, LCONST_0, LREG1, 0)

--- INLINE LOG(LREG1) -> LREG2 ---
11. Normalize mantissa to [1, 2): SFPSETEXP(127, LREG1, LREG4, 1)
12. Load coeff A = 0.1058: SFPLOADI(LREG5, 0, 0x3DD8)
13. Load coeff B = -0.7116: SFPLOADI(LREG0, 0, 0xBF36)
14. Horner step 1: SFPMAD(LREG4, LREG5, LREG0, LREG5, 0)  // x*A + B
15. Load coeff C = 2.0871: SFPLOADI(LREG0, 0, 0x4005)
16. Horner step 2: SFPMAD(LREG4, LREG5, LREG0, LREG5, 0)  // x*(x*A+B) + C
17. Load coeff D = -1.4753: SFPLOADI(LREG0, 0, 0xBFBC)
18. Horner step 3: SFPMAD(LREG4, LREG5, LREG0, LREG5, 0)  // series_result
19. Extract exponent: SFPEXEXP(LREG1, LREG4, 0)
20. Two's complement -> sign-magnitude: SFPCAST(LREG4, LREG4, 2)
21. Sign-magnitude -> FP32: SFPCAST(LREG4, LREG4, 0)
22. Load ln(2) = 0.6929: SFPLOADI(LREG0, 0, 0x3F31)
23. Combine: SFPMAD(LREG4, LREG0, LREG5, LREG2, 0)  // exp*ln2 + series = log_result

24. Handle log(0)=-inf: SFPSETCC(0, LREG1, 0x6) + SFPLOADI(LREG2, 0, 0xFF80) + SFPENCC(0, 0)

--- MULTIPLY BY 0.5 ---
25. Load 0.5 constant: SFPLOADI(LREG0, 0, 0x3F00)
26. result = 0.5 * log_result: SFPMUL(LREG0, LREG2, LCONST_0, LREG2, 0)

--- BOUNDARY OVERWRITE ---
27. Compute (|x| - 1.0) for boundary detection -> LREG0
    SFPMAD(LREG7, LCONST_1, LCONST_1, LREG0, 0x2)  // |x| * 1.0 + (-1.0) = |x| - 1

28. Boundary: |x| > 1.0 (LREG0 > 0, i.e. positive and non-zero)
    We need CC=1 where |x| > 1. Check if (|x| - 1) > 0:
    SFPSETCC(0, LREG0, 0x8)  // CC = 1 where LREG0 > 0 (positive and not zero)
    SFPLOADI(LREG2, 0, 0x7FC0)  // NaN (BF16)
    SFPENCC(0, 0)

29. Boundary: x == 1.0 (LREG0 == 0 AND original x positive)
    Load +infinity into LREG4: SFPLOADI(LREG4, 0, 0x7F80)  // +inf (BF16)
    Need to apply sign of original x to infinity.
    SFPSETSGN(0, LREG4, LREG3, 0)  // LREG3 = abs(LREG4) with sign of LREG3 = |inf| * sign(x)
    Then check where |x| == 1.0: SFPSETCC(0, LREG0, 0x6)  // CC where (|x|-1) is zero
    Copy signed infinity: SFPMOV(LREG3, LREG2, 0)  // copy sign(x)*inf into LREG2 for those lanes
    SFPENCC(0, 0)

30. Store result: SFPSTORE(LREG2, 0, ADDR_MOD_7, 0, 0)
```

### Register Allocation

| Register | Purpose |
|----------|---------|
| LREG0 | Input x (loaded), then clobbered for log coefficients, then reused for boundary check |
| LREG1 | num = (1+x), then log input argument, then preserved across log for log(0) check |
| LREG2 | den = (1-x), then log result, then final output |
| LREG3 | Copy of original x (for sign extraction in boundary handling) |
| LREG4 | recip result, then reused in log (normalized mantissa, exponent) |
| LREG5 | Log polynomial intermediate (Horner accumulator) |
| LREG6 | den sign correction workspace |
| LREG7 | |x| (saved for boundary checks after main computation) |

This uses all 8 LREGs. Register pressure is tight but sufficient.

### Function: `_calculate_atanh_(iterations)`

Standard outer loop pattern:

```cpp
inline void _calculate_atanh_(const int iterations)
{
    TTI_SFPENCC(1, 2);  // enable conditional execution
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_atanh_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
    TTI_SFPENCC(0, 2);  // disable conditional execution
}
```

## Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|--------------------|-------------------|-----------------|
| `sfpi::abs(inp)` | `TTI_SFPABS(src, dst, 1)` | `ckernel_sfpu_abs.h`, `ckernel_sfpu_trigonometry.h` (asinh) |
| `sfpi::setsgn(inf, inp)` | `TTI_SFPSETSGN(0, lreg_magnitude, lreg_sign_and_dest, 0)` | Confluence page 1170505767 SFPSETSGN section, `ckernel_ops.h` line 723 |
| `_sfpu_reciprocal_<N>(den)` | `TTI_SFPNONLINEAR(src, dst, p_sfpnonlinear::RECIP_MODE)` | `ckernel_sfpu_recip.h` (Quasar), `ckernel_sfpu_sigmoid.h` |
| `_calculate_log_body_no_init_(tmp)` | Inline log sequence (SFPSETEXP + SFPMAD*3 + SFPEXEXP + SFPCAST*2 + SFPMAD) | `ckernel_sfpu_log.h` (Quasar), same inline pattern used in phase 2's acosh/asinh |
| `sfpi::vConst1 + inp` | `TTI_SFPADD(LCONST_1, lreg, LCONST_1, dst, 0)` | Standard Quasar pattern from phase 2 |
| `sfpi::vConst1 - inp` | `TTI_SFPMAD(lreg, LCONST_1, LCONST_1, dst, 0x2)` then `TTI_SFPMOV(dst, dst, 1)` | Phase 2 pattern, negation via SFPMOV instr_mod=1 |
| `num * den` | `TTI_SFPMUL(a, b, LCONST_0, dst, 0)` | Standard Quasar multiply |
| `0.5f * den` | `TTI_SFPLOADI(lreg, 0, 0x3F00)` + `TTI_SFPMUL(lreg, src, LCONST_0, dst, 0)` | 0.5 as BF16 = 0x3F00 |
| `NaN` | `TTI_SFPLOADI(lreg, 0, 0x7FC0)` | BF16 encoding of quiet NaN (sign=0, exp=0xFF, mantissa MSB=1) |
| `+infinity` | `TTI_SFPLOADI(lreg, 0, 0x7F80)` | BF16 encoding of +inf (sign=0, exp=0xFF, mantissa=0) |
| `abs_inp > vConst1` | Check `(|x| - 1.0) > 0` via `TTI_SFPSETCC(0, lreg, 0x8)` | Positive-and-nonzero check, avoid SFPGT register pressure |
| `abs_inp == vConst1` | Check `(|x| - 1.0) == 0` via `TTI_SFPSETCC(0, lreg, 0x6)` | Zero check, same pattern as acosh's `(x-1) == 0` check |
| `v_if / v_elseif / v_else / v_endif` | Sequential SFPSETCC + SFPLOADI/SFPMOV + SFPENCC (no nesting) | Phase 2's acosh boundary handling pattern |

## Instruction Verification Against assembly.yaml

All instructions verified present in `tt_llk_quasar/instructions/assembly.yaml`:

| Instruction | Verified |
|-------------|----------|
| SFPLOAD | YES |
| SFPSTORE | YES |
| SFPLOADI | YES |
| SFPMAD | YES |
| SFPMUL | YES |
| SFPADD | YES |
| SFPMOV | YES |
| SFPABS | YES |
| SFPSETSGN | YES (opcode 0x89) |
| SFPNONLINEAR/SFPARECIP | YES (opcode 0x99) |
| SFPSETEXP | YES |
| SFPEXEXP | YES |
| SFPCAST | YES |
| SFPSETCC | YES |
| SFPENCC | YES |

## Implementation Structure

### Includes
No additional includes needed. Phase 1 already established the file with:
```cpp
#include <cstdint>
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```

### Namespace
Same as existing: `namespace ckernel { namespace sfpu { ... } }`

### Functions

| Function | Template Params | Purpose |
|----------|-----------------|---------|
| `_init_atanh_()` | none | No-op init (hardware recip needs no setup) |
| `_calculate_atanh_sfp_rows_()` | none | Inner per-2-row computation for atanh |
| `_calculate_atanh_(const int iterations)` | none | Outer iteration loop |

## Potential Issues

1. **SFPSETSGN operand ordering**: The SFPSETSGN instruction sets the sign of RG[VC] to the sign of RG[VD], storing the result in RG[VD]. The macro is `TTI_SFPSETSGN(imm12, lreg_c, lreg_dest, mod)`. So `lreg_c` provides the magnitude (abs value) and `lreg_dest` provides the sign AND receives the result. The writer must be careful to get the operand order correct.

2. **Reciprocal sign correction**: `SFPNONLINEAR RECIP_MODE` may not preserve sign correctly for all inputs. The reference explicitly does `setsgn(recip_result, denominator)` to correct the sign. This step is critical and must not be omitted.

3. **Boundary detection order**: The boundary overwrite must happen AFTER the main computation. The main computation clobbers LREG0-6 during the inline log, so boundary detection values (|x| in LREG7, original x in LREG3) must be preserved across the log computation. LREG7 and LREG3 are both safe since the inline log only uses LREG0, LREG1, LREG2, LREG4, LREG5.

4. **SFPSETCC mode for "greater than zero"**: We need CC=1 where `(|x| - 1) > 0`, which means positive and non-zero. InstrMod `0x8` checks for "positive" (sign bit clear), but we also need to exclude zero. An alternative is to use `TTI_SFPGT` to compare against a register holding 0.0. However, SFPSETCC with mode 0x8 may include +0.0 in the "positive" set. We should verify: does `SFPSETCC(0, lreg, 0x8)` return true for +0.0? If so, the `|x| == 1` boundary (which produces `|x| - 1 == +0.0`) would be incorrectly classified as `|x| > 1`. The safe approach:
   - First handle `|x| == 1` (zero check with mode 0x6)
   - Then handle `|x| > 1` (positive check with mode 0x8, but +0.0 lanes already overwritten)

   **Actually**, a better approach: since we do sequential overwrites, process boundaries in this order:
   1. First overwrite `|x| > 1` lanes with NaN (using SFPSETCC mode that detects positive non-zero, or simply "positive" since the |x|==1 case will be overwritten next)
   2. Then overwrite `|x| == 1` lanes with signed infinity (this corrects any +0.0 false positive from step 1)

   This is the order shown in the algorithm above and is safe because the `|x| == 1` overwrite happens last.

5. **Computing `1 - x` without negate-C pattern issue**: The expression `1 - x` requires computing `(-1) * x + 1`. Using SFPMAD with negate-A flag: `TTI_SFPMAD(LREG3, LCONST_1, LCONST_1, LREG2, 0x1)` would compute `(-LREG3) * 1.0 + 1.0 = 1 - x`. This is cleaner than `x*1 + (-1)` followed by negation. The writer should use this approach: InstrMod bit 0 = negate SrcA.

## Init/Uninit Symmetry

| Init Action | Uninit Reversal |
|-------------|-----------------|
| No-op (`_init_atanh_()` is empty) | No uninit needed |

There is no `_uninit_atanh_()` in the reference or in the target test infrastructure. The init is a no-op so no hardware state is modified.

## Recommended Test Formats

### Format List
Based on the analysis's "Format Support" section (float-only transcendental operation) and the architecture research's "Format Support Matrix":

```python
# Float-only SFPU op — same as sine/cosine/acosh/asinh
FORMATS = input_output_formats([
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Float32,
    DataFormat.MxFp8R,
    # MxFp8P excluded: lower precision causes quantization error amplified
    # by atanh computation (division + log) to exceed tolerance.
    # MxFp8R provides sufficient MX format coverage.
])
```

### Invalid Combination Rules
Rules for `_is_invalid_quasar_combination()` — same as the existing trigonometry test (already implemented in `test_sfpu_trigonometry_quasar.py`):

1. Quasar packer does not support non-Float32 -> Float32 when dest_acc=No
2. Float32 input -> Float16 output requires dest_acc=Yes
3. Integer and float formats cannot be mixed in input->output
4. MxFp8 input -> non-MxFp8 output: golden mismatch due to MxFp8 quantization error amplified by atanh computation (division + log chain). MxFp8 -> MxFp8 is fine since output quantization absorbs the error.

### MX Format Handling
- MxFp8R requires `implied_math_format=Yes` in test configuration
- MX formats exist only in L1, unpacked to Float16_b for SFPU math
- Combination generator must skip MX + `implied_math_format=No`

### Integer Format Handling
Not applicable -- atanh is a float-only transcendental operation.

## Testing Notes

### Input Range
Atanh is defined for |x| < 1 (open interval). Test inputs should:
- Include values in (-1, 1) with good distribution (e.g., -0.99, -0.5, 0, 0.5, 0.99)
- Include boundary values x = -1.0 and x = 1.0 (expected: -inf, +inf)
- Include out-of-range values |x| > 1 (expected: NaN)
- Use a distribution that covers the interior domain well

### Expected Precision
- Hardware reciprocal (SFPNONLINEAR RECIP_MODE) provides FP16_B precision (~7 mantissa bits)
- The inline log polynomial provides limited precision (~BF16 coefficients)
- Combined precision: atanh results will have approximately BF16 accuracy
- The `passed_test()` helper with output format tolerance should accommodate this

### Test Infrastructure Updates
The test writer must:
1. Add `SfpuType::atanh` to the enum in `tt_llk_quasar/llk_lib/llk_defs.h`
2. Add dispatch for `SfpuType::atanh` in `tests/sources/quasar/sfpu_trigonometry_quasar_test.cpp`
3. Add `MathOperation.Atanh` to the `TRIG_OPS` list in `test_sfpu_trigonometry_quasar.py`
4. Add an `prepare_atanh_inputs()` function that generates values in (-1, 1) with boundary cases
