// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Compute element-wise Hardtanh (Hard Hyperbolic Tangent) activation function using SFPU hardware
 *
 * @details Performs element-wise Hardtanh activation computation on FP32 data using the
 * Tensix Vector Unit's 32-lane SIMD engine. Implements a piecewise linear approximation
 * of the hyperbolic tangent function with configurable thresholds, providing bounded
 * output with efficient computation.
 *
 * **Mathematical Operation:**
 * For each lane i in 32-lane SIMD:
 * ```
 * output[i] = Hardtanh(input[i]) = {
 *     neg_threshold    if input[i] < neg_threshold
 *     input[i]         if neg_threshold ≤ input[i] ≤ pos_threshold
 *     pos_threshold    if input[i] > pos_threshold
 * }
 * 
 * Typically: neg_threshold = -1, pos_threshold = +1
 * Result: bounded linear function approximating tanh(x)
 * 
 * Properties:
 * - Hardtanh(0) = 0
 * - Output bounded to [neg_threshold, pos_threshold]
 * - Linear in the middle region with slope = 1
 * - Saturated (constant) outside the threshold region
 * - Efficient approximation of tanh with simpler computation
 * ```
 *
 * **Parameter Encoding (Specialized Format):**
 * The function uses a specialized parameter encoding scheme:
 * ```
 * param0 = -(neg_threshold)              // Negative of lower bound
 * param1 = -(pos_threshold - neg_threshold)  // Negative of threshold difference
 * param2 = -(pos_threshold)              // Negative of upper bound
 * 
 * Algorithm steps:
 * 1. val += param0           // Shift by negative lower bound
 * 2. Clamp val to [0, ∞)     // Remove values below lower bound
 * 3. val += param1           // Shift by negative difference
 * 4. Clamp val to (-∞, 0]    // Remove values above upper bound
 * 5. val += param2           // Final shift to correct range
 * ```
 *
 * **Algorithm Implementation:**
 * Uses a three-step clamping process with parameter addition:
 * - Step 1: Translate and clamp lower bound to zero
 * - Step 2: Translate and clamp upper bound to zero  
 * - Step 3: Final translation to target range
 * - Efficient implementation using conditional execution
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses SFPU conditional execution (`v_if`/`v_endif`) for clamping
 * - Processes 32 values simultaneously across SIMD lanes
 * - Parameters in FP16_B format for memory efficiency
 * - No loop unrolling optimization (marked with #pragma GCC unroll 0)
 *
 * **Performance Characteristics:**
 * - Latency: ~5-6 cycles per SIMD operation (3 additions + 2 clamps)
 * - Throughput: 1 instruction per cycle
 * - Peak Rate: ~5-6 GOps/sec @ 1GHz
 * - Memory Pattern: Single-input, single-output with in-place result storage
 * - Performance independent of input value distribution
 *
 * **Common Use Cases:**
 * - Neural network activation function with bounded output
 * - Efficient approximation of tanh for mobile/edge computing
 * - Gradient clipping and stabilization in training
 * - Signal processing applications requiring bounded responses
 * - Quantized neural networks with limited precision requirements
 * - Real-time systems where computational efficiency is critical
 * - Embedded AI applications with power constraints
 *
 * **Activation Function Comparison:**
 * - **vs Tanh**: Much faster computation, piecewise linear approximation
 * - **vs ReLU**: Bounded output, symmetric around zero
 * - **vs Clamp**: Specialized for tanh-like behavior with identity middle region
 * - **vs Sigmoid**: Symmetric around zero, different output range
 *
 * **SFPU Microcode:**
 * ```cpp
 * // Parameter conversion
 * sfpi::vFloat p0 = sfpi::s2vFloat16(param0);    // -(neg_threshold)
 * sfpi::vFloat p1 = sfpi::s2vFloat16(param1);    // -(pos_threshold - neg_threshold)
 * sfpi::vFloat p2 = sfpi::s2vFloat16(param2);    // -(pos_threshold)
 * 
 * #pragma GCC unroll 0
 * for (each_iteration) {
 *     sfpi::vFloat val = sfpi::dst_reg[0];       // Load input (32 lanes)
 *     
 *     val += p0;                                 // Step 1: Translate by -neg_threshold
 *     v_if (val < 0.0f) {                        // Clamp lower bound
 *         val = 0.0f;
 *     }
 *     v_endif;
 *     
 *     val += p1;                                 // Step 2: Translate by -difference
 *     v_if (val >= 0.0f) {                       // Clamp upper bound
 *         val = 0.0f;
 *     }
 *     v_endif;
 *     
 *     val += p2;                                 // Step 3: Final translation
 *     
 *     sfpi::dst_reg[0] = val;                    // Store result
 *     sfpi::dst_reg++;                           // Advance to next register
 * }
 * ```
 *
 * @tparam APPROXIMATION_MODE Not applicable for hardtanh (always exact piecewise linear).
 * @tparam ITERATIONS Template parameter defining loop unroll factor for compilation.
 *
 * @param iterations Number of SIMD operations to perform (typically 8 for full tile face)
 * @param param0 Encoded parameter: -(neg_threshold) in FP16_B format
 * @param param1 Encoded parameter: -(pos_threshold - neg_threshold) in FP16_B format
 * @param param2 Encoded parameter: -(pos_threshold) in FP16_B format
 *
 * @note Input values must be in FP32 format in SFPU destination registers
 * @note Operation is performed in-place, overwriting input data with hardtanh result
 * @note Function processes one tile face at a time (32 values per iteration)
 * @note Parameters use specialized encoding scheme for efficient computation
 * @note No initialization required - operation is stateless
 * @note Loop unrolling disabled for this implementation
 * @note For standard hardtanh with bounds [-1, +1]:
 *       param0 = -(-1) = 1, param1 = -(1-(-1)) = -2, param2 = -(1) = -1
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_hardtanh_(const int iterations, uint param0, uint param1, uint param2)
{
    // All params are in FP16_B format
    // param0 = -(neg_threshold)
    // param1 = -(pos_threshold - neg_threshold)
    // param2 = -(pos_threshold)

    sfpi::vFloat p0 = sfpi::s2vFloat16(param0);    // Convert parameter 0: -(neg_threshold)
    sfpi::vFloat p1 = sfpi::s2vFloat16(param1);    // Convert parameter 1: -(pos_threshold - neg_threshold)
    sfpi::vFloat p2 = sfpi::s2vFloat16(param2);    // Convert parameter 2: -(pos_threshold)
// SFPU microcode
#pragma GCC unroll 0
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat val = sfpi::dst_reg[0];        // Load input (32-lane vector)

        val += p0; // 12 bits                      // Step 1: Translate by -(neg_threshold)
        v_if (val < 0.0f)                           // Clamp values below zero (lower bound)
        {
            val = 0.0f;                             // Set to zero if below lower bound
        }
        v_endif;

        val += p1; // 12 bits                      // Step 2: Translate by -(pos_threshold - neg_threshold)
        v_if (val >= 0.0f)                          // Clamp values at or above zero (upper bound)
        {
            val = 0.0f;                             // Set to zero if at or above upper bound
        }
        v_endif;

        val += p2; // 12 bits                      // Step 3: Final translation by -(pos_threshold)

        sfpi::dst_reg[0] = val;                     // Store hardtanh result

        sfpi::dst_reg++;                            // Advance to next 32-element vector
    }
}

} // namespace sfpu
} // namespace ckernel
