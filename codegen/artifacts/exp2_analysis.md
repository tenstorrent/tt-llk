# Analysis: exp2

## Kernel Type
sfpu

## Reference File
`tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_exp2.h`

## Purpose
Computes 2^x (exponential base 2) for each element in the destination register tile. This is a standard transcendental floating-point function used in neural network operations and mathematical computations.

## Algorithm Summary
The exp2 operation computes 2^x by converting it to the natural exponential:
```
2^x = e^(x * ln(2))
```
Where ln(2) = 0.6931471805.

Steps:
1. Load value x from destination register
2. Multiply x by ln(2) to get x * ln(2)
3. Compute e^(x * ln(2)) using the exponential function
4. Store the result back to the destination register

On Blackhole, step 3 is a complex multi-instruction polynomial approximation (`_calculate_exponential_piecewise_`). On Quasar, step 3 is a single SFPNONLINEAR instruction with EXP_MODE.

## Template Parameters
| Parameter | Purpose | Values |
|-----------|---------|--------|
| APPROXIMATION_MODE | Controls accuracy vs. performance tradeoff | `true` (fast/approx), `false` (accurate) |
| ITERATIONS | Number of loop iterations (rows processed) | Typically face_rows / SFP_ROWS |

**Note**: On Quasar, the `ITERATIONS` template parameter is not used. Instead, `iterations` is a runtime `const int` parameter. The `APPROXIMATION_MODE` template parameter exists in the reference but the Quasar exp kernel pattern does not use it for SFPNONLINEAR (the instruction always uses approximation mode). However, existing Quasar exp/tanh kernels still template on `APPROXIMATION_MODE` and guard the SFPNONLINEAR call with `if constexpr (APPROXIMATION_MODE)`.

## Functions Identified
| Function | Purpose | Complexity |
|----------|---------|------------|
| `_calculate_exp2_()` (Blackhole) | Main calculation: loads from dest, multiplies by ln(2), calls exp piecewise, stores back | Medium (reference), Low (target) |
| `_init_exp2_()` (Blackhole) | Initialization: calls `_init_exponential_()` to set up LUT/LOADMACRO for exp | Medium (reference), Not needed (target) |

## Key Constructs Used

### Reference (Blackhole) Constructs
- **`sfpi::vFloat`**: C++ vector float abstraction for SFPU lane-parallel operations. On Quasar, TTI_* intrinsics are used directly instead.
- **`sfpi::dst_reg[0]`**: Abstraction for loading from / storing to destination register. On Quasar, this becomes explicit `TTI_SFPLOAD` / `TTI_SFPSTORE`.
- **`sfpi::dst_reg++`**: Increments destination register address. On Quasar, this becomes `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()`.
- **`_calculate_exponential_piecewise_<>()`**: Complex exp implementation using polynomial approximation, LUTs, and LOADMACRO. On Quasar, replaced entirely by single `TTI_SFPNONLINEAR(..., p_sfpnonlinear::EXP_MODE)`.
- **`_init_exponential_<>()`**: Sets up LUT constants and LOADMACRO sequences for the piecewise exp. On Quasar, no equivalent needed -- SFPNONLINEAR is hardware-internal.
- **`p_sfpu::kCONST_1_FP16B`**: Constant used as exp base scale factor. Not needed on Quasar.
- **`v * 0.6931471805f`**: Scalar multiplication using SFPI library. On Quasar, use `TTI_SFPMULI` with FP16_B immediate encoding of ln(2).

### Target (Quasar) Constructs to Use
- **`TTI_SFPLOAD(lreg_dest, mode, addr_mod, done, dest_reg_addr)`**: Load from dest register to LREG (5-argument version)
- **`TTI_SFPMULI(imm16_math, lreg_dest, instr_mod1)`**: Multiply LREG[lreg_dest] by FP16_B immediate, store back in LREG[lreg_dest]
- **`TTI_SFPNONLINEAR(lreg_c, lreg_dest, instr_mod1)`**: Hardware-accelerated transcendental function
- **`TTI_SFPSTORE(lreg_src, mode, addr_mod, done, dest_reg_addr)`**: Store from LREG to dest register (5-argument version)
- **`ckernel::math::_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()`**: Increment dest register row counter by SFP_ROWS (=2)
- **`#pragma GCC unroll 8`**: Loop unrolling hint used by all existing Quasar SFPU kernels

## Dependencies

### Reference Dependencies
- `ckernel_sfpu_exp.h` (for `_calculate_exponential_piecewise_` and `_init_exponential_`)
- `sfpi.h` (for `sfpi::vFloat`, `sfpi::dst_reg`)
- `<cstdint>`

### Target Dependencies (what the Quasar implementation will need)
- `ckernel_trisc_common.h` (standard header for TTI_* macros, addr mods)
- `cmath_common.h` (for `SFP_ROWS`, `_incr_counters_`, counter management)
- No dependency on `ckernel_sfpu_exp.h` -- unlike Blackhole, the Quasar implementation does not call any exp subroutines

## Complexity Classification
**Simple**

Quasar has the SFPNONLINEAR instruction (opcode 0x99, EXP_MODE = 0x4) which computes e^x in a single cycle. The exp2 implementation simply multiplies by ln(2) first, then calls SFPNONLINEAR. The entire kernel is ~4 TTI instructions per iteration (SFPLOAD, SFPMULI, SFPNONLINEAR, SFPSTORE), following the exact same pattern as the existing `ckernel_sfpu_exp.h` but with one extra multiply step.

No init function is needed because:
- SFPNONLINEAR is hardware-internal (no LUT setup needed)
- SFPMULI uses an inline immediate (no LREG pre-loading needed)

## Constructs Requiring Translation

| Reference Construct | What it Does | Target Equivalent |
|---------------------|-------------|-------------------|
| `sfpi::vFloat v = sfpi::dst_reg[0]` | Load value from dest register into a vector register | `TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` |
| `v = v * 0.6931471805f` | Multiply by ln(2) scalar constant | `TTI_SFPMULI(LN2_FP16B, p_sfpu::LREG0, 0)` where LN2_FP16B is the FP16_B encoding of ln(2) |
| `_calculate_exponential_piecewise_<>(v, ...)` | Compute e^v using complex polynomial/LUT | `TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE)` |
| `sfpi::dst_reg[0] = exp` | Store result back to dest register | `TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0)` |
| `sfpi::dst_reg++` | Advance to next set of rows | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` |
| `_init_exponential_<>()` | Initialize LUT/LOADMACRO for piecewise exp | **Not needed** -- SFPNONLINEAR is self-contained |
| `for (int d = 0; d < ITERATIONS; d++)` | Template-parameter iteration loop | `for (int d = 0; d < iterations; d++)` -- runtime parameter on Quasar |
| Namespace `ckernel::sfpu` | Blackhole namespace | `ckernel` outer + `sfpu` inner (dual namespace like all Quasar SFPU kernels) |

## Target Expected API

### From Parent File (ckernel_sfpu.h)
The parent file `tt_llk_quasar/common/inc/ckernel_sfpu.h` will need to add:
```cpp
#include "sfpu/ckernel_sfpu_exp2.h"
```
Currently it does NOT include exp2 -- this is a new file.

### Function Signatures Expected by Target
Based on the patterns from existing Quasar SFPU kernels (exp, tanh, sigmoid, silu, elu, rsqrt):

**Calculate function** (called from test harness via `_llk_math_eltwise_unary_sfpu_params_`):
```cpp
void _calculate_exp2_(const int iterations)
```
- NOT templated on APPROXIMATION_MODE (can be, but following exp pattern which uses `<true>` in test harness)
- Takes `iterations` as runtime parameter (not template parameter like Blackhole)
- No return value
- Loops internally over iterations

Alternative (following exp/tanh pattern with template):
```cpp
template <bool APPROXIMATION_MODE>
void _calculate_exp2_(const int iterations)
```

**Init function**: Not needed. The existing exp kernel has no init, and SFPNONLINEAR needs no setup. The Blackhole `_init_exp2_` only exists to initialize `_init_exponential_` which is not needed on Quasar.

### Template Parameters
- **KEEP from reference**: `APPROXIMATION_MODE` (bool) -- existing Quasar pattern uses this to guard SFPNONLINEAR calls with `if constexpr`
- **DROP from reference**: `ITERATIONS` as a template parameter -- Quasar uses runtime `const int iterations` instead
- **ADD (target-only)**: None

### SfpuType Enum
The Quasar `SfpuType` enum in `tt_llk_quasar/llk_lib/llk_defs.h` does NOT currently include `exp2`. It needs to be added. The Python test infrastructure already has `MathOperation.Exp2 = OpSpec("exp2", MathOpType.SFPU_UNARY)`.

### Test Harness Pattern
Based on existing test sources (e.g., `sfpu_nonlinear_quasar_test.cpp`, `sfpu_rsqrt_quasar_test.cpp`):
- The test C++ file includes the kernel header
- Uses `_llk_math_eltwise_unary_sfpu_params_<false>(_calculate_exp2_<true>, tile_idx, num_sfpu_iterations)` (following exp/tanh pattern)
- Or `_llk_math_eltwise_unary_sfpu_params_<false>(_calculate_exp2_, tile_idx, num_sfpu_iterations)` (following sigmoid/silu pattern without template)

## Format Support

### Format Domain
**Float-only** -- exp2 is a transcendental floating-point operation. It is mathematically undefined for integer types.

### Applicable Formats for Testing
Based on the operation type, target architecture support, and existing test patterns:

| Format | Applicable | Rationale |
|--------|-----------|-----------|
| Float16 | Yes | Standard FP format, supported in Quasar SFPU; used in existing nonlinear tests |
| Float16_b | Yes | Standard FP format, supported in Quasar SFPU; used in existing nonlinear tests |
| Float32 | Yes | Native SFPU format, requires dest_acc=Yes; used in existing nonlinear tests |
| Int32 | No | exp2 is transcendental FP operation, meaningless on integers |
| Int16 | No | exp2 is transcendental FP operation, meaningless on integers |
| Int8 | No | exp2 is transcendental FP operation, meaningless on integers |
| UInt8 | No | exp2 is transcendental FP operation, meaningless on integers |
| MxFp8R | Yes (optional) | L1-only format; unpacked to Float16_b for math; could be tested but existing nonlinear tests don't include it |
| MxFp8P | Yes (optional) | L1-only format; unpacked to Float16_b for math; could be tested but existing nonlinear tests don't include it |

**Recommended test formats**: Float16, Float16_b, Float32 (matching existing `test_sfpu_nonlinear_quasar.py` pattern which tests exactly these three formats for exp).

### Format-Dependent Code Paths
The kernel itself is format-agnostic. The SFPU always operates internally in FP32 regardless of the source/destination format. The `SFPLOAD`/`SFPSTORE` instructions handle format conversion automatically based on the `sfpmem` mode parameter (DEFAULT = implied format from hardware configuration).

There are no format-dependent branches in either the reference or the target implementation.

### Format Constraints
- **Float32 requires dest_acc=Yes**: When input is Float32, the dest register is in 32-bit mode, which requires `is_fp32_dest_acc_en = true`
- **Non-Float32 to Float32 output requires dest_acc=Yes**: Quasar packer limitation -- `Float16->Float32` or `Float16_b->Float32` conversion requires 32-bit dest mode
- **Float32 input to Float16 output requires dest_acc=Yes**: Cross-exponent-family conversion limitation
- **MX formats require implied_math_format=Yes**: MX formats (MxFp8R, MxFp8P) are L1-only; unpacker converts them to Float16_b
- **Integer formats not applicable**: exp2 is float-only operation
- **Invalid combinations** (from existing nonlinear test `_is_invalid_quasar_combination`):
  - `in_fmt != Float32 AND out_fmt == Float32 AND dest_acc == No` -- invalid
  - `in_fmt == Float32 AND out_fmt == Float16 AND dest_acc == No` -- invalid

## Existing Target Implementations

### Closest Match: `ckernel_sfpu_exp.h` (Quasar)
The existing Quasar exp kernel is the closest match. It uses SFPNONLINEAR with EXP_MODE in the exact same way exp2 will, minus the ln(2) multiplication:
```cpp
template <bool APPROXIMATION_MODE>
inline void _calculate_exp_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
    if constexpr (APPROXIMATION_MODE)
    {
        TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE);
    }
    TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0);
}

template <bool APPROXIMATION_MODE>
inline void _calculate_exp_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_exp_sfp_rows_<APPROXIMATION_MODE>();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
}
```

### Pattern Summary from Existing Quasar SFPU Kernels
All existing Quasar SFPU kernels follow this pattern:
1. **Headers**: `ckernel_trisc_common.h`, `cmath_common.h` (and optionally `ckernel_ops.h` for more complex operations)
2. **Namespace**: `ckernel` outer, `sfpu` inner (`namespace ckernel { namespace sfpu { ... } }`)
3. **Row function**: `_calculate_{op}_sfp_rows_()` processes SFP_ROWS=2 rows
4. **Main function**: `_calculate_{op}_(const int iterations)` loops over iterations calling the row function + `_incr_counters_`
5. **Loop**: `#pragma GCC unroll 8` + `for (int d = 0; d < iterations; d++)`
6. **SFPLOAD/SFPSTORE**: Always use 5 arguments with `ADDR_MOD_7` and `(0, 0)` for done+dest_reg_addr
7. **No init needed** for simple kernels using SFPNONLINEAR (exp, tanh, recip, sqrt have no init)
8. **Init via SFPLOADI/SFPCONFIG** for kernels needing constants (gelu, elu load constants before the loop)

## Sub-Kernel Phases

| Phase | Name | Functions | Dependencies |
|-------|------|-----------|--------------|
| 1 | basic_exp2 | `_calculate_exp2_sfp_rows_`, `_calculate_exp2_` | none |

**Rationale**: exp2 is a simple SFPU kernel with only two functions (a row-level helper and the main iteration wrapper). There is no init function needed (unlike Blackhole), and there are no variant sub-kernels. This is a single-phase implementation.

The implementation is ~20 lines total, making it one of the simplest possible SFPU kernels -- essentially the exp kernel plus one SFPMULI instruction.
