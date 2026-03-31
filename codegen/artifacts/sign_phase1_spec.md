# Specification: sign — Phase 1 (Complete Kernel)

## Kernel Type
sfpu

## Target Architecture
quasar

## Overview
Based on analysis: `codegen/artifacts/sign_analysis.md`

Implements the element-wise sign function: sign(x) = +1.0 if x > 0, -1.0 if x < 0, 0.0 if x == 0.
The output is always a floating-point value from the set {-1.0, 0.0, +1.0}.

This is a simple SFPU kernel with a single phase containing two functions:
- `_calculate_sign_sfp_rows_()` — inner loop body processing one iteration of rows
- `_calculate_sign_(const int iterations)` — outer loop with constant loading, CC setup, iteration loop, and CC teardown

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_sign.h`

## Reference Implementations Studied

- `ckernel_sfpu_elu.h`: CC pattern — `TTI_SFPSETCC(0, LREG0, 0)` for negative check, `TTI_SFPENCC(0, 0)` for CC reset, outer function with constant loading and CC enable/disable bookkeeping. Closest overall pattern to sign.
- `ckernel_sfpu_threshold.h`: CC pattern — `TTI_SFPGT` for comparison + conditional `TTI_SFPMOV` to replace values. Demonstrates conditional value replacement pattern.
- `ckernel_sfpu_lrelu.h`: Multiple sub-kernels (lrelu, relu_min, relu_max) showing various CC patterns. `_calculate_relu_max_sfp_rows_()` demonstrates two sequential CC checks with `TTI_SFPENCC(0, 0)` reset between them — directly applicable to sign's negative-then-zero check sequence.
- `ckernel_sfpu_activations.h` (hardsigmoid): Multiple sequential CC checks (`TTI_SFPSETCC` mode 0 for negative, `TTI_SFPGT` for upper bound) with `TTI_SFPENCC(0, 0)` resets between them. Also shows `TTI_SFPLOADI` used to conditionally load constant 0.0.
- `ckernel_sfpu_abs.h`: Simplest pattern — load, transform, store. No CC needed. Shows basic file structure.
- `ckernel_sfpu_trigonometry.h` / `ckernel_sfpu_log.h`: Uses `TTI_SFPSETCC(0, p_sfpu::LREG0, 0x6)` for zero check — confirms mode 6 syntax on Quasar with imm12=0.

## Algorithm in Target Architecture

### Approach: Two Sequential CC Checks

The sign kernel uses two sequential SFPSETCC checks with SFPENCC resets between them, following the proven pattern from `_calculate_relu_max_sfp_rows_()` and `_calculate_hardsigmoid_sfp_rows_()`. This avoids using SFPCOMPC (which has no argument-taking macro and is not used in any existing Quasar SFPU kernel).

**Why not SFPCOMPC?** While SFPCOMPC exists in assembly.yaml and `ckernel_ops.h`, it takes no parameters (`TTI_SFPCOMPC` is a bare macro with no arguments). No existing Quasar SFPU kernel uses it. The two-sequential-check pattern is proven and sufficient for sign.

### Pseudocode

**Inner function `_calculate_sign_sfp_rows_()`:**
1. Load input x from Dest into LREG0
2. Start with result = +1.0 by copying LREG1 (pre-loaded +1.0) into LREG3 (result register) — **unconditional copy** using InstrMod=2
3. Check if x is negative: `SFPSETCC(mode=0)` on LREG0
4. For negative lanes: copy LREG2 (pre-loaded -1.0) into LREG3 — **conditional copy**
5. Reset CC for all lanes: `SFPENCC(0, 0)`
6. Check if x is zero: `SFPSETCC(mode=6)` on LREG0
7. For zero lanes: load 0.0 directly into LREG3 — **conditional load**
8. Reset CC for all lanes: `SFPENCC(0, 0)`
9. Store LREG3 (result) to Dest

**Outer function `_calculate_sign_(const int iterations)`:**
1. Load +1.0 into LREG1 (persistent constant)
2. Load -1.0 into LREG2 (persistent constant)
3. Enable CC: `SFPENCC(1, 2)`
4. Loop `iterations` times:
   a. Call `_calculate_sign_sfp_rows_()`
   b. Increment counters by SFP_ROWS
5. Disable CC: `SFPENCC(0, 2)`

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|--------------------|-------------------|-----------------|
| `sfpi::vFloat v = sfpi::dst_reg[0]` | `TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` | ckernel_sfpu_elu.h line 20 |
| `sfpi::dst_reg[0] = sfpi::vConst1` (default +1.0) | `TTI_SFPMOV(p_sfpu::LREG1, p_sfpu::LREG3, 2)` (unconditional copy, InstrMod=2) | SFPMOV ISA spec: InstrMod=2 = unconditional copy |
| `v_if (v < 0.0F) { dst_reg[0] = vConstNeg1 }` | `TTI_SFPSETCC(0, p_sfpu::LREG0, 0)` + `TTI_SFPMOV(p_sfpu::LREG2, p_sfpu::LREG3, 0)` | ckernel_sfpu_elu.h line 22 (SFPSETCC mode 0); ckernel_sfpu_threshold.h line 25 (conditional SFPMOV) |
| `v_if (_sfpu_is_fp16_zero_(...)) { dst_reg[0] = vConst0 }` | `TTI_SFPSETCC(0, p_sfpu::LREG0, 0x6)` + `TTI_SFPLOADI(p_sfpu::LREG3, 0, 0x0000)` | ckernel_sfpu_log.h line 72 (SFPSETCC mode 6 for zero); ckernel_sfpu_activations.h line 91 (conditional SFPLOADI) |
| CC reset between checks | `TTI_SFPENCC(0, 0)` | ckernel_sfpu_lrelu.h line 89 (relu_max: reset between sequential checks) |
| `sfpi::dst_reg++` | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` | ckernel_sfpu_elu.h line 44 |
| `sfpi::vConst1` (+1.0) | `TTI_SFPLOADI(p_sfpu::LREG1, 0, 0x3F80)` — BF16 encoding of 1.0 | ckernel_sfpu_activations.h line 64 (LREG6 = 0x3F80 = 1.0) |
| `sfpi::vConstNeg1` (-1.0) | `TTI_SFPLOADI(p_sfpu::LREG2, 0, 0xBF80)` — BF16 encoding of -1.0 | BF16: sign=1, exp=0x7F, man=0 = 0xBF80 |
| CC enable before loop | `TTI_SFPENCC(1, 2)` | ckernel_sfpu_elu.h line 39 |
| CC disable after loop | `TTI_SFPENCC(0, 2)` | ckernel_sfpu_elu.h line 46 |

### Resource Allocation

| Resource | Purpose |
|----------|---------|
| LREG0 | Input value x (loaded from Dest each iteration) |
| LREG1 | Persistent constant: +1.0 (loaded once before loop) |
| LREG2 | Persistent constant: -1.0 (loaded once before loop) |
| LREG3 | Result register: accumulates the sign result through conditional operations |

**Why LREG3 for result instead of LREG0?**
Using a separate result register (LREG3) allows us to keep the original input in LREG0 for both the negative check and the zero check. If we overwrote LREG0 with the result, the second SFPSETCC (zero check) would check the result value instead of the original input. This is a critical correctness requirement.

## Implementation Structure

### Includes
```cpp
#include <cstdint>
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```
Source: Every existing Quasar SFPU kernel (elu.h, threshold.h, abs.h, lrelu.h) uses exactly these includes. The `<cstdint>` is used for `std::uint32_t` in function signatures.

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
Source: All existing Quasar SFPU kernels use this exact namespace structure.

### Functions

| Function | Template Params | Parameters | Purpose |
|----------|-----------------|------------|---------|
| `_calculate_sign_sfp_rows_` | None | None | Inner loop: load from Dest, apply sign logic, store to Dest |
| `_calculate_sign_` | None | `const int iterations` | Outer loop: load constants, enable CC, iterate, disable CC |

### Function Signatures (verified against target patterns)

```cpp
inline void _calculate_sign_sfp_rows_()
inline void _calculate_sign_(const int iterations)
```

**Verification**:
- No template parameters — matches all Quasar SFPU kernels (elu, threshold, abs, lrelu, fill)
- `const int iterations` as single param — matches `_calculate_abs_` (simplest case with no extra params)
- No `exponent_size_8` param — Quasar does not need it (SFPU operates on FP32 in LREGs)
- Call site pattern: `_llk_math_eltwise_unary_sfpu_params_<false>(ckernel::sfpu::_calculate_sign_, i, num_sfpu_iterations)`

## Instruction Sequence

### Inner Function: `_calculate_sign_sfp_rows_()`

```
// Step 1: Load input
TTI_SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)     // x from Dest -> LREG0

// Step 2: Default result = +1.0 (unconditional copy)
TTI_SFPMOV(LREG1, LREG3, 2)                        // LREG3 = +1.0 (InstrMod=2: unconditional)

// Step 3: Negative check
TTI_SFPSETCC(0, LREG0, 0)                          // CC.Res = 1 where x < 0

// Step 4: For negative lanes, result = -1.0
TTI_SFPMOV(LREG2, LREG3, 0)                        // LREG3 = -1.0 (conditional: only negative lanes)

// Step 5: Reset CC for next check
TTI_SFPENCC(0, 0)                                   // CC.Res = 1 for all lanes

// Step 6: Zero check
TTI_SFPSETCC(0, LREG0, 0x6)                        // CC.Res = 1 where x == 0

// Step 7: For zero lanes, result = 0.0
TTI_SFPLOADI(LREG3, 0, 0x0000)                     // LREG3 = 0.0 (conditional: only zero lanes)

// Step 8: Reset CC for store
TTI_SFPENCC(0, 0)                                   // CC.Res = 1 for all lanes

// Step 9: Store result
TTI_SFPSTORE(LREG3, 0, ADDR_MOD_7, 0, 0)          // store LREG3 to Dest
```

**Instruction count**: 9 instructions per row-pair iteration (all Simple EXU, 1 cycle each).

**Why this ordering works**:
1. LREG0 holds the original input throughout — never overwritten
2. LREG3 starts as +1.0 (the default for positive values)
3. Negative lanes get -1.0 written to LREG3
4. After CC reset, all lanes are active again
5. Zero lanes get 0.0 written to LREG3 (this correctly overwrites both the +1.0 default and any -1.0 from step 4, since zero is neither positive nor negative)
6. After final CC reset, store writes all lanes

### Outer Function: `_calculate_sign_(const int iterations)`

```
// Pre-loop: Load constants
TTI_SFPLOADI(LREG1, 0, 0x3F80)                     // +1.0 (BF16)
TTI_SFPLOADI(LREG2, 0, 0xBF80)                     // -1.0 (BF16)
TTI_SFPENCC(1, 2)                                   // enable CC

// Loop
#pragma GCC unroll 8
for (int d = 0; d < iterations; d++)
{
    _calculate_sign_sfp_rows_();
    _incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>();
}

// Post-loop
TTI_SFPENCC(0, 2)                                   // disable CC
```

## Init/Uninit Symmetry

This kernel does not have a separate `_init_sign_()` or `_uninit_sign_()` function. All setup (constant loading, CC enable) is done inside `_calculate_sign_()` before the loop, and teardown (CC disable) is done after the loop. This follows the same pattern as `_calculate_elu_()`, `_calculate_threshold_()`, `_calculate_lrelu_()`, and `_calculate_abs_()`.

| Init Action (in `_calculate_sign_`) | Uninit Reversal (in `_calculate_sign_`) |
|--------------------------------------|----------------------------------------|
| `TTI_SFPENCC(1, 2)` — enable CC | `TTI_SFPENCC(0, 2)` — disable CC |
| `TTI_SFPLOADI` for constants | Not needed — LREG values are transient |

## BF16 Constant Encodings

| Value | BF16 Encoding | Derivation |
|-------|---------------|------------|
| +1.0 | `0x3F80` | IEEE 754 FP32: `0x3F800000`. BF16 = upper 16 bits = `0x3F80` |
| -1.0 | `0xBF80` | IEEE 754 FP32: `0xBF800000`. BF16 = upper 16 bits = `0xBF80` |
| 0.0 | `0x0000` | IEEE 754 FP32: `0x00000000`. BF16 = upper 16 bits = `0x0000` |

Verified: `0x3F80` matches the 1.0 constant in `ckernel_sfpu_activations.h` line 64 (`_init_hardsigmoid_` loads `0x3F80` for upper clamp = 1.0).

## Potential Issues

1. **SFPMOV InstrMod=2 (unconditional copy)**: The arch research documents SFPMOV InstrMod=2 as "unconditional copy (ignores LaneEnabled)". This is needed for Step 2 to set the default +1.0 for ALL lanes regardless of CC state. If InstrMod=2 behaves differently than expected, an alternative is to use `TTI_SFPLOADI(p_sfpu::LREG3, 0, 0x3F80)` instead (conditional SFPLOADI), but this would only work because CC is freshly enabled with all lanes active at that point. Both approaches should work, but SFPMOV with InstrMod=2 is more semantically correct.

   **Fallback**: If SFPMOV InstrMod=2 causes issues, replace with `TTI_SFPLOADI(p_sfpu::LREG3, 0, 0x3F80)` which loads +1.0 directly into LREG3. Since CC.Res=1 for all lanes after `TTI_SFPENCC(1, 2)`, this conditional load would hit all lanes.

2. **SFPSETCC mode 6 zero check semantics**: The arch research notes SFPSETCC mode 6 uses SMAG32 comparison (true zero check, subnormals NOT treated as zero). The SFPU load flushes subnormals depending on format conversion. For typical float inputs, this should work correctly. If subnormal edge cases cause issues, the zero check is still correct for all normal float values.

3. **No NaN/Inf handling**: The reference Blackhole implementation does not handle NaN or Inf specially (sign(NaN) and sign(Inf) follow the same logic as normal values). The Quasar implementation follows the same behavior: sign(+Inf) = +1.0, sign(-Inf) = -1.0, sign(NaN) depends on the NaN's sign bit.

## Recommended Test Formats

Based on the analysis's "Format Support" section (float-only domain) and the architecture research's "Format Support Matrix".

### Format List

```python
SFPU_SIGN_FORMATS = input_output_formats(
    [
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Float32,
    ]
)
```

**Rationale**: Sign is a float-only operation. The format list matches the established pattern for similar Quasar SFPU kernels (abs, threshold, elu all use Float16/Float16_b/Float32). MX formats (MxFp8R, MxFp8P) are excluded for consistency with existing simple SFPU tests, even though they would technically work since they unpack to Float16_b. Integer formats are excluded because sign is a float-domain operation.

### Invalid Combination Rules

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

These rules match exactly the filtering in `test_sfpu_abs_quasar.py`, which is the closest comparable test.

### MX Format Handling
Not applicable — MX formats are excluded from the format list.

### Integer Format Handling
Not applicable — integer formats are excluded (float-only operation).

## Testing Notes

1. **Golden reference**: `torch.sign(x)` matches the expected behavior: returns +1.0 for positive, -1.0 for negative, 0.0 for zero.
2. **Input preparation**: Should include positive values, negative values, and exact zeros to exercise all three code paths. Use a distribution similar to abs test (log-uniform for magnitude, random signs), plus explicit zeros.
3. **Zero sensitivity**: The zero check path is the most sensitive — ensure test inputs include actual IEEE 754 zeros (both +0.0 and -0.0). Note: sign(-0.0) behavior depends on whether the hardware treats -0.0 as negative or zero. The SFPSETCC mode 0 (negative check) tests the sign bit, so -0.0 would be classified as negative. The SFPSETCC mode 6 (zero check) then overrides this to 0.0 if it treats -0.0 as zero. The expected behavior matches `torch.sign(-0.0) = 0.0`.
4. **MathOperation enum**: The test will need a `MathOperation.Sign` value if one does not already exist, or use a generic SFPU operation type.
5. **Test structure**: Follow `test_sfpu_abs_quasar.py` as the closest template — same format list, same combination generator pattern, same test flow.
