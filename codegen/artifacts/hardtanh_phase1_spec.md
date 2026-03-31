# Specification: hardtanh (Phase 1 -- hardtanh_core)

## Kernel Type
sfpu

## Target Architecture
quasar

## Overview
Based on analysis: `codegen/artifacts/hardtanh_analysis.md`

Hardtanh is a two-sided clamping activation: `clamp(x, min_val, max_val)`. Values above `max_val` are replaced with `max_val`; values below `min_val` are replaced with `min_val`; values in between are passed through unchanged.

The Quasar implementation uses the SFPGT + SFPMOV conditional execution pattern for both upper and lower clamping, matching the established patterns in `ckernel_sfpu_lrelu.h` (relu_max) and `ckernel_sfpu_activations.h` (hardsigmoid). This is a direct comparison approach, replacing the Blackhole reference's arithmetic offset technique.

## Phase Scope
This is Phase 1 (the only phase). Functions to implement:
- `_calculate_hardtanh_sfp_rows_()` -- inner per-row calculation
- `_calculate_hardtanh_(const int iterations, const std::uint32_t min_val, const std::uint32_t max_val)` -- outer loop wrapper

No init/uninit functions are needed. Constants are loaded inside `_calculate_hardtanh_` before the loop (matching threshold, elu, lrelu patterns).

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_hardtanh.h`

## Reference Implementations Studied

### `ckernel_sfpu_threshold.h` (Quasar) -- Primary pattern source
- **SFPGT + SFPMOV clamp pattern**: `TTI_SFPGT(0, LREG0, LREG2, 0x1)` tests `LREG2 > LREG0` (threshold > input), then `TTI_SFPMOV(LREG3, LREG0, 0)` copies replacement value where CC is set. Followed by `TTI_SFPENCC(0, 0)` to reset CC.
- **Two-parameter loading**: `TTI_SFPLOADI(LREG2, 0, (threshold >> 16))` and `TTI_SFPLOADI(LREG3, 0, (value >> 16))` load FP16_B constants from upper 16 bits of FP32-encoded params.
- **Outer loop structure**: CC enable -> `#pragma GCC unroll 8` -> loop with `_incr_counters_` -> CC disable.
- **Function signatures**: `inline void _calculate_threshold_sfp_rows_()` (no params) and `inline void _calculate_threshold_(const int iterations, const std::uint32_t threshold, const std::uint32_t value)`.

### `ckernel_sfpu_lrelu.h` (Quasar) -- relu_max upper clamp pattern
- **Upper clamp**: `TTI_SFPGT(0, LREG2, LREG0, 0x1)` tests `LREG0 > LREG2` (input > threshold), then `TTI_SFPMOV(LREG2, LREG0, 0)` copies threshold where input exceeds it.
- Confirms the argument order: `TTI_SFPGT(imm12, vc, vd, instr_mod)` tests `RG[VD] > RG[VC]`.

### `ckernel_sfpu_activations.h` (Quasar) -- hardsigmoid two-sided clamp
- **Upper clamp then lower clamp**: Uses `SFPGT + SFPMOV` for upper bound, then `SFPSETCC + SFPLOADI` for lower bound (to zero).
- **CC reset between clamps**: `TTI_SFPENCC(0, 0)` between the upper and lower clamp sequences.
- For hardtanh, we need SFPGT for both clamps (since the lower bound is arbitrary, not zero), so we use two SFPGT sequences.

### `ckernel_sfpu_elu.h` (Quasar) -- Single-param outer loop pattern
- Confirms the standard outer function structure with one SFPLOADI, CC enable/disable, and `#pragma GCC unroll 8`.

## Algorithm in Target Architecture

### Pseudocode

**Pre-loop setup** (in `_calculate_hardtanh_`):
1. `SFPLOADI LREG2 <- min_val` (load min threshold as FP16_B from upper 16 bits of FP32 param)
2. `SFPLOADI LREG3 <- max_val` (load max threshold as FP16_B from upper 16 bits of FP32 param)
3. `SFPENCC(1, 2)` -- enable conditional execution (CC.En = 1, CC.Res = 1)

**Per-iteration** (in `_calculate_hardtanh_sfp_rows_`):
1. `SFPLOAD LREG0 <- Dest[addr]` -- load input value x
2. **Upper clamp: if x > max_val, x = max_val**
   - `SFPGT(0, LREG3, LREG0, 0x1)` -- CC.Res = 1 where `LREG0 > LREG3` (x > max_val)
   - `SFPMOV(LREG3, LREG0, 0)` -- copy max_val into LREG0 where CC is set
   - `SFPENCC(0, 0)` -- reset CC.Res = 1 for all lanes (keep CC.En unchanged)
3. **Lower clamp: if x < min_val, x = min_val**
   - `SFPGT(0, LREG0, LREG2, 0x1)` -- CC.Res = 1 where `LREG2 > LREG0` (min_val > x, i.e., x < min_val)
   - `SFPMOV(LREG2, LREG0, 0)` -- copy min_val into LREG0 where CC is set
   - `SFPENCC(0, 0)` -- reset CC.Res = 1 for all lanes
4. `SFPSTORE LREG0 -> Dest[addr]` -- store clamped result

**Post-loop** (in `_calculate_hardtanh_`):
5. `SFPENCC(0, 2)` -- disable conditional execution (CC.En = 0)

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|--------------------|-------------------|-----------------|
| `sfpi::vFloat p0 = sfpi::s2vFloat16(param0)` | `TTI_SFPLOADI(p_sfpu::LREG2, 0, (min_val >> 16))` | `ckernel_sfpu_threshold.h` line 36-37 |
| `sfpi::dst_reg[0]` read | `TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` | `ckernel_sfpu_threshold.h` line 19 |
| `sfpi::dst_reg[0]` write | `TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0)` | `ckernel_sfpu_threshold.h` line 30 |
| `v_if (val > max) val = max` | `TTI_SFPGT(0, LREG3, LREG0, 0x1)` + `TTI_SFPMOV(LREG3, LREG0, 0)` | `ckernel_sfpu_lrelu.h` lines 80-82 (relu_max) |
| `v_if (val < min) val = min` | `TTI_SFPGT(0, LREG0, LREG2, 0x1)` + `TTI_SFPMOV(LREG2, LREG0, 0)` | Analysis of SFPGT: `RG[VD]>RG[VC]`, so swapping vc/vd reverses comparison |
| CC reset between clamps | `TTI_SFPENCC(0, 0)` | `ckernel_sfpu_activations.h` line 86 (hardsigmoid) |
| `sfpi::dst_reg++` | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` | `ckernel_sfpu_threshold.h` line 43 |
| `#pragma GCC unroll 0` | `#pragma GCC unroll 8` | All Quasar SFPU kernels |

All 6 instructions verified present in `tt_llk_quasar/instructions/assembly.yaml`: SFPLOAD (1), SFPLOADI (1), SFPSTORE (1), SFPGT (1), SFPMOV (1), SFPENCC (1).

### Resource Allocation

| Resource | Purpose |
|----------|---------|
| LREG0 | Working register: holds current input value from Dest, receives clamped result |
| LREG2 | Constant: min_val (loaded once before loop, persists across iterations) |
| LREG3 | Constant: max_val (loaded once before loop, persists across iterations) |
| CC.En | Conditional execution enable (set before loop, cleared after loop) |
| CC.Res | Per-lane condition result (set by SFPGT, consumed by SFPMOV, reset by SFPENCC) |

LREG1, LREG4-LREG7 are not used.

## Implementation Structure

### Includes
```cpp
#include <cstdint>
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```
Source: All existing Quasar SFPU kernels use exactly these three includes.

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
Source: All existing Quasar SFPU kernels.

### Functions

| Function | Template Params | Signature | Purpose |
|----------|-----------------|-----------|---------|
| `_calculate_hardtanh_sfp_rows_` | None | `inline void _calculate_hardtanh_sfp_rows_()` | Per-row: load, upper clamp, lower clamp, store |
| `_calculate_hardtanh_` | None | `inline void _calculate_hardtanh_(const int iterations, const std::uint32_t min_val, const std::uint32_t max_val)` | Outer: load constants, enable CC, loop, disable CC |

No template parameters. Quasar SFPU calculate functions do not use templates (confirmed in threshold, elu, lrelu, activations).

### Init/Uninit Symmetry
Not applicable. Hardtanh does not require init/uninit functions. Constants (min_val, max_val) are loaded directly inside `_calculate_hardtanh_` before the iteration loop, matching the threshold pattern. The CC enable/disable is symmetric within `_calculate_hardtanh_` itself:

| Action | Reversal |
|--------|----------|
| `TTI_SFPENCC(1, 2)` (enable CC before loop) | `TTI_SFPENCC(0, 2)` (disable CC after loop) |

## Instruction Sequence

### `_calculate_hardtanh_sfp_rows_()` -- Inner Function

```
// Load input from Dest into LREG0
TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

// Upper clamp: if x > max_val, x = max_val
TTI_SFPGT(0, p_sfpu::LREG3, p_sfpu::LREG0, 0x1);       // CC = (LREG0 > LREG3), i.e., x > max_val
TTI_SFPMOV(p_sfpu::LREG3, p_sfpu::LREG0, 0);            // copy max_val where CC set
TTI_SFPENCC(0, 0);                                        // reset CC for all lanes

// Lower clamp: if x < min_val, x = min_val
TTI_SFPGT(0, p_sfpu::LREG0, p_sfpu::LREG2, 0x1);       // CC = (LREG2 > LREG0), i.e., min_val > x
TTI_SFPMOV(p_sfpu::LREG2, p_sfpu::LREG0, 0);            // copy min_val where CC set
TTI_SFPENCC(0, 0);                                        // reset CC for all lanes

// Store result back to Dest
TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0);
```

**Pipeline analysis**: All instructions are Simple EXU (1-cycle latency) or Load/Store. No MAD instructions are used, so no NOP hazards exist. The sequence is free of pipeline conflicts.

**CC flow analysis**:
1. At entry, CC.En = 1, CC.Res = 1 (from `SFPENCC(1,2)` in outer function). All lanes are enabled.
2. `SFPGT(0, LREG3, LREG0, 0x1)`: Sets CC.Res = 1 for lanes where LREG0 > LREG3 (x > max_val). Other lanes get CC.Res = 0.
3. `SFPMOV(LREG3, LREG0, 0)`: Only executes on lanes where LaneEnabled = CC.En && CC.Res = true. Copies max_val into those lanes.
4. `SFPENCC(0, 0)`: Resets CC.Res = 1 for ALL lanes (executes unconditionally). CC.En stays 1.
5. `SFPGT(0, LREG0, LREG2, 0x1)`: Sets CC.Res = 1 for lanes where LREG2 > LREG0 (min_val > x). Note: after the upper clamp, LREG0 holds `min(x, max_val)`, so this correctly tests `min_val > min(x, max_val)`.
6. `SFPMOV(LREG2, LREG0, 0)`: Copies min_val where CC is set.
7. `SFPENCC(0, 0)`: Resets CC.Res = 1 for all lanes again. Ready for the next iteration.

### `_calculate_hardtanh_(iterations, min_val, max_val)` -- Outer Function

```
// Load constants from FP32-encoded params (upper 16 bits = BF16 value)
TTI_SFPLOADI(p_sfpu::LREG2, 0 /*Float16_b*/, (min_val >> 16));   // min_val into LREG2
TTI_SFPLOADI(p_sfpu::LREG3, 0 /*Float16_b*/, (max_val >> 16));   // max_val into LREG3
TTI_SFPENCC(1, 2);                                                 // enable CC

#pragma GCC unroll 8
for (int d = 0; d < iterations; d++)
{
    _calculate_hardtanh_sfp_rows_();
    ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
}

TTI_SFPENCC(0, 2);   // disable CC
```

## Potential Issues

1. **Parameter encoding**: The test harness passes `min_val` and `max_val` as FP32-encoded `uint32_t` values. The kernel extracts `(param >> 16)` to get the BF16 (Float16_b) representation for `SFPLOADI`. This means the actual threshold precision is limited to BF16 (7-bit mantissa). For values that cannot be exactly represented in BF16 (e.g., 0.3), there will be a small quantization error. This is the standard behavior for all Quasar SFPU kernels that take scalar parameters (threshold, elu, lrelu all use the same pattern).

2. **SFPGT imm12=0 convention**: All existing Quasar SFPU kernels use `imm12=0` for SFPGT, which selects INT32 comparison mode. This works for FP32 values because IEEE 754 bit patterns preserve ordering under integer comparison for values with the same sign. For mixed-sign comparisons (positive vs negative), INT32 mode also works correctly because negative FP32 values have the sign bit set (bit 31), making them larger as unsigned integers but interpreted as negative in 2's complement. The existing convention is correct and tested.

3. **Edge case: min_val > max_val**: If min_val > max_val, the behavior is undefined (same as PyTorch's `torch.clamp` when min > max -- max wins). In our implementation, the upper clamp runs first (clamping to max_val), then the lower clamp runs (clamping to min_val). So if min_val > max_val, the result will be min_val for all inputs. This matches PyTorch's behavior where max parameter takes precedence.

4. **SFPMOV conditional on CC**: The `SFPMOV(vc, vd, 0)` instruction with `instr_mod=0` copies VC to VD **only for lanes where LaneEnabled is true** (CC.En && CC.Res). With `instr_mod=2`, the copy would be unconditional. We use `instr_mod=0` to get conditional behavior. This is confirmed by the SFPMOV ISA documentation and by the identical usage in threshold/relu_max/hardsigmoid.

## Recommended Test Formats

### Format List

Hardtanh is a **float-only** clamping operation. Parameters are loaded as BF16 via SFPLOADI, assuming floating-point encoding. Integer formats would require a different parameter encoding path and are excluded.

```python
SFPU_HARDTANH_FORMATS = input_output_formats([
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Float32,
])
```

Rationale for exclusions:
- **Int8, UInt8, Int16, Int32**: Parameter loading assumes FP encoding (`SFPLOADI` with `instr_mod=0` = Float16_b). Integer clamping would need a different code path. Excluded.
- **MxFp8R, MxFp8P**: Technically would work (unpacked to Float16_b before SFPU), but excluded from initial testing to match the established pattern of similar kernels (threshold, elu). Can be added later.

### Invalid Combination Rules

The `_is_invalid_quasar_combination()` function should filter these cases:

```python
def _is_invalid_quasar_combination(
    fmt: FormatConfig, dest_acc: DestAccumulation
) -> bool:
    in_fmt = fmt.input_format
    out_fmt = fmt.output_format

    # Quasar packer does not support non-Float32 -> Float32 when dest_acc=No
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

These are the same rules used by threshold and elu tests. No additional hardtanh-specific filtering is needed.

### Dest Accumulation Rules

```python
dest_acc_modes = (
    (DestAccumulation.Yes,)
    if in_fmt.is_32_bit()
    else (DestAccumulation.No, DestAccumulation.Yes)
)
```

Float32 input always requires `dest_acc=Yes` (32-bit Dest mode). Float16/Float16_b inputs can use either mode.

### MX Format Handling
Not applicable -- MX formats are excluded from the initial format list.

### Integer Format Handling
Not applicable -- integer formats are excluded from the initial format list.

## Testing Notes

### Input Preparation
- Input values should span both sides of the clamp range, including values well below min_val, between min_val and max_val, and well above max_val.
- Default test constants: `min_val = -1.0`, `max_val = 1.0` (matching PyTorch defaults for `torch.nn.functional.hardtanh`).
- FP32 encoding: `min_val = -1.0` -> `0xBF800000`, `max_val = 1.0` -> `0x3F800000`.

### Golden Reference
- `torch.nn.functional.hardtanh(input, min_val=-1.0, max_val=1.0)` or equivalently `torch.clamp(input, -1.0, 1.0)`.

### C++ Test Call Pattern
```cpp
constexpr std::uint32_t min_val_param = 0xBF800000u; // -1.0f
constexpr std::uint32_t max_val_param = 0x3F800000u; //  1.0f

_llk_math_eltwise_unary_sfpu_params_<false>(
    ckernel::sfpu::_calculate_hardtanh_, i, num_sfpu_iterations, min_val_param, max_val_param);
```

### Expected Behavior
- Input = -5.0 -> Output = -1.0 (clamped to min_val)
- Input = 0.5 -> Output = 0.5 (passed through)
- Input = 3.0 -> Output = 1.0 (clamped to max_val)
- Input = -1.0 -> Output = -1.0 (boundary, equals min_val)
- Input = 1.0 -> Output = 1.0 (boundary, equals max_val)
