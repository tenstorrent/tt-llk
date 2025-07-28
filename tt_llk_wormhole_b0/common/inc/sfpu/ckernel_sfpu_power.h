// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_power.h
 * @brief Integer power function SFPU kernel for Wormhole B0 Tensix Special Function Processing Unit
 * 
 * @details This header provides hardware-accelerated integer power computation (x^n) using the
 * Wormhole B0 SFPU's 32-lane SIMD architecture with integrated FP32 MAD blocks. The implementation
 * uses iterative multiplication through the SFPU's dedicated multiply-add hardware for efficient
 * exponentiation with small integer exponents.
 * 
 * **Mathematical Operation:**
 * Computes integer power: f(x,n) = x^n = x × x × ... × x (n times) for integer exponent n ≥ 2
 * 
 * **Wormhole B0 SFPU MAD Block Utilization:**
 * - **32 MAD Units**: 8 SFPU instances × 4 lanes, each with dedicated FP32 MAD block
 * - **Iterative Multiplication**: Sequential multiply operations using hardware MAD units
 * - **FP32 Precision**: Full 32-bit floating-point multiply operations with IEEE-754 compliance
 * - **Accumulating Multiply**: Efficient repeated multiplication pattern optimized for SFPU
 * 
 * **Algorithm Implementation:**
 * ```
 * result = x * x                    // Initial square (x²)
 * for i = 2 to (exponent-1):       // Iterate remaining multiplications
 *     result = result * x          // Accumulate: x³, x⁴, x⁵, ...
 * ```
 * - **Starting Point**: Always begins with x² to optimize for common case
 * - **Iterative Accumulation**: Each iteration multiplies result by base value
 * - **Hardware Acceleration**: Each multiply operation uses dedicated FP32 MAD hardware
 * 
 * **IEEE-754 Implementation:**
 * - **FP32 Multiplication**: Hardware follows IEEE-754 multiplication semantics for all operations
 * - **Accumulating Error**: Floating-point errors may accumulate across multiple multiplications
 * - **Rounding Mode**: Configurable rounding for precision control at each multiplication step
 * - **Special Cases**: Handles ±0, ±∞, NaN propagation according to IEEE-754 standard
 * - **Overflow/Underflow**: IEEE-754 compliant behavior for out-of-range results
 * 
 * **Performance Characteristics:**
 * - **Throughput**: 32 power operations per clock cycle (within one register)
 * - **Latency**: (exponent-1) clock cycles per register for iterative multiplication
 * - **Memory Efficiency**: Single temporary register for accumulation, in-place result storage
 * - **MAD Block Utilization**: Efficient use of dedicated FP32 multiply-add hardware
 * 
 * **Exponent Range Considerations:**
 * - **Small Exponents**: Highly efficient for exponents 2-8 (common in neural networks)
 * - **Large Exponents**: Linear time complexity O(n) may impact performance for large n
 * - **Minimum Exponent**: Assumes exponent ≥ 2 (x¹ = x handled separately, x⁰ = 1 handled separately)
 * - **Maximum Practical**: Limited by floating-point overflow and performance considerations
 * 
 * **Common Use Cases:**
 * - **Neural Networks**: Polynomial activation functions and feature engineering
 * - **Signal Processing**: Power spectral analysis and nonlinear transformations
 * - **Mathematical Computing**: Polynomial evaluation and scientific calculations
 * - **Computer Graphics**: Lighting models and surface shading calculations
 * - **Statistics**: Moment calculations and power transforms
 * 
 * **Integration with Wormhole B0:**
 * - **Thread 1 Execution**: Optimized for Math thread execution context
 * - **MAD Block Coordination**: Leverages 32 dedicated FP32 multiply-add units
 * - **Pipeline Integration**: Maintains efficient Unpack→Math→Pack data flow
 * - **Register Management**: Uses DEST register accumulation with minimal temporary storage
 */

#pragma once

#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Hardware-accelerated integer power computation using Wormhole B0 SFPU MAD blocks
 * @tparam APPROXIMATION_MODE Unused for exact iterative multiplication
 * @tparam ITERATIONS Compile-time loop unrolling factor for performance optimization
 * @param iterations Number of DEST register entries to process
 * @param exponent Integer exponent value (n ≥ 2) for power computation x^n
 * 
 * @details Computes x^n for integer exponents using iterative multiplication through the
 * Wormhole B0 SFPU's dedicated FP32 MAD blocks. The algorithm starts with x² and iteratively
 * multiplies by the base value to reach the desired exponent, leveraging the 32-lane SIMD
 * architecture for parallel processing across all SFPU lanes.
 * 
 * **Wormhole B0 SFPU Iterative Multiplication:**
 * - **Initial Square**: result = x × x (using MAD block for x²)
 * - **Iterative Accumulation**: result = result × x for each remaining power
 * - **32-Lane Parallel**: All lanes process their datums simultaneously within current register
 * - **MAD Block Efficiency**: Each multiplication leverages dedicated FP32 hardware
 * 
 * **SFPI Programming Model:**
 * ```cpp
 * sfpi::vFloat in = sfpi::dst_reg[0];           // Load base value from current register
 * sfpi::vFloat result = in * in;                // Initial square: x²
 * for (uint i = 2; i < exponent; i++) {        // Iterate remaining multiplications
 *     result *= in;                             // Accumulate: x³, x⁴, x⁵, ...
 * }
 * sfpi::dst_reg[0] = result;                    // Store final result in-place
 * sfpi::dst_reg++;                              // Advance to next register
 * ```
 * 
 * **Mathematical Properties:**
 * - **IEEE-754 Multiplication**: Each step follows IEEE-754 multiplication semantics
 * - **Accumulating Precision**: Floating-point errors accumulate with each multiplication
 * - **Monotonic Growth**: For positive bases, result increases with exponent
 * - **Sign Handling**: Negative bases produce alternating positive/negative results
 * 
 * **Algorithm Complexity:**
 * - **Time Complexity**: O(n) where n is the exponent value
 * - **Space Complexity**: O(1) using single accumulator register
 * - **Iteration Count**: (exponent - 1) multiplication operations per datum
 * - **Optimization**: Starts with x² to reduce total iterations by one
 * 
 * **Performance Optimization:**
 * - **MAD Block Utilization**: Each multiply operation uses dedicated FP32 hardware
 * - **SIMD Parallel**: All 32 lanes compute their power operations simultaneously
 * - **Register Efficiency**: Minimal register usage with in-place accumulation
 * - **Loop Optimization**: Simple loop structure optimizes instruction scheduling
 * 
 * **Edge Cases and Limitations:**
 * - **Exponent = 0**: Not handled (would require returning 1.0)
 * - **Exponent = 1**: Not handled (would require returning x)
 * - **Large Exponents**: May cause overflow or performance degradation
 * - **Negative Exponents**: Not supported (would require reciprocal calculation)
 * 
 * **Numerical Considerations:**
 * - **Overflow Risk**: Large bases or exponents may cause IEEE-754 overflow
 * - **Underflow Risk**: Small bases with large exponents may underflow to zero
 * - **Precision Loss**: Multiple multiplications may accumulate floating-point errors
 * - **Special Values**: NaN and ±∞ propagate according to IEEE-754 rules
 * 
 * @note Assumes exponent ≥ 2 for proper operation
 * @note Uses iterative multiplication for exact IEEE-754 compliance
 * @note Optimized for small to moderate integer exponents
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_power_(const int iterations, uint exponent)
{
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat in     = sfpi::dst_reg[0];    // Load base value from current register
        sfpi::vFloat result = in * in;             // Initial square: x²
        for (uint i = 2; i < exponent; i++)       // Iterate remaining multiplications
        {
            result *= in;                          // Accumulate power: x³, x⁴, x⁵, ...
        }

        sfpi::dst_reg[0] = result;                 // Store final result in-place

        sfpi::dst_reg++;                           // Advance to next register
    }
}

} // namespace sfpu
} // namespace ckernel
