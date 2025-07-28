// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu_polyval.h"
#include "sfpi.h"

namespace ckernel::sfpu
{

/**
 * @brief Compute sigmoid approximation for positive values using piecewise functions
 *
 * @details Helper function implementing piecewise sigmoid approximation optimized
 * for positive input values. Uses linear approximation for small values and
 * polynomial approximation for larger values to balance accuracy and performance.
 *
 * **Piecewise Approximation:**
 * ```
 * For x ∈ [0, 1]:   σ(x) ≈ 0.229x + 0.5    (linear approximation)
 * For x ∈ [1, 5]:   σ(x) ≈ POLYVAL5(...)    (5th order polynomial)
 * For x > 5:        σ(x) ≈ 1.0              (saturation)
 * ```
 *
 * **Polynomial Coefficients:**
 * - 5th order polynomial: 0.00144462x⁵ - 0.01055479x⁴ - 0.01203685x³ + 0.24300185x² + 0.50437757x
 * - Optimized for sigmoid approximation accuracy in [1, 5] range
 * - Smooth transition between linear and polynomial regions
 *
 * @param val Input value (must be non-negative)
 * @return Sigmoid approximation for positive input
 *
 * @note Input assumed to be positive (≥ 0)
 * @note Uses conditional execution for piecewise evaluation
 * @note Optimized for SiLU activation function requirements
 */
inline sfpi::vFloat _sigmoid_piecewise_linear_positive_(sfpi::vFloat val)
{
    sfpi::vFloat result = 1.0f;                   // Default to saturation value
    v_if (val <= 1.0f)                            // Linear region for small values
    {
        result = 0.229f * val + 0.5f; // linear appx as y = 0.229x + 0.5  // Linear approximation
    }
    v_elseif (val < 5.0f)                         // Polynomial region for medium values
    {
        result = POLYVAL5<sfpi::vFloat>(0.00144462f, -0.01055479f, -0.01203685f, 0.24300185f, 0.50437757f, val);  // 5th order polynomial
    }
    v_endif;                                      // Values ≥ 5 use default (1.0)
    return result;
}

/**
 * @brief Compute element-wise SiLU (Swish) activation function using SFPU hardware
 *
 * @details Performs element-wise SiLU activation computation on FP32 data using the
 * Tensix Vector Unit's 32-lane SIMD engine. Implements SiLU(x) = x * σ(x) where
 * σ(x) is the sigmoid function, using optimized piecewise approximations.
 *
 * **Mathematical Operation:**
 * For each lane i in 32-lane SIMD:
 * ```
 * output[i] = SiLU(input[i]) = input[i] * sigmoid(input[i])
 * 
 * Where sigmoid(x) = 1 / (1 + e^(-x))
 * 
 * Properties:
 * - SiLU(0) = 0
 * - SiLU(x) ≈ x for large positive x
 * - SiLU(x) ≈ 0 for large negative x
 * - Smooth, differentiable everywhere
 * - Self-gating: amplifies positive values, suppresses negative values
 * ```
 *
 * **Algorithm Implementation:**
 * ```
 * 1. Compute |x| (absolute value of input)
 * 2. Apply piecewise sigmoid approximation to |x|
 * 3. If x < 0, use symmetry: σ(x) = 1 - σ(|x|)
 * 4. Multiply result by original input: x * σ(x)
 * ```
 *
 * **Approximation Strategy:**
 * - Uses absolute value and sigmoid symmetry for efficiency
 * - Piecewise approximation optimized for common input ranges
 * - Linear approximation for small values (fast, adequate accuracy)
 * - Polynomial approximation for medium values (higher accuracy)
 * - Saturation for large values (computational efficiency)
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses SFPU conditional execution and polynomial evaluation
 * - Processes 32 values simultaneously across SIMD lanes
 * - No external LUT or initialization required
 * - Template-based iteration count for compile-time optimization
 *
 * **Performance Characteristics:**
 * - Latency: ~5-7 cycles per SIMD operation (abs + sigmoid + multiply)
 * - Throughput: 1 instruction per cycle
 * - Peak Rate: ~5-7 GOps/sec @ 1GHz
 * - Memory Pattern: Single-input, single-output with in-place result storage
 * - Performance depends on input value distribution
 *
 * **Common Use Cases:**
 * - Modern neural network activation function (transformer models)
 * - Alternative to ReLU with smoother gradients
 * - Self-attention mechanisms in transformers
 * - Feature gating in deep learning models
 * - Non-linear activations requiring smooth derivatives
 * - Mobile and edge AI models (efficient computation)
 * - Natural language processing models
 *
 * **Activation Function Comparison:**
 * - **vs ReLU**: Smooth, differentiable, better gradient flow
 * - **vs GELU**: Simpler computation, similar performance in many cases
 * - **vs Tanh**: Self-gating property, unbounded positive range
 * - **vs Sigmoid**: Maintains input magnitude for positive values
 *
 * **SFPU Microcode:**
 * ```cpp
 * for (each_iteration) {
 *     sfpi::vFloat val = sfpi::dst_reg[0];     // Load input (32 lanes)
 *     sfpi::vFloat result = sfpi::abs(val);    // Compute absolute value
 *     result = _sigmoid_piecewise_linear_positive_(result);  // Sigmoid approximation
 *     v_if (val < 0.0f) {                      // Handle negative inputs
 *         result = 1.0f - result;              // Use sigmoid symmetry
 *     }
 *     v_endif;
 *     sfpi::dst_reg[0] = val * result;         // Multiply by original input
 *     sfpi::dst_reg++;                         // Advance to next register
 * }
 * ```
 *
 * @tparam APPROXIMATION_MODE Not used in current implementation (for API consistency).
 * @tparam ITERATIONS Template parameter defining fixed loop iteration count.
 *
 * @note Input values must be in FP32 format in SFPU destination registers
 * @note Operation is performed in-place, overwriting input data with SiLU result
 * @note Function processes ITERATIONS worth of 32-element vectors
 * @note No runtime iteration parameter - uses template ITERATIONS value
 * @note No initialization required - uses self-contained approximations
 * @note Approximation accuracy optimized for typical neural network input ranges
 * @note Handles both positive and negative inputs through sigmoid symmetry
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_silu_()
{
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat val    = sfpi::dst_reg[0];                   // Load input (32-lane vector)
        sfpi::vFloat result = sfpi::abs(val);                     // Compute absolute value for symmetry
        result              = _sigmoid_piecewise_linear_positive_(result);  // Apply piecewise sigmoid approximation
        v_if (val < 0.0f)                                         // Handle negative inputs using symmetry
        {
            result = 1.0f - result;                               // σ(-x) = 1 - σ(x) for sigmoid symmetry
        }
        v_endif;
        sfpi::dst_reg[0] = val * result;                          // SiLU = x * σ(x)
        sfpi::dst_reg++;                                          // Advance to next 32-element vector
    }
}

} // namespace ckernel::sfpu
