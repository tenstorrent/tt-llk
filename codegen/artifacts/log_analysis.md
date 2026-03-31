# Analysis: log

## Kernel Type
sfpu

## Reference File
`tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_log.h`

## Purpose
Computes the natural logarithm (ln) of each element in a tile stored in the Dest register. The result replaces the input in Dest. Supports optional base scaling (for log2, log10, etc.) via a template parameter and scale factor.

## Algorithm Summary
The kernel uses the identity `ln(x) = ln(mantissa * 2^exp) = ln(mantissa) + exp * ln(2)` to decompose the problem:

1. **Load** input from Dest into an LREG
2. **Normalize**: Set exponent to 127 (FP32 bias) to put mantissa in range [1, 2)
3. **Polynomial approximation**: 3rd-order Chebyshev in Horner form for ln(x) over [1, 2):
   - `series_result = x * (x * (x * A + B) + C) + D`
   - Coefficients (from `_calculate_log_body_`): A=0.1058, B=-0.7116, C=2.0871, D=-1.4753
4. **Extract exponent**: Get the original biased exponent as integer via SFPEXEXP
5. **Handle negative exponent**: If exp < 0, negate (two's complement to sign-magnitude)
6. **Convert exponent to float**: int32 -> FP32 via SFPCAST
7. **Combine**: result = expf * ln(2) + series_result
8. **Optional base scaling**: result *= log_base_scale_factor
9. **Handle special case**: If input == 0, result = -infinity
10. **Store** result back to Dest

## Template Parameters
| Parameter | Purpose | Values |
|-----------|---------|--------|
| `HAS_BASE_SCALING` | Whether to multiply result by a base scaling factor | `true` / `false` |
| `APPROXIMATION_MODE` | Whether to use approximate computation | `true` / `false` (unused in log body) |
| `ITERATIONS` | Compile-time iteration count hint (unused in favor of runtime `iterations`) | int |

## Functions Identified
| Function | Purpose | Complexity |
|----------|---------|------------|
| `_init_log_<APPROXIMATION_MODE>()` | Loads polynomial coefficients into programmable constant registers (vConstFloatPrgm0/1/2) | Low |
| `_calculate_log_body_<HAS_BASE_SCALING>(log_base_scale_factor, dst_idx)` | Per-iteration core: normalizes, polynomial approx, exponent extraction, combine, special cases | High |
| `_calculate_log_<APPROXIMATION_MODE, HAS_BASE_SCALING, ITERATIONS>(iterations, log_base_scale_factor)` | Main loop wrapper: calls `_calculate_log_body_` for each row group | Low (wrapper) |
| `_calculate_log_body_no_init_(base)` | Self-contained variant with inline constants; takes input as parameter, returns result | High |

## Key Constructs Used

### SFPI Constructs (Blackhole-only, must be translated to TTI_)
- `sfpi::vFloat`, `sfpi::vInt`: Vector float/int types for SFPU registers -> manual LREG management
- `sfpi::dst_reg[idx]`: Read/write Dest register -> TTI_SFPLOAD / TTI_SFPSTORE
- `sfpi::dst_reg++`: Advance Dest pointer by 32 rows -> `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()`
- `setexp(in, 127)`: Set FP32 exponent to 127 -> TTI_SFPSETEXP(127, lreg_src, lreg_dest, 1)
- `sfpi::exexp(in)`: Extract FP32 exponent as biased integer -> TTI_SFPEXEXP(lreg_src, lreg_dest, instr_mod1)
- `sfpi::setsgn(~exp + 1, 1)`: Set sign bit to 1 (negate) -> TTI_SFPNOT + TTI_SFPIADD(1, ...) + TTI_SFPSETSGN(1, ..., 1)
- `int32_to_float(exp, 0)`: Convert sign-magnitude int32 to FP32 -> TTI_SFPCAST(lreg_src, lreg_dest, 0)
- `sfpi::vConstFloatPrgm0/1/2`: Programmable constant registers -> TTI_SFPLOADI to load constants
- `sfpi::s2vFloat16a(val)`: Convert scalar to FP16a vector -> TTI_SFPLOADI with FP16_B mode
- `v_if (exp < 0) { ... } v_endif`: Conditional execution per lane -> TTI_SFPSETCC + TTI_SFPENCC
- `v_if (in == 0.0F) { ... } v_endif`: Check for zero -> TTI_SFPSETCC(0, lreg, 0x6) (zero mode)
- FP32 arithmetic (`*`, `+`): -> TTI_SFPMAD / TTI_SFPMUL / TTI_SFPADD
- `-std::numeric_limits<float>::infinity()`: Negative infinity constant -> TTI_SFPLOADI with appropriate encoding

### Computation Patterns
- **Horner polynomial evaluation**: `x * (x * (x * A + B) + C) + D` using 3 chained SFPMAD instructions
- **Exponent decomposition**: SFPSETEXP + SFPEXEXP to separate mantissa and exponent
- **Conditional per-lane execution**: For negative exponent handling and zero input special case
- **Integer-to-float conversion**: SFPCAST for converting extracted exponent to FP32

## Dependencies
- `<cstdint>` (standard integer types)
- `<limits>` (for `std::numeric_limits<float>::infinity()`)
- `"sfpi.h"` (Blackhole SFPI library - NOT available on Quasar)
- Quasar equivalents: `"ckernel_trisc_common.h"`, `"cmath_common.h"` (for TTI_ macros)

## Complexity Classification
**Complex**

Rationale: There is NO dedicated SFPNONLINEAR LOG_MODE on Quasar (unlike exp, recip, sqrt, tanh which each have a hardware approximation mode). The log kernel must be implemented entirely in software using:
- SFPSETEXP/SFPEXEXP for float decomposition
- Chained SFPMAD for polynomial evaluation (3 nested multiply-adds)
- SFPCAST for integer-to-float conversion
- SFPSETCC/SFPENCC for two separate conditional execution blocks
- Multiple constants loaded via SFPLOADI

This is significantly more complex than kernels like exp, recip, sqrt, tanh (which are single SFPNONLINEAR calls), and comparable to gelu in complexity but with different instruction patterns (polynomial evaluation vs. LUT-based).

## Constructs Requiring Translation
| Blackhole Construct | Purpose | Quasar Equivalent |
|---------------------|---------|-------------------|
| `sfpi::vFloat/vInt` | Vector types | Manual LREG allocation (LREG0-LREG7) |
| `sfpi::dst_reg[idx]` | Dest read/write | TTI_SFPLOAD / TTI_SFPSTORE |
| `sfpi::dst_reg++` | Advance Dest pointer | `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()` |
| `setexp(in, 127)` | Normalize to [1,2) | TTI_SFPSETEXP(127, lreg_src, lreg_dest, 1) |
| `sfpi::exexp(in)` | Extract exponent | TTI_SFPEXEXP(lreg_src, lreg_dest, 0) |
| `sfpi::setsgn(val, 1)` | Set sign bit | TTI_SFPSETSGN(1, lreg_src, lreg_dest, 1) |
| `~exp + 1` | Two's complement negate | TTI_SFPNOT(lreg_src, lreg_dest) + TTI_SFPIADD(1, lreg_src, lreg_dest, 0) |
| `int32_to_float(exp, 0)` | Integer to FP32 | TTI_SFPCAST(lreg_src, lreg_dest, 0) |
| `sfpi::vConstFloatPrgm0/1/2` | Programmable constants | TTI_SFPLOADI to load into LREGs |
| `x * a + b` (FP32 arithmetic) | Multiply-add | TTI_SFPMAD(lreg_x, lreg_a, lreg_b, lreg_dest, 0) |
| `v_if (exp < 0)` | Negative check | TTI_SFPSETCC(0, lreg_exp, 0) (mode 0 = negative) |
| `v_if (in == 0.0F)` | Zero check | TTI_SFPSETCC(0, lreg_in, 0x6) (mode 6 = zero) |
| `-infinity` constant | Special case result | TTI_SFPLOADI with -inf encoding |

## Target Expected API

### From Test Harness (sfpu_nonlinear_quasar_test.cpp)
The Quasar nonlinear test harness (`tests/sources/quasar/sfpu_nonlinear_quasar_test.cpp`) dispatches SFPU operations through a `sfpu_op_dispatcher` template specialization pattern. Currently, `SfpuType::log` is **NOT** in the Quasar `SfpuType` enum (`tt_llk_quasar/llk_lib/llk_defs.h`), so it will need to be added. The blackhole test harness (`sfpu_operations.h`) calls:
```cpp
_init_log_<APPROX_MODE>();
_calculate_log_<APPROX_MODE, false, ITERATIONS>(ITERATIONS, 0);
```

### From Parent File (tt_llk_quasar/common/inc/ckernel_sfpu.h)
The parent file currently does NOT `#include "sfpu/ckernel_sfpu_log.h"`. It will need to be added.

### From Closest Existing Target Kernel
The most similar existing Quasar kernels (by complexity and pattern) are:
- **ckernel_sfpu_gelu.h**: Has `_init_gelu_()` + `_calculate_gelu_(iterations)` pattern, uses SFPLOADI for constants, SFPMAD for computation
- **ckernel_sfpu_exp2.h**: Uses SFPMUL + SFPNONLINEAR chain, demonstrates NOP insertion between SFPMUL and SFPNONLINEAR
- **ckernel_sfpu_elu.h**: Uses SFPSETCC + SFPENCC for conditional execution, SFPNONLINEAR for exp
- **ckernel_sfpu_sigmoid.h**: Multi-instruction sequence (SFPMOV, SFPNONLINEAR, SFPADD, SFPNONLINEAR)

### Target Function Signatures Expected
Based on the Quasar test harness dispatch pattern (`sfpu_op_dispatcher` in sfpu_nonlinear_quasar_test.cpp):
```cpp
// Init (called once before the loop, from init_sfpu_operation_quasar())
inline void _init_log_();

// Main calculate (called via sfpu_op_dispatcher<SfpuType::log>::call)
inline void _calculate_log_(const int iterations);
```

### Template Parameters
- **KEEP**: None from the reference that match Quasar patterns
- **DROP from reference**:
  - `APPROXIMATION_MODE` template param on `_calculate_log_` (Quasar implementations do NOT use this as a template param on the main calculate function; they use runtime `if constexpr` inside `_sfp_rows_` if needed)
  - `HAS_BASE_SCALING` template param (the base test harness calls with `false, 0`; not needed for initial port)
  - `ITERATIONS` template param (Quasar uses runtime `iterations` parameter)
  - `log_base_scale_factor` parameter (not needed when HAS_BASE_SCALING=false)
- **ADD (target-only)**:
  - `_calculate_log_sfp_rows_()` inner function (all Quasar SFPU kernels use this pattern)
  - `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` (Quasar row advancement)

### Reference Features to DROP
- `_calculate_log_body_no_init_()` - This is a Blackhole helper used internally by trigonometry and binary kernels (acosh, asinh, atanh, xlogy). Not needed for the standalone log kernel port.
- `HAS_BASE_SCALING` template and `log_base_scale_factor` parameter - The standard test dispatches with `false, 0`. Can be added in a future phase if needed.
- `dst_idx` parameter on `_calculate_log_body_` - Quasar SFPU kernels use implicit Dest addressing via SFPLOAD/SFPSTORE with ADDR_MOD_7.

## Format Support

### Format Domain
**Float-only** — Logarithm is a transcendental floating-point operation. It is mathematically undefined for integer types (log of raw integer bits has no meaning). Integer inputs would need conversion to float first, which is not the responsibility of this kernel.

### Applicable Formats for Testing
Based on the operation type, target architecture support, and existing test patterns:

| Format | Applicable | Rationale |
|--------|-----------|-----------|
| Float16 | Yes | Primary float format; unpacked to FP32 in LREG for SFPU ops. Used by rsqrt/gelu/exp/etc. tests. |
| Float16_b | Yes | Primary float format; unpacked to FP32 in LREG for SFPU ops. Used by all existing SFPU tests. |
| Float32 | Yes | Native SFPU format (LREGs are FP32). Requires dest_acc=Yes. Used by rsqrt test. |
| Int32 | No | Integer type — log of integer bits is not meaningful |
| Int16 | No | Integer type |
| Int8 | No | Integer type |
| UInt8 | No | Integer type |
| MxFp8R | Yes (L1 only) | Unpacked to Float16_b by hardware before SFPU operates. Valid for testing as L1 storage format. |
| MxFp8P | Yes (L1 only) | Unpacked to Float16_b by hardware before SFPU operates. Valid for testing as L1 storage format. |

### Format-Dependent Code Paths
The reference kernel is **format-agnostic** at the SFPU level. It operates purely in FP32 space (all LREG values are FP32). There are no format-dependent branches in `ckernel_sfpu_log.h`. The format handling is entirely managed by the unpacker (L1 -> registers) and packer (registers -> L1) stages, which are outside this kernel.

### Format Constraints
- **Float32 input/output**: Requires `dest_acc=Yes` (32-bit Dest mode). This is because Float32 data requires the destination register to be in 32-bit mode, which is only enabled when dest accumulation is on.
- **Non-Float32 to Float32 conversion**: Requires `dest_acc=Yes` on Quasar (packer limitation when Dest is in 16-bit mode).
- **Float32 to Float16 conversion**: Requires `dest_acc=Yes` on Quasar (exponent family crossing).
- **MxFp8R/MxFp8P**: L1-only formats. Hardware unpacks to Float16_b for math. Requires `implied_math_format=Yes`.
- **Cross-exponent-family**: Float16_b (expB) input with Float16 (expA) output requires dest_acc=Yes.
- **Integer and float formats cannot be mixed** in input->output.

## Existing Target Implementations
No existing `ckernel_sfpu_log.h` exists in `tt_llk_quasar/common/inc/sfpu/`.

The following related Quasar SFPU kernels provide pattern guidance:

| Kernel | Relevance | Key Patterns |
|--------|-----------|--------------|
| `ckernel_sfpu_gelu.h` | Init + constants + multi-instruction compute | `_init_gelu_()` loads constants via SFPLOADI and `_sfpu_load_config32_`. `_calculate_gelu_sfp_rows_()` uses SFPLOAD, SFPLUTFP32, SFPMAD, SFPSTORE. |
| `ckernel_sfpu_exp2.h` | SFPMUL + SFPNONLINEAR chain with NOP | Shows SFPMUL before SFPNONLINEAR requires SFPNOP(0,0,0) between them. |
| `ckernel_sfpu_elu.h` | Conditional execution | Uses SFPSETCC(0, LREG, 0) for negative check, SFPENCC(0,0) to reset, SFPENCC(1,2)/SFPENCC(0,2) to enable/disable CC. |
| `ckernel_sfpu_sigmoid.h` | Multi-instruction chain | SFPMOV + SFPNONLINEAR + SFPADD + SFPNONLINEAR sequence. |
| `ckernel_sfpu_rsqrt.h` | Chained SFPNONLINEAR calls | SQRT then RECIP without NOP between (different sub-units, no data hazard). |
| `ckernel_sfpu_abs.h` | Simple single-instruction | Minimal pattern showing SFPLOAD + single op + SFPSTORE. |

### Common Quasar SFPU Patterns
1. **File structure**: `#pragma once`, includes `ckernel_trisc_common.h` and `cmath_common.h`, `namespace ckernel { namespace sfpu { ... } }`
2. **Init function** (if needed): `inline void _init_op_()` - loads constants via `TTI_SFPLOADI` or `_sfpu_load_config32_`
3. **Per-row function**: `inline void _calculate_op_sfp_rows_()` - SFPLOAD, compute, SFPSTORE
4. **Main loop**: `inline void _calculate_op_(const int iterations)` with `#pragma GCC unroll 8`, calls sfp_rows + `_incr_counters_`
5. **Conditional execution**: `TTI_SFPENCC(1, 2)` before loop, `TTI_SFPENCC(0, 2)` after loop; inside: `TTI_SFPSETCC` to set CC, `TTI_SFPENCC(0, 0)` to clear

## Sub-Kernel Phases

The log kernel has two logical phases:

| Phase | Name | Functions | Dependencies |
|-------|------|-----------|--------------|
| 1 | Basic log (natural logarithm) | `_init_log_()`, `_calculate_log_sfp_rows_()`, `_calculate_log_(iterations)` | none |

**Rationale for single phase**: The core log kernel consists of one init function and one calculate function pair. The `_calculate_log_body_no_init_` variant from Blackhole is a reference-only helper used by trigonometry/binary kernels and should NOT be ported (those callers don't exist on Quasar yet). The `HAS_BASE_SCALING` variant is not exercised by the standard test and can be added later if needed.

Phase 1 includes:
- `_init_log_()`: Loads polynomial coefficients (A, B for Horner form) and ln(2) constant into LREGs
- `_calculate_log_sfp_rows_()`: The inner per-2-rows computation (normalize, polynomial eval, exponent extraction, combine, special cases)
- `_calculate_log_(const int iterations)`: The main loop wrapper with `#pragma GCC unroll 8`

This is a single phase because all three functions are tightly coupled and cannot be tested independently -- the init loads constants used by the calculation, and the main loop is just a wrapper around the per-row function.
