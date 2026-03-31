# Analysis: abs

## Kernel Type
SFPU

## Reference File
`tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_abs.h`

## Purpose
Computes the element-wise absolute value of input data in the Dest register. For floating-point data, this clears the sign bit. For integer data (not implemented in the reference), this negates negative values.

## Algorithm Summary
The algorithm is trivially simple:
1. Load each value from the Dest register
2. Apply the absolute value operation (clear sign bit for float, negate if negative for int)
3. Store the result back to the Dest register
4. Advance to the next set of rows

There are no multi-step computations, no LUT lookups, no init/uninit phases, and no constants required.

## Template Parameters

| Parameter | Purpose | Values | Target Status |
|-----------|---------|--------|---------------|
| `APPROXIMATION_MODE` | Blackhole template parameter, unused in abs logic | `true`/`false` | **DROP** — Quasar simple SFPU ops do not use templates |
| `ITERATIONS` | Blackhole template parameter for iteration count | Compile-time int | **DROP** — Quasar passes iterations as runtime parameter |

## Functions Identified

| Function | Purpose | Complexity |
|----------|---------|------------|
| `_calculate_abs_` | Iterate over rows, applying abs to each | Low |

The reference has exactly ONE function. There is no init function, no uninit function, and no helper sub-function.

## Key Constructs Used

- **`sfpi::vFloat`**: SFPI vector float type — loads a row of float data from the Dest register. On Quasar, this is replaced by explicit `TTI_SFPLOAD`.
- **`sfpi::dst_reg[0]`**: Reads/writes data from/to the current Dest register row. On Quasar, this is `TTI_SFPLOAD` (read) and `TTI_SFPSTORE` (write).
- **`sfpi::abs(v)`**: Computes abs of vFloat by clearing the sign bit. On Quasar, this maps to `TTI_SFPABS(lreg_c, lreg_dest, instr_mod1)` with `instr_mod1=1` for FP32 mode.
- **`sfpi::dst_reg++`**: Advances the Dest register read/write pointer. On Quasar, this is `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()`.
- **`#include "sfpi.h"`**: Blackhole SFPI intrinsics header. On Quasar, replaced by `"ckernel_trisc_common.h"` and `"cmath_common.h"` (and optionally `"ckernel_ops.h"` for TTI macros).

## Dependencies

### Blackhole Reference
- `sfpi.h` — SFPI vector intrinsics (vFloat, dst_reg, abs)

### Quasar Target (expected)
- `ckernel_trisc_common.h` — Common Quasar TRISC definitions
- `cmath_common.h` — Math common definitions including `_incr_counters_` and `SFP_ROWS`
- `ckernel_ops.h` — TTI instruction macros (TTI_SFPLOAD, TTI_SFPABS, TTI_SFPSTORE)
- Constants: `p_sfpu::LREG0`, `p_sfpu::sfpmem::DEFAULT`, `ADDR_MOD_7`, `ckernel::math::SFP_ROWS`

## Complexity Classification
**Simple**

Quasar has a direct hardware instruction `TTI_SFPABS` (opcode 0x7D) that performs the exact operation. The port is a 1:1 instruction mapping requiring only 3 TTI instructions: SFPLOAD, SFPABS, SFPSTORE. This is the simplest possible SFPU kernel pattern on Quasar.

## Constructs Requiring Translation

| Blackhole Construct | What It Does | Quasar Equivalent |
|---|---|---|
| `sfpi::vFloat v = sfpi::dst_reg[0]` | Load row from Dest into vector register | `TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` |
| `sfpi::abs(v)` | Clear sign bit of float value | `TTI_SFPABS(p_sfpu::LREG0, p_sfpu::LREG0, 1)` (instr_mod1=1 for FP32 mode) |
| `sfpi::dst_reg[0] = result` | Store result back to Dest | `TTI_SFPSTORE(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` |
| `sfpi::dst_reg++` | Advance Dest pointer | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` |
| `template <bool APPROXIMATION_MODE, int ITERATIONS>` | Template params | DROP — use `inline void _calculate_abs_(const int iterations)` |
| `#include "sfpi.h"` | SFPI header | `#include "ckernel_ops.h"`, `#include "ckernel_trisc_common.h"`, `#include "cmath_common.h"` |

## Target Expected API

### From the Test Harness (`tests/helpers/include/sfpu_operations.h`)
The shared test harness calls:
```cpp
_calculate_abs_<APPROX_MODE, ITERATIONS>(ITERATIONS);
```
However, this test file is guarded by `#ifndef ARCH_QUASAR` for the SfpuType enum and is used by Blackhole/Wormhole tests. Quasar uses its own dedicated test files (e.g., `sfpu_square_quasar_test.cpp`, `sfpu_rsqrt_quasar_test.cpp`).

### From Quasar Dedicated Test Pattern (`tests/sources/quasar/sfpu_square_quasar_test.cpp`)
The Quasar SFPU test pattern calls:
```cpp
_llk_math_eltwise_unary_sfpu_params_<false>(ckernel::sfpu::_calculate_abs_, i, num_sfpu_iterations);
```
This expects the function signature: `inline void _calculate_abs_(const int iterations)` (NO template parameters).

### Function Signatures Expected by Target
- **Main function**: `inline void _calculate_abs_(const int iterations)` — matches Quasar SFPU pattern (no templates)
- **Helper function**: `inline void _calculate_abs_sfp_rows_()` — processes one set of SFP_ROWS (=2 rows), called by main loop
- **No init function needed** — abs has no state to initialize
- **No uninit function needed** — abs has no state to restore

### Template Parameters
- **KEEP from reference**: None
- **DROP (reference-only)**: `APPROXIMATION_MODE`, `ITERATIONS` — Quasar simple SFPU ops do not use templates
- **ADD (target-only)**: None

### Target Features NOT in Reference
- Two-level function structure: `_calculate_abs_sfp_rows_()` helper + `_calculate_abs_()` iteration loop (following established Quasar pattern from relu, square, rsqrt)
- `#pragma GCC unroll 8` on the iteration loop
- `ckernel::math::_incr_counters_` call instead of `dst_reg++`
- Explicit TTI instruction macros instead of SFPI intrinsics

### Reference Features to DROP
- `sfpi.h` include and all SFPI intrinsics (vFloat, dst_reg[], abs())
- Template parameters `APPROXIMATION_MODE` and `ITERATIONS`

## Format Support

### Format Domain
**Universal** — abs is mathematically valid for both floating-point and integer data types. The SFPABS instruction supports both FP32 mode (instr_mod1=1, clears sign bit) and INT32 mode (instr_mod1=0, negates negative values). However, the reference implementation only implements the float path, and the Quasar implementation should follow the same pattern for the initial implementation.

### Applicable Formats for Testing

| Format | Applicable | Rationale |
|--------|-----------|-----------|
| Float16 | Yes | Standard float format, unpacked to FP32 in LREG for abs |
| Float16_b | Yes | Most common format, unpacked to FP32 in LREG |
| Float32 | Yes | Full precision abs, requires 32-bit Dest mode (dest_acc=Yes) |
| Int32 | No (initial) | Would require separate SFPABS mode (instr_mod1=0) and SMAG32 load; not in reference |
| Int16 | No | Limited SFPU support for Int16 abs |
| Int8 | No | Limited range, not standard for SFPU abs |
| UInt8 | No | Unsigned type, abs is identity |
| MxFp8R | Conditional | MX format unpacked to Float16_b by hardware, then abs applies normally; requires implied_math_format=Yes |
| MxFp8P | Conditional | Same as MxFp8R |

### Recommended Test Formats (Primary)
Based on existing Quasar SFPU test patterns (`test_sfpu_square_quasar.py`, `test_sfpu_nonlinear_quasar.py`):
```python
[DataFormat.Float16, DataFormat.Float32, DataFormat.Float16_b]
```
These three formats are the standard set used by ALL existing Quasar SFPU tests. MxFp8R/MxFp8P could be added as secondary test formats if the MX infrastructure is verified.

### Format-Dependent Code Paths
The reference implementation (`_calculate_abs_`) is **format-agnostic** for floating-point types. It uses `sfpi::vFloat` and `sfpi::abs()` which operate on FP32 data regardless of the original format. The SFPLOAD/SFPSTORE instructions handle format conversion automatically via "implied format" mode.

There are **no format-dependent branches** in the reference. The code applies the same abs operation regardless of whether the original data was Float16, Float16_b, or Float32 (all are promoted to FP32 in the LREG).

### Format Constraints
- **Float32 input/output**: Requires `dest_acc=Yes` (32-bit Dest mode) for both input and output. Otherwise the Dest register is in 16-bit mode and cannot store Float32 directly.
- **Non-Float32 to Float32 conversion**: `in_fmt != Float32, out_fmt == Float32, dest_acc=No` is INVALID on Quasar (packer limitation).
- **Float32 to Float16**: `in_fmt == Float32, out_fmt == Float16, dest_acc=No` is INVALID on Quasar (gasket conversion limitation).
- **MxFp8R/MxFp8P**: If used, require `implied_math_format=Yes` because MX formats exist only in L1 and are unpacked to Float16_b.
- **Int32**: Would require `dest_acc=Yes` for 32-bit Dest mode, and SFPLOAD with `p_sfpu::sfpmem::INT32` mode, and SFPABS with `instr_mod1=0` for integer abs.
- **Cross-exponent-family**: ExpB input (Float16_b, Float32) to Float16 (expA) output without dest_acc requires special handling (format combination outlier).

## Existing Target Implementations

The following existing Quasar SFPU kernels establish the implementation pattern:

### Pattern Template (from ckernel_sfpu_relu.h — closest simple analog)
```cpp
#pragma once
#include "ckernel_trisc_common.h"
#include "cmath_common.h"

namespace ckernel {
namespace sfpu {

inline void _calculate_{op}_sfp_rows_() {
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
    // ... operation ...
    TTI_SFPSTORE(p_sfpu::LREG{N}, 0, ADDR_MOD_7, 0, 0);
}

inline void _calculate_{op}_(const int iterations) {
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++) {
        _calculate_{op}_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
}

} // namespace sfpu
} // namespace ckernel
```

### Key Observations from Existing Kernels
1. **All simple Quasar SFPU kernels** follow the two-function pattern: `_sfp_rows_()` helper + `_calculate_()` loop
2. **relu, square, rsqrt, silu, add** all use `ADDR_MOD_7` for load/store
3. **relu, square** use `TTI_SFPSTORE(p_sfpu::LREG{N}, 0, ADDR_MOD_7, 0, 0)` — note the `0` for instr_mod (implied format) in SFPSTORE
4. Some include `"ckernel_ops.h"` explicitly (square), others rely on transitive includes
5. The `#pragma GCC unroll 8` is present on all iteration loops
6. **No existing Quasar kernel uses SFPABS** — abs will be the first to use it
7. Functions are NOT templated — they take `const int iterations` as a runtime parameter
8. Store uses `p_sfpu::sfpmem::DEFAULT` (square) or plain `0` (relu) — both are equivalent (DEFAULT=0)

## Sub-Kernel Phases

| Phase | Name | Functions | Dependencies |
|-------|------|-----------|--------------|
| 1 | basic_abs | `_calculate_abs_sfp_rows_`, `_calculate_abs_` | none |

**Ordering rationale**: The abs kernel has only ONE logical sub-kernel. There is no init/uninit pair, no int32 variant, and no approximation modes. The entire kernel is a single SFPLOAD + SFPABS + SFPSTORE sequence with an iteration loop. This is the simplest possible SFPU kernel — a single phase covers everything.
