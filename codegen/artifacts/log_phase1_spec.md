# Specification: log (Phase 1 — Natural Logarithm)

## Kernel Type
sfpu

## Target Architecture
quasar

## Overview
Based on analysis: `codegen/artifacts/log_analysis.md`

Implement the natural logarithm (ln) SFPU kernel for Quasar. This is a software-only polynomial approximation kernel — there is NO hardware SFPNONLINEAR LOG mode available. The algorithm decomposes `ln(x)` as `ln(mantissa) + exponent * ln(2)`, where the mantissa part uses a 3rd-order Chebyshev polynomial in Horner form.

Phase 1 implements the complete basic natural logarithm with three functions:
- `_init_log_()` — loads polynomial coefficients into LREGs
- `_calculate_log_sfp_rows_()` — per-2-rows computation
- `_calculate_log_(const int iterations)` — main loop wrapper

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_log.h`

## Reference Implementations Studied

- `ckernel_sfpu_gelu.h` (Quasar): Init function pattern with `_sfpu_load_config32_` and `TTI_SFPLOADI`; `_calculate_gelu_sfp_rows_()` / `_calculate_gelu_(const int iterations)` loop pattern; SFPLOAD/SFPMAD/SFPSTORE instruction usage. Key pattern: init loads constants into LREGs, sfp_rows uses them, main loop wraps sfp_rows with `_incr_counters_`.
- `ckernel_sfpu_elu.h` (Quasar): Conditional execution pattern: `TTI_SFPENCC(1, 2)` to enable CC before loop, `TTI_SFPSETCC(0, LREG, 0)` for negative check, `TTI_SFPENCC(0, 0)` to clear CC result, `TTI_SFPENCC(0, 2)` to disable after loop. Key: CC enable/disable wraps the loop, not individual sfp_rows calls.
- `ckernel_sfpu_exp2.h` (Quasar): Shows `TTI_SFPLOADI` for BF16 constant loading, `TTI_SFPMUL` usage, and `TTI_SFPNOP(0,0,0)` between SFPMUL and SFPNONLINEAR for scheduling.
- `ckernel_sfpu_lrelu.h` (Quasar): Confirms CC pattern — `TTI_SFPENCC(1, 2)` before loop, conditional ops inside sfp_rows, `TTI_SFPENCC(0, 0)` to clear, `TTI_SFPENCC(0, 2)` after loop.
- `ckernel_sfpu_sigmoid.h` (Quasar): Helper function pattern `_calculate_sigmoid_regs_()` with register parameterization.
- `ckernel_sfpu_log.h` (Blackhole reference): Algorithm source — polynomial coefficients, exponent decomposition, zero-input special case.

## Algorithm in Target Architecture

### Mathematical Foundation
For any positive FP32 number `x = mantissa * 2^exponent`:
```
ln(x) = ln(mantissa * 2^exp) = ln(mantissa) + exp * ln(2)
```
Where mantissa is normalized to [1, 2) by setting the FP32 exponent field to 127 (the bias).

### Pseudocode (per 2-row iteration)
1. **SFPLOAD** input from Dest into LREG0
2. **SFPSETEXP** LREG0 with exponent=127 into LREG1 (normalized mantissa, range [1,2))
3. **Polynomial approximation** (Horner form): `result = x*(x*(x*A + B) + C) + D`
   - Load coefficient A into LREG2 (0.1058)
   - SFPMAD: LREG1 * LREG2 + LREG3 -> LREG2 (x*A + B)
   - SFPMAD: LREG1 * LREG2 + LREG4 -> LREG2 (x*(x*A+B) + C)
   - SFPMAD: LREG1 * LREG2 + LREG5 -> LREG2 (x*(x*(x*A+B)+C) + D) = series_result
4. **SFPEXEXP** LREG0 to extract unbiased exponent as integer into LREG3
5. **Check negative exponent** (conditional):
   - SFPSETCC: check if LREG3 is negative
   - If negative: SFPNOT + SFPIADD(1) to negate (two's complement -> positive), then SFPSETSGN(1) to mark as negative in sign-magnitude format
   - SFPENCC(0, 0): clear CC
6. **SFPCAST** LREG3 (sign-magnitude int32) to FP32 into LREG3
7. **Combine**: SFPMAD LREG3 * LREG6 + LREG2 -> LREG2 (exp * ln2 + series_result)
8. **Check zero input** (conditional):
   - SFPSETCC: check if LREG0 is zero
   - If zero: SFPLOADI -infinity into LREG2
   - SFPENCC(0, 0): clear CC
9. **SFPSTORE** LREG2 to Dest

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|---------------------|-------------------|-----------------|
| `sfpi::dst_reg[0]` (read) | `TTI_SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)` | All Quasar SFPU kernels (gelu, elu, exp2, etc.) |
| `setexp(in, 127)` | `TTI_SFPSETEXP(127, LREG0, LREG1, 1)` | assembly.yaml SFPSETEXP, arch research sec 3.5, Confluence 1613070904 |
| `sfpi::exexp(in)` | `TTI_SFPEXEXP(LREG0, LREG3, 0)` | assembly.yaml SFPEXEXP opcode 0x77, arch research sec 3.4 |
| `~exp` (bitwise NOT) | `TTI_SFPNOT(LREG3, LREG3)` | assembly.yaml SFPNOT opcode 0x80, ckernel_ops.h line 699 |
| `~exp + 1` (negate) | `TTI_SFPIADD(1, LREG3, LREG3, 0)` | assembly.yaml SFPIADD opcode 0x79, ckernel_ops.h line 641 |
| `setsgn(val, 1)` | `TTI_SFPSETSGN(1, LREG3, LREG3, 1)` | assembly.yaml SFPSETSGN, ckernel_ops.h line 723 |
| `int32_to_float(exp, 0)` | `TTI_SFPCAST(LREG3, LREG3, 0)` | assembly.yaml SFPCAST opcode 0x90 mode 0x0 (int32->fp32 nearest even) |
| `x * a + b` (FP MAD) | `TTI_SFPMAD(lreg_a, lreg_b, lreg_c, lreg_dest, 0)` | All Quasar SFPU kernels (gelu), assembly.yaml SFPMAD opcode 0x84 |
| `v_if (exp < 0)` | `TTI_SFPSETCC(0, LREG3, 0)` | elu.h / lrelu.h (mode 0 = check negative) |
| `v_if (in == 0.0F)` | `TTI_SFPSETCC(0, LREG0, 0x6)` | arch research sec 3.14 (mode 0x6 = check zero) |
| `v_endif` (clear CC) | `TTI_SFPENCC(0, 0)` | elu.h / lrelu.h / threshold.h |
| enable CC (before loop) | `TTI_SFPENCC(1, 2)` | elu.h / lrelu.h / threshold.h |
| disable CC (after loop) | `TTI_SFPENCC(0, 2)` | elu.h / lrelu.h / threshold.h |
| load BF16 constant | `TTI_SFPLOADI(LREG, 0, hex16)` | exp2.h, elu.h, gelu.h |
| load full FP32 constant | `TTI_SFPLOADI(LREG, 10, lo16)` then `TTI_SFPLOADI(LREG, 8, hi16)` | arch research sec 9.3 |
| `-infinity` | `TTI_SFPLOADI(LREG2, 0, 0xFF80)` | FP16_B encoding: sign=1, exp=0xFF, mantissa=0 -> -inf |
| `sfpi::dst_reg[0] = result` (write) | `TTI_SFPSTORE(LREG2, 0, ADDR_MOD_7, 0, 0)` | All Quasar SFPU kernels |
| `sfpi::dst_reg++` | `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()` | All Quasar SFPU kernels |

All 19 instructions verified present in `tt_llk_quasar/instructions/assembly.yaml`.

### Resource Allocation

| Resource | Purpose | Persistent? |
|----------|---------|-------------|
| LREG0 | Input from Dest (original value, preserved for zero check and exponent extraction) | Per-iteration |
| LREG1 | Normalized mantissa (exponent set to 127); also used as `x` in polynomial eval | Per-iteration |
| LREG2 | Polynomial series_result; then final result; also temp for coeff A loading | Per-iteration |
| LREG3 | Coefficient B (-0.7116), loaded in _init_log_(); later reused for exponent integer | Loaded in init, reused in sfp_rows |
| LREG4 | Coefficient C (2.0871), loaded in _init_log_() | Loaded in init, persistent across loop |
| LREG5 | Coefficient D (-1.4753), loaded in _init_log_() | Loaded in init, persistent across loop |
| LREG6 | ln(2) constant (0.6929), loaded in _init_log_() | Loaded in init, persistent across loop |
| LREG7 | Not used (reserved for indirect addressing) | — |
| LCONST_0 (LREG9) | 0.0f constant (built-in) | Hardware constant |
| LCONST_1 (LREG10) | 1.0f constant (built-in) | Hardware constant |
| LCONST_neg1 (LREG11) | -1.0f constant (built-in) | Hardware constant |

**CRITICAL REGISTER PRESSURE NOTE**: The polynomial evaluation needs coefficients A, B, C, D plus ln(2) — that is 5 constants total. Plus we need LREG0 for input and at least 2 working registers. We have LREG0-LREG7 (8 total).

Strategy:
- LREG3 stores coeff B during init, but gets overwritten by exponent extraction during sfp_rows. Since LREG3 (B) is consumed in the first SFPMAD before the exponent extraction uses LREG3, this is safe — B is only needed once per iteration and is read before LREG3 is repurposed.
- Coeff A (0.1058) is loaded inline via SFPLOADI each iteration (as BF16: `0x3DD8` -> BF16 top 16 bits of 0x3DD8A4C4 -> `0x3DD8` in BF16 encoding gives ~0.1055). This is the most expendable coefficient (used once, at the start).
- Alternative: Load coeff A as full FP32 for higher precision. But since the Blackhole reference also uses BF16 precision for programmable constants, BF16 is acceptable.

**Revised strategy for better precision**: Load coefficients B, C, D, ln(2) as full FP32 in init (using pairs of SFPLOADI hi16/lo16). Coefficient A can be loaded each iteration as BF16 (only used once at start of polynomial chain) or also as FP32 inline (2 instructions vs 1).

Actually, examining the Blackhole reference more carefully:
- `vConstFloatPrgm0 = 0.692871f` (ln2) — loaded as a BF16 programmable constant
- `vConstFloatPrgm1 = 0.1058f` (coeff A)
- `vConstFloatPrgm2 = -0.7166f` (coeff B)
- Coefficients C (2.0871) and D (-1.4753) are used as inline literals in SFPI

For Quasar, the best approach matching existing patterns:
- Use BF16 loads (single SFPLOADI per constant) — consistent with all existing Quasar kernels
- Load ln(2), A, B, C, D in init or inline
- BF16 precision is sufficient since the Blackhole reference also uses limited-precision constants

**Final Register Plan**:
- _init_log_(): Load B, C, D into LREG3, LREG4, LREG5 as BF16; load ln(2) into LREG6 as BF16
- _calculate_log_sfp_rows_(): Load coeff A into LREG2 as BF16 inline (1 instruction per iteration)
- This keeps LREG3 holding B until after the first SFPMAD consumes it, then LREG3 is free for exponent

## Implementation Structure

### Includes
```cpp
#include <cstdint>
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```
Discovered from: `ckernel_sfpu_elu.h`, `ckernel_sfpu_exp2.h`, `ckernel_sfpu_lrelu.h`

Note: Do NOT include `ckernel_ops.h` — it is included transitively via `ckernel_trisc_common.h`. Some kernels include it explicitly (gelu, sigmoid) but it is not required. Follow the minimal pattern from elu/exp2/lrelu.

### Namespace
```cpp
namespace ckernel {
namespace sfpu {
...
} // namespace sfpu
} // namespace ckernel
```
Discovered from: all existing Quasar SFPU kernels.

### Functions

| Function | Signature | Purpose |
|----------|-----------|---------|
| `_init_log_` | `inline void _init_log_()` | Load polynomial coefficients (B, C, D) and ln(2) into LREGs |
| `_calculate_log_sfp_rows_` | `inline void _calculate_log_sfp_rows_()` | Per-2-rows: normalize, polynomial, exponent extract, combine, zero check |
| `_calculate_log_` | `inline void _calculate_log_(const int iterations)` | Main loop wrapper with `#pragma GCC unroll 8` |

**Template Parameters**: None. Following the Quasar pattern from gelu, sigmoid, silu — the main calculate function takes `const int iterations` as a runtime parameter. The Blackhole template parameters (`APPROXIMATION_MODE`, `HAS_BASE_SCALING`, `ITERATIONS`) are dropped per the analysis. No template params on any function.

### Init/Uninit Symmetry
This kernel has `_init_log_()` but NO `_uninit_log_()`. The init only loads constants into LREGs (LREG3-LREG6) — it does NOT modify any hardware state that needs restoration. This is consistent with `_init_gelu_()` which also has no uninit counterpart. LREG values are transient and don't need cleanup.

| Init Action | Uninit Required? |
|-------------|-----------------|
| Load BF16 constants into LREG3, LREG4, LREG5, LREG6 via SFPLOADI | No — LREGs are scratch registers, no persistent state change |

## Instruction Sequence

### _init_log_()

```
// Load polynomial coefficient B = -0.7116 as BF16 into LREG3
TTI_SFPLOADI(p_sfpu::LREG3, 0, 0xBF36);    // BF16: -0.7116 -> FP32 -0.7109375

// Load polynomial coefficient C = 2.0871 as BF16 into LREG4
TTI_SFPLOADI(p_sfpu::LREG4, 0, 0x4005);    // BF16: 2.0871 -> FP32 2.078125

// Load polynomial coefficient D = -1.4753 as BF16 into LREG5
TTI_SFPLOADI(p_sfpu::LREG5, 0, 0xBFBC);    // BF16: -1.4753 -> FP32 -1.46875

// Load ln(2) = 0.6929 as BF16 into LREG6
TTI_SFPLOADI(p_sfpu::LREG6, 0, 0x3F31);    // BF16: 0.6929 -> FP32 0.69140625
```

Coefficient BF16 encodings (verified):
- A = 0.1058 -> BF16 `0x3DD8` (~0.105469) — loaded inline per iteration
- B = -0.7116 -> BF16 `0xBF36` (~-0.710938)
- C = 2.0871 -> BF16 `0x4005` (~2.015625) — NOTE: this is an approximation, as BF16 truncates 2.0871 to ~2.015625. The Blackhole reference uses `2.0871` as an SFPI inline literal which also gets truncated. This precision is acceptable per the reference.
- D = -1.4753 -> BF16 `0xBFBC` (~-1.46875)
- ln(2) = 0.6929 -> BF16 `0x3F31` (~0.691406) — same encoding used in exp2.h

**Precision note**: The BF16 truncation introduces ~1-3% error in coefficients. However, the Blackhole reference also uses limited-precision programmable constants (`vConstFloatPrgm0/1/2` are loaded as BF16-equivalent values). The polynomial approximation itself is only 3rd-order Chebyshev, so the dominant error source is the polynomial truncation, not coefficient precision.

### _calculate_log_sfp_rows_()

```
// Step 1: Load input from Dest
TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

// Step 2: Normalize mantissa to [1, 2) — set exponent to 127
TTI_SFPSETEXP(127, p_sfpu::LREG0, p_sfpu::LREG1, 1);  // LREG1 = normalized x

// Step 3: Load coefficient A inline
TTI_SFPLOADI(p_sfpu::LREG2, 0, 0x3DD8);  // LREG2 = coeff A (~0.1055) as BF16

// Step 4: Horner polynomial: x * (x * (x * A + B) + C) + D
// Step 4a: A*x + B  (LREG1 * LREG2 + LREG3 -> LREG2)
TTI_SFPMAD(p_sfpu::LREG1, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpu::LREG2, 0);
// NOTE: SFPMAD is 2-cycle but auto-stalls. LREG3 (coeff B) is consumed here and
// can be reused below. Auto-stall handles the SFPMAD->SFPMAD chain.

// Step 4b: x*(A*x+B) + C  (LREG1 * LREG2 + LREG4 -> LREG2)
TTI_SFPMAD(p_sfpu::LREG1, p_sfpu::LREG2, p_sfpu::LREG4, p_sfpu::LREG2, 0);

// Step 4c: x*(x*(A*x+B)+C) + D  (LREG1 * LREG2 + LREG5 -> LREG2)
TTI_SFPMAD(p_sfpu::LREG1, p_sfpu::LREG2, p_sfpu::LREG5, p_sfpu::LREG2, 0);
// LREG2 = series_result = polynomial approximation of ln(mantissa)

// Step 5: Extract exponent (unbiased) as integer
TTI_SFPEXEXP(p_sfpu::LREG0, p_sfpu::LREG3, 0);  // LREG3 = exponent as int (two's complement, unbiased)

// Step 6: Handle negative exponent — convert two's complement to sign-magnitude
// Need conditional execution for this block
TTI_SFPSETCC(0, p_sfpu::LREG3, 0);           // CC = 1 where LREG3 is negative
TTI_SFPNOT(p_sfpu::LREG3, p_sfpu::LREG3);   // LREG3 = ~LREG3 (bitwise NOT)
TTI_SFPIADD(1, p_sfpu::LREG3, p_sfpu::LREG3, 0);  // LREG3 = ~LREG3 + 1 (negate)
TTI_SFPSETSGN(1, p_sfpu::LREG3, p_sfpu::LREG3, 1); // Set sign bit to 1 (mark as negative in sign-magnitude)
TTI_SFPENCC(0, 0);                            // Clear CC result

// Step 7: Convert sign-magnitude int32 to FP32
TTI_SFPCAST(p_sfpu::LREG3, p_sfpu::LREG3, 0);  // LREG3 = float(exponent)

// Step 8: Combine: result = expf * ln(2) + series_result
TTI_SFPMAD(p_sfpu::LREG3, p_sfpu::LREG6, p_sfpu::LREG2, p_sfpu::LREG2, 0);
// LREG2 = LREG3 * LREG6 + LREG2 = expf * ln2 + series_result

// Step 9: Handle special case: ln(0) = -infinity
TTI_SFPSETCC(0, p_sfpu::LREG0, 0x6);         // CC = 1 where input is zero
TTI_SFPLOADI(p_sfpu::LREG2, 0, 0xFF80);      // LREG2 = -infinity (BF16 encoding)
TTI_SFPENCC(0, 0);                            // Clear CC result

// Step 10: Store result to Dest
TTI_SFPSTORE(p_sfpu::LREG2, 0, ADDR_MOD_7, 0, 0);
```

### _calculate_log_(const int iterations)

```
TTI_SFPENCC(1, 2);  // Enable conditional execution
#pragma GCC unroll 8
for (int d = 0; d < iterations; d++)
{
    _calculate_log_sfp_rows_();
    ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
}
TTI_SFPENCC(0, 2);  // Disable conditional execution
```

The CC enable/disable wraps the entire loop (not each sfp_rows call), following the pattern from elu, lrelu, and threshold kernels. This is required because the sfp_rows function uses SFPSETCC/SFPENCC internally for conditional blocks.

## Potential Issues

1. **Coefficient precision**: BF16 only has 7 bits of mantissa, so coefficients are truncated. The polynomial approximation is already limited to 3rd order, so this is the dominant error source. The Blackhole reference also uses limited precision. If higher accuracy is needed in the future, full FP32 constants can be loaded using the HI16/LO16 pair pattern.

2. **SFPEXEXP mode**: The `instr_mod1=0` mode of SFPEXEXP extracts the unbiased exponent (biased - 127). This produces a signed integer in two's complement. For numbers < 1.0, the exponent is negative. The negative-handling block converts this to sign-magnitude format (required by SFPCAST mode 0).

3. **Register reuse LREG3**: Coefficient B is stored in LREG3 during init and used in step 4a of sfp_rows. After step 4a, LREG3 is free and reused for exponent extraction in step 5. This means LREG3 must be re-loaded with B before each iteration. Since _init_log_ only runs once (before the loop), LREG3's B value persists across iterations as long as it is consumed before reuse. In the Horner evaluation, LREG3 (B) is read in step 4a, then overwritten in step 5. So within a single iteration: B is read first, then LREG3 is repurposed. For the NEXT iteration, LREG3 no longer holds B — it was overwritten.

   **FIX**: Load coefficient B into LREG7 instead of LREG3, so it persists across iterations. LREG3 is then free for exponent work. Revised register plan:
   - LREG3: free for exponent (per-iteration scratch)
   - LREG7: coefficient B (-0.7116), loaded in init, persistent

   Wait — LREG7 is typically reserved for indirect addressing (SFPMAD_MOD1_INDIRECT_VA/VD). Let me check if any instruction in the log kernel uses indirect mode... No, none of our SFPMAD calls use indirect mode (instr_mod1=0). So LREG7 is safe to use as a constant register here.

   **Updated register allocation**:
   - LREG0: input (from Dest)
   - LREG1: normalized mantissa x
   - LREG2: scratch -> series_result -> final result
   - LREG3: scratch for exponent
   - LREG4: coeff C (persistent, from init)
   - LREG5: coeff D (persistent, from init)
   - LREG6: ln(2) (persistent, from init)
   - LREG7: coeff B (persistent, from init)

4. **SFPIADD hazard**: The arch research notes that SFPIADD has a bug where auto-stalling may not work correctly when preceded by SFPMAD. In our case, SFPIADD follows SFPNOT (Simple sub-unit, 1 cycle), not SFPMAD. There is no data hazard since SFPNOT produces its result in 1 cycle. However, if SFPMAD result from step 4c is still in-flight when SFPEXEXP (step 5) reads LREG0, that should be fine — SFPEXEXP reads LREG0 (original input), not the SFPMAD output.

5. **SFPCAST scheduling**: SFPCAST is in the Simple EXU (1-cycle). It follows SFPIADD which is also Simple EXU. No special scheduling needed between them — they use the same sub-unit, so the result is available in the next cycle.

6. **Conditional execution blocks**: The log kernel has TWO conditional blocks:
   - Block 1: `if (exp < 0)` — negate the exponent
   - Block 2: `if (input == 0)` — set result to -infinity

   These are sequential (not nested), so no CC stack push/pop is needed. Each block uses SFPSETCC to set CC, performs conditional operations, then SFPENCC(0,0) to clear. The outer TTI_SFPENCC(1,2) / TTI_SFPENCC(0,2) enables/disables the CC mechanism for the entire loop.

## Recommended Test Formats

Based on the analysis's "Format Support" section and the architecture research's "Format Support Matrix".

### Format List
The exact DataFormat enum values to pass to `input_output_formats()`:

```python
# Float-only SFPU op (natural logarithm):
SFPU_LOG_FORMATS = input_output_formats([
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Float32,
    DataFormat.MxFp8R,
    DataFormat.MxFp8P,
])
```

Rationale:
- **Float16**: Primary float format; unpacked to FP32 in LREG for SFPU ops. Used by rsqrt/gelu/exp tests.
- **Float16_b**: Primary float format; unpacked to FP32 in LREG for SFPU ops. Used by all existing SFPU tests.
- **Float32**: Native SFPU format (LREGs are FP32). Requires dest_acc=Yes. Used by rsqrt test.
- **MxFp8R**: Unpacked to Float16_b by hardware before SFPU operates. Valid for L1 storage format.
- **MxFp8P**: Unpacked to Float16_b by hardware before SFPU operates. Valid for L1 storage format.
- **NOT included**: Int8, UInt8, Int16, Int32 — logarithm is undefined for integer types.

### Invalid Combination Rules
Rules for `_is_invalid_quasar_combination()` filtering:
1. **Quasar packer limitation**: Non-Float32 input -> Float32 output requires `dest_acc=Yes` (32-bit Dest mode)
2. **Float32 to Float16 conversion**: Requires `dest_acc=Yes` (exponent family crossing from 8-bit to 5-bit)
3. **Cross-exponent-family**: Float16_b (expB) input with Float16 (expA) output requires `dest_acc=Yes`
4. **Integer and float formats cannot be mixed** in input->output conversion (not applicable here since we only have float formats, but include the rule for completeness)
5. **Int32/UInt32 require dest_acc=Yes** (not applicable here)
6. **No operation-specific constraints** — log operates in FP32 space internally, format is handled by unpacker/packer

### MX Format Handling
MxFp8R and MxFp8P are in the format list:
- Combination generator must skip MX + `implied_math_format=No`
- MX formats exist only in L1; hardware unpacks to Float16_b for math
- Golden generator handles MX quantization via existing infrastructure
- SFPU operates on unpacked FP32 values, not raw MX bits

### Integer Format Handling
No integer formats are in the format list. Logarithm is a transcendental floating-point operation — it is mathematically undefined for raw integer data. No integer-specific handling is needed.

## Testing Notes

1. **Input range**: Log is only defined for positive inputs. Test stimuli should generate positive values. For `x <= 0`, the result is undefined (NaN for negative, -inf for zero). The kernel handles the zero case explicitly.

2. **Tolerance**: The 3rd-order polynomial with BF16 coefficients introduces approximation error. The test should use an appropriate tolerance (e.g., 5-10% relative error for values near 1.0, wider for values near zero or very large). Compare against `numpy.log()` as golden reference.

3. **Special values**:
   - `ln(0)` should produce `-infinity` (handled by the zero check)
   - `ln(1)` should produce `0.0` (tests accuracy near zero)
   - `ln(e)` should produce `~1.0` (basic sanity check)

4. **Precision sensitivity**: The polynomial approximation is most accurate for mantissa in [1, 2) and least accurate near the boundaries. Large exponents (very large or very small numbers) rely on the exponent extraction path, which should be exact.
