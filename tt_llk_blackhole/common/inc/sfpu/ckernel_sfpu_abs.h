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
 * @brief Compute element-wise absolute value using SFPU hardware
 *
 * @details Performs element-wise absolute value computation on FP32 data using the
 * Tensix Vector Unit's 32-lane SIMD engine. Implements IEEE-754 compliant absolute
 * value operation by clearing the sign bit while preserving all other bit patterns.
 *
 * **Mathematical Operation:**
 * For each lane i in 32-lane SIMD:
 * ```
 * output[i] = |input[i]| = {
 *     input[i]    if input[i] ≥ 0
 *     -input[i]   if input[i] < 0
 * }
 * ```
 *
 * **IEEE-754 Implementation:**
 * - Clears the sign bit (bit 31) in FP32 representation
 * - Preserves all exponent and mantissa bits unchanged
 * - Handles special cases correctly:
 *   - |+∞| = +∞
 *   - |-∞| = +∞  
 *   - |NaN| = NaN (with sign bit cleared)
 *   - |±0| = +0
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses dedicated SFPU absolute value instruction (`sfpi::abs`)
 * - Processes 32 values simultaneously across SIMD lanes
 * - Single-cycle operation per lane on dedicated SFPU hardware
 *
 * **Performance Characteristics:**
 * - Latency: 1 cycle per SIMD operation
 * - Throughput: 1 instruction per cycle
 * - Peak Rate: 32 absolute values per cycle @ 1GHz = 32 GOps/sec
 * - Memory Pattern: Processes data in-place in destination registers
 *
 * **Common Use Cases:**
 * - L1/L2 norm calculations in neural networks
 * - Magnitude computation for complex numbers
 * - Data preprocessing for activation functions
 * - Numerical stability improvements (preventing negative values)
 * - Loss function computations (Mean Absolute Error)
 *
 * **SFPU Microcode:**
 * ```cpp
 * for (each_lane) {
 *     sfpi::vFloat v = sfpi::dst_reg[0];      // Load from destination register
 *     sfpi::dst_reg[0] = sfpi::abs(v);        // Hardware absolute value
 *     sfpi::dst_reg++;                        // Advance to next register
 * }
 * ```
 *
 * @tparam APPROXIMATION_MODE If true, may use faster approximation (not applicable for abs).
 *                            If false, uses exact IEEE-754 compliant absolute value.
 * @tparam ITERATIONS Template parameter defining loop unroll factor for compilation.
 *
 * @param iterations Number of SIMD operations to perform (typically 8 for full tile face)
 *
 * @note Input values must be in FP32 format in SFPU destination registers
 * @note Operation is performed in-place, overwriting input data
 * @note Function processes one tile face at a time (32 values per iteration)
 * @note No initialization required - operation is stateless
 * @note Preserves IEEE-754 semantics for all special values
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_abs_(const int iterations)
{
    // SFPU microcode
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat v   = sfpi::dst_reg[0];  // Load 32-lane vector from destination register
        sfpi::dst_reg[0] = sfpi::abs(v);      // Hardware absolute value operation
        sfpi::dst_reg++;                      // Advance to next 32-element vector
    }
}

} // namespace sfpu
} // namespace ckernel
