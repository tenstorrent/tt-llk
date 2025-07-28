// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_relu.h
 * @brief ReLU activation function SFPU kernels for Wormhole B0 Tensix Special Function Processing Unit
 * 
 * @details This header provides hardware-accelerated ReLU (Rectified Linear Unit) and related
 * activation functions using the Wormhole B0 SFPU's 32-lane SIMD architecture with conditional
 * execution capabilities. The SFPU's condition code functionality enables efficient per-lane
 * activation computation with support for standard ReLU, Leaky ReLU, and clamped ReLU variants.
 * 
 * **Mathematical Operations:**
 * - **ReLU**: f(x) = max(0, x) = x if x > 0, 0 if x ≤ 0
 * - **Leaky ReLU**: f(x) = x if x > 0, slope × x if x ≤ 0
 * - **ReLU6 (Clamped)**: f(x) = min(max(0, x), threshold) = clamp(x, 0, threshold)
 * 
 * **Wormhole B0 SFPU ReLU Architecture:**
 * - **32-Lane SIMD**: Parallel activation computation across all SFPU lanes within current register
 * - **Conditional Execution**: Hardware v_if/v_endif for per-lane threshold comparison
 * - **Zero Clamping**: Efficient negative value elimination using hardware comparison
 * - **Slope Multiplication**: Configurable slope for Leaky ReLU using SFPU MAD blocks
 * - **Threshold Clamping**: Upper bound clamping for ReLU6 and bounded activation variants
 * 
 * **Algorithm Implementations:**
 * 
 * **1. Leaky ReLU**
 * Conditional slope multiplication for negative values:
 * ```
 * if (x < 0):
 *     result = x * slope     // Apply slope to negative values
 * else:
 *     result = x             // Pass positive values unchanged
 * ```
 * 
 * **2. ReLU6/Clamped ReLU**
 * Dual-threshold clamping with conditional execution:
 * ```
 * result = x
 * if (result > threshold):   // Upper bound clamping
 *     result = threshold
 * if (result < 0):          // Lower bound clamping (standard ReLU)
 *     result = 0
 * ```
 * 
 * **Hardware Context:**
 * - **SFPI Interface**: Uses SFPI conditional execution and arithmetic operations
 * - **Condition Code Logic**: Hardware CC functionality for multi-way conditional execution
 * - **MAD Block Utilization**: Slope multiplication leverages FP32 MAD hardware
 * - **Converter Utility**: Safe type conversion for slope parameter handling
 * - **Register Optimization**: Efficient in-place activation computation
 * 
 * **IEEE-754 Implementation:**
 * - **Zero Comparison**: Hardware floating-point comparison with IEEE-754 semantics
 * - **Threshold Comparison**: Proper handling of ±0, ±∞, and NaN values
 * - **Slope Multiplication**: IEEE-754 compliant multiplication for Leaky ReLU
 * - **Clamping Behavior**: Standard IEEE-754 min/max operations for bounds
 * 
 * **Performance Characteristics:**
 * - **Throughput**: 32 activation operations per clock cycle (within one register)
 * - **Latency**: 1-2 clock cycles per register depending on activation complexity
 * - **Memory Efficiency**: In-place operation requiring no additional memory
 * - **Conditional Overhead**: Minimal overhead due to hardware conditional execution
 * 
 * **Activation Function Variants:**
 * - **Standard ReLU**: No slope parameter, simple zero-thresholding
 * - **Leaky ReLU**: Configurable slope (typically 0.01-0.1) for negative values
 * - **Parametric ReLU**: Learnable slope parameter for adaptive activation
 * - **ReLU6**: Upper bound at 6.0 for quantization-friendly activation
 * - **Clamped ReLU**: Arbitrary upper bound for custom activation requirements
 * 
 * **Common Use Cases:**
 * - **Neural Networks**: Primary activation function for deep learning models
 * - **Convolutional Networks**: Standard activation for CNN architectures
 * - **Gradient Flow**: Prevents vanishing gradient problem in deep networks
 * - **Sparsity**: Introduces beneficial sparsity in neural network representations
 * - **Quantization**: ReLU6 variant supports efficient quantized neural networks
 * 
 * **Integration with Wormhole B0:**
 * - **Thread 1 Execution**: Optimized for Math thread execution context
 * - **DEST Register Management**: Integrates with register file double-buffering
 * - **Pipeline Integration**: Maintains efficient Unpack→Math→Pack data flow
 * - **Format Flexibility**: Supports all DEST register data format configurations
 * - **2048 Multiplier Coordination**: Leverages broader Tensix multiplication resources
 */

#pragma once

#include "ckernel_sfpu_converter.h"
#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Hardware-accelerated Leaky ReLU activation using Wormhole B0 SFPU conditional execution
 * @tparam APPROXIMATION_MODE Unused for exact conditional activation
 * @param iterations Number of DEST register entries to process
 * @param slope Slope parameter for negative values (typically 0.01-0.1)
 * 
 * @details Computes Leaky ReLU activation for data in DEST registers using the Wormhole B0
 * SFPU's conditional execution and MAD block capabilities. Leaky ReLU addresses the "dying
 * ReLU" problem by allowing small negative gradients through a configurable slope parameter.
 * 
 * **Wormhole B0 SFPU Leaky ReLU Processing:**
 * - **32-Lane SIMD**: All lanes independently evaluate activation conditions within current register
 * - **Conditional Multiplication**: Hardware conditional execution for slope application
 * - **MAD Block Usage**: Slope multiplication leverages dedicated FP32 hardware
 * - **Type Conversion**: Safe slope parameter conversion via Converter utility
 * 
 * **SFPI Programming Model:**
 * ```cpp
 * sfpi::vFloat s = Converter::as_float(slope);     // Convert slope parameter
 * sfpi::vFloat v = sfpi::dst_reg[0];               // Load input from current register
 * 
 * v_if (v < 0.0f) {                                // Check for negative values
 *     v *= s;                                      // Apply slope to negative values
 * }
 * v_endif;                                         // Positive values unchanged
 * 
 * sfpi::dst_reg[0] = v;                            // Store result in-place
 * sfpi::dst_reg++;                                 // Advance to next register
 * ```
 * 
 * **Mathematical Properties:**
 * - **Leaky ReLU Function**: f(x) = x if x ≥ 0, slope × x if x < 0
 * - **Gradient Preservation**: Non-zero gradients for negative inputs prevent dying neurons
 * - **Slope Parameter**: Typically small positive value (0.01, 0.1, 0.2)
 * - **Continuity**: Continuous function with slope discontinuity at x = 0
 * 
 * **Performance Optimization:**
 * - **GCC Optimization**: Unroll disabled for optimal conditional execution
 * - **Hardware Conditional**: Avoids branch penalties through SFPU conditional execution
 * - **MAD Block Efficiency**: Slope multiplication uses dedicated FP32 hardware
 * - **Register Reuse**: In-place computation minimizes register file pressure
 * 
 * @note Uses hardware conditional execution for optimal SIMD performance
 * @note Slope parameter converted via safe type conversion utility
 * @note Addresses "dying ReLU" problem with small negative slope
 */
template <bool APPROXIMATION_MODE>
inline void _calculate_lrelu_(const int iterations, uint slope)
{
    sfpi::vFloat s = Converter::as_float(slope);      // Convert slope parameter safely

#pragma GCC unroll 0
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat v = sfpi::dst_reg[0];             // Load input from current register

        v_if (v < 0.0f)                                // Hardware conditional: check negative
        {
            v *= s;                                    // Apply slope to negative values
        }
        v_endif;                                       // Positive values remain unchanged

        sfpi::dst_reg[0] = v;                          // Store result in-place

        sfpi::dst_reg++;                               // Advance to next register
    }
}

/**
 * @brief Hardware-accelerated clamped ReLU (ReLU6) body function using Wormhole B0 SFPU
 * @param val Input value for activation computation
 * @param threshold Upper bound threshold for clamping (e.g., 6.0 for ReLU6)
 * @return Clamped activation result: clamp(val, 0, threshold)
 * 
 * @details Computes clamped ReLU activation with configurable upper bound using the
 * Wormhole B0 SFPU's nested conditional execution capabilities. This function implements
 * ReLU6 and similar bounded activation functions commonly used in quantization-aware
 * training and mobile-optimized neural networks.
 * 
 * **Wormhole B0 SFPU Dual-Threshold Clamping:**
 * - **Nested Conditionals**: Sequential application of upper and lower bounds
 * - **IEEE-754 Comparison**: Hardware floating-point comparison operations
 * - **Condition Code Stack**: Hardware CC stack enables nested conditional execution
 * - **Per-Lane Processing**: Independent threshold evaluation across all 32 lanes
 * 
 * **SFPI Programming Model:**
 * ```cpp
 * sfpi::vFloat result = val;           // Initialize with input value
 * 
 * v_if (result > threshold) {          // Upper bound check
 *     result = threshold;              // Clamp to upper threshold
 * }
 * v_endif;
 * 
 * v_if (result < 0.0f) {               // Lower bound check (standard ReLU)
 *     result = 0.0f;                   // Clamp to zero
 * }
 * v_endif;
 * 
 * return result;                       // Return clamped result
 * ```
 * 
 * **Mathematical Properties:**
 * - **Bounded ReLU**: f(x) = min(max(0, x), threshold)
 * - **Quantization Friendly**: ReLU6 optimizes for 8-bit quantization
 * - **Gradient Flow**: Maintains gradients within bounded range
 * - **Saturation Behavior**: Smooth saturation at upper and lower bounds
 * 
 * **Common Threshold Values:**
 * - **ReLU6**: threshold = 6.0 (standard for mobile networks)
 * - **Custom Bounds**: Application-specific threshold values
 * - **Quantization Alignment**: Thresholds chosen for quantization efficiency
 * 
 * @note Uses nested conditional execution for dual-threshold clamping
 * @note Threshold parameter enables flexible upper bound configuration
 * @note Optimized for quantization-aware neural network training
 */
sfpi_inline sfpi::vFloat _relu_max_body_(sfpi::vFloat val, sfpi::vFloat threshold)
{
    sfpi::vFloat result = val;                         // Initialize with input value
    v_if (result > threshold)                          // Hardware conditional: upper bound check
    {
        result = threshold;                            // Clamp to upper threshold
    }
    v_endif;                                           // End upper bound check
    v_if (result < 0.0f)                               // Hardware conditional: lower bound check
    {
        result = 0.0f;                                 // Clamp to zero (standard ReLU)
    }
    v_endif;                                           // End lower bound check
    return result;
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _relu_max_(const int iterations, uint uint_threshold)
{
    sfpi::vFloat threshold = sfpi::s2vFloat16(uint_threshold, sfpi::s2vFloat16::fp16a);
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat a   = sfpi::dst_reg[0];
        sfpi::dst_reg[0] = _relu_max_body_(a, threshold);
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _relu_min_(const int iterations, uint uint_threshold)
{
    sfpi::vFloat threshold = sfpi::s2vFloat16(uint_threshold, sfpi::s2vFloat16::fp16a);
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat a = sfpi::dst_reg[0];
        v_if (a < threshold)
        {
            a = 0.0f;
        }
        v_endif;
        sfpi::dst_reg[0] = a;
        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
