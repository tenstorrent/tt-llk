# Analysis: gelu

## Kernel Type
SFPU

## Reference File
`tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_gelu.h`

## Purpose
Implements the Gaussian Error Linear Unit (GELU) activation function, which is defined as:
- **GELU(x) = x * CDF(x)** where CDF is the cumulative distribution function of the standard normal distribution

The GELU function provides a smooth, non-linear activation that has become popular in transformer architectures. The kernel also includes GELU derivative calculation for backpropagation.

## Algorithm Summary

### GELU Calculation
Two modes are supported based on `APPROXIMATION_MODE`:

1. **Approximation Mode (`APPROXIMATION_MODE=true`)**:
   - Uses a 6-piece LUT (Look-Up Table) approach via `lut2_sign()`
   - Formula: `result = 0.5 * x + lut2_sign(x)`
   - LUT coefficients stored in LREG0-2 and LREG4-6
   - Provides piecewise linear approximation with sign handling

2. **Accurate Mode (`APPROXIMATION_MODE=false`)**:
   - Uses CDF approximation: `_calculate_cdf_appx_(in, scaled=true)`
   - CDF uses polynomial approximation for x in ranges [0,2.5) and [2.5,5]
   - Handles negative values by computing `1 - CDF(-x)`

### GELU Derivative Calculation
Two modes are supported:

1. **Approximation Mode**:
   - Uses 6-piece LUT via `lut2()` with `lut_mode=1`
   - Piecewise linear model for different x ranges
   - Adds 1.0 to negative results

2. **Accurate Mode**:
   - Computes: `partial = exp(-0.5*x^2) * x * (1/sqrt(2*pi))`
   - Combines with LUT result and adds 0.5
   - Uses `_calculate_exponential_body_<false>()` for precise exponential

## Template Parameters

| Parameter | Purpose | Values |
|-----------|---------|--------|
| `APPROXIMATION_MODE` | Controls precision vs speed tradeoff | `true` (fast), `false` (accurate) |
| `ITERATIONS` | Number of elements to process per call | Typically 8 (unrolled) |

## Functions Identified

| Function | Purpose | Complexity |
|----------|---------|------------|
| `_calculate_gelu_<APPROXIMATION_MODE, ITERATIONS>()` | Main entry point - dispatches to appx or accurate | Low |
| `_calculate_gelu_appx_<ITERATIONS>()` | Approximation mode implementation | Medium |
| `_calculate_gelu_accurate_<ITERATIONS>()` | Accurate mode implementation | Medium |
| `_calculate_gelu_core_<APPROXIMATION_MODE>()` | Core GELU helper (returns intermediate result) | Low |
| `_calculate_gelu_derivative_<APPROXIMATION_MODE, ITERATIONS>()` | GELU derivative for backprop | High |
| `_init_gelu_<APPROXIMATION_MODE>()` | Initialize LUT coefficients for GELU | Medium |
| `_init_gelu_derivative_<APPROXIMATION_MODE>()` | Initialize LUT coefficients for derivative | Medium |

## Key Constructs Used

### SFPI Constructs
- [x] `sfpi::dst_reg[n]` - Read/write destination registers
- [x] `sfpi::dst_reg++` - Increment destination register pointer
- [x] `sfpi::l_reg[LRegs::LReg*]` - Local register access (LReg0-6)
- [x] `sfpi::vFloat` - Vector float type
- [x] `sfpi::vUInt` - Vector unsigned integer type
- [x] `sfpi::vConstFloatPrgm0` - Programmable constant (0.5f)
- [x] `sfpi::s2vFloat16b()` - Scalar to vector float16b conversion
- [x] `v_if` / `v_endif` - Conditional vector operations
- [x] `v_else` / `v_elseif` - Conditional branches

### Built-in Functions
- [x] `lut2_sign()` - 6-entry LUT with sign handling (approximation mode)
- [x] `lut2()` - 6-entry LUT lookup
- [x] `lut()` - 3-entry LUT lookup (used in derivative accurate mode)
- [x] `_sfpu_load_imm32_()` - Load 32-bit immediate to local register
- [x] `_sfpu_load_imm16_()` - Load 16-bit immediate to local register

### Dependencies on Other Kernels
- [x] `_calculate_cdf_appx_()` from `ckernel_sfpu_cdf.h` - CDF approximation
- [x] `_calculate_exponential_body_<false>()` from `ckernel_sfpu_exp.h` - Exponential calculation
- [x] `_init_exponential_<false, false, 0x3F800000>()` - Exponential initialization
- [x] `POLYVAL5()` from `ckernel_sfpu_polyval.h` - Polynomial evaluation (used in CDF)

### Loop Constructs
- [x] `#pragma GCC unroll 8` - Unroll loop 8 times (approximation)
- [x] `#pragma GCC unroll 0` - Disable unrolling (derivative)

## Dependencies

### Header Files
```cpp
#include "ckernel_sfpu_cdf.h"        // _calculate_cdf_appx_()
#include "ckernel_sfpu_exp.h"        // _calculate_exponential_body_(), _init_exponential_()
#include "ckernel_sfpu_load_config.h" // _sfpu_load_imm32_(), _sfpu_load_imm16_()
#include "sfpi.h"                    // SFPI types and operations
#include "sfpi_fp16.h"               // FP16 support
```

### Transitive Dependencies
- `ckernel_sfpu_cdf.h` depends on:
  - `ckernel.h`
  - `ckernel_defs.h`
  - `ckernel_sfpu_polyval.h`
  - `sfpi.h`

- `ckernel_sfpu_exp.h` depends on:
  - `ckernel_addrmod.h`
  - `ckernel_ops.h`
  - `ckernel_sfpu_recip.h`
  - `lltt.h`

## LUT Coefficient Analysis

### GELU Initialization (`_init_gelu_`)
Piecewise linear approximation coefficients for 6 ranges:

| Range | LREG | Slope | Intercept |
|-------|------|-------|-----------|
| [0, 0.5) | LREG0_lo, LREG4_lo | 0.1928 | -0.0150 |
| [0.5, 1.0) | LREG0_hi, LREG4_hi | 0.4939 | -0.1605 |
| [1.0, 1.5) | LREG1_lo, LREG5_lo | 0.6189 | -0.2797 |
| [1.5, 2.0) | LREG1_hi, LREG5_hi | 0.6099 | -0.2635 |
| [2.0, 3.0) | LREG2_lo, LREG6_lo | 0.5402 | -0.1194 |
| >= 3.0 | LREG2_hi, LREG6_hi | 0.50 | 0.0 |

### GELU Derivative Initialization (`_init_gelu_derivative_`)

**Approximation Mode** - Piecewise linear for `gelu'(x)`:

| Range | A (slope) | B (intercept) |
|-------|-----------|---------------|
| x <= 0.5 | 0.8 | 0.5 |
| x <= 1.0 | 0.4 | 0.7 |
| x <= 1.5 | 0.1 | 0.99 |
| x <= 2.0 | -0.09 | 1.27 |
| x <= 3.0 | -0.075 | 1.235 |
| x > 3.0 | 0 | 1.0 |

**Accurate Mode**:
- Uses 3-entry LUT with `imm2 = 0xFF10`
- Combines with Gaussian: `exp(-x^2/2) * x / sqrt(2*pi)`

## Quasar Translation Notes

### Architecture Differences
1. **No SFPI Library**: Quasar uses TTI instructions directly instead of SFPI C++ wrapper
2. **Different Register Model**: Quasar uses explicit TTI_SFPLOAD/TTI_SFPSTORE with LREG naming
3. **No lut/lut2 Functions**: Quasar has SFPNONLINEAR for built-in functions (EXP, RECIP, TANH)
4. **No dst_reg++**: Quasar uses `_incr_counters_<>()` to advance dest pointer

### Translation Challenges

1. **LUT Operations**:
   - BH uses `lut()`, `lut2()`, `lut2_sign()` with L-registers
   - Quasar may need to implement equivalent using SFPMAD chains or SFPNONLINEAR
   - Consider using polynomial approximation instead of LUT

2. **CDF Approximation**:
   - BH uses `POLYVAL5()` polynomial evaluation
   - Quasar can implement using SFPMUL/SFPADD chains (Horner's method)
   - `v_if`/`v_endif` must be translated to conditional moves or separate execution paths

3. **Exponential Dependency**:
   - BH uses complex `_calculate_exponential_body_<false>()`
   - Quasar has built-in `SFPNONLINEAR(EXP_MODE)` - much simpler

4. **Iteration Pattern**:
   - BH: `dst_reg++` in loop
   - Quasar: `_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>()`

### Recommended Approach for Quasar

**For Approximation Mode**:
1. Use simpler polynomial approximation for GELU: `x * sigmoid(1.702 * x)`
2. Or tanh-based: `0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715*x^3)))`
3. Leverage existing Quasar `SFPNONLINEAR(EXP_MODE)` and `SFPNONLINEAR(RECIP_MODE)`

**For Accurate Mode**:
1. Implement CDF polynomial using SFPMUL/SFPADD chains
2. Use SFPNONLINEAR for exponential computations

### Complexity Assessment
- **GELU Approximation**: Medium (needs sigmoid or tanh chain)
- **GELU Accurate**: High (polynomial evaluation + conditionals)
- **GELU Derivative**: High (multiple math operations)

Recommendation: Start with approximation mode using sigmoid-based formula as it maps well to existing Quasar primitives.

## Existing Quasar Implementation
**No** - `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_gelu.h` does not exist.

### Related Quasar Implementations Available
- `ckernel_sfpu_sigmoid.h` - Can be used as reference for structure
- `ckernel_sfpu_exp.h` - Uses SFPNONLINEAR(EXP_MODE)
- `ckernel_sfpu_tanh.h` - Uses SFPNONLINEAR(TANH_MODE)
- `ckernel_sfpu_recip.h` - Uses SFPNONLINEAR(RECIP_MODE)

These existing implementations show the pattern:
```cpp
// Load from dest
TTI_SFPLOAD(LREG0, ...);

// Compute using SFPNONLINEAR or arithmetic
TTI_SFPNONLINEAR(input_reg, output_reg, mode);

// Store to dest
TTI_SFPSTORE(result_reg, ...);

// Increment counters
_incr_counters_<...>();
```

## Summary Statistics

| Metric | Value |
|--------|-------|
| Functions | 7 |
| Template Parameters | 2 (APPROXIMATION_MODE, ITERATIONS) |
| Dependencies | 5 headers |
| LUT Entries | 6 per approximation |
| Complexity | High |
| Quasar Status | Not implemented |
