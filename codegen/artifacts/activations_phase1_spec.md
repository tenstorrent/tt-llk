# Specification: activations — Phase 1 (CELU)

## Kernel Type
SFPU

## Target Architecture
Quasar

## Overview
Based on analysis: `codegen/artifacts/activations_analysis.md`

Phase 1 implements the CELU (Continuously Differentiable Exponential Linear Unit) activation function. CELU is defined as:
- For x >= 0: output = x (identity)
- For x < 0: output = alpha * (exp(x / alpha) - 1)

This is structurally almost identical to the existing ELU kernel on Quasar (`ckernel_sfpu_elu.h`), with one key difference: CELU divides by alpha before the exp, requiring an additional multiply by `alpha_recip` (1/alpha) and an additional parameter.

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_activations.h`

## Reference Implementations Studied

- **`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_elu.h`**: CLOSEST to CELU. Provides the exact pattern for conditional negative-branch processing using SFPSETCC/SFPENCC, hardware exp via SFPNONLINEAR(EXP_MODE), exp-minus-one via SFPADD with LCONST_neg1, and alpha multiply via SFPMUL. Register allocation pattern (LREG0=data, LREG1=work, LREG2=alpha) directly reusable. The main loop pattern with `iterations` parameter and `_incr_counters_` is identical.
- **`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_lrelu.h`**: Confirms the CC enable/disable bracketing pattern (`TTI_SFPENCC(1,2)` before loop, `TTI_SFPENCC(0,2)` after loop) and single-parameter SFPLOADI loading pattern.
- **`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_sigmoid.h`**: Confirms TTI instruction chaining with no explicit NOPs needed between dependent instructions (hardware handles hazards).

## Algorithm in Target Architecture

### Pseudocode

```
_calculate_celu_(iterations, alpha, alpha_recip):
  1. Load alpha into LREG2:       SFPLOADI(LREG2, FP16_B, alpha >> 16)
  2. Load alpha_recip into LREG3:  SFPLOADI(LREG3, FP16_B, alpha_recip >> 16)
  3. Enable CC:                    SFPENCC(1, 2)
  4. For each iteration (d = 0..iterations-1):
     a. Call _calculate_celu_sfp_rows_()
     b. Increment dest counters by SFP_ROWS
  5. Disable CC:                   SFPENCC(0, 2)

_calculate_celu_sfp_rows_():
  1. Load dest[row] into LREG0:  SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)
  2. Set CC for negative lanes:  SFPSETCC(0, LREG0, 0)
     --- Following instructions execute only for lanes where x < 0 ---
  3. Multiply x by alpha_recip:  SFPMUL(LREG0, LREG3, LCONST_0, LREG1, 0)
     // LREG1 = x * alpha_recip = x / alpha
  4. Compute exp(x/alpha):       SFPNONLINEAR(LREG1, LREG1, EXP_MODE)
     // LREG1 = exp(x / alpha)
  5. Subtract 1:                 SFPADD(LCONST_1, LREG1, LCONST_neg1, LREG1, 0)
     // LREG1 = 1.0 * exp(x/alpha) + (-1.0) = exp(x/alpha) - 1
  6. Multiply by alpha:          SFPMUL(LREG2, LREG1, LCONST_0, LREG0, 0)
     // LREG0 = alpha * (exp(x/alpha) - 1)
     --- End conditional region ---
  7. Clear CC (all lanes active): SFPENCC(0, 0)
  8. Store LREG0 to dest:        SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0)
```

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|--------------------|-------------------|-----------------|
| `sfpi::dst_reg[0]` (load) | `TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` | `ckernel_sfpu_elu.h` line 20 |
| `sfpi::dst_reg[0] = v` (store) | `TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0)` | `ckernel_sfpu_elu.h` line 32 |
| `sfpi::dst_reg++` | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` | `ckernel_sfpu_elu.h` line 44 |
| `v_if (v < 0.0f)` | `TTI_SFPSETCC(0, p_sfpu::LREG0, 0)` | `ckernel_sfpu_elu.h` line 22 |
| `v_endif` (reset all lanes) | `TTI_SFPENCC(0, 0)` | `ckernel_sfpu_elu.h` line 30 |
| `Converter::as_float(param0)` (alpha) | `TTI_SFPLOADI(p_sfpu::LREG2, 0 /*FP16_B*/, (alpha >> 16))` | `ckernel_sfpu_elu.h` line 38 |
| `Converter::as_float(param1)` (alpha_recip) | `TTI_SFPLOADI(p_sfpu::LREG3, 0 /*FP16_B*/, (alpha_recip >> 16))` | Same pattern as above, uses LREG3 |
| `v * alpha_recip` | `TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG3, p_sfpu::LCONST_0, p_sfpu::LREG1, 0)` | SFPMUL pattern from `ckernel_sfpu_elu.h` line 28 |
| `_calculate_exponential_body_(x)` | `TTI_SFPNONLINEAR(p_sfpu::LREG1, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE)` | `ckernel_sfpu_elu.h` line 24 |
| `exp_val - 1.0f` | `TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG1, p_sfpu::LCONST_neg1, p_sfpu::LREG1, 0)` | `ckernel_sfpu_elu.h` line 26 |
| `alpha * (exp_val - 1)` | `TTI_SFPMUL(p_sfpu::LREG2, p_sfpu::LREG1, p_sfpu::LCONST_0, p_sfpu::LREG0, 0)` | `ckernel_sfpu_elu.h` line 28 |
| CC enable bracket | `TTI_SFPENCC(1, 2)` | `ckernel_sfpu_elu.h` line 39 |
| CC disable bracket | `TTI_SFPENCC(0, 2)` | `ckernel_sfpu_elu.h` line 46 |

### Resource Allocation

| Resource | Purpose |
|----------|---------|
| LREG0 | Input data from Dest; also holds final output |
| LREG1 | Working register for intermediate values (x/alpha, exp result, exp-1) |
| LREG2 | Alpha parameter (loaded once before loop, persists across iterations) |
| LREG3 | Alpha_recip parameter (1/alpha, loaded once before loop) |
| LCONST_0 (index 9) | Fixed constant 0.0 — used as addend in SFPMUL (multiply-only = a*b+0) |
| LCONST_1 (index 10) | Fixed constant 1.0 — used as multiplier in SFPADD (add-only = 1*b+c) |
| LCONST_neg1 (index 11) | Programmable constant -1.0 — used as addend in SFPADD for exp(x)-1 |
| CC.Res | Condition code result — set for negative lanes, used to mask positive lanes |

**Difference from ELU**: ELU uses LREG0/LREG1/LREG2 (3 registers). CELU needs one additional register (LREG3) for alpha_recip. This is safe — 8 LREGs are available, and LREG4-7 remain unused.

## Implementation Structure

### Includes
```cpp
#include <cstdint>
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```
Source: Identical to `ckernel_sfpu_elu.h` (lines 6-9). No additional includes needed.

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
Source: Standard Quasar SFPU pattern used in all existing kernels.

### Functions (Phase 1 only)

| Function | Template Params | Purpose |
|----------|-----------------|---------|
| `_calculate_celu_sfp_rows_()` | None | Per-row CELU computation for SFP_ROWS (2 rows) |
| `_calculate_celu_(const int iterations, const std::uint32_t alpha, const std::uint32_t alpha_recip)` | None | Main entry point: load params, loop, cleanup |

**No template params**: Quasar SFPU kernels do not use APPROXIMATION_MODE templates (hardware instructions have fixed precision). The `iterations` parameter is runtime, not a template parameter. This matches all existing Quasar SFPU kernels (ELU, LReLU, sigmoid, etc.).

## Instruction Sequence

### _calculate_celu_sfp_rows_()

```cpp
inline void _calculate_celu_sfp_rows_()
{
    // 1. Load from Dest into LREG0
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

    // 2. Set CC.Res = 1 for lanes where LREG0 is negative (x < 0)
    TTI_SFPSETCC(0, p_sfpu::LREG0, 0);

    // 3. Multiply x by alpha_recip: LREG1 = x / alpha
    //    (only executes for negative lanes due to CC)
    TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG3, p_sfpu::LCONST_0, p_sfpu::LREG1, 0);

    // 4. exp(x / alpha) -> LREG1
    TTI_SFPNONLINEAR(p_sfpu::LREG1, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE);

    // 5. exp(x/alpha) - 1 -> LREG1
    TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG1, p_sfpu::LCONST_neg1, p_sfpu::LREG1, 0);

    // 6. alpha * (exp(x/alpha) - 1) -> LREG0
    TTI_SFPMUL(p_sfpu::LREG2, p_sfpu::LREG1, p_sfpu::LCONST_0, p_sfpu::LREG0, 0);

    // 7. Clear CC — all lanes active again
    TTI_SFPENCC(0, 0);

    // 8. Store LREG0 back to Dest
    TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0);
}
```

**Key correctness note**: For x >= 0 lanes, CC.Res is 0 (not set), so instructions in steps 3-6 do not modify LREG0 for those lanes. The original x value is preserved in LREG0 and written back, implementing the identity branch.

### _calculate_celu_()

```cpp
inline void _calculate_celu_(const int iterations, const std::uint32_t alpha, const std::uint32_t alpha_recip)
{
    // Load parameters into LREGs (before enabling CC so all lanes load)
    TTI_SFPLOADI(p_sfpu::LREG2, 0 /*FP16_B*/, (alpha >> 16));
    TTI_SFPLOADI(p_sfpu::LREG3, 0 /*FP16_B*/, (alpha_recip >> 16));

    // Enable condition code system
    TTI_SFPENCC(1, 2);

#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_celu_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }

    // Disable condition code system
    TTI_SFPENCC(0, 2);
}
```

## Init/Uninit Symmetry

CELU does **not** have a separate init or uninit function. The CC enable/disable is handled within `_calculate_celu_` itself (SFPENCC(1,2) at start, SFPENCC(0,2) at end). Parameters are loaded at the start of the main function. This matches the ELU pattern exactly.

| Init Action | Uninit Reversal |
|-------------|-----------------|
| `TTI_SFPENCC(1, 2)` — enable CC | `TTI_SFPENCC(0, 2)` — disable CC |
| Load alpha into LREG2 | No reversal needed (transient register state) |
| Load alpha_recip into LREG3 | No reversal needed (transient register state) |

## Potential Issues

1. **SFPMUL 2-cycle latency with SFPNONLINEAR 1-cycle**: Step 3 (SFPMUL) has 2-cycle latency. Step 4 (SFPNONLINEAR) reads LREG1 which was written by step 3. Per the architecture research, there are "no LREG-to-LREG data hazards" — hardware guarantees correctness without explicit bubbles. Confirmed by existing kernels that chain SFPMUL output into subsequent instructions without NOPs.

2. **alpha_recip precision**: The alpha_recip parameter is passed as FP16_B encoded in a uint32. The upper 16 bits are extracted via `>> 16`. This gives BFloat16 precision (~3 decimal digits). For common alpha values (e.g., 1.0 -> alpha_recip=1.0), this is exact. For unusual values, there may be small precision loss.

3. **Exp overflow for large negative x/alpha**: If x is very negative and alpha is small, x/alpha can be a large negative number. The hardware exp approximation flushes subnormal inputs to signed zero, so exp(very_negative) -> 0.0, which gives CELU -> alpha*(0-1) = -alpha. This is mathematically correct behavior.

4. **Phase 2 compatibility**: Phase 2 (Hardsigmoid) will add `_init_hardsigmoid_`, `_calculate_hardsigmoid_sfp_rows_`, and `_calculate_hardsigmoid_` below the CELU functions. The functions are completely independent — no shared state or resources.

## Calling Convention

The test harness will call CELU via:
```cpp
_llk_math_eltwise_unary_sfpu_params_<false>(
    ckernel::sfpu::_calculate_celu_,
    tile_idx,
    num_sfpu_iterations,
    alpha_param,
    alpha_recip_param
);
```

Where:
- `alpha_param` = IEEE 754 float32 representation of alpha (e.g., `0x3F800000` for 1.0)
- `alpha_recip_param` = IEEE 754 float32 representation of 1/alpha (e.g., `0x3F800000` for 1.0)

The `_llk_math_eltwise_unary_sfpu_params_` function (in `llk_math_eltwise_unary_sfpu_common.h`) uses variadic templates to forward these parameters to the sfpu function. This is the same mechanism used by ELU (which passes a single `slope` parameter).

## Recommended Test Formats

### Format List

CELU is a **float-only** operation (uses exp(), which is undefined for integers). The recommended test formats are:

```python
SFPU_CELU_FORMATS = input_output_formats([
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Float32,
])
```

MX formats (MxFp8R, MxFp8P) are excluded from initial testing for simplicity. They can be added later once the core floating-point formats pass. MX formats are L1 storage only and unpack to Float16_b, so the SFPU math is identical -- only the unpack/pack path differs.

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

    # Float32 input -> Float16 output requires dest_acc=Yes on Quasar
    if (
        in_fmt == DataFormat.Float32
        and out_fmt == DataFormat.Float16
        and dest_acc == DestAccumulation.No
    ):
        return True

    return False
```

These rules are identical to the ELU test (`test_sfpu_elu_quasar.py`). No CELU-specific format constraints exist beyond the standard Quasar infrastructure rules.

### MX Format Handling
MX formats excluded from initial format list. If added later:
- MX + `implied_math_format=No` combinations must be skipped
- MX formats exist only in L1, unpacked to Float16_b for math
- SFPU sees Float16_b data — math path identical

### Integer Format Handling
Not applicable. CELU is float-only (uses exp). Integer formats are excluded entirely.

## Testing Notes

1. **Test inputs should span both positive and negative ranges**: The positive branch (identity) and negative branch (alpha*(exp(x/alpha)-1)) must both be exercised. Use the same input preparation strategy as ELU (`prepare_elu_inputs` style), but note that for CELU the negative branch computes exp(x/alpha) rather than exp(x), so input scaling should account for the alpha parameter.

2. **Alpha parameter encoding**: Alpha and alpha_recip are passed as IEEE 754 float32 representations. The C++ test should define these as `constexpr std::uint32_t`. For alpha=1.0, CELU is mathematically identical to ELU with alpha=1.0. Testing with alpha=1.0 first allows direct comparison with ELU golden results.

3. **Golden generator**: The golden reference should use `torch.nn.functional.celu(input, alpha=alpha_value)` from PyTorch.

4. **SfpuType enum**: A `Celu` entry must be added to the `SfpuType` enum in `tt_llk_quasar/llk_lib/llk_defs.h` for test infrastructure. The test-writer agent handles this.

5. **Parent file include**: `tt_llk_quasar/common/inc/ckernel_sfpu.h` must add `#include "sfpu/ckernel_sfpu_activations.h"`. The test-writer or writer agent handles this.
