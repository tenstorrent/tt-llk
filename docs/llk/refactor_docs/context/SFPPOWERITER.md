# SFPPOWERITER - SFPU Iterative Power Function

**Function**: Integer power computation using binary exponentiation  
**Instruction Scope**: SFPU (Special Function Processing Unit)  
**Architecture**: Wormhole B0 Tensix  
**Data Flow**: DEST → SFPU → DEST

## Overview

The SFPU iterative power function computes integer powers (x^n) using an optimized binary exponentiation algorithm. This implementation is specifically designed for integer exponents and leverages the Wormhole B0 SFPU's 32-lane SIMD architecture for efficient parallel computation of power operations.

## Function Signature

### Core Implementation
```cpp
template <bool APPROXIMATION_MODE, int ITERATIONS = 8>
inline void calculate_power_iterative(const uint exponent);
```

**Parameters**:
- **APPROXIMATION_MODE**: Template parameter for potential accuracy vs. speed trade-offs
- **ITERATIONS**: Number of DEST register faces to process (default: 8)
- **exponent**: Unsigned integer exponent value (runtime parameter)

## Binary Exponentiation Algorithm

### Mathematical Foundation
Computes `x^n` using the binary exponentiation method, which reduces complexity from O(n) to O(log n):

**Algorithm**: Exponentiation by squaring
```
x^n = x^(2^k * b_k + 2^(k-1) * b_(k-1) + ... + 2^1 * b_1 + 2^0 * b_0)
    = (x^(2^k))^b_k * (x^(2^(k-1)))^b_(k-1) * ... * (x^2)^b_1 * (x^1)^b_0
```

### Implementation Details
**Core Loop Structure**:
```cpp
uint exp = exponent;
vFloat in = dst_reg[0];     // Base value
vFloat result = 1.0f;       // Initialize accumulator

while (exp > 0) {
    if (exp & 1) {          // If current bit is 1
        result *= in;       // Multiply accumulator by current power
    }
    in *= in;               // Square the base for next bit
    exp >>= 1;              // Shift to next bit
}
```

### Bit-by-Bit Processing
**Binary Decomposition**: The algorithm processes each bit of the exponent:
- **Bit 0 (LSB)**: Contributes `x^1` if set
- **Bit 1**: Contributes `x^2` if set  
- **Bit k**: Contributes `x^(2^k)` if set

**Example** (x^13 where 13 = 1101₂):
```
Initial: result = 1, in = x, exp = 13 (1101₂)

Iteration 1: exp & 1 = 1 → result = 1 * x = x
            in = x * x = x², exp = 6 (110₂)

Iteration 2: exp & 1 = 0 → result unchanged = x  
            in = x² * x² = x⁴, exp = 3 (11₂)

Iteration 3: exp & 1 = 1 → result = x * x⁴ = x⁵
            in = x⁴ * x⁴ = x⁸, exp = 1 (1₂)

Iteration 4: exp & 1 = 1 → result = x⁵ * x⁸ = x¹³
            in = x⁸ * x⁸ = x¹⁶, exp = 0

Final: result = x¹³
```

## Hardware Optimization

### SIMD Parallelization
**8-Way Parallel Processing**: Processes 8 DEST register faces simultaneously:
```cpp
#pragma GCC unroll 8
for (int d = 0; d < 8; d++) {
    // Each iteration processes one DEST face
    uint exp = exponent;           // Same exponent for all faces
    vFloat in = dst_reg[0];        // Base value from current face
    vFloat result = 1.0f;          // Individual accumulator
    
    // Binary exponentiation loop...
    
    dst_reg[0] = result;           // Store result
    dst_reg++;                     // Move to next face
}
```

### Compiler Optimization
**Loop Unrolling**: The `#pragma GCC unroll 8` directive ensures:
- **No Loop Overhead**: Eliminates loop control instructions
- **Pipeline Efficiency**: Enables optimal instruction scheduling
- **Parallel Execution**: All 8 faces can be processed concurrently

## Performance Characteristics

### Computational Complexity
- **Bit Operations**: O(log₂(exponent)) iterations per face
- **SIMD Throughput**: 8 power computations per SFPU cycle
- **Maximum Efficiency**: 32-lane SIMD utilization (4 lanes × 8 faces)

### Iteration Count Examples
| Exponent | Binary | Iterations | Operations |
|----------|--------|------------|------------|
| 1 | 1₂ | 1 | 1 multiply |
| 2 | 10₂ | 2 | 1 multiply |
| 4 | 100₂ | 3 | 1 multiply |
| 8 | 1000₂ | 4 | 1 multiply |
| 15 | 1111₂ | 4 | 4 multiplies |
| 255 | 11111111₂ | 8 | 8 multiplies |

### Hardware Resource Utilization
- **Register Usage**: Minimal (3 variables: exp, in, result)
- **Memory Access**: Direct DEST register access only
- **ALU Operations**: Multiplication and bit manipulation only

## Data Flow and Register Management

### Input/Output Pattern
**Input**: Base values loaded in DEST registers
```
DEST[0] = x₁, DEST[1] = x₂, ..., DEST[7] = x₈
```

**Output**: Power results in same DEST registers
```  
DEST[0] = x₁ⁿ, DEST[1] = x₂ⁿ, ..., DEST[7] = x₈ⁿ
```

### Register Advancement
**Sequential Processing**: Automatic progression through DEST faces:
```cpp
dst_reg[0] = result;    // Store computed power
dst_reg++;              // Advance to next face
```

## Special Cases and Edge Conditions

### Mathematical Edge Cases
- **x^0 = 1**: Handled by initial `result = 1.0f` (loop never executes)
- **x^1 = x**: Single iteration, direct multiplication
- **0^n**: Depends on base value (0^0 is typically 1 by convention)

### Floating-Point Considerations
- **Infinity**: IEEE-754 rules apply (∞^n behavior)
- **NaN Propagation**: NaN inputs propagate through multiplication
- **Precision**: Accumulated rounding errors in repeated multiplication

## Usage Patterns

### Basic Integer Power
```cpp
// Compute x^5 for data in DEST registers
calculate_power_iterative<false, 8>(5);
```

### High-Performance Batch Processing
```cpp
// Process multiple exponents efficiently
for (uint exp : {2, 3, 4, 8, 16}) {
    // Load base values into DEST
    calculate_power_iterative<false, 8>(exp);
    // Process results
}
```

### Integration with Other SFPU Operations
```cpp
// Complex computation: (x^n) * y + z
calculate_power_iterative<false, 8>(n);  // x^n
// Load y values and multiply
// Load z values and add
```

## Optimization Guidelines

### Exponent Selection
**Efficient Exponents**: Powers of 2 require minimal iterations:
- **2^k**: Only k+1 iterations, single multiplication
- **2^k - 1**: Maximum iterations for k-bit number

**Inefficient Exponents**: Numbers with many 1-bits in binary representation
- **255 (11111111₂)**: 8 multiplications
- **170 (10101010₂)**: 4 multiplications (better)

### Memory Access Patterns
**Sequential Access**: Optimal for cache performance
```cpp
// Good: Sequential DEST register access
for (int face = 0; face < 8; face++) {
    // Process dst_reg[face]
}
```

## Error Considerations

### Numerical Stability
- **Overflow Risk**: Large bases with large exponents
- **Underflow Risk**: Small bases with large exponents  
- **Precision Loss**: Accumulates with iteration count

### Range Limitations
- **Exponent Size**: Limited by `uint` size (typically 32-bit)
- **Result Range**: IEEE-754 floating-point limits apply
- **Base Constraints**: No specific restrictions on base values

## Hardware Dependencies

### SFPU Requirements
- **Multiplication Units**: Standard IEEE-754 floating-point multiply
- **Integer ALU**: For exponent bit manipulation (`&`, `>>` operations)
- **Register File**: Standard DEST register access

### Instruction Dependencies
- **No Special Instructions**: Uses standard SFPU multiply operations
- **No LUT Required**: Pure algorithmic implementation
- **No Initialization**: Self-contained function

## Performance Tuning

### Template Specialization
**APPROXIMATION_MODE**: Currently unused but available for future optimizations:
```cpp
template <bool APPROXIMATION_MODE>
// Could enable faster, less accurate multiplication modes
```

### Iteration Control
**ITERATIONS Parameter**: Adjustable for different DEST register configurations:
- **ITERATIONS = 4**: Process half DEST register file
- **ITERATIONS = 16**: Process full double-size configuration

---

**Related Instructions**: SFPMUL, SFPLOAD, SFPSTORE  
**Use Cases**: Polynomial evaluation, tensor operations, mathematical modeling  
**Architecture Reference**: Wormhole B0 SFPU Programming Guide 