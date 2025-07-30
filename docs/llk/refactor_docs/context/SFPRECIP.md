# SFPRECIP - SFPU Reciprocal Function

**Function**: Reciprocal (1/x) computation using Newton-Raphson iteration  
**Instruction Scope**: SFPU (Special Function Processing Unit)  
**Architecture**: Wormhole B0 Tensix  
**Data Flow**: DEST → SFPU → DEST

## Overview

The SFPU reciprocal function computes the multiplicative inverse (1/x) of floating-point numbers using a hardware-accelerated Newton-Raphson iterative algorithm. This function is optimized for the Wormhole B0 SFPU's 32-lane SIMD architecture and provides configurable precision through iteration control.

## Function Signatures

### Core Template Function
```cpp
template <int max_iter = 3>
sfpi_inline sfpi::vFloat _sfpu_reciprocal_(const sfpi::vFloat in);
```

### High-Level Wrapper
```cpp
template <bool APPROXIMATION_MODE, bool is_fp32_dest_acc_en, int ITERATIONS = 8>
inline void calculate_reciprocal();
```

### Initialization Function
```cpp
template <bool APPROXIMATION_MODE>
void recip_init();
```

## Newton-Raphson Algorithm

### Mathematical Foundation
The SFPU reciprocal uses Newton-Raphson iteration to solve `f(x) = 1/x - a = 0` for `x = 1/a`:

```
x_{n+1} = x_n * (2 - a * x_n)
```

### Initial Approximation Strategy
**Range Reduction**: Input is normalized to [0.5, 1.0) by manipulating the IEEE-754 exponent:
```cpp
val = setexp(val, 126); // Force exponent to 126 → range [0.5, 1.0)
```

**First Guess**: Uses hardwired constant 1.442695f (ln2_recip) for consistency with Grayskull:
```cpp
sfpi::vFloat result = vConstLn2Recip * (val * vConstLn2Recip + two);
```

### Iterative Refinement
**Newton-Raphson Loop** (configurable iterations):
```cpp
for (int s_iter = 0; s_iter < (max_iter - 1); s_iter++) {
    result = result * (val * result + two);
}
```

**Precision Control**:
- **APPROXIMATION_MODE = true**: 2 iterations (faster, lower precision)
- **APPROXIMATION_MODE = false**: 3 iterations (higher precision)

## IEEE-754 Exponent Manipulation

### Exponent Reconstruction
After computing the mantissa reciprocal, the result exponent is reconstructed:

```cpp
sfpi::vInt orig_exp = exexp(in);      // Extract original exponent
sfpi::vInt new_exp = exexp(result);   // Extract result exponent

// Compute: new_exp = 127 - orig_exp (with bias adjustment)
new_exp -= orig_exp;
new_exp += 126;
```

### Overflow Protection
**Underflow Detection**: Prevents division by very large numbers:
```cpp
v_if (new_exp < 0) {
    result = 0.0F;    // Saturate to zero
    new_exp = 0;
} v_endif;
```

## Hardware Integration

### SFPU Constants Loading
**Initialization Phase**:
```cpp
sfpi::vConstFloatPrgm0 = 1.442695f; // ln2_recip (Newton-Raphson seed)
sfpi::vConstFloatPrgm1 = 2.0f;      // Constant 2.0 for iteration
```

### 32-Lane SIMD Processing
**Parallel Computation**: Processes 8 DEST register faces simultaneously:
```cpp
#pragma GCC unroll 8
for (int d = 0; d < iterations; d++) {
    sfpi::vFloat in = sfpi::dst_reg[0];
    sfpi::vFloat out = _sfpu_reciprocal_<APPROXIMATION_MODE ? 2 : 3>(in);
    // Sign handling and format conversion...
    sfpi::dst_reg++;
}
```

## Sign Handling

### Magnitude-Phase Processing
**Sign Extraction**: Processes absolute value and restores sign:
```cpp
sfpi::vFloat val = sfpi::setsgn(in, 1); // Force positive for processing

// After computation:
v_if (in < 0.0F) {
    out = -out; // Restore negative sign
} v_endif;
```

## Data Format Support

### Output Format Control
**FP32 Mode** (`is_fp32_dest_acc_en = true`):
```cpp
sfpi::dst_reg[0] = out; // Direct FP32 output
```

**FP16B Mode** (`is_fp32_dest_acc_en = false`):
```cpp
sfpi::dst_reg[0] = sfpi::reinterpret<sfpi::vFloat>(float_to_fp16b(out, 0));
```

## Performance Characteristics

### Computational Complexity
- **Clock Cycles**: ~3-6 cycles per DEST face (depending on iteration count)
- **Throughput**: 32 reciprocals per SFPU cycle (full SIMD utilization)
- **Pipeline**: Single-cycle per iteration with no dependencies

### Accuracy vs. Speed Trade-off
| Mode | Iterations | Relative Error | Use Case |
|------|------------|---------------|----------|
| APPROXIMATION_MODE = true | 2 | ~1e-6 | Fast approximations |
| APPROXIMATION_MODE = false | 3 | ~1e-7 | High precision needs |

## Common Usage Patterns

### Basic Reciprocal Computation
```cpp
// Initialize constants
recip_init<false>();

// Compute 1/x for data in DEST registers
calculate_reciprocal<false, true, 8>();
```

### Division Implementation
```cpp
// To compute a/b, calculate a * (1/b)
// 1. Load 'b' into DEST
calculate_reciprocal<false, true, 8>();

// 2. Multiply by 'a' using standard SFPU multiply
```

## Error Handling

### Special Value Handling
- **Input = 0.0**: Results in +∞ (IEEE-754 compliant)
- **Input = ∞**: Results in 0.0
- **Input = NaN**: Propagates NaN

### Numerical Stability
- **Denormalized Numbers**: Automatic promotion to zero
- **Very Small Numbers**: Saturated to prevent overflow

## Hardware Dependencies

### SFPU Requirements
- **Constant Registers**: 2 programmable constants (vConstFloatPrgm0, vConstFloatPrgm1)
- **SIMD Width**: 32-lane processing capability
- **IEEE-754 Support**: Exponent extraction and manipulation

### Memory Requirements
- **Register Usage**: Minimal (uses built-in DEST registers)
- **No LUT**: Pure algorithmic implementation (no lookup tables)

## Integration Notes

### Thread Synchronization
- **DEST Register Access**: Requires proper semaphore coordination
- **Pipeline Safety**: Self-contained operation (no cross-thread dependencies)

### Configuration State
- **Dual Config**: Supports Wormhole B0 dual configuration states
- **Runtime Switching**: Can change precision mode without reinitialization

---

**Related Instructions**: SFPMUL, SFPLOAD, SFPSTORE  
**See Also**: Division operations, matrix inversion routines  
**Architecture Reference**: Wormhole B0 SFPU Programming Guide 