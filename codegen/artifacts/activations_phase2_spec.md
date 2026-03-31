# Specification: activations -- Phase 2 (Hardsigmoid)

## Kernel Type
SFPU

## Target Architecture
Quasar

## Overview
Based on analysis: `codegen/artifacts/activations_analysis.md`

Phase 2 implements the Hardsigmoid activation function. Hardsigmoid is defined as:
  `hardsigmoid(x) = clamp(x * slope + offset, 0, 1)` where slope = 1/6, offset = 0.5

This is a multiply-add followed by a two-sided clamp to [0, 1]. The clamp pattern is directly available from the existing `_calculate_relu_max_sfp_rows_` function in `ckernel_sfpu_lrelu.h`, which implements `clamp(x, 0, threshold)` using `SFPGT` + conditional `SFPMOV` for the upper bound, and `SFPSETCC` + conditional `SFPLOADI(0)` for the lower bound (ReLU).

Phase 1 (CELU) functions are already written and tested in the target file. This phase appends three new functions below the existing CELU code.

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_activations.h`

## Reference Implementations Studied

- **`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_lrelu.h` (`_calculate_relu_max_sfp_rows_`)**: CLOSEST to Hardsigmoid clamping. Implements `clamp(x, 0, threshold)` using exactly the pattern needed:
  1. `SFPGT(0, LREG2, LREG0, 0x1)` -- sets CC for lanes where LREG0 > LREG2 (value > threshold)
  2. `SFPMOV(LREG2, LREG0, 0)` -- conditionally copies threshold into LREG0 where value exceeded threshold
  3. `SFPSETCC(0, LREG0, 0)` -- sets CC for lanes where LREG0 is negative
  4. `SFPLOADI(LREG0, 0, 0)` -- conditionally loads 0 into negative lanes
  5. `SFPENCC(0, 0)` -- clears CC

  This is the exact `clamp(x, 0, threshold)` operation where threshold is in LREG2. For hardsigmoid, threshold = 1.0 and the input is `x * slope + offset`.

- **`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_lrelu.h` (`_calculate_lrelu_sfp_rows_`)**: Confirms the `SFPMAD` pattern for multiply-add under CC: `SFPMAD(LREG0, LREG2, LCONST_0, LREG0, 0)` computes `LREG0 * LREG2 + 0.0`. For hardsigmoid we need `x * slope + offset`, which is `SFPMAD(LREG0, LREGslope, LREGoffset, LREG0, 0)`.

- **`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_gelu.h` (`_init_gelu_`)**: Confirms the init pattern for loading constants via `TTI_SFPLOADI` into persistent LREGs (LREG4-7) before the computation loop. Constants loaded in init persist across iterations.

- **`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_log.h` (`_init_log_`)**: Also confirms the `TTI_SFPLOADI(lreg, 0 /*FP16_B*/, imm16)` pattern for loading BFloat16 constants into LREGs during init.

## Algorithm in Target Architecture

### Pseudocode

```
_init_hardsigmoid_():
  1. Load slope (1/6) into LREG4:   SFPLOADI(LREG4, FP16_B, 0x3E2A)
  2. Load offset (0.5) into LREG5:  SFPLOADI(LREG5, FP16_B, 0x3F00)
  3. Load one (1.0) into LREG6:     SFPLOADI(LREG6, FP16_B, 0x3F80)

_calculate_hardsigmoid_(iterations):
  1. Enable CC:                     SFPENCC(1, 2)
  2. For each iteration (d = 0..iterations-1):
     a. Call _calculate_hardsigmoid_sfp_rows_()
     b. Increment dest counters by SFP_ROWS
  3. Disable CC:                    SFPENCC(0, 2)

_calculate_hardsigmoid_sfp_rows_():
  1. Load dest[row] into LREG0:    SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)
  2. Compute x * slope + offset:   SFPMAD(LREG0, LREG4, LREG5, LREG0, 0)
     // LREG0 = x * (1/6) + 0.5  (MAD: VA*VB + VC -> VD)
  3. Insert NOP for MAD latency:   SFPNOP
     // SFPMAD has 2-cycle latency; SFPGT reads LREG0 as a register input.
     // The NOP ensures the MAD result in LREG0 is committed before SFPGT reads it.
  4. Clamp upper bound to 1.0:
     a. SFPGT(0, LREG6, LREG0, 0x1) -- set CC for lanes where LREG0 > 1.0
        // SFPGT encoding: sets CC.Res where lreg_c > lreg_dest (b > c in description)
        // With (LREG6, LREG0): CC.Res=1 where LREG0 > LREG6, i.e., value > 1.0
     b. SFPMOV(LREG6, LREG0, 0)    -- copy 1.0 into LREG0 for lanes exceeding 1.0
  5. Clamp lower bound to 0.0:
     a. SFPSETCC(0, LREG0, 0)      -- set CC for lanes where LREG0 is negative
     b. SFPLOADI(LREG0, 0, 0)      -- load 0.0 into negative lanes
  6. Clear CC (all lanes active):   SFPENCC(0, 0)
  7. Store LREG0 to dest:          SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0)
```

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|--------------------|-------------------|-----------------|
| `sfpi::vConstFloatPrgm0 = 0.166...` (slope) | `TTI_SFPLOADI(p_sfpu::LREG4, 0, 0x3E2A)` | `ckernel_sfpu_log.h` line 21 (SFPLOADI with FP16_B mode 0) |
| `sfpi::vConstFloatPrgm1 = 0.5` (offset) | `TTI_SFPLOADI(p_sfpu::LREG5, 0, 0x3F00)` | `ckernel_sfpu_gelu.h` line 20 (0x3F00 = BF16 for 0.5) |
| `v * slope + offset` | `TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, 0)` | `ckernel_sfpu_lrelu.h` line 22 (SFPMAD pattern for multiply-add) |
| `_relu_max_body_(tmp, 1.0f)` upper clamp | `TTI_SFPGT(0, p_sfpu::LREG6, p_sfpu::LREG0, 0x1)` + `TTI_SFPMOV(p_sfpu::LREG6, p_sfpu::LREG0, 0)` | `ckernel_sfpu_lrelu.h` lines 80-82 (_calculate_relu_max_sfp_rows_) |
| `_relu_max_body_(tmp, 1.0f)` lower clamp (ReLU) | `TTI_SFPSETCC(0, p_sfpu::LREG0, 0)` + `TTI_SFPLOADI(p_sfpu::LREG0, 0, 0)` | `ckernel_sfpu_lrelu.h` lines 85-87 (_calculate_relu_max_sfp_rows_) |
| `sfpi::dst_reg[0]` (load) | `TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` | `ckernel_sfpu_lrelu.h` line 77 |
| `sfpi::dst_reg[0] = v` (store) | `TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0)` | `ckernel_sfpu_lrelu.h` line 92 |
| `sfpi::dst_reg++` | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` | All existing Quasar SFPU kernels |
| CC enable bracket | `TTI_SFPENCC(1, 2)` | `ckernel_sfpu_lrelu.h` line 99 |
| CC disable bracket | `TTI_SFPENCC(0, 2)` | `ckernel_sfpu_lrelu.h` line 106 |

### Resource Allocation

| Resource | Purpose |
|----------|---------|
| LREG0 | Input data from Dest; also holds intermediate (x*slope+offset) and final output |
| LREG4 | Slope constant (1/6 = 0x3E2A in BF16), loaded in init, persists across iterations |
| LREG5 | Offset constant (0.5 = 0x3F00 in BF16), loaded in init, persists across iterations |
| LREG6 | Upper clamp constant (1.0 = 0x3F80 in BF16), loaded in init, persists across iterations |
| LCONST_0 (index 9) | NOT used by hardsigmoid (available for other phases) |
| LCONST_1 (index 10) | NOT directly used (we use LREG6 for 1.0 to avoid conflict with CELU's usage) |
| CC.Res | Condition code result -- used for both upper clamp (SFPGT) and lower clamp (SFPSETCC) |

**Register conflict avoidance**: CELU (Phase 1) uses LREG0-3. Hardsigmoid uses LREG0 (data) and LREG4-6 (constants). There is no conflict because the functions are never called simultaneously -- CELU and Hardsigmoid are independent activation types called separately. However, we use LREG4-6 rather than LREG2-3 to cleanly separate resource allocation from CELU, making the code easier to reason about.

**Why LREG6 for 1.0 instead of LCONST_1**: LCONST_1 (index 10) is a fixed constant = 1.0, but it cannot be used as a direct register argument to SFPGT. The SFPGT instruction takes lreg_c and lreg_dest as register indices (0-7 for LREGs). Constants at indices 8-14 cannot be placed in the lreg_c position for SFPGT. Therefore, we must load 1.0 into a regular LREG.

## Implementation Structure

### Includes
No new includes needed. The file already has (from Phase 1):
```cpp
#include <cstdint>
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```

### Namespace
Same as Phase 1:
```cpp
namespace ckernel
{
namespace sfpu
{
// Phase 1 (CELU) functions... already written and tested
// Phase 2 (Hardsigmoid) functions... to be appended below
} // namespace sfpu
} // namespace ckernel
```

### Functions (Phase 2 only -- append after Phase 1)

| Function | Template Params | Purpose |
|----------|-----------------|---------|
| `_init_hardsigmoid_()` | None | Load slope, offset, and upper clamp constants into LREG4-6 |
| `_calculate_hardsigmoid_sfp_rows_()` | None | Per-row hardsigmoid computation for SFP_ROWS (2 rows) |
| `_calculate_hardsigmoid_(const int iterations)` | None | Main entry point: enable CC, loop, disable CC |

**No template params**: Consistent with all Quasar SFPU kernels. The Blackhole `_init_hardsigmoid_<APPROXIMATION_MODE>()` template parameter is dropped because Quasar hardware instructions have fixed precision.

## Instruction Sequence

### _init_hardsigmoid_()

```cpp
// Load hardsigmoid constants into persistent LREGs
// slope = 1/6 = 0.166015625 (BF16: 0x3E2A)
// offset = 0.5 (BF16: 0x3F00)
// upper_clamp = 1.0 (BF16: 0x3F80)
inline void _init_hardsigmoid_()
{
    TTI_SFPLOADI(p_sfpu::LREG4, 0 /*FP16_B*/, 0x3E2A); // slope = 1/6
    TTI_SFPLOADI(p_sfpu::LREG5, 0 /*FP16_B*/, 0x3F00); // offset = 0.5
    TTI_SFPLOADI(p_sfpu::LREG6, 0 /*FP16_B*/, 0x3F80); // upper clamp = 1.0
}
```

**Precision note**: BF16 representation of 1/6 is 0x3E2A = 0.166015625 (vs exact 0.16666...). This is ~0.4% relative error. The Blackhole implementation also stores 1/6 as a programmable constant (which goes through FP16_B), so precision is comparable.

### _calculate_hardsigmoid_sfp_rows_()

```cpp
// Calculates Hardsigmoid for number of rows of output SFPU ops (Quasar = 2 rows)
// hardsigmoid(x) = clamp(x * (1/6) + 0.5, 0, 1)
inline void _calculate_hardsigmoid_sfp_rows_()
{
    // 1. Load from Dest into LREG0
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

    // 2. Compute x * slope + offset: LREG0 = LREG0 * LREG4 + LREG5
    //    SFPMAD: RG[VD] = (RG[VA] * RG[VB]) + RG[VC]
    TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, 0);

    // 3. NOP to let SFPMAD result (2-cycle latency) settle before SFPGT reads LREG0
    TTI_SFPNOP(0, 0, 0);

    // 4. Clamp upper bound: if LREG0 > 1.0, set LREG0 = 1.0
    //    SFPGT(0, lreg_c=LREG6, lreg_dest=LREG0, 0x1): sets CC where LREG0 > LREG6
    TTI_SFPGT(0, p_sfpu::LREG6, p_sfpu::LREG0, 0x1);
    TTI_SFPMOV(p_sfpu::LREG6 /*src*/, p_sfpu::LREG0 /*dest*/, 0); // copy 1.0 where value > 1.0

    // 5. Clamp lower bound: if LREG0 < 0, set LREG0 = 0 (ReLU)
    TTI_SFPSETCC(0, p_sfpu::LREG0, 0); // CC.Res = 1 for negative lanes
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0); // load 0.0 into negative lanes

    // 6. Clear CC -- all lanes active again
    TTI_SFPENCC(0, 0);

    // 7. Store LREG0 back to Dest
    TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0);
}
```

**Key correctness notes**:
1. The SFPMAD computes `LREG0 * LREG4 + LREG5 -> LREG0`, i.e., `x * slope + offset`. After this, LREG0 holds the pre-clamp result.
2. The NOP after SFPMAD is needed because SFPMAD has 2-cycle latency and SFPGT needs to read the result from LREG0. The pattern from `_calculate_celu_sfp_rows_` (Phase 1) demonstrates this same concern -- there a NOP was inserted between SFPMUL and SFPNONLINEAR for the same reason. However, the architecture research states "No LREG-to-LREG data hazards: Hardware guarantees no explicit bubbles needed for RAW hazards between LREG operations." We include the NOP for safety, matching the Phase 1 pattern. If it proves unnecessary, it can be removed without functional impact.
3. The upper clamp uses SFPGT to compare LREG0 against LREG6 (1.0). The SFPGT instruction with `(0, lreg_c=LREG6, lreg_dest=LREG0, 0x1)` sets CC.Res=1 for lanes where `LREG0 > LREG6`. Then SFPMOV conditionally copies LREG6 (1.0) into LREG0 for those lanes.
4. The lower clamp uses SFPSETCC to detect negative values and SFPLOADI to zero them out. SFPSETCC overwrites CC.Res, so the upper-clamp CC state is replaced -- this is correct because the two clamps are applied sequentially.
5. The order (upper clamp first, then lower clamp) matters: after the MAD, values range from -infinity to +infinity. Upper clamping to 1.0 ensures no value exceeds 1.0. Then lower clamping to 0.0 ensures no value is below 0.0. The final result is in [0, 1].

### _calculate_hardsigmoid_()

```cpp
// Implements hardsigmoid: clamp(x * (1/6) + 0.5, 0, 1)
// Constants (slope, offset, upper_clamp) must be loaded by _init_hardsigmoid_() before calling
inline void _calculate_hardsigmoid_(const int iterations)
{
    TTI_SFPENCC(1, 2); // enable cc
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_hardsigmoid_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
    TTI_SFPENCC(0, 2); // disable cc
}
```

## Init/Uninit Symmetry

Hardsigmoid uses `_init_hardsigmoid_()` to load constants before the main computation, but does NOT have a corresponding `_uninit_`. This matches the GELU and LOG init patterns on Quasar -- they load LREGs in init and never restore them. LREGs are transient per-SFPU-invocation state; the test harness framework handles SFPU state management.

| Init Action | Uninit Reversal |
|-------------|-----------------|
| Load slope into LREG4 | No reversal needed (transient LREG state) |
| Load offset into LREG5 | No reversal needed (transient LREG state) |
| Load 1.0 into LREG6 | No reversal needed (transient LREG state) |
| `TTI_SFPENCC(1, 2)` in _calculate_ | `TTI_SFPENCC(0, 2)` at end of _calculate_ |

## Potential Issues

1. **SFPMAD 2-cycle latency before SFPGT**: The NOP between SFPMAD and SFPGT ensures the multiply-add result is available. The architecture says no LREG hazards, but a NOP costs nothing (1 cycle) and guarantees correctness. Phase 1 uses a similar NOP pattern.

2. **BF16 precision of slope (1/6)**: The BF16 value 0x3E2A represents 0.166015625, not exactly 1/6 = 0.16666... This introduces ~0.4% relative error in the slope. The Blackhole reference also uses BFloat16 for the programmable constant, so precision is comparable. For values near the clamp boundaries (x near -3 or x near 3), this precision loss could shift the boundary slightly, but this is expected behavior for hardware BF16 math.

3. **SFPGT operand ordering**: The SFPGT instruction description in assembly.yaml is "b > c". Based on the existing `_calculate_relu_max_sfp_rows_` code, `TTI_SFPGT(0, lreg_c, lreg_dest, 0x1)` sets CC.Res where `lreg_dest > lreg_c`. This is confirmed by:
   - Line 80: `TTI_SFPGT(0, LREG2, LREG0, 0x1)` with comment "If x > threshold" -- here LREG0 = x, LREG2 = threshold, so CC is set where LREG0 > LREG2.
   - The description says "b > c" where b = lreg_dest position, c = lreg_c position.

4. **Interaction with CELU CC state**: Hardsigmoid and CELU are never called together. Each enables CC at the start (`SFPENCC(1,2)`) and disables it at the end (`SFPENCC(0,2)`). No stale CC state can leak between them.

5. **LREG persistence**: Constants loaded in `_init_hardsigmoid_()` (LREG4-6) persist across all iterations within `_calculate_hardsigmoid_()`. They are not modified by any instruction in the row function. This is confirmed by the GELU pattern where LREG6 (0.5) is loaded in `_init_gelu_` and used across all iterations.

## Calling Convention

The test harness will call Hardsigmoid via:
```cpp
// Init is called once before the tile loop
ckernel::sfpu::_init_hardsigmoid_();

// For each tile:
_llk_math_eltwise_unary_sfpu_params_<false>(
    ckernel::sfpu::_calculate_hardsigmoid_,
    tile_idx,
    num_sfpu_iterations
);
```

Where:
- `_init_hardsigmoid_()` is called once before the main computation loop to load constants
- `_calculate_hardsigmoid_` takes only `iterations` as a parameter (no uint32_t params)
- The `_llk_math_eltwise_unary_sfpu_params_<false>()` function forwards the iterations parameter

This matches the GELU calling convention: init is called once, then `_calculate_gelu_` is called per tile.

## Recommended Test Formats

### Format List

Hardsigmoid is a **float-only** operation (multiply-add with clamping -- meaningless on integers). The recommended test formats are:

```python
SFPU_HARDSIGMOID_FORMATS = input_output_formats([
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Float32,
])
```

MX formats (MxFp8R, MxFp8P) are excluded from initial testing for simplicity, matching the Phase 1 (CELU) decision. They can be added later -- MX formats unpack to Float16_b, so the SFPU math path is identical.

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

These rules are identical to Phase 1 (CELU) and the standard Quasar SFPU test infrastructure. No hardsigmoid-specific format constraints exist.

### MX Format Handling
MX formats excluded from initial format list. If added later:
- MX + `implied_math_format=No` combinations must be skipped
- MX formats exist only in L1, unpacked to Float16_b for math
- SFPU sees Float16_b data -- math path identical

### Integer Format Handling
Not applicable. Hardsigmoid is float-only. Integer formats are excluded entirely.

## Testing Notes

1. **Test inputs should span wide range**: Hardsigmoid has three distinct regions:
   - x <= -3: output = 0.0 (fully clamped low)
   - -3 < x < 3: output = x/6 + 0.5 (linear region)
   - x >= 3: output = 1.0 (fully clamped high)

   Test inputs should include values in all three regions, including boundary values near -3 and 3.

2. **Golden generator**: Use `torch.nn.functional.hardsigmoid(input)` from PyTorch. Note that PyTorch uses the same formula: `clamp(x/6 + 0.5, 0, 1)`.

3. **Tolerance considerations**: The BF16 approximation of 1/6 (0.166015625 vs 0.16666...) means results will have a small but systematic bias. The test tolerance should account for this -- standard BF16 SFPU tolerance (e.g., 1-2 ULP for BFloat16 results) should suffice.

4. **SfpuType enum**: A `Hardsigmoid` entry must be added to the `SfpuType` enum in `tt_llk_quasar/llk_lib/llk_defs.h` for test infrastructure. The test-writer agent handles this.

5. **Parent file include**: `tt_llk_quasar/common/inc/ckernel_sfpu.h` must include `sfpu/ckernel_sfpu_activations.h`. This should already be in place from Phase 1 -- verify before adding a duplicate include.

6. **Init must be called before compute**: The test harness must call `_init_hardsigmoid_()` once before the tile loop. This is the same pattern used by GELU (which calls `_init_gelu_()` before computing tiles).
