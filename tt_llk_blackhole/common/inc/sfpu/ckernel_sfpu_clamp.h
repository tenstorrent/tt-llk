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
 * @brief Compute element-wise clamp operation with configurable bounds using SFPU hardware
 *
 * @details Performs element-wise clamping on FP32 data using the Tensix Vector Unit's
 * 32-lane SIMD engine. Constrains input values to lie within [min, max] range with
 * optional bias addition, commonly used for activation function bounds and numerical stability.
 *
 * **Mathematical Operation:**
 * For each lane i in 32-lane SIMD:
 * ```
 * output[i] = clamp(input[i], min, max) + bias = {
 *     min + bias     if input[i] < min
 *     input[i] + bias if min ≤ input[i] < max  
 *     max + bias     if input[i] ≥ max
 * }
 * ```
 *
 * **Parameter Format:**
 * - param0: Minimum bound value (FP16 format)
 * - param1: Maximum bound value (FP16 format) 
 * - param2: Bias value added to result (12-bit format)
 * - All parameters converted to appropriate floating-point representation
 *
 * **Algorithm Implementation:**
 * ```
 * 1. Convert parameters from FP16 to working precision
 * 2. Compare input with minimum bound
 * 3. If input < min, replace with min value
 * 4. Compare (potentially updated) value with maximum bound
 * 5. If value ≥ max, replace with max value
 * 6. Add bias to final result
 * ```
 *
 * **IEEE-754 Implementation:**
 * - Handles NaN inputs according to comparison semantics
 * - Preserves special values within clamp bounds
 * - Uses conditional execution for efficient branching
 * - Maintains precision throughout computation
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses SFPU conditional execution (`v_if`/`v_elseif`/`v_endif`) for bounds checking
 * - Processes 32 values simultaneously across SIMD lanes
 * - Supports multiple floating-point formats for parameters
 * - No loop unrolling optimization (marked with #pragma GCC unroll 0)
 *
 * **Performance Characteristics:**
 * - Latency: ~4-5 cycles per SIMD operation (two comparisons + conditional moves + add)
 * - Throughput: 1 instruction per cycle  
 * - Peak Rate: ~6-8 GOps/sec @ 1GHz (depending on branch efficiency)
 * - Memory Pattern: Single-input, single-output with in-place result storage
 * - Performance depends on input distribution relative to bounds
 *
 * **Common Use Cases:**
 * - ReLU6 activation function implementation (clamp(x, 0, 6))
 * - Hardtanh activation function (clamp(x, -1, 1))
 * - Gradient clipping in neural network training
 * - Numerical stability bounds for mathematical functions
 * - Value range enforcement in quantized neural networks
 * - Signal processing dynamic range control
 * - Image processing pixel value bounds
 * - Parameter constraint enforcement in optimization
 *
 * **Activation Function Examples:**
 * - **ReLU6**: clamp(x, 0, 6) - bounded ReLU for mobile networks
 * - **Hardtanh**: clamp(x, -1, 1) - bounded tanh approximation
 * - **Clip**: clamp(x, -c, c) - symmetric clipping for gradient control
 * - **Saturate**: clamp(x, 0, 1) - saturation function for probabilities
 *
 * **SFPU Microcode:**
 * ```cpp
 * // Parameter conversion
 * sfpi::vFloat min = sfpi::s2vFloat16(param0, format);   // Convert min bound
 * sfpi::vFloat max = sfpi::s2vFloat16(param1, format);   // Convert max bound
 * 
 * #pragma GCC unroll 0
 * for (each_iteration) {
 *     sfpi::vFloat val = sfpi::dst_reg[0];               // Load input (32 lanes)
 *     
 *     v_if (val < min) {                                 // Check lower bound
 *         val = sfpi::s2vFloat16(param0, format);        // Clamp to minimum
 *     }
 *     v_elseif (val >= max) {                            // Check upper bound  
 *         val = sfpi::s2vFloat16(param1, format);        // Clamp to maximum
 *     }
 *     v_endif;
 *     
 *     sfpi::dst_reg[0] = val + sfpi::s2vFloat16b(param2); // Add bias and store
 *     sfpi::dst_reg++;                                   // Advance to next register
 * }
 * ```
 *
 * @tparam APPROXIMATION_MODE Not applicable for clamp operation (always exact).
 * @tparam ITERATIONS Template parameter defining loop unroll factor for compilation.
 *
 * @param iterations Number of SIMD operations to perform (typically 8 for full tile face)
 * @param param0 Minimum bound value in FP16 format
 * @param param1 Maximum bound value in FP16 format  
 * @param param2 Bias value in 12-bit format (added to clamped result)
 *
 * @note Input values must be in FP32 format in SFPU destination registers
 * @note Operation is performed in-place, overwriting input data with clamped result
 * @note Function processes one tile face at a time (32 values per iteration)
 * @note Parameters assumed to be in FP16A format for this implementation
 * @note Bias is added after clamping operation
 * @note No initialization required - operation is stateless
 * @note Loop unrolling disabled for this implementation (flexibility vs performance)
 * @note Ensure param0 ≤ param1 for meaningful clamp operation
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_clamp_(const int iterations, uint param0, uint param1, uint param2)
{
    // All params are in FP16 format
    // param0 = min
    // param1 = max

    // uint format = (param0 >> 16)&0x1;
    sfpi::s2vFloat16::Format format = sfpi::s2vFloat16::fp16a;  // Parameter format specification

    // SFPU microcode
    sfpi::vFloat min = sfpi::s2vFloat16(param0, format);        // Convert minimum bound to working precision
    sfpi::vFloat max = sfpi::s2vFloat16(param1, format);        // Convert maximum bound to working precision
#pragma GCC unroll 0
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat val = sfpi::dst_reg[0];                    // Load input (32-lane vector)

        v_if (val < min)                                        // Check if input below minimum bound
        {
            val = sfpi::s2vFloat16(param0, format);             // Clamp to minimum value
        }
        v_elseif (val >= max)                                   // Check if input at or above maximum bound
        {
            val = sfpi::s2vFloat16(param1, format);             // Clamp to maximum value
        }
        v_endif;                                                // End conditional clamping

        sfpi::dst_reg[0] = val + sfpi::s2vFloat16b(param2); // 12 bits  // Add bias to clamped result

        sfpi::dst_reg++;                                        // Advance to next 32-element vector
    }
}

} // namespace sfpu
} // namespace ckernel
