# SFPGELU - SFPU GELU Activation Function

**Function**: Gaussian Error Linear Units (GELU) activation function  
**Instruction Scope**: SFPU (Special Function Processing Unit)  
**Architecture**: Wormhole B0 Tensix  
**Data Flow**: DEST → SFPU → DEST

## Overview

The SFPU GELU function implements the Gaussian Error Linear Units activation function, which is essential for modern transformer architectures and neural networks. The implementation provides two computational modes: a high-speed lookup table (LUT) based approximation and a more accurate mathematical evaluation using cumulative distribution function (CDF) approximation.

## Mathematical Foundation

### GELU Definition
The GELU activation function is mathematically defined as:
```
GELU(x) = x * Φ(x) = x * (1/2) * [1 + erf(x/√2)]
```

Where:
- **Φ(x)**: Cumulative distribution function of standard normal distribution
- **erf(x)**: Error function

### Alternative Approximations
**Tanh Approximation** (used in accurate mode):
```
GELU(x) ≈ (1/2) * x * [1 + tanh(√(2/π) * (x + 0.044715 * x³))]
```

## Function Signatures

### Core Implementation Functions
```cpp
template <bool APPROXIMATION_MODE, int ITERATIONS = 8>
inline void calculate_gelu();
```

### Derivative Function
```cpp
template <bool APPROXIMATION_MODE, int ITERATIONS = 8>
inline void calculate_gelu_derivative();
```

### Initialization Functions
```cpp
template <bool APPROXIMATION_MODE>
void gelu_init();

template <bool APPROXIMATION_MODE>
void gelu_derivative_init();
```

## Dual-Mode Implementation

### Approximation Mode (APPROXIMATION_MODE = true)
**Method**: 6-piece piecewise linear lookup table  
**Performance**: High-speed, optimized for inference  
**Accuracy**: ~1e-4 relative error

**LUT Segments**:
```cpp
// Piecewise linear approximation:
// x ∈ [0.0, 0.5]: y = 0.1928*x - 0.0150
// x ∈ [0.5, 1.0]: y = 0.4939*x - 0.1605  
// x ∈ [1.0, 1.5]: y = 0.6189*x - 0.2797
// x ∈ [1.5, 2.0]: y = 0.6099*x - 0.2635
// x ∈ [2.0, 3.0]: y = 0.5402*x - 0.1194
// x ∈ [3.0, ∞):  y = 0.5000 (saturation)
```

### Accurate Mode (APPROXIMATION_MODE = false)  
**Method**: CDF approximation with exponential computation  
**Performance**: Higher computational cost  
**Accuracy**: ~1e-6 relative error

**Algorithm**:
```cpp
// Core computation using tanh approximation:
result = (in * in) * (in * 0.044715f) + in;  // x + 0.044715*x³
result *= 0.79788f;                           // √(2/π) scaling
result = x * cdf_approximation(result);       // Apply CDF
```

## Hardware-Accelerated LUT Implementation

### LUT Register Configuration
**Approximation Mode Initialization**:
```cpp
// LUT coefficients stored in local registers
_sfpu_load_imm32_(0, 0x37E7322B);  // Slopes for ranges 0-0.5, 0.5-1.0
_sfpu_load_imm32_(4, 0xB12286D8);  // Y-intercepts for ranges 0-0.5, 0.5-1.0
_sfpu_load_imm32_(1, 0x38E138F3);  // Slopes for ranges 1.0-1.5, 1.5-2.0
_sfpu_load_imm32_(5, 0xB437B479);  // Y-intercepts for ranges 1.0-1.5, 1.5-2.0
_sfpu_load_imm32_(2, 0x38003852);  // Slopes for ranges 2.0-3.0, 3.0+
_sfpu_load_imm32_(6, 0x7c00afa4);  // Y-intercepts for ranges 2.0-3.0, 3.0+
```

### Sign-Aware LUT Processing
**Symmetric Extension**: Exploits GELU's odd-function-like behavior for negative inputs:
```cpp
sfpi::vFloat result = lut2_sign(in, l0, l1, l2, l4, l5, l6);
result = half_in + result;  // Final combination: x/2 + LUT(x)
```

**Algorithm**:
```
For x ≥ 0: GELU(x) = x/2 + LUT(x)
For x < 0:  GELU(x) = x/2 - LUT(-x)  (antisymmetric correction)
```

## SIMD Processing Pipeline

### 8-Way Parallel Computation
**Unrolled Loop Structure**:
```cpp
#pragma GCC unroll 8
for (int d = 0; d < ITERATIONS; d++) {
    sfpi::vFloat in = sfpi::dst_reg[0];      // Load input
    
    // Mode-specific computation
    if constexpr (APPROXIMATION_MODE) {
        result = lut2_sign(in, l0, l1, l2, l4, l5, l6);
        result = half_in + result;
    } else {
        result = _calculate_cdf_appx_(in, scaled);
    }
    
    sfpi::dst_reg[0] = result;               // Store result
    sfpi::dst_reg++;                         // Advance register
}
```

### Register Management
**Local Register Preservation**: LUT coefficients are saved/restored:
```cpp
// Save LUT state
sfpi::vUInt l0 = sfpi::l_reg[sfpi::LRegs::LReg0];
// ... processing ...
// Restore LUT state  
sfpi::l_reg[sfpi::LRegs::LReg0] = l0;
```

## GELU Derivative Implementation

### Mathematical Definition
```
d/dx GELU(x) = Φ(x) + x * φ(x)
```
Where:
- **Φ(x)**: CDF of standard normal
- **φ(x)**: PDF of standard normal = (1/√(2π)) * exp(-x²/2)

### Approximation Mode Derivative
**6-Piece Linear Model**:
```cpp
// Piecewise linear derivative approximation:
// x ∈ [0.0, 0.5]: dy/dx = 0.8*x + 0.5
// x ∈ [0.5, 1.0]: dy/dx = 0.4*x + 0.7
// x ∈ [1.0, 1.5]: dy/dx = 0.1*x + 0.99
// x ∈ [1.5, 2.0]: dy/dx = -0.09*x + 1.27
// x ∈ [2.0, 3.0]: dy/dx = -0.075*x + 1.235
// x ∈ [3.0, ∞):  dy/dx = 1.0
```

### Accurate Mode Derivative
**Analytical Computation**:
```cpp
sfpi::vFloat neg_half_sq_in = in * in * -0.5f;
sfpi::vFloat exp = _calculate_exponential_body_<false>(neg_half_sq_in);
sfpi::vFloat partial = exp * in * 0.3989423f;  // x * φ(x)
sfpi::vFloat result = _calculate_gelu_core_<true>(in);
result = lut(result, l0, l1, imm2);            // Φ(x) approximation
return partial + result + 0.5f;               // Φ(x) + x*φ(x)
```

## Performance Characteristics

### Computational Complexity
| Mode | Cycles/Face | Throughput | Accuracy |
|------|-------------|------------|----------|
| Approximation | ~2-3 | 32 operations/cycle | 1e-4 |
| Accurate | ~8-12 | 32 operations/cycle | 1e-6 |

### Memory Usage
- **LUT Storage**: 6 local registers (48 bytes)
- **Working Registers**: 3-4 temporary registers
- **Constants**: 1 programmable constant (0.5f)

## Usage Patterns

### Basic GELU Activation
```cpp
// Initialize LUT coefficients
gelu_init<true>();  // Use approximation mode

// Process DEST register data
calculate_gelu<true, 8>();
```

### Training with Derivatives
```cpp
// Forward pass
gelu_init<false>();
calculate_gelu<false, 8>();

// Backward pass (gradient computation)
gelu_derivative_init<false>();
calculate_gelu_derivative<false, 8>();
```

### High-Throughput Inference
```cpp
// One-time initialization
gelu_init<true>();

// Batch processing
for (auto& batch : neural_network_layers) {
    // Load data into DEST registers
    calculate_gelu<true, 8>();  // Fast LUT-based computation
    // Continue with next layer
}
```

## Error Analysis

### Approximation Mode Accuracy
**Maximum Error Regions**:
- **Peak Error**: Around x = ±1.5 (transition regions)
- **Low Error**: Near x = 0 and |x| > 3
- **Relative Error**: < 0.01% for |x| < 2

### Numerical Stability
**Range Handling**:
- **Large Inputs** (|x| > 6): Asymptotic behavior (GELU(x) ≈ x for x >> 0)
- **Small Inputs** (|x| < 0.1): High precision near origin
- **Overflow Protection**: Saturated values for extreme inputs

## Hardware Dependencies

### SFPU Requirements
- **LUT Hardware**: 6-entry lookup table support
- **Local Registers**: 6 registers for coefficient storage
- **Exponential Unit**: For accurate mode CDF computation
- **Sign Processing**: Hardware sign detection and manipulation

### Memory Requirements
- **Register File**: Standard DEST register access
- **Local Storage**: 6 × 32-bit coefficient storage
- **No External Memory**: Self-contained implementation

## Integration Considerations

### Neural Network Integration
**Layer Compatibility**: 
- **Transformer Blocks**: Drop-in replacement for ReLU
- **Feed-Forward Networks**: Smooth activation alternative
- **Attention Mechanisms**: Improved gradient flow

### Precision Requirements
**Mixed Precision Support**:
- **FP16 Mode**: Optimized for inference workloads
- **FP32 Mode**: Higher precision for training

### Pipeline Efficiency
**Batching Strategy**:
```cpp
// Efficient batch processing
for (int layer = 0; layer < num_layers; layer++) {
    // Load batch into DEST
    calculate_gelu<APPROXIMATION_MODE, 8>();
    // Continue pipeline without stalls
}
```

---

**Related Functions**: Error function (erf), CDF approximation, Tanh activation  
**Use Cases**: Transformer networks, BERT, GPT architectures  
**Architecture Reference**: Wormhole B0 SFPU Programming Guide 