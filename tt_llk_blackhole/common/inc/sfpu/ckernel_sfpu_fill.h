// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_ops.h"
#include "ckernel_sfpu_converter.h"
#include "ckernel_sfpu_load_config.h"

namespace ckernel::sfpu
{

/**
 * @brief Fill SFPU destination registers with a constant floating-point value
 *
 * @details Performs element-wise fill operation on FP32 data using the Tensix Vector Unit's
 * 32-lane SIMD engine. Sets all destination register elements to the specified constant
 * floating-point value, commonly used for tensor initialization and constant operations.
 *
 * **Mathematical Operation:**
 * For each lane i in 32-lane SIMD:
 * ```
 * output[i] = value (constant for all lanes and iterations)
 * ```
 *
 * **Algorithm Implementation:**
 * - Single constant value broadcasted to all 32 SIMD lanes
 * - No input dependency - purely generative operation
 * - Constant value loaded once and reused across all iterations
 * - Efficient initialization without memory bandwidth requirements
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses SFPU register assignment for constant broadcasting
 * - Processes 32 values simultaneously across SIMD lanes
 * - Template-based iteration count for compile-time optimization
 * - No conditional execution or complex arithmetic required
 *
 * **Performance Characteristics:**
 * - Latency: ~1 cycle per SIMD operation (simple register assignment)
 * - Throughput: 1 instruction per cycle
 * - Peak Rate: 32 fill operations per cycle @ 1GHz = 32 GOps/sec
 * - Memory Pattern: Zero-input, full-output (purely generative)
 * - Performance independent of fill value
 *
 * **Common Use Cases:**
 * - Tensor initialization with constant values (zeros, ones, etc.)
 * - Bias addition preparation in neural networks
 * - Gradient initialization for optimization algorithms
 * - Mask generation for attention mechanisms
 * - Padding value insertion in convolution operations
 * - Background value setting for image processing
 * - Constant term generation in mathematical expressions
 *
 * **SFPU Microcode:**
 * ```cpp
 * sfpi::vFloat fill_val = value;                 // Load constant value
 * 
 * for (each_iteration) {
 *     sfpi::dst_reg[0] = fill_val;               // Broadcast to all 32 lanes
 *     sfpi::dst_reg++;                           // Advance to next register
 * }
 * ```
 *
 * @tparam APPROXIMATION_MODE Not applicable for fill operation (always exact).
 * @tparam ITERATIONS Template parameter defining fixed loop iteration count.
 *
 * @param value Constant floating-point value to fill into all destination elements
 *
 * @note Function processes ITERATIONS worth of 32-element vectors
 * @note No runtime iteration parameter - uses template ITERATIONS value
 * @note No initialization required - operation is stateless
 * @note Overwrites any existing data in destination registers
 * @note Value is broadcast to all SIMD lanes simultaneously
 * @note Highly efficient for large-scale constant initialization
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_fill_(const float value)
{
    // SFPU microcode
    sfpi::vFloat fill_val = value;                  // Load constant value for broadcasting

    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::dst_reg[0] = fill_val;                // Broadcast constant to all 32 lanes
        sfpi::dst_reg++;                            // Advance to next 32-element vector
    }
}

/**
 * @brief Fill SFPU destination registers with a constant integer value
 *
 * @details Performs element-wise fill operation on integer data using the Tensix Vector Unit's
 * 32-lane SIMD engine. Sets all destination register elements to the specified constant
 * integer value, optimized for integer tensor initialization and discrete operations.
 *
 * **Mathematical Operation:**
 * For each lane i in 32-lane SIMD:
 * ```
 * output[i] = value (constant integer for all lanes and iterations)
 * ```
 *
 * **Algorithm Implementation:**
 * - Single constant integer value broadcasted to all 32 SIMD lanes
 * - Integer data path for exact discrete value representation
 * - No input dependency - purely generative operation
 * - Constant value loaded once and reused across all iterations
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses SFPU integer register assignment for constant broadcasting
 * - Processes 32 integer values simultaneously across SIMD lanes
 * - Template-based iteration count for compile-time optimization
 * - Dedicated integer data path for discrete operations
 *
 * **Performance Characteristics:**
 * - Latency: ~1 cycle per SIMD operation (simple register assignment)
 * - Throughput: 1 instruction per cycle
 * - Peak Rate: 32 fill operations per cycle @ 1GHz = 32 GOps/sec
 * - Memory Pattern: Zero-input, full-output (purely generative)
 * - Performance independent of fill value
 *
 * **Common Use Cases:**
 * - Index tensor initialization for attention mechanisms
 * - Label tensor preparation for classification tasks
 * - Mask value generation for sequence processing
 * - Counter initialization for iterative algorithms
 * - Discrete state initialization in state machines
 * - Integer constant generation for quantized operations
 * - Padding value insertion for discrete data
 *
 * @tparam APPROXIMATION_MODE Not applicable for fill operation (always exact).
 * @tparam ITERATIONS Template parameter defining fixed loop iteration count.
 *
 * @param value Constant integer value to fill into all destination elements
 *
 * @note Function processes ITERATIONS worth of 32-element vectors
 * @note No runtime iteration parameter - uses template ITERATIONS value
 * @note No initialization required - operation is stateless
 * @note Overwrites any existing data in destination registers
 * @note Value is broadcast to all SIMD lanes simultaneously
 * @note Uses integer data path for exact discrete representation
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_fill_int_(const int value)
{
    // SFPU microcode
    sfpi::vInt fill_val = value;                    // Load constant integer value for broadcasting

    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::dst_reg[0] = fill_val;                // Broadcast constant integer to all 32 lanes
        sfpi::dst_reg++;                            // Advance to next 32-element vector
    }
}

/**
 * @brief Fill SFPU destination registers with a constant value using bitcast interpretation
 *
 * @details Performs element-wise fill operation using bitcast conversion from uint32 to float
 * using the Tensix Vector Unit's 32-lane SIMD engine. Allows precise control over the bit
 * pattern of floating-point values, useful for special value generation and bit-level operations.
 *
 * **Mathematical Operation:**
 * For each lane i in 32-lane SIMD:
 * ```
 * output[i] = bitcast<float>(value_bit_mask) (reinterpret uint32 bits as float)
 * ```
 *
 * **Bitcast Operation:**
 * - Direct bit-level reinterpretation without numeric conversion
 * - Allows generation of special IEEE-754 values (NaN, infinity, denormals)
 * - Preserves exact bit patterns for precision-critical applications
 * - Enables creation of values not easily expressible as float literals
 *
 * **Algorithm Implementation:**
 * - Uses `Converter::as_float()` for safe bitcast operation
 * - Single constant bit pattern broadcasted to all 32 SIMD lanes
 * - No input dependency - purely generative operation
 * - Bit pattern loaded once and reused across all iterations
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses SFPU type conversion utilities for bitcast
 * - Processes 32 values simultaneously across SIMD lanes
 * - Template-based iteration count for compile-time optimization
 * - Leverages floating-point data path after bitcast conversion
 *
 * **Performance Characteristics:**
 * - Latency: ~1-2 cycles per SIMD operation (bitcast + assignment)
 * - Throughput: 1 instruction per cycle
 * - Peak Rate: 32 fill operations per cycle @ 1GHz = 32 GOps/sec
 * - Memory Pattern: Zero-input, full-output (purely generative)
 * - Slight overhead for bitcast conversion
 *
 * **Common Use Cases:**
 * - IEEE-754 special value generation (NaN, ±∞, ±0)
 * - Denormalized number creation for numerical testing
 * - Bit-pattern-specific floating-point values
 * - Debugging and testing with specific float representations
 * - Custom floating-point constant generation
 * - Precision-critical scientific computing applications
 * - Hardware validation with specific bit patterns
 *
 * **Special Value Examples:**
 * ```cpp
 * 0x00000000  // +0.0
 * 0x80000000  // -0.0
 * 0x7F800000  // +∞
 * 0xFF800000  // -∞
 * 0x7FC00000  // NaN (quiet)
 * 0x3F800000  // 1.0
 * 0xBF800000  // -1.0
 * ```
 *
 * @tparam APPROXIMATION_MODE Not applicable for fill operation (always exact).
 * @tparam ITERATIONS Template parameter defining fixed loop iteration count.
 *
 * @param value_bit_mask 32-bit value to be reinterpreted as floating-point bit pattern
 *
 * @note Function processes ITERATIONS worth of 32-element vectors
 * @note No runtime iteration parameter - uses template ITERATIONS value
 * @note No initialization required - operation is stateless
 * @note Overwrites any existing data in destination registers
 * @note Bit pattern is broadcast to all SIMD lanes simultaneously
 * @note Use with caution - can generate invalid or special floating-point values
 * @note Useful for testing and precision-critical applications
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_fill_bitcast_(const uint32_t value_bit_mask)
{
    // SFPU microcode
    sfpi::vFloat fill_val = Converter::as_float(value_bit_mask);  // Bitcast uint32 to float

    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::dst_reg[0] = fill_val;                // Broadcast bitcast constant to all 32 lanes
        sfpi::dst_reg++;                            // Advance to next 32-element vector
    }
}
} // namespace ckernel::sfpu
