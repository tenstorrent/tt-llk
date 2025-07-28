// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Compute element-wise square (x²) using SFPU hardware
 *
 * @details Performs element-wise square computation on FP32 data using the
 * Tensix Vector Unit's 32-lane SIMD engine. Implements IEEE-754 compliant
 * multiplication operation to compute x * x for each element.
 *
 * **Mathematical Operation:**
 * For each lane i in 32-lane SIMD:
 * ```
 * output[i] = input[i]² = input[i] * input[i]
 * ```
 *
 * **IEEE-754 Implementation:**
 * - Uses hardware floating-point multiplication for exact results
 * - Preserves all IEEE-754 semantics for multiplication
 * - Handles special cases correctly:
 *   - (+∞)² = +∞
 *   - (-∞)² = +∞
 *   - (±0)² = +0
 *   - NaN² = NaN
 *   - (-x)² = x² (always produces positive result for finite inputs)
 * - Maintains full precision for intermediate and final results
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses dedicated SFPU floating-point multiplication unit
 * - Processes 32 values simultaneously across SIMD lanes
 * - Single multiplication operation per lane
 * - Optimized with GCC unroll directive for performance
 *
 * **Performance Characteristics:**
 * - Latency: 1-2 cycles per SIMD operation (multiplication)
 * - Throughput: 1 instruction per cycle
 * - Peak Rate: 32 square operations per cycle @ 1GHz = 32 GOps/sec
 * - Memory Pattern: Single-input, single-output with in-place result storage
 * - Loop unrolled by factor of 8 for optimal pipeline utilization
 *
 * **Common Use Cases:**
 * - L2 norm calculations in neural networks
 * - Variance and standard deviation computations
 * - Quadratic loss functions (MSE, RMSE)
 * - Energy/power calculations in signal processing
 * - Euclidean distance computations
 * - Polynomial feature generation in machine learning
 * - Activation function implementations (x² gates)
 *
 * **SFPU Microcode:**
 * ```cpp
 * #pragma GCC unroll 8                        // Optimize loop unrolling
 * for (each_iteration) {
 *     sfpi::vFloat in = sfpi::dst_reg[0];      // Load input (32 lanes)
 *     sfpi::vFloat result = in * in;           // Hardware multiplication
 *     sfpi::dst_reg[0] = result;               // Store result
 *     sfpi::dst_reg++;                         // Advance to next register
 * }
 * ```
 *
 * @tparam APPROXIMATION_MODE If true, may use faster approximation (not applicable for square).
 *                            If false, uses exact IEEE-754 compliant multiplication.
 * @tparam ITERATIONS Template parameter defining loop unroll factor for compilation.
 *
 * @param iterations Number of SIMD operations to perform (typically 8 for full tile face)
 *
 * @note Input values must be in FP32 format in SFPU destination registers
 * @note Operation is performed in-place, overwriting input data with squared result
 * @note Function processes one tile face at a time (32 values per iteration)
 * @note No initialization required - operation is stateless
 * @note Loop unrolled by factor of 8 for optimal performance on Tensix architecture
 * @note Always produces non-negative results for finite real inputs
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_square_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat in     = sfpi::dst_reg[0];  // Load input (32-lane vector)
        sfpi::vFloat result = in * in;           // Hardware floating-point multiplication

        sfpi::dst_reg[0] = result;               // Store squared result

        sfpi::dst_reg++;                         // Advance to next 32-element vector
    }
}

} // namespace sfpu
} // namespace ckernel
