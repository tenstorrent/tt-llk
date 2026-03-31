# Specification: elu (Phase 1 -- ELU core)

## Kernel Type
sfpu

## Target Architecture
quasar

## Overview
Based on analysis: `codegen/artifacts/elu_analysis.md`

Phase 1 implements the complete ELU activation function for Quasar. ELU computes:
- For x >= 0: output = x (identity, value left untouched in Dest)
- For x < 0: output = alpha * (exp(x) - 1)

This is the only phase (single sub-kernel). Two functions are generated:
1. `_calculate_elu_sfp_rows_()` -- inner helper that processes SFP_ROWS (2) rows per call
2. `_calculate_elu_(const int iterations, const std::uint32_t slope)` -- outer loop with parameter setup

The approach combines the conditional-execution pattern from lrelu (for negative-value branching) with the hardware exp approximation from sigmoid/exp (SFPNONLINEAR EXP_MODE).

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_elu.h`

## Reference Implementations Studied

- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_lrelu.h`: Primary pattern source -- same structure (slope parameter loaded via SFPLOADI into LREG2, conditional execution via SFPSETCC/SFPENCC, outer loop with `_incr_counters_`). ELU extends lrelu's negative-branch to compute exp(x)-1 instead of a simple multiply.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_sigmoid.h`: Demonstrates SFPNONLINEAR(EXP_MODE) usage on Quasar -- reading from one LREG and writing exp result to another. Also shows SFPADD pattern for adding constants.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_exp.h`: Confirms SFPNONLINEAR(EXP_MODE) instruction invocation: `TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE)`.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_silu.h`: Shows how to compose multiple operations (sigmoid then multiply), confirming register allocation conventions.

## Algorithm in Target Architecture

### Pseudocode

**Before the loop (in `_calculate_elu_`):**
1. Load alpha parameter into LREG2 as BF16 immediate: `TTI_SFPLOADI(LREG2, 0, slope >> 16)`
2. Enable conditional execution globally: `TTI_SFPENCC(1, 2)`

**Per iteration (in `_calculate_elu_sfp_rows_`):**
1. Load value x from Dest into LREG0: `TTI_SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)`
2. Set CC.Res for lanes where LREG0 < 0: `TTI_SFPSETCC(0, LREG0, 0)`
3. Compute exp(x) into LREG1: `TTI_SFPNONLINEAR(LREG0, LREG1, EXP_MODE)`
4. Compute exp(x) - 1 into LREG1: `TTI_SFPADD(LCONST_1, LREG1, LCONST_neg1, LREG1, 0)` -- computes (1.0 * LREG1) + (-1.0) = exp(x) - 1
5. Compute alpha * (exp(x) - 1) into LREG0: `TTI_SFPMUL(LREG2, LREG1, LCONST_0, LREG0, 0)` -- computes LREG2 * LREG1 + 0 = alpha * (exp(x)-1)
6. Reset CC.Res to 1 for all lanes: `TTI_SFPENCC(0, 0)`
7. Store LREG0 to Dest: `TTI_SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0)`

**After the loop (in `_calculate_elu_`):**
8. Disable conditional execution globally: `TTI_SFPENCC(0, 2)`

**Key design point**: Steps 3-5 only execute in lanes where x < 0 (CC.Res=1). For lanes where x >= 0, LREG0 retains the original value loaded in step 1 (identity). Step 7 stores LREG0 unconditionally, but since CC is still enabled, the SFPSTORE also only executes for negative lanes. After step 6 resets CC.Res=1, SFPSTORE writes all lanes. Wait -- reviewing the lrelu pattern more carefully:

**Corrected understanding from lrelu**: In lrelu, the sequence is:
1. SFPLOAD into LREG0
2. SFPSETCC (set CC for negative lanes)
3. SFPMAD (multiply by slope) -- only executes for negative lanes
4. SFPENCC(0,0) -- reset CC.Res=1 for all lanes
5. SFPSTORE -- executes for ALL lanes (CC.Res=1 everywhere)

So SFPSTORE writes all lanes after CC reset. For positive lanes, LREG0 still contains the original loaded value. For negative lanes, LREG0 was overwritten by the MAD result. This is correct because the identity for positive values is preserved in LREG0.

For ELU, the same pattern applies. The final result in LREG0 is:
- Positive lanes: original x (loaded in step 1, never modified because CC prevented steps 3-5)
- Negative lanes: alpha * (exp(x) - 1) (computed in steps 3-5, written to LREG0 in step 5)

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|--------------------|-------------------|-----------------|
| `Converter::as_float(slope)` | `TTI_SFPLOADI(LREG2, 0, (slope >> 16))` | ckernel_sfpu_lrelu.h line 33 |
| `sfpi::dst_reg[0]` (load) | `TTI_SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)` | ckernel_sfpu_lrelu.h line 18 |
| `v_if (v < 0.0f)` | `TTI_SFPSETCC(0, LREG0, 0)` mode 0 = negative check | ckernel_sfpu_lrelu.h line 20 |
| `_sfpu_exp_21f_bf16_<true>(v)` | `TTI_SFPNONLINEAR(LREG0, LREG1, p_sfpnonlinear::EXP_MODE)` | ckernel_sfpu_exp.h line 22 |
| `exp(x) - vConst1` | `TTI_SFPADD(LCONST_1, LREG1, LCONST_neg1, LREG1, 0)` | ckernel_sfpu_sigmoid.h line 26 (similar add pattern) |
| `s * v_exp` | `TTI_SFPMUL(LREG2, LREG1, LCONST_0, LREG0, 0)` | lrelu line 22 (similar SFPMAD for multiply) |
| `v_endif` / CC reset | `TTI_SFPENCC(0, 0)` | ckernel_sfpu_lrelu.h line 24 |
| `sfpi::dst_reg[0] = result` | `TTI_SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0)` | ckernel_sfpu_lrelu.h line 27 |
| `sfpi::dst_reg++` | `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()` | ckernel_sfpu_lrelu.h line 39 |
| CC enable (before loop) | `TTI_SFPENCC(1, 2)` | ckernel_sfpu_lrelu.h line 34 |
| CC disable (after loop) | `TTI_SFPENCC(0, 2)` | ckernel_sfpu_lrelu.h line 41 |

All instructions verified against assembly.yaml:
- SFPARECIP (opcode 0x99, maps to TTI_SFPNONLINEAR) -- confirmed present
- SFPMAD/SFPADD/SFPMUL (opcodes 0x84/0x85/0x86) -- confirmed present
- SFPSETCC (opcode 0x7B) -- confirmed present
- SFPENCC (opcode 0x8A) -- confirmed present
- SFPLOAD (opcode 0x70), SFPSTORE (opcode 0x72), SFPLOADI (opcode 0x71) -- confirmed present

### Resource Allocation

| Resource | Purpose |
|----------|---------|
| LREG0 | Input value from Dest; also final result for store |
| LREG1 | Intermediate: exp(x), then exp(x)-1 |
| LREG2 | Alpha (slope) parameter, loaded once before loop |
| LCONST_0 (=9) | Zero constant, used as addend in SFPMUL |
| LCONST_1 (=10) | 1.0 constant, used as multiplier in SFPADD |
| LCONST_neg1 (=11) | -1.0 constant, used as addend in SFPADD (subtracts 1) |

## Implementation Structure

### Includes
```cpp
#pragma once

#include <cstdint>

#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```

Note: No `ckernel_ops.h` needed (lrelu doesn't include it either, and all required TTI macros are available via `ckernel_trisc_common.h`). No `sfpi.h` needed (Quasar ELU uses only TTI macros).

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

### Functions

| Function | Template Params | Signature | Purpose |
|----------|-----------------|-----------|---------|
| `_calculate_elu_sfp_rows_` | none | `inline void _calculate_elu_sfp_rows_()` | Processes SFP_ROWS (2) rows: load, conditional exp/sub/mul, store |
| `_calculate_elu_` | none | `inline void _calculate_elu_(const int iterations, const std::uint32_t slope)` | Loads alpha, enables CC, loops over iterations calling sfp_rows + incr_counters, disables CC |

No template parameters. Quasar SFPU kernels use runtime parameters, not templates. This matches the pattern from lrelu, threshold, and other Quasar SFPU kernels.

No init/uninit functions. The alpha parameter is loaded inline at the start of `_calculate_elu_`, and CC enable/disable are handled within the same function. This follows the exact same pattern as lrelu.

## Instruction Sequence

### `_calculate_elu_sfp_rows_()` -- Inner Helper

```
// Load from Dest into LREG0
TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

// Set CC.Res = 1 for lanes where LREG0 is negative (x < 0)
TTI_SFPSETCC(0, p_sfpu::LREG0, 0);

// exp(x) -> LREG1  [only for negative lanes]
TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE);

// exp(x) - 1 -> LREG1: computes (1.0 * LREG1) + (-1.0)  [only for negative lanes]
TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG1, p_sfpu::LCONST_neg1, p_sfpu::LREG1, 0);

// alpha * (exp(x)-1) -> LREG0: computes (LREG2 * LREG1) + 0  [only for negative lanes]
TTI_SFPMUL(p_sfpu::LREG2, p_sfpu::LREG1, p_sfpu::LCONST_0, p_sfpu::LREG0, 0);

// Reset CC.Res = 1 for all lanes
TTI_SFPENCC(0, 0);

// Store LREG0 to Dest (all lanes now active)
TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0);
```

### `_calculate_elu_(const int iterations, const std::uint32_t slope)` -- Outer Loop

```
// Load alpha (slope) into LREG2 as BF16 immediate (upper 16 bits of float32)
TTI_SFPLOADI(p_sfpu::LREG2, 0 /*Float16_b*/, (slope >> 16));

// Enable conditional execution globally
TTI_SFPENCC(1, 2);

#pragma GCC unroll 8
for (int d = 0; d < iterations; d++)
{
    _calculate_elu_sfp_rows_();
    ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
}

// Disable conditional execution globally
TTI_SFPENCC(0, 2);
```

## Init/Uninit Symmetry
Not applicable. ELU has no separate init/uninit functions. The CC enable/disable is symmetric within `_calculate_elu_` itself:
- `TTI_SFPENCC(1, 2)` at the start enables CC
- `TTI_SFPENCC(0, 2)` at the end disables CC

This exactly matches the lrelu pattern.

## Potential Issues

1. **SFPNONLINEAR EXP_MODE accuracy**: The hardware exp approximation may be less accurate than the Blackhole polynomial `_sfpu_exp_21f_bf16_`. For small negative x, exp(x) is close to 1, so exp(x)-1 may lose precision due to catastrophic cancellation. However, this is the same approximation used by all other Quasar SFPU kernels (sigmoid, exp, silu) and is the only practical approach on this architecture.

2. **Alpha parameter precision**: The slope/alpha is loaded as BF16 immediate (upper 16 bits of float32). This truncates the alpha value to BF16 precision, which is a known limitation shared with lrelu. The test should use alpha values that are exactly representable in BF16.

3. **Conditional execution and SFPSTORE**: After SFPENCC(0,0) resets CC.Res=1, SFPSTORE writes all lanes. For positive lanes, LREG0 retains the original value from SFPLOAD (identity). For negative lanes, LREG0 was overwritten by SFPMUL result. This is correct.

4. **Register write conflict**: SFPMUL writes to LREG0, which is the same register that held the input. This is intentional -- we're done with the input value in negative lanes, and we need the result in LREG0 for SFPSTORE. Positive lanes never execute SFPMUL, so their LREG0 is preserved.

## Recommended Test Formats

### Format List
ELU is a float-only operation (requires exp()). Based on the analysis "Format Support" section and the arch research "Format Support Matrix", the applicable formats are Float16, Float16_b, Float32, MxFp8R, and MxFp8P.

However, for initial testing (matching the pattern of existing Quasar SFPU tests like sfpu_nonlinear and sfpu_threshold), use the core float formats:

```python
SFPU_ELU_FORMATS = input_output_formats([
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Float32,
])
```

MxFp8R and MxFp8P are excluded from the initial format list because:
- All existing Quasar SFPU tests (nonlinear, square, threshold, rsqrt) use only Float16/Float16_b/Float32
- MX format testing requires implied_math_format=Yes filtering, adding complexity
- MX can be added later once core functionality is validated

### Invalid Combination Rules

The `_is_invalid_quasar_combination()` function should implement these rules:

```python
def _is_invalid_quasar_combination(
    fmt: FormatConfig, dest_acc: DestAccumulation
) -> bool:
    in_fmt = fmt.input_format
    out_fmt = fmt.output_format

    # Rule 1: Quasar packer does not support non-Float32 to Float32
    # conversion when dest_acc=No (16-bit Dest cannot produce 32-bit output)
    if (
        in_fmt != DataFormat.Float32
        and out_fmt == DataFormat.Float32
        and dest_acc == DestAccumulation.No
    ):
        return True

    # Rule 2: Float32 input -> Float16 output requires dest_acc=Yes
    # (cross-precision conversion needs 32-bit intermediate)
    if (
        in_fmt == DataFormat.Float32
        and out_fmt == DataFormat.Float16
        and dest_acc == DestAccumulation.No
    ):
        return True

    return False
```

These are the same two rules used by the sfpu_nonlinear and sfpu_threshold tests, which are the standard Quasar SFPU format constraints.

### MX Format Handling
Not included in the initial format list. If MX formats (MxFp8R, MxFp8P) are added later:
- Skip combinations where MX format is paired with implied_math_format=No
- MX formats exist only in L1; hardware unpacks to Float16_b before reaching Dest/SFPU
- The golden generator handles MX quantization via existing infrastructure

### Integer Format Handling
Not applicable. ELU is a float-only operation. Integer formats (Int8, UInt8, Int16, Int32) must never be included in the format list.

## Testing Notes

1. **Alpha parameter**: The test should use a known alpha value (e.g., alpha=1.0 encoded as 0x3F800000). The kernel extracts `(slope >> 16)` = 0x3F80 as BF16 immediate.

2. **Input range**: For good test coverage, inputs should include:
   - Positive values (test identity path)
   - Negative values (test exp/sub/mul path)
   - Zero (boundary condition)
   - Values near zero (test precision of exp(x)-1 for small x)

3. **Golden function**: `ELU(x, alpha) = x if x >= 0, alpha * (exp(x) - 1) if x < 0`

4. **Tolerance**: Due to SFPNONLINEAR hardware approximation and BF16 truncation, tolerance should be similar to other SFPU nonlinear tests (sigmoid, exp). The test framework's default tolerance handling should suffice.

5. **Test harness pattern**: ELU requires its own test file (like threshold) since it takes a `slope` parameter. It cannot use the standard sfpu_nonlinear test dispatcher. The C++ test should call `_llk_math_eltwise_unary_sfpu_params_<false>(ckernel::sfpu::_calculate_elu_, i, num_sfpu_iterations, slope_param)`.
