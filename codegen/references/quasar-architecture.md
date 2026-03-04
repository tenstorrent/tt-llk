# Quasar SFPU Architecture Reference

This document provides the essential architecture knowledge needed to write Quasar SFPU kernels.

## Overview

Quasar's Vector Engine (SFPU) has:
- **32 SFPU lanes** organized in 4x8 grid
- **FP32 precision** internally
- **8 Local Registers** (LREG0-LREG7) per lane
- **3 constant registers** (LCONST_0, LCONST_1, LCONST_neg1)
- **Conditional execution** per lane via condition codes

## Required Includes

```cpp
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```

## Namespace Structure

```cpp
namespace ckernel
{
namespace sfpu
{
    // Your SFPU kernel functions here
} // namespace sfpu
} // namespace ckernel
```

---

## Core Instructions

### TTI_SFPLOAD - Load from Dest to LREG

Loads data from the destination register file into an LREG.

```cpp
TTI_SFPLOAD(lreg_ind, instr_mod0, sfpu_addr_mode, dest_reg_addr, done)
```

**Parameters**:
| Parameter | Description | Common Values |
|-----------|-------------|---------------|
| `lreg_ind` | Target LREG (0-7) | `p_sfpu::LREG0` |
| `instr_mod0` | Data format | `p_sfpu::sfpmem::DEFAULT`, `FP32`, `FP16B` |
| `sfpu_addr_mode` | Address mode register | `ADDR_MOD_7` (all zeros) |
| `dest_reg_addr` | Dest register address | `0` |
| `done` | Bank switch flag | `0` |

**Example**:
```cpp
// Load from dest into LREG0 using default format
TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
```

**Format modes** (`p_sfpu::sfpmem::`):
- `DEFAULT` (0b0000) - Format from config register
- `FP16A` (0b0001) - FP16 format
- `FP16B` (0b0010) - BFloat16 format
- `FP32` (0b0011) - FP32 format
- `INT32` (0b0100) - INT32 format
- `UINT8` (0b0101) - UINT8 format
- `UINT16` (0b0110) - UINT16 format

---

### TTI_SFPLOADI - Load Immediate to LREG

Loads an immediate 16-bit value into an LREG.

```cpp
TTI_SFPLOADI(lreg_ind, instr_mod0, imm16)
```

**Example**:
```cpp
// Load 1.0f (bfloat16 = 0x3F80) into LREG3
TTI_SFPLOADI(p_sfpu::LREG3, 0 /*Float16_b*/, 0x3F80);
```

---

### TTI_SFPSTORE - Store LREG to Dest

Stores an LREG value back to the destination register file.

```cpp
TTI_SFPSTORE(lreg_ind, done, sfpu_addr_mode, dest_reg_addr, instr_mod0)
```

**Example**:
```cpp
// Store LREG1 to dest
TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0);
```

---

### TTI_SFPNONLINEAR - Nonlinear Operations

Single-cycle hardware approximations for common nonlinear functions.

```cpp
TTI_SFPNONLINEAR(lreg_src, lreg_dest, mode)
```

**Modes** (`p_sfpnonlinear::`):
| Mode | Value | Operation |
|------|-------|-----------|
| `RECIP_MODE` | 0x0 | Reciprocal (1/x) |
| `RELU_MODE` | 0x2 | ReLU (max(0, x)) |
| `SQRT_MODE` | 0x3 | Square root |
| `EXP_MODE` | 0x4 | Exponential (e^x) |
| `TANH_MODE` | 0x5 | Hyperbolic tangent |

**Examples**:
```cpp
// ReLU: LREG0 -> LREG1
TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::RELU_MODE);

// Exponential: LREG0 -> LREG1
TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE);

// Reciprocal: LREG0 -> LREG1
TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::RECIP_MODE);
```

---

### TTI_SFPMAD - Multiply-Add

Computes `dest = src_a * src_b + src_c` (fused multiply-add).

```cpp
TTI_SFPMAD(lreg_src_a, lreg_src_b, lreg_src_c, lreg_dest, instr_mod1)
```

**Parameters**:
- `lreg_src_a`: First multiplicand (can be LREG or LCONST)
- `lreg_src_b`: Second multiplicand (can be LREG or LCONST)
- `lreg_src_c`: Addend (can be LREG or LCONST)
- `lreg_dest`: Destination LREG
- `instr_mod1`: Modifier (usually 0)

**Example**:
```cpp
// LREG4 = LREG0 * LREG1 + LREG2
TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LREG2, p_sfpu::LREG4, 0);
```

---

### TTI_SFPMUL - Multiply

Computes `dest = src_a * src_b + 0` (multiply only).

```cpp
TTI_SFPMUL(lreg_src_a, lreg_src_b, lconst_0, lreg_dest, instr_mod1)
```

**Example**:
```cpp
// LREG1 = LREG0 * (-1) = -LREG0
TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LCONST_neg1, p_sfpu::LCONST_0, p_sfpu::LREG1, 0);
```

---

### TTI_SFPADD - Add

Computes `dest = src_a * 1 + src_c` (add only, with multiplier=1).

```cpp
TTI_SFPADD(lconst_1, lreg_src_b, lreg_src_c, lreg_dest, instr_mod1)
```

**Example**:
```cpp
// LREG4 = 1 * LREG2 + LREG3 = LREG2 + LREG3
TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpu::LREG4, 0);
```

---

## Register Constants

Defined in `p_sfpu::` namespace:

| Constant | Value | Description |
|----------|-------|-------------|
| `LREG0` - `LREG7` | 0-7 | Local registers |
| `LCONST_0` | 9 | Constant 0.0 |
| `LCONST_1` | 10 | Constant 1.0 |
| `LCONST_neg1` | 11 | Constant -1.0 |

---

## Iteration Pattern

SFPU operations process 2 rows at a time. Use the `_incr_counters_` template to advance:

```cpp
template <bool APPROXIMATION_MODE>
inline void _calculate_operation_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_operation_sfp_rows_<APPROXIMATION_MODE>();

        // Increment dest counter by SFP_ROWS (2 rows)
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
}
```

---

## Complete Examples

### Example 1: ReLU (Simple)

```cpp
#pragma once
#include "ckernel_trisc_common.h"
#include "cmath_common.h"

namespace ckernel
{
namespace sfpu
{

inline void _calculate_relu_sfp_rows_()
{
    // Load from dest into LREG0
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

    // Apply ReLU: max(0, x)
    TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::RELU_MODE);

    // Store result to dest
    TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0);
}

inline void _calculate_relu_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_relu_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
}

} // namespace sfpu
} // namespace ckernel
```

### Example 2: Sigmoid (Complex)

```cpp
#pragma once
#include "ckernel_trisc_common.h"
#include "cmath_common.h"

namespace ckernel
{
namespace sfpu
{

// Sigmoid(x) = 1 / (1 + exp(-x))
template <bool APPROXIMATION_MODE>
inline void _calculate_sigmoid_sfp_rows_()
{
    // Load x from dest into LREG0
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

    if constexpr (APPROXIMATION_MODE)
    {
        // Step 1: LREG1 = -x (negate x)
        TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LCONST_neg1, p_sfpu::LCONST_0, p_sfpu::LREG1, 0);

        // Step 2: LREG2 = exp(-x)
        TTI_SFPNONLINEAR(p_sfpu::LREG1, p_sfpu::LREG2, p_sfpnonlinear::EXP_MODE);

        // Step 3: LREG4 = 1 + exp(-x)
        // LREG3 holds 1.0f (loaded in _init_sigmoid_)
        TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpu::LREG4, 0);

        // Step 4: LREG5 = 1/(1+exp(-x)) = sigmoid(x)
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
    // Load constant 1.0f into LREG3 (bfloat16 = 0x3F80)
    TTI_SFPLOADI(p_sfpu::LREG3, 0 /*Float16_b*/, 0x3F80);
}

} // namespace sfpu
} // namespace ckernel
```

---

## Querying assembly.yaml

For instruction details not covered here, grep assembly.yaml:

```bash
# Get SFPLOAD details
grep -A 30 "^SFPLOAD:" tt_llk_quasar/instructions/assembly.yaml

# Get all SFPU instructions
grep "^SFP" tt_llk_quasar/instructions/assembly.yaml

# Get specific instruction
grep -A 20 "^SFPSETCC:" tt_llk_quasar/instructions/assembly.yaml
```

---

## Key Differences from Blackhole

| Aspect | Blackhole | Quasar |
|--------|-----------|--------|
| API Style | `sfpi::` C++ library | `TTI_*` direct instructions |
| Load/Store | `dst_reg[0]` | `TTI_SFPLOAD` / `TTI_SFPSTORE` |
| Increment | `dst_reg++` | `_incr_counters_<...>()` |
| Constants | C++ literals | `LCONST_0`, `LCONST_1`, `LCONST_neg1` |
| Includes | `#include "sfpi.h"` | `#include "ckernel_trisc_common.h"` |

See `references/porting-guide.md` for detailed translation patterns.
