# SFPU Operations - Wormhole B0 Detailed Documentation

**File Location**: `tt_llk_wormhole_b0/common/inc/sfpu/`
**Architecture**: Wormhole B0 Tensix
**Component**: Special Function Processing Unit Operations

## Overview

The Special Function Processing Unit (SFPU) provides hardware-accelerated implementations of complex mathematical functions essential for neural network operations. The Wormhole B0 SFPU implementation offers high-performance activation functions, transcendental operations, and specialized neural network primitives with dual-mode execution for optimal speed-accuracy tradeoffs.

## SFPU Architecture Patterns

### Dual-Mode Implementation Philosophy

Most SFPU operations implement two execution modes optimized for different use cases:

```cpp
template <bool APPROXIMATION_MODE, int ITERATIONS = 8>
inline void calculate_operation();
```

- **Approximation Mode** (`APPROXIMATION_MODE = true`): High-speed, lower precision
- **Accurate Mode** (`APPROXIMATION_MODE = false`): Higher precision, increased computational cost

### Vector Processing Model

All SFPU operations work on 32-lane vectors with SIMD execution:

```cpp
#pragma GCC unroll 8
for (int d = 0; d < ITERATIONS; d++) {
    sfpi::vFloat in = sfpi::dst_reg[0];      // Load 32-lane vector
    sfpi::vFloat result = operation_body(in); // Process all lanes
    sfpi::dst_reg[0] = result;               // Store result
    sfpi::dst_reg++;                         // Advance to next vector
}
```

## Core Activation Functions

### GELU (Gaussian Error Linear Units) (`ckernel_sfpu_gelu.h`)

Implements the critical GELU activation function with both LUT-based approximation and CDF-based accurate computation:

#### Core Implementation
```cpp
template <bool APPROXIMATION_MODE>
inline sfpi::vFloat _calculate_gelu_core_(sfpi::vFloat in) {
    sfpi::vFloat result;
    if constexpr (APPROXIMATION_MODE) {
        result = in;  // Input for LUT processing
    } else {
        // Tanh approximation: GELU(x) ≈ 0.5 * x * [1 + tanh(√(2/π) * (x + 0.044715*x³))]
        result = (in * in) * (in * sfpi::s2vFloat16b(0.044715f)) + in;
        result *= sfpi::s2vFloat16b(0.79788f);  // √(2/π) scaling
    }
    return result;
}
```

#### Approximation Mode (LUT-Based)
```cpp
template <int ITERATIONS>
inline void _calculate_gelu_appx_() {
    // Save LUT coefficient registers
    sfpi::vUInt l0 = sfpi::l_reg[sfpi::LRegs::LReg0];
    sfpi::vUInt l1 = sfpi::l_reg[sfpi::LRegs::LReg1];
    sfpi::vUInt l2 = sfpi::l_reg[sfpi::LRegs::LReg2];
    // ... additional coefficient registers

    #pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++) {
        sfpi::vFloat in = sfpi::dst_reg[0];
        sfpi::vFloat half = sfpi::vConstFloatPrgm0;  // 0.5 constant
        sfpi::vFloat half_in = in * half;

        // 6-piece piecewise linear LUT with sign handling
        sfpi::vFloat result = lut2_sign(in, l0, l1, l2, l4, l5, l6);
        result = half_in + result;  // Final: x/2 + LUT(x)

        sfpi::dst_reg[0] = result;
        sfpi::dst_reg++;
    }

    // Restore LUT coefficient registers
    sfpi::l_reg[sfpi::LRegs::LReg0] = l0;
    // ... restore additional registers
}
```

**Performance**: ~2-3 cycles per face, ~1e-4 relative error

#### Accurate Mode (CDF-Based)
```cpp
template <int ITERATIONS>
inline void _calculate_gelu_accurate_() {
    constexpr bool scaled = true;
    #pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++) {
        sfpi::vFloat in = sfpi::dst_reg[0];
        sfpi::vFloat result = _calculate_cdf_appx_(in, scaled);  // CDF approximation
        sfpi::dst_reg[0] = result;
        sfpi::dst_reg++;
    }
}
```

**Performance**: ~8-12 cycles per face, ~1e-6 relative error

### ReLU Variants (`ckernel_sfpu_relu.h`)

#### Standard ReLU with Conditional Processing
```cpp
sfpi_inline sfpi::vFloat _relu_max_body_(sfpi::vFloat val, sfpi::vFloat threshold) {
    sfpi::vFloat result = val;
    v_if (result > threshold) {
        result = threshold;    // Clamp to maximum threshold
    }
    v_endif;
    v_if (result < 0.0f) {
        result = 0.0f;        // Standard ReLU clipping
    }
    v_endif;
    return result;
}
```

#### Leaky ReLU Implementation
```cpp
template <bool APPROXIMATION_MODE>
inline void _calculate_lrelu_(const int iterations, uint slope) {
    sfpi::vFloat s = Converter::as_float(slope);

    #pragma GCC unroll 0
    for (int d = 0; d < iterations; d++) {
        sfpi::vFloat v = sfpi::dst_reg[0];

        v_if (v < 0.0f) {
            v *= s;  // Apply slope to negative values
        }
        v_endif;

        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}
```

## Transcendental Functions

### Exponential Function (`ckernel_sfpu_exp.h`)

#### Advanced Exponential Implementation
```cpp
sfpi_inline sfpi::vFloat _sfpu_exp_(sfpi::vFloat val) {
    // Extract and condition exponent for numerical stability
    sfpi::vInt exp = exexp(val);
    v_if (exp >= 0) {
        val = setexp(val, 126);  // Replace exponent with -1 (bias 127 - 1)
    }
    v_endif;

    // Horner form polynomial evaluation
    sfpi::vFloat tmp = val * sfpi::vConst0p8373 + sfpi::s2vFloat16b(0.863281);
    val = val * tmp + sfpi::vConst1;

    // Exponent restoration with iterative squaring
    v_if (exp >= 0) {
        val = val * val;
        for (int s_iter = 0; s_iter < 7; s_iter++) {
            exp = exp - 1;
            v_and(exp >= 0);  // Narrow predication
            val = val * val;
        }
    }
    v_endif;

    return val;
}
```

#### Dual-Mode Exponential Body
```cpp
template <bool APPROXIMATION_MODE>
sfpi_inline sfpi::vFloat _calculate_exponential_body_(sfpi::vFloat in) {
    sfpi::vFloat out;

    if constexpr (APPROXIMATION_MODE) {
        // Fast bit-manipulation approach
        constexpr int FRAC_BITS = 3;
        constexpr uint SP_BIAS = 127 << FRAC_BITS;

        sfpi::vFloat vConstLn2Recip = sfpi::vConstFloatPrgm0;  // 1/ln(2)
        sfpi::vFloat conv = in * vConstLn2Recip;

        // Convert to 7.3 fixed-point and bias for IEEE 754
        sfpi::vInt c23_73 = p_exp::C23_73;
        sfpi::vInt tmp = sfpi::reinterpret<sfpi::vInt>(conv) - c23_73;
        tmp += SP_BIAS;

        // Shift to exponent field position
        out = sfpi::reinterpret<sfpi::vFloat>(tmp << (10 - FRAC_BITS));
    } else {
        // Accurate polynomial-based approach
        out = _sfpu_exp_(sfpi::setsgn(in, 0));  // Force positive

        v_if (in < 0) {
            out = _sfpu_reciprocal_(out);  // Handle negative input
        }
        v_endif;
    }

    return out;
}
```

### Square Root (`ckernel_sfpu_sqrt.h`)

#### Approximation vs. Accurate Square Root
```cpp
template <bool APPROXIMATION_MODE, int RECIPROCAL_ITERATIONS>
sfpi_inline sfpi::vFloat _calculate_sqrt_body_(sfpi::vFloat val) {
    sfpi::vFloat result;

    if constexpr (APPROXIMATION_MODE) {
        // Fast bit-manipulation approximation
        sfpi::vUInt magic = sfpi::vConstIntPrgm0;
        sfpi::vUInt val_s = magic + sfpi::reinterpret<sfpi::vUInt>(val);
        val_s >>= 1;  // Divide exponent by 2
        result = sfpi::reinterpret<sfpi::vFloat>(val_s);
    } else {
        // Newton-Raphson reciprocal square root method
        v_if (val != 0.0f) {
            sfpi::vUInt magic = sfpi::vConstIntPrgm0;
            sfpi::vFloat approx = sfpi::reinterpret<sfpi::vFloat>(
                magic - (sfpi::reinterpret<sfpi::vUInt>(val) >> 1)
            );

            // Iterative refinement: x*r*(1.5f - xhalf*r*r)
            for (int r = 0; r < RECIPROCAL_ITERATIONS; r++) {
                approx = ((approx * approx) * (val * -0.5f) + 1.5f) * approx;
            }

            result = approx * val;  // Convert reciprocal root to root
        }
        v_else {
            result = val;  // sqrt(0) = 0
        }
        v_endif;
    }
    return result;
}
```

## Advanced Neural Network Operations

### Sigmoid Function (`ckernel_sfpu_sigmoid.h`)

```cpp
template <bool APPROXIMATION_MODE>
inline void _calculate_sigmoid_body_(sfpi::vFloat in) {
    if constexpr (APPROXIMATION_MODE) {
        // LUT-based approximation
        // sigmoid(x) ≈ 0.5 + 0.5 * tanh(x/2)
        sfpi::vFloat scaled_input = in * 0.5f;
        sfpi::vFloat tanh_result = lookup_tanh_approximation(scaled_input);
        return 0.5f + 0.5f * tanh_result;
    } else {
        // Accurate exponential-based computation
        // sigmoid(x) = 1 / (1 + exp(-x))
        sfpi::vFloat neg_in = -in;
        sfpi::vFloat exp_neg = _calculate_exponential_body_<false>(neg_in);
        return _sfpu_reciprocal_(1.0f + exp_neg);
    }
}
```

### Tanh Function (`ckernel_sfpu_tanh.h`)

```cpp
template <bool APPROXIMATION_MODE>
inline void _calculate_tanh_body_(sfpi::vFloat in) {
    if constexpr (APPROXIMATION_MODE) {
        // Rational approximation for |x| < 1
        // tanh(x) ≈ x * (27 + x²) / (27 + 9*x²)
        sfpi::vFloat x_sq = in * in;
        sfpi::vFloat num = in * (27.0f + x_sq);
        sfpi::vFloat den = 27.0f + 9.0f * x_sq;
        return num / den;
    } else {
        // Exponential-based accurate computation
        // tanh(x) = (exp(2x) - 1) / (exp(2x) + 1)
        sfpi::vFloat two_x = 2.0f * in;
        sfpi::vFloat exp_2x = _calculate_exponential_body_<false>(two_x);
        return (exp_2x - 1.0f) / (exp_2x + 1.0f);
    }
}
```

## Specialized Operations

### Type Conversion (`ckernel_sfpu_typecast.h`)

```cpp
template <bool APPROXIMATION_MODE>
inline void _calculate_typecast_fp32_to_fp16a_() {
    #pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++) {
        sfpi::vFloat val = sfpi::dst_reg[0];

        // Convert FP32 to FP16A format
        sfpi::vUInt fp32_bits = sfpi::reinterpret<sfpi::vUInt>(val);

        // Extract sign, exponent, and mantissa
        sfpi::vUInt sign = fp32_bits & 0x80000000;
        sfpi::vUInt exp = (fp32_bits >> 23) & 0xFF;
        sfpi::vUInt mant = (fp32_bits >> 13) & 0x3FF;

        // Adjust exponent bias (127 -> 15)
        exp = exp - 112;  // 127 - 15 = 112

        // Reassemble as FP16A in upper 16 bits
        sfpi::vUInt fp16a = (sign >> 16) | (exp << 10) | mant;
        sfpi::dst_reg[0] = sfpi::reinterpret<sfpi::vFloat>(fp16a << 16);

        sfpi::dst_reg++;
    }
}
```

### Quantization Operations (`ckernel_sfpu_quant.h`)

```cpp
template <bool APPROXIMATION_MODE>
inline void _calculate_quantize_int8_(const int iterations, uint scale_factor) {
    sfpi::vFloat scale = Converter::as_float(scale_factor);

    #pragma GCC unroll 8
    for (int d = 0; d < iterations; d++) {
        sfpi::vFloat val = sfpi::dst_reg[0];

        // Scale and round
        val = val * scale;

        // Clamp to INT8 range [-128, 127]
        v_if (val > 127.0f) {
            val = 127.0f;
        }
        v_endif;
        v_if (val < -128.0f) {
            val = -128.0f;
        }
        v_endif;

        // Round to integer
        sfpi::vFloat rounded = val + 0.5f;
        v_if (val < 0.0f) {
            rounded = val - 0.5f;
        }
        v_endif;

        sfpi::dst_reg[0] = rounded;
        sfpi::dst_reg++;
    }
}
```

## Performance Optimization Techniques

### Register Management

#### LUT Coefficient Preservation
```cpp
template <int ITERATIONS>
inline void _operation_with_lut_() {
    // Save LUT state to prevent corruption
    sfpi::vUInt l0 = sfpi::l_reg[sfpi::LRegs::LReg0];
    sfpi::vUInt l1 = sfpi::l_reg[sfpi::LRegs::LReg1];

    // Perform operation using LUT
    operation_body();

    // Restore LUT state for subsequent operations
    sfpi::l_reg[sfpi::LRegs::LReg0] = l0;
    sfpi::l_reg[sfpi::LRegs::LReg1] = l1;
}
```

#### Constant Loading Optimization
```cpp
template <bool APPROXIMATION_MODE>
inline void _init_operation_() {
    if constexpr (APPROXIMATION_MODE) {
        // Load constants optimized for approximation mode
        sfpi::vConstFloatPrgm0 = sfpi::s2vFloat16b(0.5f);
        sfpi::vConstIntPrgm0 = 0x3F000000;  // Magic constant
    } else {
        // Load constants optimized for accurate mode
        sfpi::vConstFloatPrgm0 = sfpi::s2vFloat16b(1.4426950f);  // 1/ln(2)
        sfpi::vConstFloatPrgm1 = sfpi::s2vFloat16b(0.69314718f); // ln(2)
    }
}
```

### Conditional Execution Optimization

#### Predication Patterns
```cpp
// Efficient conditional processing using v_if/v_endif
v_if (condition) {
    // Operations only execute on lanes where condition is true
    result = complex_operation(input);
}
v_else {
    // Alternative path for false conditions
    result = simple_operation(input);
}
v_endif;
```

#### Narrow Predication
```cpp
// Progressively narrow active lanes for efficiency
v_if (major_condition) {
    preliminary_operation();

    v_and(minor_condition);  // Further narrow active lanes
    expensive_operation();   // Only executes on subset
}
v_endif;
```

## Performance Characteristics

### Cycle Costs by Operation Category

| Operation Category | Approximation Mode | Accurate Mode | Notes |
|--------------------|-------------------|---------------|-------|
| **Activation Functions** | 2-4 cycles | 6-12 cycles | LUT vs. polynomial |
| **Transcendental** | 3-6 cycles | 8-16 cycles | Complexity dependent |
| **Type Conversion** | 1-2 cycles | 2-4 cycles | Format dependent |
| **Comparison** | 1-2 cycles | 1-2 cycles | Hardware optimized |

### Accuracy vs. Performance Tradeoffs

| Operation | Approximation Error | Accurate Error | Speedup |
|-----------|---------------------|----------------|---------|
| **GELU** | ~1e-4 | ~1e-6 | 3-4x |
| **Exponential** | ~1e-3 | ~1e-6 | 2-3x |
| **Square Root** | ~1e-4 | ~1e-7 | 2x |
| **Sigmoid** | ~1e-4 | ~1e-6 | 2-3x |

## Integration Patterns

### Standard SFPU Operation Workflow
```cpp
// 1. Initialize operation constants
_init_operation_<APPROXIMATION_MODE>();

// 2. Configure operation parameters
operation_config_t config = {
    .approximation_mode = APPROXIMATION_MODE,
    .iterations = 8,
    .custom_parameters = operation_specific_params
};

// 3. Execute operation
_calculate_operation_<APPROXIMATION_MODE, 8>(config);
```

### Multi-Operation Sequences
```cpp
// Efficient chaining of SFPU operations
_init_gelu_<true>();
_calculate_gelu_<true, 8>();

// No re-initialization needed for same mode
_calculate_relu_<true, 8>();
_calculate_sigmoid_<true, 8>();
```

### Error Handling and Validation
```cpp
// Input validation for numerical stability
template <bool APPROXIMATION_MODE>
inline void _safe_operation_(sfpi::vFloat input) {
    // Clamp input to valid range
    v_if (input > MAX_INPUT_VALUE) {
        input = MAX_INPUT_VALUE;
    }
    v_endif;
    v_if (input < MIN_INPUT_VALUE) {
        input = MIN_INPUT_VALUE;
    }
    v_endif;

    // Proceed with operation
    result = _operation_body_<APPROXIMATION_MODE>(input);
}
```

## Advanced Features

### Custom LUT Implementation
```cpp
// Application-specific lookup table operations
template <int LUT_SIZE>
inline void _custom_lut_operation_() {
    // Load custom LUT coefficients
    for (int i = 0; i < LUT_SIZE; i++) {
        sfpi::l_reg[i] = custom_lut_coefficients[i];
    }

    // Use hardware LUT instructions
    sfpi::vFloat result = lut(input, l_reg[0], l_reg[1], l_reg[2]);
}
```

### Dynamic Mode Selection
```cpp
// Runtime mode selection based on accuracy requirements
template <typename ConfigType>
inline void _adaptive_operation_(const ConfigType& config) {
    if (config.require_high_accuracy) {
        _calculate_operation_<false, 8>();  // Accurate mode
    } else {
        _calculate_operation_<true, 8>();   // Approximation mode
    }
}
```

---

**Next**: [Math Operations Detailed Documentation](math_operations_detailed.md)
**Related**: [Core Infrastructure](core_infrastructure_detailed.md), [SFPU Context Documentation](context/SFPGELU.md)
**Implementation**: Source files in `tt_llk_wormhole_b0/common/inc/sfpu/`
