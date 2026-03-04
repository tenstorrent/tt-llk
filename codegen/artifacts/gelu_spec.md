# Specification: gelu

## Kernel Type
SFPU

## Overview
Based on analysis: `codegen/artifacts/gelu_analysis.md`

The GELU (Gaussian Error Linear Unit) activation function is defined as:
- **GELU(x) = x * CDF(x)** where CDF is the standard normal cumulative distribution function

For Quasar, we will implement GELU using the sigmoid-based approximation:
- **GELU(x) = x * sigmoid(1.702 * x)**

This approach is recommended because:
1. Quasar has hardware support for both exp() and reciprocal() via SFPNONLINEAR
2. sigmoid(z) = 1 / (1 + exp(-z)) maps directly to existing Quasar primitives
3. The sigmoid-based approximation is accurate and efficient
4. Avoids complex LUT-based implementations from Blackhole

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_gelu.h`

## Algorithm in Quasar

### Mathematical Formula
```
GELU(x) = x * sigmoid(1.702 * x)
        = x * (1 / (1 + exp(-1.702 * x)))
```

### Pseudocode
1. Load input x from dest register
2. Scale x: compute z = 1.702 * x
3. Negate: compute -z
4. Compute exp(-z)
5. Add 1: compute (1 + exp(-z))
6. Compute reciprocal: sigmoid = 1 / (1 + exp(-z))
7. Multiply: result = x * sigmoid
8. Store result to dest register
9. Increment counters

### Key Instruction Mappings
| BH Construct | Quasar Instruction |
|--------------|-------------------|
| `dst_reg[0]` load | `TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0)` |
| `dst_reg[0] = val` | `TTI_SFPSTORE(p_sfpu::LREGn, 0, ADDR_MOD_7, 0, 0)` |
| `dst_reg++` | `ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>()` |
| `_sfpu_exp_(x)` | `TTI_SFPNONLINEAR(src, dest, p_sfpnonlinear::EXP_MODE)` |
| `_sfpu_reciprocal_(x)` | `TTI_SFPNONLINEAR(src, dest, p_sfpnonlinear::RECIP_MODE)` |
| `x * scalar` | `TTI_SFPMUL(LREG_x, LREG_scalar, LCONST_0, LREG_dest, 0)` |
| `a + b` | `TTI_SFPADD(LCONST_1, LREG_a, LREG_b, LREG_dest, 0)` |
| `lut2_sign()` / CDF | Replaced with sigmoid approximation |

### Resource Allocation
| Resource | Purpose |
|----------|---------|
| `LREG0` | Input x (loaded from dest) |
| `LREG1` | Intermediate: z = 1.702 * x |
| `LREG2` | Intermediate: -z |
| `LREG3` | Intermediate: exp(-z) |
| `LREG4` | Intermediate: 1 + exp(-z) |
| `LREG5` | Intermediate: sigmoid = 1/(1+exp(-z)) |
| `LREG6` | Constant: 1.702 (loaded in init) |
| `LREG7` | Constant: 1.0f (loaded in init) |

## Implementation Structure

### Includes
```cpp
#pragma once

#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```

### Namespace
```cpp
namespace ckernel
{
namespace sfpu
{
    // GELU kernel functions
} // namespace sfpu
} // namespace ckernel
```

### Functions
| Function | Template Params | Purpose |
|----------|-----------------|---------|
| `_calculate_gelu_sfp_rows_<APPROXIMATION_MODE>()` | `APPROXIMATION_MODE` | Process 2 rows of SFPU output |
| `_calculate_gelu_<APPROXIMATION_MODE>(iterations)` | `APPROXIMATION_MODE` | Main entry - loop over iterations |
| `_init_gelu_<APPROXIMATION_MODE>()` | `APPROXIMATION_MODE` | Load constants (1.702, 1.0) |

## Instruction Sequence

### Init Function
```cpp
template <bool APPROXIMATION_MODE>
inline void _init_gelu_()
{
    // Load constant 1.702f into LREG6
    // 1.702 in bfloat16 = 0x3FDA (approximately)
    // 1.702 = 1.702 * 2^0 = 0x3FDA (sign=0, exp=127, mantissa=0xDA>>1)
    // More precise: 1.702 in IEEE754 float = 0x3FD9FBE7
    // bfloat16 (upper 16 bits) = 0x3FDA
    TTI_SFPLOADI(p_sfpu::LREG6, 0 /*Float16_b*/, 0x3FDA);

    // Load constant 1.0f into LREG7
    // 1.0 in bfloat16 = 0x3F80
    TTI_SFPLOADI(p_sfpu::LREG7, 0 /*Float16_b*/, 0x3F80);
}
```

### Main Function
```cpp
// Calculates GELU for 2 rows of SFPU output
// GELU(x) = x * sigmoid(1.702 * x)
template <bool APPROXIMATION_MODE>
inline void _calculate_gelu_sfp_rows_()
{
    // Step 1: Load x from dest into LREG0
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

    if constexpr (APPROXIMATION_MODE)
    {
        // Step 2: Compute z = 1.702 * x -> LREG1
        // LREG6 contains 1.702 (from init)
        TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG6, p_sfpu::LCONST_0, p_sfpu::LREG1, 0);

        // Step 3: Negate z -> LREG2 = -z
        TTI_SFPMUL(p_sfpu::LREG1, p_sfpu::LCONST_neg1, p_sfpu::LCONST_0, p_sfpu::LREG2, 0);

        // Step 4: Compute exp(-z) -> LREG3
        TTI_SFPNONLINEAR(p_sfpu::LREG2, p_sfpu::LREG3, p_sfpnonlinear::EXP_MODE);

        // Step 5: Compute 1 + exp(-z) -> LREG4
        // LREG7 contains 1.0 (from init)
        TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG3, p_sfpu::LREG7, p_sfpu::LREG4, 0);

        // Step 6: Compute sigmoid = 1/(1+exp(-z)) -> LREG5
        TTI_SFPNONLINEAR(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpnonlinear::RECIP_MODE);

        // Step 7: Compute result = x * sigmoid -> LREG1
        TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG5, p_sfpu::LCONST_0, p_sfpu::LREG1, 0);
    }

    // Step 8: Store result from LREG1 into dest register
    TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0);
}

template <bool APPROXIMATION_MODE>
inline void _calculate_gelu_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_gelu_sfp_rows_<APPROXIMATION_MODE>();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
}
```

## GELU Derivative (Optional - Future Extension)

The GELU derivative is more complex and may be implemented separately:
```
GELU'(x) = sigmoid(1.702*x) + x * sigmoid(1.702*x) * (1 - sigmoid(1.702*x)) * 1.702
         = sigmoid(1.702*x) * (1 + 1.702*x*(1 - sigmoid(1.702*x)))
```

For the initial implementation, we focus on forward GELU only. The derivative can be added later following the same patterns.

## Constants Reference

| Constant | Float Value | bfloat16 Hex | Usage |
|----------|-------------|--------------|-------|
| 1.0 | 1.0f | 0x3F80 | Adding 1 to exp result |
| 1.702 | 1.702f | 0x3FDA | GELU approximation scale factor |
| -1.0 | -1.0f | 0xBF80 | Negation (use LCONST_neg1) |
| 0.5 | 0.5f | 0x3F00 | (not needed for sigmoid approach) |

## Potential Issues

1. **Precision of 1.702 constant**: The bfloat16 representation 0x3FDA is approximately 1.703125, which is close enough for the approximation mode.

2. **Large input values**: For very large |x|, the sigmoid approaches 0 or 1:
   - For large positive x: GELU(x) approaches x
   - For large negative x: GELU(x) approaches 0
   - The SFPNONLINEAR instructions should handle these edge cases in hardware.

3. **Register pressure**: We use 8 local registers (LREG0-7). This is within the limit but leaves no spare registers. If derivative is added, consider optimizing register reuse.

4. **Non-approximation mode**: Currently only APPROXIMATION_MODE=true is implemented. For higher accuracy, a polynomial-based approach could be added in the else branch.

## Testing Notes

1. **Compilation check**: Run `scripts/check_compile.py` after implementation
   ```bash
   cd codegen
   source ../tests/.venv/bin/activate
   PYTHONPATH=.. python scripts/check_compile.py ../tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_gelu.h -v
   ```

2. **Functional verification**: Compare output against reference implementation:
   - Test inputs: [-3.0, -1.0, -0.5, 0.0, 0.5, 1.0, 3.0]
   - Expected GELU outputs (approximate):
     - GELU(-3.0) ~ -0.004
     - GELU(-1.0) ~ -0.159
     - GELU(-0.5) ~ -0.154
     - GELU(0.0) = 0.0
     - GELU(0.5) ~ 0.346
     - GELU(1.0) ~ 0.841
     - GELU(3.0) ~ 2.996

3. **Edge cases to verify**:
   - Zero input: should return 0
   - Large positive: should return approximately x
   - Large negative: should return approximately 0
   - Subnormal numbers

## Design Decisions

1. **Sigmoid-based approximation**: Chosen over LUT-based or CDF-based approaches because:
   - Maps directly to Quasar SFPNONLINEAR (EXP, RECIP)
   - Simpler implementation than porting Blackhole's LUT system
   - Good balance of accuracy and performance

2. **No derivative in initial implementation**: GELU derivative is complex and can be added as a separate function later. The forward pass is the priority.

3. **Template structure**: Follows the established Quasar pattern with `APPROXIMATION_MODE` template parameter for consistency with other SFPU kernels.
