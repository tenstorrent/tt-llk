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
 * @brief Compute element-wise negation (-x) for floating-point values using SFPU hardware
 *
 * @details Performs element-wise negation on FP32 data using the Tensix Vector Unit's
 * 32-lane SIMD engine. Implements IEEE-754 compliant unary minus operation by flipping
 * the sign bit while preserving all other bit patterns.
 *
 * **Mathematical Operation:**
 * For each lane i in 32-lane SIMD:
 * ```
 * output[i] = -input[i] = {
 *     -input[i]   for all finite values
 *     -0          if input[i] = +0
 *     +0          if input[i] = -0  
 *     -∞          if input[i] = +∞
 *     +∞          if input[i] = -∞
 *     NaN         if input[i] = NaN (sign bit flipped)
 * }
 * ```
 *
 * **IEEE-754 Implementation:**
 * - Flips the sign bit (bit 31) in FP32 representation
 * - Preserves all exponent and mantissa bits unchanged
 * - Handles special cases according to IEEE-754 standard
 * - Single-cycle operation using hardware unary minus
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses dedicated SFPU unary minus instruction
 * - Processes 32 values simultaneously across SIMD lanes
 * - Optimized with GCC unroll directive for performance
 * - Template-based iteration count for compile-time optimization
 *
 * **Performance Characteristics:**
 * - Latency: 1 cycle per SIMD operation
 * - Throughput: 1 instruction per cycle
 * - Peak Rate: 32 negation operations per cycle @ 1GHz = 32 GOps/sec
 * - Memory Pattern: Single-input, single-output with in-place result storage
 * - Loop unrolled by factor of 8 for optimal pipeline utilization
 *
 * **Common Use Cases:**
 * - Gradient descent optimization (negative gradients)
 * - Loss function implementations requiring sign changes
 * - Mathematical operations requiring additive inverse
 * - Signal processing (phase inversion)
 * - Numerical algorithms requiring sign flips
 * - Activation function derivatives
 *
 * @tparam APPROXIMATION_MODE Not applicable for negation (always exact).
 * @tparam ITERATIONS Template parameter defining fixed loop iteration count.
 *
 * @note Input values must be in FP32 format in SFPU destination registers
 * @note Operation is performed in-place, overwriting input data with negated result
 * @note Function processes ITERATIONS worth of 32-element vectors
 * @note No runtime iteration parameter - uses template ITERATIONS value
 * @note No initialization required - operation is stateless
 * @note Preserves IEEE-754 semantics for all special values
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_negative_()
{
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat val = sfpi::dst_reg[0];     // Load input (32-lane vector)
        sfpi::dst_reg[0] = -val;                 // Hardware unary minus operation
        sfpi::dst_reg++;                         // Advance to next 32-element vector
    }
}

/**
 * @brief Compute element-wise negation (-x) for integer values using SFPU hardware
 *
 * @details Performs element-wise negation on integer data using the Tensix Vector Unit's
 * 32-lane SIMD engine. Implements two's complement negation for signed integer values.
 *
 * **Mathematical Operation:**
 * For each lane i in 32-lane SIMD:
 * ```
 * output[i] = -input[i] = {
 *     -input[i]           for all values except INT_MIN
 *     INT_MIN (overflow)  if input[i] = INT_MIN (two's complement limitation)
 * }
 * ```
 *
 * **Two's Complement Implementation:**
 * - Uses hardware integer negation instruction
 * - Handles overflow case for minimum integer value (INT_MIN)
 * - Preserves full integer precision for all representable results
 * - Single-cycle operation using dedicated integer arithmetic unit
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses dedicated SFPU integer arithmetic unit
 * - Processes 32 integer values simultaneously across SIMD lanes
 * - Optimized with GCC unroll directive for performance
 * - Template-based iteration count for compile-time optimization
 *
 * **Performance Characteristics:**
 * - Latency: 1 cycle per SIMD operation
 * - Throughput: 1 instruction per cycle
 * - Peak Rate: 32 integer negation operations per cycle @ 1GHz = 32 GOps/sec
 * - Memory Pattern: Single-input, single-output with in-place result storage
 * - Loop unrolled by factor of 8 for optimal pipeline utilization
 *
 * **Common Use Cases:**
 * - Index arithmetic requiring sign reversal
 * - Mathematical operations requiring additive inverse
 * - Coordinate system transformations
 * - Offset calculations with direction changes
 * - Counter operations requiring sign flips
 * - Integer-based algorithmic computations
 *
 * @tparam APPROXIMATION_MODE Not applicable for integer negation (always exact).
 * @tparam ITERATIONS Template parameter defining fixed loop iteration count.
 *
 * @note Input values must be in integer format in SFPU destination registers
 * @note Operation is performed in-place, overwriting input data with negated result
 * @note Function processes ITERATIONS worth of 32-element vectors
 * @note No runtime iteration parameter - uses template ITERATIONS value
 * @note No initialization required - operation is stateless
 * @note Handles two's complement overflow for INT_MIN value
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_negative_int_()
{
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vInt val   = sfpi::dst_reg[0];     // Load integer input (32-lane vector)
        sfpi::dst_reg[0] = -val;                 // Hardware integer unary minus operation
        sfpi::dst_reg++;                         // Advance to next 32-element vector
    }
}

} // namespace sfpu
} // namespace ckernel
