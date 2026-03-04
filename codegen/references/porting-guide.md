# Blackhole → Quasar SFPU Porting Guide

This guide provides translation patterns for porting SFPU kernels from Blackhole to Quasar.

## Overview

| Aspect | Blackhole | Quasar |
|--------|-----------|--------|
| **API** | `sfpi::` C++ library | `TTI_*` direct instructions |
| **Style** | High-level, operator overloading | Low-level, explicit instructions |
| **Complexity** | More features, abstractions | Simpler, fewer instructions |

---

## Include Files

**Blackhole**:
```cpp
#include "sfpi.h"
#include "sfpi_fp16.h"
#include "ckernel_sfpu_load_config.h"
```

**Quasar**:
```cpp
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```

---

## Load from Dest Register

**Blackhole**:
```cpp
sfpi::vFloat val = sfpi::dst_reg[0];
```

**Quasar**:
```cpp
TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
// Value is now in LREG0
```

---

## Store to Dest Register

**Blackhole**:
```cpp
sfpi::dst_reg[0] = result;
```

**Quasar**:
```cpp
// Assuming result is in LREG1
TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0);
```

---

## Increment Dest Pointer

**Blackhole**:
```cpp
sfpi::dst_reg++;
```

**Quasar**:
```cpp
ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
```

---

## Constants

**Blackhole** - C++ literals:
```cpp
sfpi::vFloat one = 1.0f;
sfpi::vFloat neg = -1.0f;
sfpi::vFloat zero = 0.0f;
```

**Quasar** - Built-in constants:
```cpp
// Use p_sfpu:: constants directly in instructions
p_sfpu::LCONST_0    // 0.0
p_sfpu::LCONST_1    // 1.0
p_sfpu::LCONST_neg1 // -1.0
```

**Quasar** - Load custom constants:
```cpp
// Load 1.0f (bfloat16 = 0x3F80) into LREG3
TTI_SFPLOADI(p_sfpu::LREG3, 0 /*Float16_b*/, 0x3F80);
```

---

## Arithmetic Operations

### Negation

**Blackhole**:
```cpp
sfpi::vFloat neg_x = -x;
```

**Quasar**:
```cpp
// LREG1 = LREG0 * (-1) + 0 = -LREG0
TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LCONST_neg1, p_sfpu::LCONST_0, p_sfpu::LREG1, 0);
```

### Addition

**Blackhole**:
```cpp
sfpi::vFloat sum = a + b;
```

**Quasar**:
```cpp
// LREG4 = 1 * LREG2 + LREG3 = LREG2 + LREG3
TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpu::LREG4, 0);
```

### Multiplication

**Blackhole**:
```cpp
sfpi::vFloat prod = a * b;
```

**Quasar**:
```cpp
// LREG4 = LREG0 * LREG1 + 0
TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LCONST_0, p_sfpu::LREG4, 0);
```

### Multiply-Add (FMA)

**Blackhole**:
```cpp
sfpi::vFloat result = a * b + c;
```

**Quasar**:
```cpp
// LREG5 = LREG0 * LREG1 + LREG2
TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LREG2, p_sfpu::LREG5, 0);
```

---

## Nonlinear Operations

**Blackhole** - Uses custom implementations or lut:
```cpp
// ReLU via comparison
v_if (val < 0.0f) { val = 0.0f; } v_endif;

// Exponential via series expansion
val = _sfpu_exp_(val);

// Reciprocal via series
val = _sfpu_reciprocal_<2>(val);
```

**Quasar** - Single-instruction hardware approximations:
```cpp
// ReLU
TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::RELU_MODE);

// Exponential
TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE);

// Reciprocal
TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::RECIP_MODE);

// Square root
TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::SQRT_MODE);

// Tanh
TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::TANH_MODE);
```

---

## Conditional Execution

**Blackhole** - SFPI conditionals:
```cpp
v_if (val < 0.0f)
{
    val = 0.0f;
}
v_endif;
```

**Quasar** - Use C++ constexpr or TTI_SFPSETCC:
```cpp
// Option 1: C++ constexpr (compile-time)
if constexpr (APPROXIMATION_MODE)
{
    // approximation path
}

// Option 2: Use hardware ReLU for max(0, x)
TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::RELU_MODE);

// Option 3: TTI_SFPSETCC for per-lane conditions (advanced)
TTI_SFPSETCC(0, p_sfpu::LREG0, 0, 0);  // Set condition if LREG0 < 0
// ... conditional operations ...
TTI_SFPENCC(0, 0, 0, 0);  // Clear condition
```

---

## Complete Translation Example: Sigmoid

### Blackhole Sigmoid

```cpp
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_sigmoid_(const int iterations)
{
    constexpr int lut_mode = 0;
    sfpi::vUInt l0 = sfpi::l_reg[sfpi::LRegs::LReg0];
    // ... more lreg setup ...

#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat val = sfpi::dst_reg[0];

        // Uses LUT-based approximation
        sfpi::dst_reg[0] = lut2(val, l0, l1, l2, l4, l5, l6, lut_mode) + 0.5f;

        sfpi::dst_reg++;
    }
    // ... restore lregs ...
}
```

### Quasar Sigmoid

```cpp
// Sigmoid(x) = 1 / (1 + exp(-x))
template <bool APPROXIMATION_MODE>
inline void _calculate_sigmoid_sfp_rows_()
{
    // Load x from dest into LREG0
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

    if constexpr (APPROXIMATION_MODE)
    {
        // Step 1: LREG1 = -x
        TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LCONST_neg1, p_sfpu::LCONST_0, p_sfpu::LREG1, 0);

        // Step 2: LREG2 = exp(-x)
        TTI_SFPNONLINEAR(p_sfpu::LREG1, p_sfpu::LREG2, p_sfpnonlinear::EXP_MODE);

        // Step 3: LREG4 = 1 + exp(-x)
        // Note: LREG3 contains 1.0f (loaded in _init_sigmoid_)
        TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpu::LREG4, 0);

        // Step 4: LREG5 = 1 / (1 + exp(-x))
        TTI_SFPNONLINEAR(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpnonlinear::RECIP_MODE);
    }

    // Store result
    TTI_SFPSTORE(p_sfpu::LREG5, 0, ADDR_MOD_7, 0, 0);
}

template <bool APPROXIMATION_MODE>
inline void _calculate_sigmoid_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_sigmoid_sfp_rows_<APPROXIMATION_MODE>();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
}

template <bool APPROXIMATION_MODE>
inline void _init_sigmoid_()
{
    // Load 1.0f into LREG3
    TTI_SFPLOADI(p_sfpu::LREG3, 0, 0x3F80);
}
```

---

## Key Translation Rules

1. **Simplify when possible**: Quasar has hardware support for common nonlinear functions. Don't port complex BH series expansions - use `TTI_SFPNONLINEAR` instead.

2. **Track register usage**: Quasar requires explicit LREG management. Plan which LREGs hold which values.

3. **No operator overloading**: Replace `a + b` with explicit `TTI_SFPADD` calls.

4. **Constants require loading**: Use `TTI_SFPLOADI` for constants beyond LCONST_0/1/neg1.

5. **Always increment counters**: Replace `dst_reg++` with `_incr_counters_<>()`.

6. **Use approximation modes**: Quasar's `TTI_SFPNONLINEAR` provides hardware approximations. BH's software implementations may not be needed.

---

## Common Patterns

### Pattern: Load → Process → Store

```cpp
inline void _calculate_operation_sfp_rows_()
{
    // 1. Load
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

    // 2. Process (one or more operations)
    // ... TTI_SFPNONLINEAR, TTI_SFPMAD, etc. ...

    // 3. Store
    TTI_SFPSTORE(p_sfpu::LREGn, 0, ADDR_MOD_7, 0, 0);
}
```

### Pattern: Iteration Loop

```cpp
template <bool APPROXIMATION_MODE>
inline void _calculate_operation_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_operation_sfp_rows_<APPROXIMATION_MODE>();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
}
```

### Pattern: Init Function for Constants

```cpp
template <bool APPROXIMATION_MODE>
inline void _init_operation_()
{
    // Load any constants needed for the operation
    TTI_SFPLOADI(p_sfpu::LREG3, 0, 0x3F80);  // 1.0f in bfloat16
}
```

---

## Existing Quasar Examples

Use these as reference implementations:

| Operation | File |
|-----------|------|
| ReLU | `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_relu.h` |
| Exp | `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_exp.h` |
| Recip | `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_recip.h` |
| Sqrt | `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_sqrt.h` |
| Tanh | `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_tanh.h` |
| Sigmoid | `tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_sigmoid.h` |
