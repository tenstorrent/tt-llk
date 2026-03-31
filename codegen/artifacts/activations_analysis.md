# Analysis: activations

## Kernel Type
SFPU

## Reference File
`tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_activations.h`

## Purpose
The activations kernel provides a unified dispatch mechanism for multiple activation functions. On Blackhole, it implements two activation types via template specializations:
1. **CELU** (Continuously Differentiable Exponential Linear Unit): `CELU(x) = max(0,x) + min(0, alpha * (exp(x/alpha) - 1))`
2. **Hardsigmoid**: `Hardsigmoid(x) = clamp(x * slope + offset, 0, 1)` where slope=1/6, offset=0.5

## Algorithm Summary

### CELU Algorithm
For each element:
- If x >= 0: output = x (identity)
- If x < 0: output = alpha * (exp(x / alpha) - 1)
- Parameters: `param0` = alpha (FP16_B encoded in uint32), `param1` = alpha_recip (1/alpha, FP16_B encoded)

### Hardsigmoid Algorithm
For each element:
- Compute: tmp = x * (1/6) + 0.5
- Clamp result to [0, 1]: output = max(0, min(tmp, 1))
- No parameters needed; slope and offset are loaded during init

## Template Parameters
| Parameter | Purpose | Values |
|-----------|---------|--------|
| `APPROXIMATION_MODE` | Controls precision vs performance tradeoff for sub-operations (exp) | true/false |
| `ACTIVATION_TYPE` | Selects which activation function to apply | `ActivationType::Celu`, `ActivationType::Hardsigmoid` |
| `ITERATIONS` | Number of row-groups to process per tile face (default 8 on Blackhole) | Integer, default 8 |

## Functions Identified
| Function | Purpose | Complexity |
|----------|---------|------------|
| `ActivationImpl<..., Celu>::apply(v, param0, param1)` | Apply CELU to a single vFloat value | Medium |
| `ActivationImpl<..., Hardsigmoid>::apply(v)` | Apply Hardsigmoid to a single vFloat value | Medium |
| `apply_activation<...>(v, param0, param1)` | 2-param dispatch wrapper | Low |
| `apply_activation<...>(v)` | 0-param dispatch wrapper | Low |
| `_calculate_activation_<..., ITERATIONS>(param0, param1)` | Main loop with 2 params (used by CELU) | Medium |
| `_calculate_activation_<..., ITERATIONS>()` | Main loop with 0 params (used by Hardsigmoid) | Medium |
| `_init_hardsigmoid_<APPROXIMATION_MODE>()` | Init: load slope=1/6 and offset=0.5 into programmable constants | Low |

## Key Constructs Used
- **SFPI intrinsics (vFloat, v_if/v_endif, dst_reg)**: Blackhole-only C++ vector intrinsics. NOT available on Quasar. Must be replaced with TTI macros.
- **sfpi::vFloat**: Blackhole vector float type representing a SIMD lane's value. On Quasar, values live in LREGs (LREG0-LREG7).
- **sfpi::dst_reg[0] / sfpi::dst_reg++**: Blackhole mechanism for reading/writing Dest register. On Quasar, use `TTI_SFPLOAD`/`TTI_SFPSTORE` + `_incr_counters_`.
- **v_if (v < 0.0f) {...} v_endif**: Blackhole conditional execution on vector elements. On Quasar, use `TTI_SFPSETCC` + `TTI_SFPENCC` for condition code-based lane masking.
- **sfpi::vConstFloatPrgm0 / vConstFloatPrgm1**: Blackhole programmable constant registers. On Quasar, use `_sfpu_load_config32_` to write to constant register indices 11-14, or `TTI_SFPLOADI` to load constants into LREGs.
- **Converter::as_float(param)**: Blackhole helper converting uint32 (FP16_B encoded) to vFloat. On Quasar, use `TTI_SFPLOADI(lreg, 0 /*FP16_B*/, param >> 16)`.
- **_calculate_exponential_body_<APPROXIMATION_MODE>(x)**: Blackhole exp function returning vFloat. On Quasar, use `TTI_SFPNONLINEAR(src_reg, dest_reg, p_sfpnonlinear::EXP_MODE)`.
- **_relu_max_body_(val, threshold)**: Blackhole helper clamping to [0, threshold]. On Quasar, must be decomposed into comparison + conditional move + relu.
- **ActivationType enum**: Defined in Blackhole's `ckernel_defs.h`. NOT present on Quasar. Must define it or use a different dispatch mechanism.
- **Template struct specialization (ActivationImpl)**: Blackhole dispatch pattern. On Quasar, use separate named functions following the `_calculate_<op>_` pattern.
- **#pragma GCC unroll 8**: Loop unrolling hint. Same on Quasar.

## Dependencies
- `ckernel_defs.h` (for ActivationType enum -- Blackhole only)
- `sfpi.h` (for SFPI intrinsics -- Blackhole only)
- `sfpu/ckernel_sfpu_converter.h` (for Converter::as_float -- Blackhole only)
- `sfpu/ckernel_sfpu_exp.h` (for _calculate_exponential_body_ -- Blackhole only)
- `sfpu/ckernel_sfpu_relu.h` (for _relu_max_body_ -- Blackhole only)

For Quasar, the dependencies will be:
- `ckernel_trisc_common.h` (TTI macros)
- `cmath_common.h` (_incr_counters_, _sfpu_load_config32_)
- `ckernel_ops.h` (additional TTI macros like TTI_SFPGT if needed)
- No external SFPU kernel dependencies -- all building blocks will be inlined

## Complexity Classification
**Medium**

Rationale: Both CELU and Hardsigmoid can be composed from Quasar primitives that already exist in other kernels:
- CELU is very similar to ELU (already on Quasar), differing only in the pre-multiplication by alpha_recip before exp.
- Hardsigmoid requires a multiply-add followed by clamping to [0, 1], which can be done with SFPMAD + SFPGT + SFPSETCC + conditional moves.

The challenge is medium because:
1. No ActivationType enum exists on Quasar, requiring a design decision on dispatch.
2. The hardsigmoid clamping to [0,1] requires combining max(0,x) and min(x,1) which needs multiple condition code operations.
3. No test harness or calling convention exists yet for this kernel on Quasar.

## Constructs Requiring Translation

| Reference Construct | What It Does | Quasar Equivalent |
|---------------------|-------------|-------------------|
| `sfpi::vFloat v = sfpi::dst_reg[0]` | Load value from Dest into vector variable | `TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` |
| `sfpi::dst_reg[0] = v` | Store vector variable back to Dest | `TTI_SFPSTORE(lreg, 0, ADDR_MOD_7, 0, 0)` |
| `sfpi::dst_reg++` | Advance Dest pointer | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` |
| `v_if (v < 0.0f) {...} v_endif` | Conditional lane execution for negative values | `TTI_SFPSETCC(0, lreg, 0)` (set CC for negative), instructions execute, `TTI_SFPENCC(0, 0)` (clear CC) |
| `Converter::as_float(param0)` | Convert uint32 FP16_B to vFloat | `TTI_SFPLOADI(lreg, 0 /*FP16_B*/, param >> 16)` |
| `_calculate_exponential_body_<APPROX>(x)` | Compute exp(x) | `TTI_SFPNONLINEAR(src_lreg, dest_lreg, p_sfpnonlinear::EXP_MODE)` |
| `v * alpha_recip` | Multiply by scalar | `TTI_SFPMUL(lreg_val, lreg_alpha_recip, p_sfpu::LCONST_0, lreg_dest, 0)` |
| `alpha * (exp_val - 1.0f)` | Subtract 1 then multiply by alpha | `TTI_SFPADD(LCONST_1, lreg_exp, LCONST_neg1, lreg_tmp, 0)` then `TTI_SFPMUL(lreg_alpha, lreg_tmp, LCONST_0, lreg_dest, 0)` |
| `sfpi::vConstFloatPrgm0 = 0.166...f` | Set programmable constant 0 | `_sfpu_load_config32_(dest_idx, upper16, lower16)` or use `TTI_SFPLOADI` to load into LREG |
| `_relu_max_body_(tmp, 1.0f)` | Clamp to [0, threshold] | `TTI_SFPNONLINEAR(src, dest, RELU_MODE)` for max(0,x), then `TTI_SFPGT` + conditional `TTI_SFPMOV` for min(x, threshold) |
| `ActivationType` enum + template dispatch | Select activation at compile time | Separate `_calculate_celu_` / `_calculate_hardsigmoid_` functions |

## Target Expected API

### Integration Points on Quasar

**Parent file**: `tt_llk_quasar/common/inc/ckernel_sfpu.h`
- Currently does NOT include `ckernel_sfpu_activations.h`
- Will need a new `#include "sfpu/ckernel_sfpu_activations.h"` line added

**Test harness**: No test exists for activations on Quasar (`tests/sources/quasar/sfpu_activations_quasar_test.cpp` does not exist, nor does `tests/python_tests/quasar/test_sfpu_activations_quasar.py`).

**Closest existing Quasar kernels and their API patterns**:

For CELU (closest: `ckernel_sfpu_elu.h`):
```cpp
inline void _calculate_celu_sfp_rows_()     // per-row computation
inline void _calculate_celu_(const int iterations, const std::uint32_t alpha, const std::uint32_t alpha_recip)  // main loop with params
```

For Hardsigmoid (closest: `ckernel_sfpu_lrelu.h` for conditional pattern, `ckernel_sfpu_relu.h` + `ckernel_sfpu_threshold.h` for clamping):
```cpp
inline void _init_hardsigmoid_()             // load slope/offset constants
inline void _calculate_hardsigmoid_sfp_rows_()  // per-row computation
inline void _calculate_hardsigmoid_(const int iterations)  // main loop
```

**SfpuType enum**: `tt_llk_quasar/llk_lib/llk_defs.h` currently does NOT have entries for `celu` or `hardsigmoid`. These would need to be added for test infrastructure.

### Function Signatures the Target Expects

Based on existing Quasar SFPU kernel patterns, the expected API is:

1. **CELU** (with params, similar to ELU):
   - `inline void _calculate_celu_sfp_rows_()` -- per-row computation (uses pre-loaded LREGs for alpha/alpha_recip)
   - `inline void _calculate_celu_(const int iterations, const std::uint32_t alpha, const std::uint32_t alpha_recip)` -- main entry point
   - Called via: `_llk_math_eltwise_unary_sfpu_params_<false>(ckernel::sfpu::_calculate_celu_, tile_idx, num_sfpu_iterations, alpha_param, alpha_recip_param)`

2. **Hardsigmoid** (with init, no params):
   - `inline void _init_hardsigmoid_()` -- load slope=1/6 and offset=0.5 into LREGs or constants
   - `inline void _calculate_hardsigmoid_sfp_rows_()` -- per-row computation
   - `inline void _calculate_hardsigmoid_(const int iterations)` -- main entry point
   - Called via: `_llk_math_eltwise_unary_sfpu_params_<false>(ckernel::sfpu::_calculate_hardsigmoid_, tile_idx, num_sfpu_iterations)`

### Template params to KEEP from reference
- None. Quasar SFPU kernels do not use `APPROXIMATION_MODE` template params in the TTI-based implementations (the hardware instructions have fixed precision).

### Template params to DROP (reference-only)
- `APPROXIMATION_MODE` -- not used in Quasar TTI pattern (hardware exp/recip are always approximate)
- `ActivationType` -- replaced by separate named functions
- `ITERATIONS` -- replaced by `const int iterations` runtime parameter

### Template params to ADD (target-only)
- None needed.

### Reference features NOT present in the target (DROP these)
- `ActivationType` enum and template dispatch pattern
- `ActivationImpl` struct template specialization pattern
- `apply_activation` wrapper functions
- SFPI intrinsics (vFloat, v_if/v_endif, dst_reg, Converter)
- `_calculate_exponential_body_` (use TTI_SFPNONLINEAR directly)
- `_relu_max_body_` (decompose into TTI primitives)

## Format Support

### Format Domain
**Float-only**

Both CELU and Hardsigmoid are floating-point activation functions. CELU uses exponential (exp), and Hardsigmoid uses multiply-add with clamping. Neither is defined for integer inputs.

### Applicable Formats for Testing
Based on the operation type, target architecture support, and existing test patterns:

| Format | Applicable | Rationale |
|--------|-----------|-----------|
| Float16 | Yes | Fully supported; used in existing SFPU tests (e.g., ELU, nonlinear) |
| Float16_b | Yes | Primary format; most common for SFPU kernels on Quasar |
| Float32 | Yes | Supported with dest_acc_en=true; used in existing SFPU tests |
| Int32 | No | Integer format; activation functions are float-only |
| Int16 | No | Integer format; not applicable |
| Int8 | No | Integer format; not applicable |
| UInt8 | No | Integer format; not applicable |
| MxFp8R | No (initially) | L1-only format, unpacked to Float16_b. Exclude from initial testing for simplicity; can be added later |
| MxFp8P | No (initially) | Same as MxFp8R; exclude initially |

### Format-Dependent Code Paths
The reference kernel is format-agnostic. All format handling is done transparently by the SFPU load/store EXU (converting to/from FP32 internally). There are no format-dependent branches in the activation logic itself.

### Format Constraints
- **Float32**: Requires `dest_acc_en = DestAccumulation.On` (32-bit Dest mode) since Float32 uses 32-bit containers in Dest
- **MxFp8R/MxFp8P**: Require `implied_math_format = ImpliedMathFormat.Yes` (MX formats are L1 storage only, unpacked to Float16_b)
- **Cross-exponent-family conversions**: Float16 (expB) input with Float16_b output (or vice versa) may require `dest_acc_en`
- **Integer and float formats cannot be mixed** in input-to-output
- No operation-specific format constraints beyond the standard SFPU infrastructure rules

## Existing Target Implementations

### Quasar SFPU Kernels (patterns to follow)

The following existing Quasar SFPU kernels provide patterns directly relevant to implementing activations:

1. **`ckernel_sfpu_elu.h`** -- CLOSEST to CELU. Uses:
   - `TTI_SFPLOADI` to load alpha parameter into LREG2
   - `TTI_SFPENCC(1, 2)` / `TTI_SFPENCC(0, 2)` for CC enable/disable bracketing
   - `TTI_SFPSETCC(0, LREG0, 0)` for negative-value lane masking
   - `TTI_SFPNONLINEAR(LREG0, LREG1, EXP_MODE)` for hardware exp
   - `TTI_SFPADD(LCONST_1, LREG1, LCONST_neg1, LREG1, 0)` for exp(x)-1
   - `TTI_SFPMUL` for alpha multiplication
   - Standard `_incr_counters_` loop

2. **`ckernel_sfpu_lrelu.h`** -- Pattern for conditional multiply. Uses:
   - `TTI_SFPSETCC` for negative detection
   - `TTI_SFPMAD` for conditional multiply-add
   - CC bracketing

3. **`ckernel_sfpu_lrelu.h` (_calculate_relu_max_)** -- CLOSEST to Hardsigmoid clamping. Uses:
   - `TTI_SFPGT` for greater-than comparison
   - `TTI_SFPMOV` for conditional value replacement
   - `TTI_SFPSETCC` for negative detection (ReLU part)
   - `TTI_SFPLOADI` with 0 for zeroing

4. **`ckernel_sfpu_threshold.h`** -- Pattern for comparison + conditional move. Uses:
   - `TTI_SFPGT` for threshold comparison
   - `TTI_SFPMOV` for conditional replacement

5. **`ckernel_sfpu_sigmoid.h`** -- Pattern for multi-step computation chains. Uses:
   - Multiple TTI instructions chained (negate -> exp -> add -> recip)
   - Helper function `_calculate_sigmoid_regs_` for reuse

### Key Quasar Constants Available
- `p_sfpu::LCONST_0` = 9 (value 0.0)
- `p_sfpu::LCONST_1` = 10 (value 1.0)
- `p_sfpu::LCONST_neg1` = 11 (value -1.0)

## Sub-Kernel Phases

The kernel contains two independent activation types with different parameter patterns. Each should be implemented as a separate phase.

| Phase | Name | Functions | Dependencies |
|-------|------|-----------|--------------|
| 1 | CELU | `_calculate_celu_sfp_rows_()`, `_calculate_celu_(iterations, alpha, alpha_recip)` | none |
| 2 | Hardsigmoid | `_init_hardsigmoid_()`, `_calculate_hardsigmoid_sfp_rows_()`, `_calculate_hardsigmoid_(iterations)` | none |

**Ordering rationale**:
- **Phase 1 (CELU) first**: Very similar to the existing ELU kernel on Quasar. The main difference is an extra multiply by alpha_recip before exp and taking two params (alpha + alpha_recip) instead of one. Low risk, leverages known-working ELU pattern.
- **Phase 2 (Hardsigmoid) second**: Requires a novel clamping-to-[0,1] pattern that combines multiply-add with two-sided clamping (max(0, min(x, 1))). More complex than CELU because the clamp pattern needs SFPGT + conditional move + SFPSETCC + zeroing, which is a more intricate CC manipulation sequence.

**No mutual dependencies**: CELU and Hardsigmoid are independent activation types. Neither calls functions from the other phase.
