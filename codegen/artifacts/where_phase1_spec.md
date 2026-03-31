# Specification: where (Phase 1 -- Basic DISABLE_SFPLOADMACRO Path)

## Kernel Type
sfpu

## Target Architecture
quasar

## Overview
Based on analysis: `codegen/artifacts/where_analysis.md`

Phase 1 implements the basic conditional select operation (`where`), which computes:
```
result[i] = (condition[i] == 0) ? false_value[i] : true_value[i]
```

This phase implements only the `DISABLE_SFPLOADMACRO` code path: a straightforward
load-setcc-load-encc-store sequence. The `_init_where_` function is a no-op stub
(SFPLOADMACRO macro configuration is deferred to Phase 2).

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_where.h`

## Reference Implementations Studied

- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_add.h`: **Critical pattern** -- multi-tile
  access with runtime `TT_SFPLOAD`/`TT_SFPSTORE` using `offset_idx + (d << 1)` to iterate
  through Dest rows. This is the exact same iteration model the "where" kernel needs (explicit
  offsets, no `_incr_counters_`). Uses `p_sfpu::sfpmem::DEFAULT` for format.

- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_sign.h`: Conditional execution pattern
  using `TTI_SFPSETCC` + `TTI_SFPENCC`. Shows the Quasar 3-param `TTI_SFPSETCC(imm12, lreg_c, mod1)`
  and 2-param `TTI_SFPENCC(imm12, mod1)` API. Shows `TTI_SFPENCC(1, 2)` / `TTI_SFPENCC(0, 2)` as
  global CC enable/disable wrapper. Shows `TTI_SFPENCC(0, 0)` for resetting CC within loop body.

- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_threshold.h`: Conditional replacement pattern
  using SFPGT + SFPMOV with CC. Shows the simple `_sfp_rows_` + outer loop + `_incr_counters_`
  structure (which we do NOT follow because "where" uses explicit offsets like `add` does).

- `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_fill.h`: Shows `p_sfpu::sfpmem::DEFAULT` usage
  and basic no-op init pattern. Shows `p_sfpu::sfpmem::INT32` for explicit integer format mode.

- `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_where.h`: Reference semantics -- the algorithm
  to port. We use its semantics but adapt to Quasar instruction APIs and patterns.

## Algorithm in Target Architecture

### Pseudocode

```
_calculate_where_<APPROXIMATION_MODE, data_format, ITERATIONS>(dst_in0, dst_in1, dst_in2, dst_out):
  1. Compute base offsets: offsetN = (dst_index_inN * 32) << 1
  2. Determine format mode (mod0) from data_format template parameter
  3. For each iteration d in [0, ITERATIONS):
     a. SFPLOAD LREG0 from Dest[offset0 + (d << 1)]  -- load condition
     b. SFPLOAD LREG1 from Dest[offset1 + (d << 1)]  -- load true_value
     c. SFPSETCC: set CC where LREG0 == 0
     d. SFPLOAD LREG1 from Dest[offset2 + (d << 1)]  -- load false_value (ONLY into CC-set lanes)
     e. SFPENCC(0, 0): reset CC (all lanes active)
     f. SFPSTORE LREG1 to Dest[offset3 + (d << 1)]   -- store mixed result

_init_where_<APPROXIMATION_MODE>():
  No-op (SFPLOADMACRO configuration deferred to Phase 2)
```

After step (d), LREG1 contains:
- `true_value` in lanes where condition != 0 (CC was not set, so SFPLOAD did not write)
- `false_value` in lanes where condition == 0 (CC was set, so SFPLOAD overwrote)

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|---|---|---|
| `TT_SFPLOAD(lreg, mod0, addr_mode, dest_addr)` (4 params) | `TT_SFPLOAD(lreg, mod0, addr_mode, 0, dest_addr)` (5 params, done=0) | `ckernel_sfpu_add.h` line 19-20 |
| `TT_SFPSTORE(lreg, mod0, addr_mode, dest_addr)` (4 params) | `TT_SFPSTORE(lreg, mod0, addr_mode, 0, dest_addr)` (5 params, done=0) | `ckernel_sfpu_add.h` line 26 |
| `TTI_SFPSETCC(0, LREG0, 0, sfpi::SFPSETCC_MOD1_LREG_EQ0)` (4 params) | `TTI_SFPSETCC(0, p_sfpu::LREG0, 0x6)` (3 params, mod1=0x6 for EQ0) | `ckernel_sfpu_sign.h` line 40 |
| `TTI_SFPENCC(0, 0, 0, sfpi::SFPENCC_MOD1_EU_R1)` (4 params) | `TTI_SFPENCC(0, 0)` (2 params, reset CC) | `ckernel_sfpu_sign.h` line 37, 45 |
| `InstrModLoadStore::LO16` (Float16_b mode) | `p_sfpu::sfpmem::DEFAULT` (auto-resolve) | `ckernel_sfpu_add.h`, `ckernel_sfpu_sign.h`, `ckernel_sfpu_threshold.h` (all use DEFAULT) |
| `InstrModLoadStore::INT32` (32-bit mode) | `p_sfpu::sfpmem::DEFAULT` (auto-resolve) | Same -- DEFAULT resolves based on SrcB format config register |
| Offset formula `(dst_index * 32) << 1` | Same formula: `(dst_index * 32) << 1` | Blackhole reference, confirmed by Quasar add kernel offset pattern |
| Row increment via replay | `+ (d << 1)` per-iteration offset | `ckernel_sfpu_add.h` lines 19-20, 26 |
| `ADDR_MOD_7` (zero increment) | `ADDR_MOD_7` (same) | `llk_math_eltwise_ternary_sfpu.h` configures ADDR_MOD_7 with zero increments |

### Key Design Decision: Format Mode

The Blackhole reference uses an explicit format mode (`InstrModLoadStore::LO16` for Float16_b,
`InstrModLoadStore::INT32` for all others). On Quasar, the standard pattern across ALL existing
SFPU kernels (add, sign, threshold, fill, etc.) is to use `p_sfpu::sfpmem::DEFAULT` (value 0),
which auto-resolves based on the SrcB format configuration register set up by the unpack/math
init pipeline.

**Decision**: Use `p_sfpu::sfpmem::DEFAULT` for all SFPLOAD/SFPSTORE operations. This:
- Matches every existing Quasar SFPU kernel pattern
- Eliminates format-dependent code paths entirely
- Correctly handles all formats because the format config registers are set by the framework
- Is simpler and less error-prone than mapping DataFormat to sfpmem constants

### Key Design Decision: Iteration Model

The Blackhole reference uses a replay buffer (`lltt::record(0, 6)` + `lltt::replay(0, 6)`) to
replay 6 instructions `ITERATIONS` times. On Quasar, the `ckernel_sfpu_add.h` kernel shows the
correct pattern for multi-offset access: a simple `for` loop with `(d << 1)` offset per iteration.

**Decision**: Use the `ckernel_sfpu_add.h` iteration model -- a `for` loop where each load/store
address includes `+ (d << 1)` to advance through rows. This:
- Matches the existing Quasar multi-offset SFPU pattern (add kernel)
- Avoids the complexity of replay buffer API translation
- Is functionally equivalent (both process one row pair per iteration)

Note: The Blackhole reference uses `ITERATIONS` as a template parameter. The Quasar test harness
calls `_calculate_where_<false, DataFormat, iterations>(0, 1, 2, 0)` where `iterations` is a
`constexpr int` (value 32). To match the test harness signature exactly, `ITERATIONS` must remain
a template parameter (not a function argument).

### Key Design Decision: No CC Enable/Disable Wrapper

In typical Quasar SFPU kernels (sign, threshold, hardtanh, lrelu), the outer function wraps
the loop with `TTI_SFPENCC(1, 2)` / `TTI_SFPENCC(0, 2)` to enable/disable conditional
execution globally.

For "where", this is **required** because step (c) uses `TTI_SFPSETCC` which sets the CC
register, and step (d) uses a conditional `TT_SFPLOAD` that only writes to CC-set lanes.
Without `TTI_SFPENCC(1, 2)` first, the CC state from a previous kernel could pollute results.

**Decision**: Wrap the loop with `TTI_SFPENCC(1, 2)` before and `TTI_SFPENCC(0, 2)` after.
Within the loop, use `TTI_SFPENCC(0, 0)` after the conditional load to reset CC before the store.

### Resource Allocation

| Resource | Purpose |
|---|---|
| LREG0 | Temporary: holds condition value for SFPSETCC comparison |
| LREG1 | Result: holds true_value, then selectively overwritten with false_value in CC-set lanes |
| ADDR_MOD_7 | Zero-increment address mode (configured by ternary framework) |
| CC register | Per-lane condition code: set where condition == 0 |

## Implementation Structure

### Includes

```cpp
#include <cstdint>
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```
(Discovered from `ckernel_sfpu_sign.h`, `ckernel_sfpu_add.h`, and all other Quasar SFPU kernels.)

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
(Discovered from all existing Quasar SFPU kernels.)

### Functions

| Function | Template Params | Purpose |
|---|---|---|
| `_init_where_<APPROXIMATION_MODE>` | `bool APPROXIMATION_MODE` | No-op stub for Phase 1 (SFPLOADMACRO config deferred to Phase 2) |
| `_calculate_where_<APPROXIMATION_MODE, data_format, ITERATIONS>` | `bool APPROXIMATION_MODE, DataFormat data_format, int ITERATIONS` | Main compute: conditional select across 3 input tiles into 1 output tile |

### Init/Uninit Symmetry

Phase 1's `_init_where_` is a no-op, so there is nothing to reverse. No `_uninit_where_` is needed.
The ternary framework's `_llk_math_eltwise_ternary_sfpu_init_` / `_llk_math_eltwise_ternary_sfpu_done_`
handles framework-level state (addr mods, counters).

| Init Action | Uninit Reversal |
|---|---|
| (none -- no-op) | (none needed) |

## Instruction Sequence

### `_init_where_` (Phase 1: no-op)

```
template <bool APPROXIMATION_MODE>
inline void _init_where_()
{
    // No-op for Phase 1 (DISABLE_SFPLOADMACRO path).
    // SFPLOADMACRO macro configuration will be added in Phase 2.
}
```

### `_calculate_where_` (Phase 1: DISABLE_SFPLOADMACRO path)

```
template <bool APPROXIMATION_MODE, DataFormat data_format, int ITERATIONS>
inline void _calculate_where_(
    const std::uint32_t dst_index_in0,
    const std::uint32_t dst_index_in1,
    const std::uint32_t dst_index_in2,
    const std::uint32_t dst_index_out)
{
    // Compute base Dest row offsets: each tile is 32 rows, <<1 for 16-bit addressing
    int offset0 = (dst_index_in0 * 32) << 1;  // condition
    int offset1 = (dst_index_in1 * 32) << 1;  // true_value
    int offset2 = (dst_index_in2 * 32) << 1;  // false_value
    int offset3 = (dst_index_out * 32) << 1;  // output

    TTI_SFPENCC(1, 2);  // enable conditional execution globally

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        // Step 1: Load condition into LREG0
        TT_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset0 + (d << 1));

        // Step 2: Load true_value into LREG1
        TT_SFPLOAD(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset1 + (d << 1));

        // Step 3: Set CC where LREG0 == 0 (condition is zero)
        TTI_SFPSETCC(0, p_sfpu::LREG0, 0x6);

        // Step 4: Load false_value into LREG1 -- CC-gated, only writes to lanes where condition == 0
        TT_SFPLOAD(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset2 + (d << 1));

        // Step 5: Reset CC (all lanes active for store)
        TTI_SFPENCC(0, 0);

        // Step 6: Store result from LREG1 to output
        TT_SFPSTORE(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset3 + (d << 1));
    }

    TTI_SFPENCC(0, 2);  // disable conditional execution globally
}
```

### Critical Correctness Notes

1. **CC-gated SFPLOAD**: Step 4's `TT_SFPLOAD` into LREG1 only writes to lanes where CC is set.
   CC was set in step 3 for lanes where condition == 0. So LREG1 ends up with:
   - Lanes where condition != 0: true_value (from step 2, unchanged)
   - Lanes where condition == 0: false_value (from step 4, overwrote)

2. **CC reset before store**: Step 5 resets CC so the SFPSTORE in step 6 writes ALL lanes
   to Dest, not just the CC-set lanes.

3. **Global CC enable/disable**: `TTI_SFPENCC(1, 2)` at the start enables the CC mechanism.
   `TTI_SFPENCC(0, 2)` at the end disables it. This follows the standard Quasar pattern
   seen in sign, threshold, hardtanh, and lrelu kernels.

4. **Iteration stride**: `(d << 1)` increments the Dest row address by 2 per iteration,
   matching `SFP_ROWS = 2`. With `ITERATIONS = 32`, this covers all 64 rows of the tile
   (32 iterations * 2 rows/iteration = 64 rows).

5. **`done = 0`**: All SFPLOAD/SFPSTORE target Dest (not SrcS), so `done` is 0.

6. **SFPLOAD is CC-gated on writes, not reads**: SFPLOAD itself always reads from Dest.
   The CC gating affects which lanes of the target LREG are written. This is confirmed
   by the Blackhole reference's design, where SFPLOAD into LREG1 selectively updates
   only CC-set lanes.

## Potential Issues

1. **SFPLOAD CC-gating behavior**: The design relies on `TT_SFPLOAD` being CC-gated (only
   writing to CC-set lanes). This is the documented behavior per the SFPSTORE ISA page
   ("LaneEnabled" controls which lanes are written) and is the same mechanism used in the
   Blackhole reference. If Quasar's SFPLOAD behavior differs, step 4 may overwrite all
   lanes and break the conditional selection. Verify via test.

2. **Pipeline hazards**: The Blackhole reference uses a replay buffer partly for pipeline
   efficiency. The Quasar loop-based approach may have different pipeline behavior.
   However, the add kernel uses the same loop approach successfully, so this is unlikely
   to be an issue.

3. **No static_assert**: The Blackhole reference has a `static_assert` restricting to
   Float32/Float16_b/Int32/UInt32. For Phase 1, we omit this restriction since the
   operation is universal (conditional select works on any data type) and we use
   `p_sfpu::sfpmem::DEFAULT` which auto-resolves format. If format-specific issues
   arise during testing, a static_assert can be added later.

4. **ITERATIONS = 32 covers one face (64 rows)**: The test harness calls the kernel once
   with `iterations=32`. Each iteration processes SFP_ROWS=2 rows, so 32 * 2 = 64 rows.
   The ternary framework does NOT call the kernel per-face (unlike unary SFPU kernels).
   Instead, the 3 input tiles are all in the same Dest half (tiles at offsets 0, 1, 2),
   and the kernel processes 32 rows covering one face's worth.

## Recommended Test Formats

Based on the analysis's "Format Support" section and the architecture research's "Format Support Matrix".

The "where" operation is a **universal** conditional select -- no arithmetic is performed
on data values. All formats that the SFPU can load/store are applicable.

### Format List

The exact DataFormat enum values for the existing `test_zzz_ttnn_where.py` test:

```python
# The existing test uses these 3 formats with same=True:
# [DataFormat.Float16_b, DataFormat.Float32, DataFormat.Int32]
# This is sufficient for Phase 1 validation since it covers:
# - 16-bit float (Float16_b with dest_acc=No)
# - 32-bit float (Float32 with dest_acc=Yes)
# - 32-bit integer (Int32 with dest_acc=Yes)
```

For the comprehensive Quasar-specific test (when created by the test-writer agent):

```python
# Universal op: ALL Quasar-supported formats are semantically valid.
FORMATS = input_output_formats([
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Float32,
    DataFormat.Tf32,
    DataFormat.Int8,
    DataFormat.UInt8,
    DataFormat.Int16,
    DataFormat.UInt16,
    DataFormat.Int32,
    DataFormat.MxFp8R,
    DataFormat.MxFp8P,
])
```

**Justification for including all 11 formats**:
- The "where" operation performs NO arithmetic -- it only selects between two values based
  on a condition being zero or non-zero.
- SFPSETCC with mod1=0x6 (EQ0 check) works on any bit pattern in sign-magnitude
  representation -- zero is zero regardless of format.
- SFPLOAD/SFPSTORE with `DEFAULT` mode auto-resolves format based on config registers.
- Every format in the list is confirmed loadable/storable by the SFPU per architecture research.

### Invalid Combination Rules

Rules for `_is_invalid_quasar_combination()`:

```python
def _is_invalid_quasar_combination(fmt, dest_acc):
    in_fmt = fmt.input_format
    out_fmt = fmt.output_format

    # 1. Quasar packer does not support non-Float32 -> Float32 when dest_acc=No
    if in_fmt != DataFormat.Float32 and out_fmt == DataFormat.Float32 and dest_acc == DestAccumulation.No:
        return True

    # 2. Float32 input -> Float16 output requires dest_acc=Yes on Quasar
    if in_fmt == DataFormat.Float32 and out_fmt == DataFormat.Float16 and dest_acc == DestAccumulation.No:
        return True

    # 3. Integer and float formats cannot be mixed in input -> output conversion
    #    (int input must have int output, float input must have float output)
    int_formats = {DataFormat.Int8, DataFormat.UInt8, DataFormat.Int16, DataFormat.UInt16, DataFormat.Int32}
    float_formats = {DataFormat.Float16, DataFormat.Float16_b, DataFormat.Float32, DataFormat.Tf32, DataFormat.MxFp8R, DataFormat.MxFp8P}
    if (in_fmt in int_formats and out_fmt in float_formats) or (in_fmt in float_formats and out_fmt in int_formats):
        return True

    return False
```

Additional combination generation rules:
- 32-bit formats (Float32, Int32, Tf32) require `dest_acc=Yes` (only generate Yes for these)
- 16-bit formats (Float16, Float16_b, Int8, UInt8, Int16, UInt16) can use either `dest_acc=No` or `dest_acc=Yes`
- MX formats (MxFp8R, MxFp8P) require `implied_math_format=Yes` (skip MX + implied_math_format=No)

### MX Format Handling

MxFp8R and MxFp8P are L1-only formats:
- Unpacker converts MX -> Float16_b before writing to Dest
- SFPU operates on Float16_b representation in Dest
- Packer converts Float16_b -> MX when writing back to L1
- The `p_sfpu::sfpmem::DEFAULT` mode handles this automatically
- Combination generator must skip MX + `implied_math_format=No`
- Golden generator handles MX quantization via existing infrastructure

### Integer Format Handling

Int8, UInt8, Int16, UInt16, Int32:
- SFPSETCC with mod1=0x6 checks for zero -- works on sign-magnitude representation
  (zero is `0x00000000` regardless of integer format interpretation)
- SFPLOAD/SFPSTORE with DEFAULT mode handles the format conversion
- Input preparation must generate integer-range values (not float distributions)
- The condition tensor needs values that are either 0 or non-zero in the integer format
- Golden generator must use integer arithmetic (torch.where with integer tensors)
- `format_dict` in `helpers/llk_params.py` must map the format to appropriate torch dtype

## Testing Notes

1. **Three test cases**: The existing test uses "mixed" (random condition), "all_ones"
   (all true), and "all_zeros" (all false). Phase 1 should cover all three to verify
   the conditional logic.

2. **Condition tensor construction**: The condition tensor (dst_index_in0) should have
   a mix of zero and non-zero values. For integer formats, ensure zeros are exact zeros
   (not small values that round to zero in float but are non-zero in integer).

3. **Same input/output format**: The existing test uses `same=True`, meaning input and
   output formats match. This is the natural use case for "where" (select from same-format
   tensors). Cross-format combinations are a secondary concern.

4. **dest_acc requirements**: Float32/Tf32/Int32 must use dest_acc=Yes. Float16_b with
   dest_acc=Yes should be skipped (the existing test does this). This matches the
   existing `test_zzz_ttnn_where.py` skip logic.

5. **Comparison with Blackhole test**: The existing test already works for Blackhole with
   Float16_b, Float32, Int32. Phase 1 should at minimum pass these same formats on Quasar.
