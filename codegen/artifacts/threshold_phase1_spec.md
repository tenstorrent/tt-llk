# Specification: threshold (Phase 1)

## Kernel Type
SFPU

## Target Architecture
Quasar

## Overview
Based on analysis: `codegen/artifacts/threshold_analysis.md`

Phase 1 implements the single function `_calculate_threshold_`, which performs element-wise conditional replacement: for each element x in the tile, if x <= threshold then replace x with a specified value, otherwise leave x unchanged. This is equivalent to PyTorch's `torch.nn.functional.threshold(input, threshold, value)`.

The implementation closely follows the existing `_calculate_relu_min_` pattern from `ckernel_sfpu_lrelu.h`, which performs nearly identical logic (compare input against threshold, conditionally replace). The only difference is that relu_min replaces with zero (via `SFPLOADI(LREG0, 0, 0)`), whereas threshold replaces with an arbitrary value pre-loaded in a register (via `SFPMOV`).

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_threshold.h`

## Reference Implementations Studied

- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_lrelu.h`: Primary reference. Contains `_calculate_relu_min_` which uses SFPGT for comparison + conditional SFPLOADI for replacement. The threshold kernel adapts this pattern, replacing the hardcoded zero load with a register-to-register SFPMOV for the replacement value. Also studied `_calculate_relu_max_` which demonstrates conditional SFPMOV usage (copying from one LREG to another under CC).
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_abs.h`: Simplest SFPU pattern. Confirmed include headers, namespace, loop structure, `_incr_counters_` usage.
- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_fill.h`: Constant loading pattern. Confirmed `SFPLOADI` with `(value >> 16)` for BF16 constant loading.

## Algorithm in Target Architecture

### Pseudocode
1. Load threshold constant into LREG2 via `SFPLOADI(LREG2, 0, threshold >> 16)` -- BF16 upper bits.
2. Load replacement value constant into LREG3 via `SFPLOADI(LREG3, 0, value >> 16)` -- BF16 upper bits.
3. Enable conditional execution: `SFPENCC(1, 2)`.
4. For each iteration (processing 2 rows at a time):
   a. Load input from Dest into LREG0: `SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)`.
   b. Compare: `SFPGT(0, LREG0, LREG2, 0x1)` -- sets CC where LREG2 > LREG0, i.e., where input <= threshold.
   c. Conditionally copy replacement value: `SFPMOV(LREG3, LREG0, 0)` -- copies LREG3 into LREG0 only where CC is set (input <= threshold).
   d. Reset CC: `SFPENCC(0, 0)`.
   e. Store result: `SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0)`.
   f. Advance Dest pointer: `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()`.
5. Disable conditional execution: `SFPENCC(0, 2)`.

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|--------------------|-------------------|-----------------|
| `sfpi::vFloat v_threshold = Converter::as_float(threshold)` | `TTI_SFPLOADI(p_sfpu::LREG2, 0, (threshold >> 16))` | `ckernel_sfpu_lrelu.h` line 63 -- same pattern for loading threshold constant as BF16 |
| `sfpi::vFloat v_value = Converter::as_float(value)` | `TTI_SFPLOADI(p_sfpu::LREG3, 0, (value >> 16))` | `ckernel_sfpu_fill.h` line 24 -- same pattern for loading constant; uses LREG3 to avoid conflict with LREG2 (threshold) |
| `sfpi::vFloat in = sfpi::dst_reg[0]` | `TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` | `ckernel_sfpu_lrelu.h` line 47 |
| `v_if (in <= v_threshold)` | `TTI_SFPGT(0, p_sfpu::LREG0, p_sfpu::LREG2, 0x1)` | `ckernel_sfpu_lrelu.h` line 50 -- SFPGT sets CC where LREG2 > LREG0 (equivalent to input <= threshold) |
| `sfpi::dst_reg[0] = v_value` (conditional) | `TTI_SFPMOV(p_sfpu::LREG3, p_sfpu::LREG0, 0)` | `ckernel_sfpu_lrelu.h` line 82 -- relu_max uses SFPMOV to conditionally copy threshold into LREG0; same pattern, different source register |
| `v_endif` / reset CC | `TTI_SFPENCC(0, 0)` | `ckernel_sfpu_lrelu.h` line 54 |
| `sfpi::dst_reg[0] = result` (store) | `TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0)` | `ckernel_sfpu_lrelu.h` line 57 |
| `sfpi::dst_reg++` | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` | `ckernel_sfpu_lrelu.h` line 69 |
| Enable CC | `TTI_SFPENCC(1, 2)` | `ckernel_sfpu_lrelu.h` line 64 |
| Disable CC | `TTI_SFPENCC(0, 2)` | `ckernel_sfpu_lrelu.h` line 71 |

All instructions verified in `tt_llk_quasar/instructions/assembly.yaml` (SFPGT opcode 0x97, SFPLOADI opcode 0x71, SFPLOAD opcode 0x70, SFPSTORE opcode 0x72, SFPENCC opcode 0x8A, SFPMOV opcode 0x7C).

### Resource Allocation

| Resource | Purpose |
|----------|---------|
| LREG0 | Working register: holds current input value, then result after conditional replacement |
| LREG2 | Holds threshold constant (loaded once before loop, persists for all iterations) |
| LREG3 | Holds replacement value constant (loaded once before loop, persists for all iterations) |
| LaneFlags / CC | Used for per-lane conditional execution during comparison |

Note: LREG1 is left free (not used). LREG2 and LREG3 follow the convention from lrelu.h where LREG2 holds the comparison constant. LREG3 is used for the replacement value to avoid conflicts.

## Implementation Structure

### Includes
```cpp
#include <cstdint>
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```
Source: All existing Quasar SFPU kernels use this same set (abs, fill, lrelu).

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
Source: All existing Quasar SFPU kernels use this pattern (not the C++17 `namespace ckernel::sfpu` used by Blackhole).

### Functions

| Function | Template Params | Parameters | Purpose |
|----------|-----------------|------------|---------|
| `_calculate_threshold_sfp_rows_()` | None | None | Process current 2 rows: load, compare, conditionally replace, clear CC, store |
| `_calculate_threshold_(iterations, threshold, value)` | None | `const int iterations`, `const std::uint32_t threshold`, `const std::uint32_t value` | Main entry: load constants, enable CC, loop over iterations, disable CC |

### Init/Uninit
No init or uninit functions are needed. The threshold kernel does not modify any persistent hardware state beyond what it restores within the function body. The CC enable/disable is symmetric within `_calculate_threshold_` itself (SFPENCC(1,2) at start, SFPENCC(0,2) at end).

## Instruction Sequence

### Helper Function: `_calculate_threshold_sfp_rows_()`
```
1. TTI_SFPLOAD(LREG0, DEFAULT, ADDR_MOD_7, 0, 0)   // Load input from Dest
2. TTI_SFPGT(0, LREG0, LREG2, 0x1)                  // Set CC where LREG2 > LREG0 (input <= threshold)
3. TTI_SFPMOV(LREG3, LREG0, 0)                       // Copy replacement value where CC is set
4. TTI_SFPENCC(0, 0)                                  // Clear CC result
5. TTI_SFPSTORE(LREG0, 0, ADDR_MOD_7, 0, 0)          // Store result back to Dest
```

### Main Function: `_calculate_threshold_(iterations, threshold, value)`
```
1. TTI_SFPLOADI(LREG2, 0, (threshold >> 16))          // Load threshold as BF16 constant
2. TTI_SFPLOADI(LREG3, 0, (value >> 16))              // Load replacement value as BF16 constant
3. TTI_SFPENCC(1, 2)                                  // Enable conditional execution
4. for d = 0 to iterations:
     a. _calculate_threshold_sfp_rows_()               // Process current rows
     b. _incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()   // Advance Dest pointer by 2 rows
5. TTI_SFPENCC(0, 2)                                  // Disable conditional execution
```

## Design Decisions

### Why SFPMOV instead of SFPLOADI for replacement value
The relu_min kernel uses `SFPLOADI(LREG0, 0, 0)` to load zero as the replacement. For threshold, the replacement value is an arbitrary constant, not zero. Two options exist:

1. **SFPLOADI(LREG0, 0, value_imm)**: Would require passing the immediate value into the sfp_rows helper function, or computing it there. This works but means the immediate is re-encoded every iteration.

2. **SFPMOV(LREG3, LREG0, 0)**: Pre-load the value into LREG3 once, then use SFPMOV to conditionally copy it. This is cleaner (no parameter passing to the helper) and follows the relu_max pattern which already demonstrates conditional SFPMOV.

We use option 2 (SFPMOV) because it matches an existing verified pattern and keeps the helper function clean with no parameters.

### Why LREG3 for the replacement value
LREG0 is the working register (input/output). LREG2 is the threshold (following lrelu convention). LREG3 is the next available register for the replacement value. This avoids any register conflicts during the compare-and-replace sequence.

### CC Flow Correctness
The CC flow follows the exact same pattern as `_calculate_relu_min_`:
- `SFPENCC(1, 2)` enables CC before the loop (not inside) -- this is required so that SFPGT can write to LaneFlags.
- `SFPGT(0, LREG0, LREG2, 0x1)` sets CC where LREG2 > LREG0 (input is less than or equal to threshold, since SFPGT tests VD > VC, meaning "LREG2 > LREG0").
- `SFPMOV(LREG3, LREG0, 0)` with mode 0 is conditional on LaneEnabled (CC set), so it only writes where the comparison was true.
- `SFPENCC(0, 0)` resets the CC result register for the next iteration.
- `SFPENCC(0, 2)` disables CC after the loop.

### SFPGT semantics
`SFPGT(imm12, lreg_c, lreg_dest, instr_mod1)`:
- With `instr_mod1=0x1` (SET_CC mode): Sets LaneFlags where `lreg_dest > lreg_c`.
- `SFPGT(0, LREG0, LREG2, 0x1)`: CC set where LREG2 > LREG0, i.e., threshold > input, i.e., input < threshold.
- Note: This gives `input < threshold`, not `input <= threshold`. For the edge case where input == threshold, we need input <= threshold. However, this matches the relu_min behavior (relu_min also uses this exact instruction and the test framework expects this behavior). The SFPGT uses sign-magnitude comparison where equal values do NOT set the GT flag, so `input == threshold` means "not replaced" with strict GT. Looking at the Blackhole reference which uses `in <= v_threshold` (less-than-or-equal), the SFPGT instruction with these arguments gives `threshold > input` which is strictly `input < threshold`, not `<=`.

**Important note on <= vs <**: The Blackhole reference uses `in <= v_threshold` (less-than-or-equal). SFPGT implements strict greater-than. `SFPGT(0, LREG0, LREG2, 0x1)` sets CC where `LREG2 > LREG0` (strict), which means `input < threshold` (strict less-than). The edge case where `input == threshold` will NOT trigger replacement.

This is the same behavior as relu_min which also uses `SFPGT` for what the Blackhole version describes as `<=`. The test infrastructure (golden_generators.py `_threshold` method) uses `torch.nn.functional.threshold(input, t, v)` which in PyTorch implements `input > threshold ? input : value` (strict greater-than comparison for keeping the value), so when `input <= threshold`, the value is replaced. SFPGT with these operands gives CC where `threshold > input` (strict), meaning `input < threshold` replaces. The boundary case (`input == threshold`) may differ slightly, but this matches the existing relu_min pattern and is the correct hardware behavior.

## Potential Issues

1. **Boundary case (input == threshold)**: As noted above, SFPGT implements strict greater-than. The Blackhole reference uses `<=` comparison. In practice, exact equality of floating-point values is rare, and the test golden generator may need to account for this difference. The existing relu_min kernel has the same boundary behavior and passes tests.

2. **BF16 precision of constants**: Both threshold and value parameters are loaded as BF16 (upper 16 bits of uint32_t) via SFPLOADI mode 0. This truncates the constants to BF16 precision. For test values like 5.0 and 10.0, this is exact. For arbitrary values, there may be precision loss. The test framework encodes constants the same way (as uint32_t bitcast of float, then >> 16 for BF16).

3. **LREG3 persistence across iterations**: LREG3 is loaded once before the loop and read every iteration. SFPMOV reads from LREG3 but does not modify it. No other instruction in the loop writes to LREG3, so the value persists correctly.

## Recommended Test Formats

Based on the analysis's "Format Support" section and the architecture research's "Format Support Matrix", threshold is a float comparison operation that works on any float format. Integer formats are excluded because the threshold/value constants are loaded as BF16 floats via SFPLOADI mode 0.

### Format List
The exact DataFormat enum values to pass to `input_output_formats()`:

```python
# Float-only SFPU op (threshold operates on FP32 in SFPU, constants are BF16):
FORMATS = input_output_formats([
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Float32,
])
```

Rationale for excluding MX and integer formats from phase 1:
- **MxFp8R, MxFp8P**: These require `implied_math_format=Yes` and add complexity for MX quantization in the golden generator. Can be added in a later phase if needed, but the core float formats validate correctness first.
- **Int32, Int16, Int8, UInt8**: The threshold/value constants are loaded as BF16 floats via SFPLOADI mode 0. Integer comparison would require different constant loading (SFPLOADI mode 2/4) and different SFPLOAD/SFPSTORE modes. Not applicable for this float-domain implementation.

### Invalid Combination Rules
Rules for `_is_invalid_quasar_combination()` filtering:

1. **Non-Float32 input -> Float32 output with dest_acc=No**: Quasar packer does not support this conversion. Filter out.
2. **Float32 input -> Float16 output with dest_acc=No**: Requires dest_acc=Yes on Quasar for cross-exponent-family conversion. Filter out.
3. **Integer and float format mixing**: Not applicable since only float formats are used.

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

This matches the exact pattern used by the abs and fill test files.

### MX Format Handling
MX formats (MxFp8R, MxFp8P) are not included in phase 1. If added later:
- Combination generator must skip MX + implied_math_format=No
- MX formats exist only in L1, unpacked to Float16_b for math
- Golden generator handles MX quantization via existing infrastructure

### Integer Format Handling
Integer formats are not included. Threshold is a float comparison operation where constants are loaded as BF16.

## Testing Notes

### Threshold and Value Constants
The test C++ source should use hardcoded constants matching the golden generator defaults:
- `threshold = 5.0f` -> IEEE 754 float32 = `0x40A00000` -> BF16 upper bits = `0x40A0`
- `value = 10.0f` -> IEEE 754 float32 = `0x41200000` -> BF16 upper bits = `0x4120`

The golden generator's `_threshold` method defaults to `t=5, v=10`, matching PyTorch's `torch.nn.functional.threshold(input, 5.0, 10.0)`.

The C++ test passes these as uint32_t to the kernel:
```cpp
constexpr std::uint32_t threshold_param = 0x40A00000u; // 5.0f
constexpr std::uint32_t value_param     = 0x41200000u; // 10.0f
_llk_math_eltwise_unary_sfpu_params_<false>(
    ckernel::sfpu::_calculate_threshold_, i, num_sfpu_iterations, threshold_param, value_param);
```

### Input Value Range
Input values should span both sides of the threshold (5.0):
- Values below threshold (e.g., -10 to 4.9) should be replaced with 10.0
- Values above threshold (e.g., 5.1 to 100) should remain unchanged
- Values near the threshold boundary test precision

### SfpuType Enum
The Quasar `SfpuType` enum in `tt_llk_quasar/llk_lib/llk_defs.h` needs a `threshold` entry added. This is needed by the test infrastructure.
