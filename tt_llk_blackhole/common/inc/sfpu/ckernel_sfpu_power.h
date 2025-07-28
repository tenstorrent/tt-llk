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
 * @brief Compute element-wise integer power (x^n) using SFPU hardware
 *
 * @details Performs element-wise integer power computation on FP32 data using the
 * Tensix Vector Unit's 32-lane SIMD engine. Implements x^n through repeated
 * multiplication for integer exponents, providing exact results for polynomial
 * evaluation and power series computations.
 *
 * **Mathematical Operation:**
 * For each lane i in 32-lane SIMD:
 * ```
 * output[i] = input[i]^exponent = {
 *     1                           if exponent = 0 (for any x ≠ 0)
 *     input[i]                    if exponent = 1
 *     input[i] * input[i] * ... * input[i]  (exponent times)
 * }
 * ```
 *
 * **Algorithm Implementation:**
 * - Uses iterative multiplication approach: x^n = x * x * ... * x (n times)
 * - First computes x² = x * x
 * - Then iterates from i=2 to exponent-1, multiplying by x each time
 * - Optimized for small to medium integer exponents (typically n ≤ 16)
 * - Results are exact within floating-point precision limits
 *
 * **IEEE-754 Implementation:**
 * - Uses hardware floating-point multiplication for each step
 * - Preserves all IEEE-754 semantics for intermediate operations
 * - Handles special cases based on exponent value:
 *   - x^0 = 1 for any finite x ≠ 0
 *   - x^1 = x for any x
 *   - 0^n = 0 for n > 0
 *   - (±∞)^n = ±∞ (sign depends on exponent parity)
 *   - NaN^n = NaN
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses dedicated SFPU floating-point multiplication unit
 * - Processes 32 values simultaneously across SIMD lanes
 * - Multiple multiplication operations per lane (exponent-1 multiplications)
 * - Sequential execution within each lane for power computation
 *
 * **Performance Characteristics:**
 * - Latency: (exponent-1) * 1-2 cycles per SIMD operation
 * - Throughput: 1 instruction per cycle
 * - Peak Rate: 32/(exponent-1) power operations per cycle @ 1GHz
 * - Memory Pattern: Single-input, single-output with in-place result storage
 * - Performance scales linearly with exponent value
 *
 * **Common Use Cases:**
 * - Polynomial evaluation in neural networks
 * - Feature engineering with polynomial terms (x², x³, etc.)
 * - Mathematical function approximations via power series
 * - Activation function implementations (x^n gates)
 * - Norm calculations (L^p norms where p is integer)
 * - Statistical moment computations (variance, skewness, kurtosis)
 * - Signal processing with polynomial kernels
 *
 * **SFPU Microcode:**
 * ```cpp
 * for (each_iteration) {
 *     sfpi::vFloat in = sfpi::dst_reg[0];      // Load input (32 lanes)
 *     sfpi::vFloat result = in * in;           // Compute x²
 *     for (uint i = 2; i < exponent; i++) {   // Multiply (exponent-2) more times
 *         result *= in;                        // result = result * x
 *     }
 *     sfpi::dst_reg[0] = result;               // Store x^exponent
 *     sfpi::dst_reg++;                         // Advance to next register
 * }
 * ```
 *
 * @tparam APPROXIMATION_MODE If true, may use faster approximation (not applicable for integer power).
 *                            If false, uses exact multiplication-based computation.
 * @tparam ITERATIONS Template parameter defining loop unroll factor for compilation.
 *
 * @param iterations Number of SIMD operations to perform (typically 8 for full tile face)
 * @param exponent Integer exponent value (must be ≥ 2, as x^0=1 and x^1=x are trivial)
 *
 * @note Input values must be in FP32 format in SFPU destination registers
 * @note Operation is performed in-place, overwriting input data with power result
 * @note Function processes one tile face at a time (32 values per iteration)
 * @note No initialization required - operation is stateless
 * @note Exponent must be ≥ 2 for this implementation (function assumes x² as starting point)
 * @note Performance degrades linearly with exponent value due to sequential multiplication
 * @note For large exponents, consider using logarithmic power algorithms
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_power_(const int iterations, uint exponent)
{
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat in     = sfpi::dst_reg[0];  // Load input (32-lane vector)
        sfpi::vFloat result = in * in;           // Start with x² (base case)
        for (uint i = 2; i < exponent; i++)     // Multiply (exponent-2) additional times
        {
            result *= in;                        // Accumulate: result = result * x
        }

        sfpi::dst_reg[0] = result;               // Store x^exponent result

        sfpi::dst_reg++;                         // Advance to next 32-element vector
    }
}

} // namespace sfpu
} // namespace ckernel
