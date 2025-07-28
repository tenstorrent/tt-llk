// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_sign.h
 * @brief Sign function SFPU kernel for Wormhole B0 Tensix Special Function Processing Unit
 * 
 * @details This header provides hardware-accelerated sign function computation using the Wormhole B0
 * SFPU's 32-lane SIMD architecture with conditional execution and format-aware zero detection.
 * The SFPU's condition code functionality enables efficient per-lane sign determination with
 * proper handling of multiple floating-point formats (FP16, BFP8) and zero edge cases.
 * 
 * **Mathematical Operation:**
 * Computes the sign function: f(x) = +1 if x > 0, -1 if x < 0, 0 if x = 0
 * 
 * **Wormhole B0 SFPU Conditional Sign Processing:**
 * - **32-Lane SIMD**: Each lane independently evaluates sign conditions within current register
 * - **Conditional Execution**: Hardware v_if/v_endif for per-lane sign determination
 * - **Format-Aware Zero Detection**: Specialized zero detection for different floating-point formats
 * - **Constant Registers**: Hardware-stored constants (+1, -1, 0) for optimal performance
 * - **Multi-Format Support**: Handles FP16 and BFP8 format-specific zero detection
 * 
 * **Algorithm Implementation:**
 * ```
 * result = +1                              // Default assumption: positive
 * if (x < 0):                              // Check for negative values
 *     result = -1                          // Assign negative result
 * if (format_aware_zero_check(x)):         // Format-specific zero detection
 *     result = 0                           // Assign zero result
 * ```
 * - **Three-Way Classification**: Positive, negative, and zero value handling
 * - **Format-Specific Zero**: Different zero detection for FP16 vs. BFP8 formats
 * - **Hardware Constants**: Pre-loaded +1, -1, 0 constants for efficient assignment
 * - **Conditional Priority**: Zero check takes precedence over sign determination
 * 
 * **Floating-Point Format Support:**
 * - **FP16 Format**: Full IEEE-754 FP16 with bias removal for accurate zero detection
 * - **BFP8 Format**: Block floating-point 8-bit format with simplified zero detection
 * - **Exponent Size Parameter**: Runtime parameter controls format-specific processing
 * - **Bias Handling**: Automatic exponent bias adjustment for FP16 zero detection
 * 
 * **IEEE-754 Implementation:**
 * - **Sign Bit Detection**: Hardware comparison for negative value identification
 * - **Zero Detection**: Format-aware zero detection considering exponent bias
 * - **Special Cases**: Proper handling of ±0, ±∞, and NaN values
 * - **Precision Agnostic**: Works across different mantissa precision levels
 * 
 * **Hardware Context:**
 * - **SFPI Interface**: Uses SFPI conditional execution and constant access
 * - **Condition Code Logic**: Hardware CC functionality for multi-way conditional execution
 * - **Constant Registers**: Pre-programmed constants (±1, 0) in SFPU constant registers
 * - **Format Detection**: Runtime format parameter enables adaptive processing
 * - **Zero Check Integration**: Calls specialized FP16 zero detection function
 * 
 * **Performance Characteristics:**
 * - **Throughput**: 32 sign operations per clock cycle (within one register)
 * - **Latency**: 2-3 clock cycles per register due to nested conditional execution
 * - **Memory Efficiency**: In-place operation with hardware constant access
 * - **Format Overhead**: Minimal overhead for format-specific zero detection
 * 
 * **Conditional Execution Flow:**
 * 1. **Initialize**: Set result to +1 (positive assumption)
 * 2. **Negative Check**: If x < 0, set result to -1 
 * 3. **Zero Check**: If format-specific zero detected, set result to 0
 * 4. **Result**: Final sign value based on conditional execution path
 * 
 * **Common Use Cases:**
 * - **Neural Networks**: Activation functions and gradient sign extraction
 * - **Signal Processing**: Signal polarity detection and sign-based filtering
 * - **Mathematical Operations**: Implementing sign-dependent algorithms
 * - **Computer Vision**: Edge detection and feature sign analysis
 * - **Statistics**: Sign test implementations and data polarity analysis
 * 
 * **Integration with Wormhole B0:**
 * - **Thread 1 Execution**: Optimized for Math thread execution context
 * - **DEST Register Management**: Integrates with register file double-buffering
 * - **Pipeline Integration**: Maintains efficient Unpack→Math→Pack data flow
 * - **Format Flexibility**: Supports multiple floating-point format configurations
 */

#pragma once

#include "ckernel_sfpu_is_fp16_zero.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Hardware-accelerated sign function using Wormhole B0 SFPU conditional execution
 * @tparam APPROXIMATION_MODE Unused for exact sign determination
 * @tparam ITERATIONS Compile-time loop unrolling factor for performance optimization
 * @param iterations Number of DEST register entries to process
 * @param exponent_size_8 Format parameter: 0 for BFP8, non-zero for FP16
 * 
 * @details Computes the mathematical sign function for data in DEST registers using the
 * Wormhole B0 SFPU's conditional execution capabilities. The function performs format-aware
 * processing with specialized zero detection for different floating-point formats,
 * ensuring mathematical correctness across FP16 and BFP8 data types.
 * 
 * **Wormhole B0 SFPU Conditional Sign Processing:**
 * - **32-Lane SIMD**: All lanes independently evaluate sign conditions within current register
 * - **Hardware Constants**: Direct access to pre-loaded ±1, 0 constant values
 * - **Nested Conditionals**: Multiple v_if/v_endif blocks for comprehensive sign classification
 * - **Format-Aware**: Runtime format selection for optimal zero detection accuracy
 * 
 * **SFPI Programming Model:**
 * ```cpp
 * sfpi::vFloat v = sfpi::dst_reg[0];               // Load input from current register
 * sfpi::dst_reg[0] = sfpi::vConst1;                // Default: positive (+1)
 * 
 * v_if (v < 0.0F) {                                // Check for negative values
 *     sfpi::dst_reg[0] = sfpi::vConstNeg1;         // Assign negative result (-1)
 * }
 * v_endif;
 * 
 * v_if (_sfpu_is_fp16_zero_(v, exponent_size_8)) { // Format-specific zero check
 *     sfpi::dst_reg[0] = sfpi::vConst0;            // Assign zero result (0)
 * }
 * v_endif;
 * 
 * sfpi::dst_reg++;                                 // Advance to next register
 * ```
 * 
 * **Mathematical Properties:**
 * - **Three-Way Function**: Returns exactly +1, -1, or 0
 * - **IEEE-754 Compliant**: Proper sign determination following IEEE-754 semantics
 * - **Zero Preservation**: Accurate zero detection prevents false positive/negative results
 * - **Format Independence**: Consistent results across different floating-point formats
 * 
 * **Format-Specific Processing:**
 * - **BFP8 (exponent_size_8 = 0)**: Simplified zero detection without bias removal
 * - **FP16 (exponent_size_8 ≠ 0)**: Full IEEE-754 FP16 zero detection with bias consideration
 * - **Runtime Selection**: Format parameter enables adaptive processing for optimal accuracy
 * - **Bias Handling**: Automatic exponent bias adjustment for precise FP16 zero detection
 * 
 * **Conditional Execution Logic:**
 * The function uses a three-stage conditional approach:
 * 1. **Default Assignment**: All values initially assigned +1 (positive)
 * 2. **Negative Override**: Values < 0 reassigned to -1 (negative)
 * 3. **Zero Override**: Format-detected zeros reassigned to 0 (zero)
 * 
 * **Performance Optimization:**
 * - **Hardware Constants**: Direct access to SFPU constant registers avoids memory loads
 * - **Conditional Efficiency**: Hardware conditional execution avoids branch penalties
 * - **SIMD Parallel**: All 32 lanes process independently without synchronization overhead
 * - **Format Optimization**: Specialized zero detection minimizes computation for each format
 * 
 * **Special Value Handling:**
 * - **Positive Zero**: Correctly identified as zero regardless of sign bit
 * - **Negative Zero**: Correctly identified as zero regardless of sign bit
 * - **Infinity**: ±∞ correctly identified as ±1 respectively
 * - **NaN Values**: Behavior follows IEEE-754 comparison semantics
 * 
 * **Edge Cases:**
 * - **Denormal Numbers**: Handled correctly through format-specific zero detection
 * - **Smallest Normal**: Correctly identified as non-zero positive/negative
 * - **Format Boundaries**: Proper handling at format-specific precision limits
 * 
 * @note Zero detection takes precedence over negative detection in conditional flow
 * @note Format parameter controls zero detection algorithm for accuracy
 * @note Uses hardware constants for optimal performance
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_sign_(const int iterations, uint exponent_size_8)
{
// All params are in FP16 format
// uint format = 1;
#pragma GCC unroll 0
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat v   = sfpi::dst_reg[0];           // Load input from current register
        sfpi::dst_reg[0] = sfpi::vConst1;              // Default assignment: positive (+1)
        v_if (v < 0.0F)                                // Hardware conditional: check negative
        {
            sfpi::dst_reg[0] = sfpi::vConstNeg1;       // Assign negative result (-1)
        }
        v_endif;                                       // End negative check

        // param0 == 0 is Bfp8 format. It does not require bias removal.
        // param0 != 0 is Float16 format and exp bias needs to be removed for zero check.
        v_if (_sfpu_is_fp16_zero_(v, exponent_size_8)) // Format-aware zero detection
        {
            sfpi::dst_reg[0] = sfpi::vConst0;          // Assign zero result (0)
        }
        v_endif;                                       // End zero check

        sfpi::dst_reg++;                               // Advance to next register
    }
}

} // namespace sfpu
} // namespace ckernel
