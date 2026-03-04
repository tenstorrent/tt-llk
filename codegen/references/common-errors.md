# Common Compilation Errors and Fixes

This document catalogs common errors encountered when writing Quasar SFPU kernels and how to fix them.

## Running Compilation Check

```bash
cd codegen
source ../tests/.venv/bin/activate
PYTHONPATH=.. python scripts/check_compile.py ../tt_llk_quasar/common/inc/sfpu/your_kernel.h
PYTHONPATH=.. python scripts/check_compile.py your_kernel.h -v  # verbose output
```

---

## Error Categories

### 1. Undefined TTI Instruction

**Error Message**:
```
error: 'TTI_SFPLOAD_WRONG' was not declared in this scope; did you mean 'TTI_SFPLOADMACRO'?
```

**Cause**: Typo in instruction name or using non-existent instruction.

**Fix**: Check the correct instruction name in `ckernel_ops.h` or `quasar-architecture.md`.

| Wrong | Correct |
|-------|---------|
| `TTI_SFPLOAD_WRONG` | `TTI_SFPLOAD` |
| `TTI_SFPARECIP` | `TTI_SFPNONLINEAR` (with RECIP_MODE) |
| `TTI_SFPEXP` | `TTI_SFPNONLINEAR` (with EXP_MODE) |

**Available TTI instructions**:
- `TTI_SFPLOAD`, `TTI_SFPLOADI`, `TTI_SFPSTORE`
- `TTI_SFPMAD`, `TTI_SFPADD`, `TTI_SFPMUL`
- `TTI_SFPNONLINEAR` (for RELU, EXP, RECIP, SQRT, TANH)
- `TTI_SFPSETCC`, `TTI_SFPENCC`, `TTI_SFPCOMPC`

---

### 2. Undefined Constant

**Error Message**:
```
error: 'LCONST_2' is not a member of 'ckernel::p_sfpu'
```

**Cause**: Using a constant that doesn't exist.

**Fix**: Only these constants are available in `p_sfpu::`:

| Constant | Value | Description |
|----------|-------|-------------|
| `LREG0` - `LREG7` | 0-7 | Local registers |
| `LCONST_0` | 9 | Constant 0.0 |
| `LCONST_1` | 10 | Constant 1.0 |
| `LCONST_neg1` | 11 | Constant -1.0 |

**For other constants**, load them with `TTI_SFPLOADI`:
```cpp
// Load 2.0f (bfloat16 = 0x4000) into LREG3
TTI_SFPLOADI(p_sfpu::LREG3, 0, 0x4000);
```

---

### 3. Wrong Namespace

**Error Message**:
```
error: 'LREG0' is not a member of 'ckernel::sfpu'
```

**Cause**: Using `sfpu::LREG0` instead of `p_sfpu::LREG0`.

**Fix**: Use correct namespace prefixes:

| Wrong | Correct |
|-------|---------|
| `sfpu::LREG0` | `p_sfpu::LREG0` |
| `sfpu::LCONST_1` | `p_sfpu::LCONST_1` |
| `ckernel::RELU_MODE` | `p_sfpnonlinear::RELU_MODE` |

**Namespace reference**:
- `p_sfpu::` - SFPU register constants (LREG0-7, LCONST_*, sfpmem::)
- `p_sfpnonlinear::` - Nonlinear operation modes (RELU_MODE, EXP_MODE, etc.)
- `ckernel::math::` - Math utilities (_incr_counters_, SFP_ROWS)

---

### 4. Missing Namespace Qualifier

**Error Message**:
```
error: '_incr_counters_' was not declared in this scope;
did you mean 'ckernel::math::_incr_counters_'?
```

**Cause**: Missing namespace prefix for math utilities.

**Fix**: Add the full namespace:

```cpp
// Wrong
_incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>();

// Correct
ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
```

---

### 5. Missing Include

**Error Message**:
```
error: 'ADDR_MOD_7' was not declared in this scope
```

**Cause**: Missing required include file.

**Fix**: Ensure these includes are at the top of your file:
```cpp
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```

---

### 6. Wrong Template Parameter

**Error Message**:
```
error: expected primary-expression before '>' token
```

**Cause**: Using wrong syntax for template function call.

**Fix**: Ensure template syntax is correct:
```cpp
// Wrong
_calculate_sigmoid_<APPROXIMATION_MODE>(iterations);

// Also wrong (if inside template function that doesn't pass template param)
_calculate_sigmoid_(iterations);

// Correct
_calculate_sigmoid_<APPROXIMATION_MODE>(iterations);
```

---

### 7. Wrong Argument Count

**Error Message**:
```
error: too many arguments to function 'TTI_SFPNONLINEAR'
```

**Cause**: Passing wrong number of arguments to instruction.

**Fix**: Check instruction signature:

```cpp
// SFPLOAD takes 5 arguments
TTI_SFPLOAD(lreg_ind, instr_mod0, sfpu_addr_mode, dest_reg_addr, done);

// SFPNONLINEAR takes 3 arguments
TTI_SFPNONLINEAR(lreg_src, lreg_dest, mode);

// SFPMAD takes 5 arguments
TTI_SFPMAD(lreg_src_a, lreg_src_b, lreg_src_c, lreg_dest, instr_mod1);
```

---

### 8. Wrong Mode for SFPNONLINEAR

**Error Message**:
```
error: 'SIGMOID_MODE' is not a member of 'ckernel::p_sfpnonlinear'
```

**Cause**: There is no direct SIGMOID_MODE - sigmoid must be composed from other operations.

**Fix**: Available modes are:
```cpp
p_sfpnonlinear::RECIP_MODE  // 0x0 - Reciprocal (1/x)
p_sfpnonlinear::RELU_MODE   // 0x2 - ReLU (max(0, x))
p_sfpnonlinear::SQRT_MODE   // 0x3 - Square root
p_sfpnonlinear::EXP_MODE    // 0x4 - Exponential (e^x)
p_sfpnonlinear::TANH_MODE   // 0x5 - Hyperbolic tangent
```

For sigmoid, compose: `sigmoid(x) = 1 / (1 + exp(-x))`

---

### 9. Missing Init Function

**Error Message**:
```
error: '_init_sigmoid_' was not declared in this scope
```

**Cause**: Init function referenced but not defined.

**Fix**: Define the init function:
```cpp
template <bool APPROXIMATION_MODE>
inline void _init_sigmoid_()
{
    // Load constants needed for operation
    TTI_SFPLOADI(p_sfpu::LREG3, 0, 0x3F80);  // 1.0f
}
```

**Note**: Init functions are optional. Only define them if you need to preload constants.

---

### 10. Linker Errors

**Error Message**:
```
undefined reference to 'ckernel::math::_incr_counters_...'
```

**Cause**: Header-only functions used incorrectly or missing template instantiation.

**Fix**: Ensure:
1. Function is `inline` or defined in header
2. Template parameters are provided
3. Full namespace is used

---

## Quick Reference: Valid Instruction Syntax

```cpp
// Load from dest
TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

// Load immediate
TTI_SFPLOADI(p_sfpu::LREG3, 0, 0x3F80);  // 1.0f in bfloat16

// Store to dest
TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0);

// Multiply: LREG1 = LREG0 * LCONST_neg1 + 0 = -LREG0
TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LCONST_neg1, p_sfpu::LCONST_0, p_sfpu::LREG1, 0);

// Add: LREG4 = 1 * LREG2 + LREG3
TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpu::LREG4, 0);

// Multiply-add: LREG5 = LREG0 * LREG1 + LREG2
TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LREG2, p_sfpu::LREG5, 0);

// Nonlinear operations
TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::RELU_MODE);
TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE);
TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::RECIP_MODE);

// Increment counter
ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
```

---

## Debugging Tips

1. **Start simple**: Get a minimal operation working first (e.g., just load + store)

2. **Check existing examples**: Compare with working kernels in `tt_llk_quasar/common/inc/sfpu/`

3. **Use verbose mode**: `check_compile.py -v` shows full compiler output

4. **One change at a time**: When fixing errors, change one thing and recompile

5. **Read the suggestions**: Compiler often suggests correct name (e.g., "did you mean...?")
