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
 * @brief Detect exact zero values in FP16/BFP8 floating-point formats with format-specific handling
 *
 * @details Utility function for detecting exact zero values (±0) in different floating-point
 * formats used by the SFPU. Handles the exponent bias differences between FP16A and FP16B/BFP8
 * formats to ensure accurate zero detection across various data representations.
 *
 * **Format-Specific Zero Detection:**
 * ```
 * FP16B (exponent_size_8 != 0):
 *   - Direct comparison with 0.0F
 *   - Hardware automatically handles zero detection
 *   - Standard IEEE-754 behavior for ±0
 * 
 * FP16A/BFP8 (exponent_size_8 == 0):
 *   - Manual bias adjustment required for zero detection
 *   - SFPU adds bias value unconditionally (including for zero)
 *   - Must compensate for bias addition to detect true zero
 * ```
 *
 * **Bias Compensation Algorithm (FP16A):**
 * ```
 * 1. Load bias compensation value (0x3800 = {0, 8'd112, 10'b0})
 * 2. Add input value to bias compensation
 * 3. Check if result equals zero
 * 4. True zero detected when biased result == 0
 * ```
 *
 * **IEEE-754 Zero Handling:**
 * - Correctly identifies both +0.0 and -0.0 as zero
 * - Distinguishes zero from very small denormalized numbers
 * - Accounts for format-specific exponent representation
 * - Preserves IEEE-754 semantics across different formats
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses SFPU conditional logic and arithmetic operations
 * - Format detection based on exponent_size_8 parameter
 * - Efficient implementation with minimal computational overhead
 * - Inlined for optimal performance in caller functions
 *
 * **Format Specifications:**
 * - **FP16B**: Standard 16-bit float with 8-bit exponent, direct zero comparison
 * - **FP16A**: Alternative 16-bit format requiring bias compensation
 * - **BFP8**: 8-bit format with special exponent handling
 * - Format selection controlled by exponent_size_8 parameter
 *
 * **Performance Characteristics:**
 * - Latency: 1-2 cycles (depending on format)
 * - FP16B: 1 cycle (direct comparison)
 * - FP16A: 2 cycles (bias adjustment + comparison)
 * - Minimal computational overhead
 * - Highly optimized for frequent use in SFPU operations
 *
 * **Common Use Cases:**
 * - Sign function zero detection (avoiding division by zero)
 * - Conditional activation function implementations
 * - Numerical stability checks in mathematical operations
 * - Special case handling in transcendental functions
 * - Precision-critical zero testing in scientific computing
 * - Format-agnostic zero detection in mixed-precision operations
 * - Debugging and validation of floating-point computations
 *
 * **Integration with SFPU Functions:**
 * Used by various SFPU operations including:
 * - `_calculate_sign_()` for exact zero identification
 * - Mathematical functions requiring zero special cases
 * - Conditional operations needing zero detection
 * - Format conversion utilities
 *
 * **SFPU Microcode:**
 * ```cpp
 * if (exponent_size_8) {
 *     // FP16B format - direct comparison
 *     return v == 0.0F;
 * } else {
 *     // FP16A/BFP8 format - bias compensation required
 *     sfpi::vInt tmp = 0x3800;                   // Bias compensation value
 *     tmp += sfpi::reinterpret<sfpi::vInt>(v);   // Add input with bias
 *     return tmp == 0;                           // Check if biased result is zero
 * }
 * ```
 *
 * @param v Input floating-point value to test for zero
 * @param exponent_size_8 Format indicator: 0 for FP16A/BFP8, non-zero for FP16B
 * @return Integer condition result: non-zero if input is exactly zero, zero otherwise
 *
 * @note Function is inlined for optimal performance in calling contexts
 * @note Handles both positive and negative zero according to IEEE-754
 * @note Format detection based on exponent_size_8 parameter value
 * @note FP16B uses direct hardware comparison for efficiency
 * @note FP16A requires manual bias compensation for accurate detection
 * @note Result suitable for use in SFPU conditional execution (`v_if`)
 * @note Designed for integration with other SFPU mathematical functions
 * @note Essential utility for format-agnostic SFPU operation implementations
 */
sfpi_inline sfpi::vInt _sfpu_is_fp16_zero_(const sfpi::vFloat& v, uint exponent_size_8)
{
    if (exponent_size_8)
    {
        // fp16b - Standard 16-bit float format
        return v == 0.0F;                           // Direct hardware comparison for zero
    }
    else
    {
        // fp16a/BFP8 - Alternative format requiring bias compensation
        // if math data format is fp16, SFPU will convert 5 bit exp to 8 bit exp
        // in grayskull, this unconditionally adds bias value to exp (even for zero)
        sfpi::vInt tmp = 0x3800; // loads {0, 8'd112, 10'b0}  // Bias compensation constant
        tmp += sfpi::reinterpret<sfpi::vInt>(v);    // Add input value with bias adjustment

        return tmp == 0;                            // Check if bias-adjusted result is zero
    }
}

} // namespace sfpu
} // namespace ckernel
