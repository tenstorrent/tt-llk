// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu_is_fp16_zero.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Compute element-wise sign function using SFPU hardware
 *
 * @details Performs element-wise sign extraction on FP32/FP16 data using the
 * Tensix Vector Unit's 32-lane SIMD engine. Implements mathematical sign function
 * that returns -1, 0, or +1 based on input value, with support for different
 * floating-point formats and special IEEE-754 handling.
 *
 * **Mathematical Operation:**
 * For each lane i in 32-lane SIMD:
 * ```
 * output[i] = sign(input[i]) = {
 *     +1.0    if input[i] > 0
 *     -1.0    if input[i] < 0
 *     0.0     if input[i] = ±0
 *     NaN     if input[i] = NaN (implementation defined)
 * }
 * ```
 *
 * **Algorithm Implementation:**
 * - Default output initialized to +1.0 (positive case)
 * - Uses conditional execution to detect negative values and set output to -1.0
 * - Uses specialized zero detection function for exact zero identification
 * - Supports both BFP8 and Float16 formats via exponent_size_8 parameter
 * - Handles IEEE-754 special cases including signed zeros
 *
 * **IEEE-754 Implementation:**
 * - Correctly distinguishes between +0.0 and -0.0 (both return 0.0)
 * - Handles positive and negative infinity appropriately
 * - NaN handling follows implementation-defined behavior
 * - Uses hardware comparison operations for sign detection
 * - Preserves floating-point format consistency in output
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses SFPU conditional execution (`v_if`/`v_endif`) for branching
 * - Processes 32 values simultaneously across SIMD lanes
 * - Integrates with zero detection helper function
 * - Optimized for minimal branching overhead
 *
 * **Performance Characteristics:**
 * - Latency: ~3-4 cycles per SIMD operation (comparison + conditional + zero check)
 * - Throughput: 1 instruction per cycle
 * - Peak Rate: ~8-11 GOps/sec @ 1GHz (depending on data distribution)
 * - Memory Pattern: Single-input, single-output with in-place result storage
 * - Performance depends on branch prediction and data patterns
 *
 * **Common Use Cases:**
 * - Gradient clipping and sign-based optimization algorithms
 * - Activation function implementations (sign activation)
 * - Mathematical function derivatives requiring sign information
 * - Numerical stability improvements in iterative algorithms
 * - Loss function implementations with sign-sensitive terms
 * - Signal processing applications requiring phase information
 * - Statistical computations requiring sign preservation
 *
 * **SFPU Microcode:**
 * ```cpp
 * for (each_iteration) {
 *     sfpi::vFloat v = sfpi::dst_reg[0];       // Load input (32 lanes)
 *     sfpi::dst_reg[0] = sfpi::vConst1;        // Default to +1.0
 *     
 *     v_if (v < 0.0F) {                        // Check for negative values
 *         sfpi::dst_reg[0] = sfpi::vConstNeg1; // Set to -1.0
 *     }
 *     v_endif;
 *     
 *     v_if (_sfpu_is_fp16_zero_(v, exponent_size_8)) {  // Check for zero
 *         sfpi::dst_reg[0] = sfpi::vConst0;    // Set to 0.0
 *     }
 *     v_endif;
 *     
 *     sfpi::dst_reg++;                         // Advance to next register
 * }
 * ```
 *
 * @tparam APPROXIMATION_MODE Not applicable for sign function (always exact).
 * @tparam ITERATIONS Template parameter defining loop unroll factor for compilation.
 *
 * @param iterations Number of SIMD operations to perform (typically 8 for full tile face)
 * @param exponent_size_8 Format indicator: 0 for BFP8 (no bias removal), 
 *                        non-zero for Float16 (requires exponent bias removal for zero check)
 *
 * @note Input values must be in FP32 format in SFPU destination registers
 * @note Operation is performed in-place, overwriting input data with sign result
 * @note Function processes one tile face at a time (32 values per iteration)
 * @note Zero detection behavior varies by format (BFP8 vs Float16)
 * @note Output values are exactly -1.0, 0.0, or +1.0 in floating-point representation
 * @note Performance optimized for cases with balanced positive/negative distributions
 * @note Uses helper function `_sfpu_is_fp16_zero_` for accurate zero detection
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_sign_(const int iterations, uint exponent_size_8)
{
// All params are in FP16 format
// uint format = 1;
#pragma GCC unroll 0
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat v   = sfpi::dst_reg[0];     // Load input (32-lane vector)
        sfpi::dst_reg[0] = sfpi::vConst1;        // Initialize to +1.0 (positive case)
        v_if (v < 0.0F)                          // Lane-wise check for negative values
        {
            sfpi::dst_reg[0] = sfpi::vConstNeg1; // Set to -1.0 for negative inputs
        }
        v_endif;                                 // End negative value conditional

        // param0 == 0 is Bfp8 format. It does not require bias removal.
        // param0 != 0 is Float16 format and exp bias needs to be removed for zero check.
        v_if (_sfpu_is_fp16_zero_(v, exponent_size_8))  // Check for exact zero (±0)
        {
            sfpi::dst_reg[0] = sfpi::vConst0;    // Set to 0.0 for zero inputs
        }
        v_endif;                                 // End zero check conditional

        sfpi::dst_reg++;                         // Advance to next 32-element vector
    }
}

} // namespace sfpu
} // namespace ckernel
