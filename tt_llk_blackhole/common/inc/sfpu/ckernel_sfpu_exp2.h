// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_exp2.h
 * @brief Base-2 exponential function implementation for SFPU hardware
 *
 * @details This file implements the base-2 exponential function (2^x) by leveraging
 * the existing natural exponential (e^x) infrastructure through mathematical base
 * conversion. This approach provides high performance while reusing the optimized
 * exponential algorithms and hardware acceleration features.
 *
 * **Mathematical Foundation:**
 * The implementation uses the fundamental base conversion identity:
 * ```
 * 2^x = e^(x * ln(2))
 * ```
 * 
 * Where ln(2) ≈ 0.6931471805599453 is the natural logarithm of 2.
 *
 * **Algorithm Strategy:**
 * 1. **Base Conversion**: Multiply input by ln(2) to convert from base-2 to natural base
 * 2. **Exponential Computation**: Apply optimized e^x algorithms from ckernel_sfpu_exp.h
 * 3. **Result**: Direct output (no post-processing needed)
 *
 * **Performance Characteristics:**
 * - **Accuracy**: Inherits precision characteristics of underlying e^x implementation
 * - **Range**: Same safe input range as natural exponential: approximately [-42, 89] after scaling
 * - **Speed**: Minimal overhead over e^x (single multiplication + function call)
 * - **Hardware**: Utilizes same SFPU optimizations (approximation modes, range checking)
 *
 * **Use Cases:**
 * - **Neural Networks**: Power-of-2 activation functions and attention mechanisms
 * - **Signal Processing**: Frequency domain operations and power spectral analysis
 * - **Computer Graphics**: Exponential falloff and lighting calculations
 * - **Scientific Computing**: Algorithms naturally expressed in base-2 exponentials
 */

#pragma once

#include "ckernel_sfpu_exp.h"
#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel::sfpu
{

/**
 * @brief Compute base-2 exponential function (2^x) using base conversion
 *
 * @details Implements 2^x by converting to natural exponential computation:
 * ```
 * 2^x = e^(x * ln(2))
 * ```
 *
 * **Algorithm Steps:**
 * 1. **Base Conversion**: Multiply input by ln(2) = 0.6931471805
 * 2. **Exponential**: Call optimized e^x implementation with converted input
 * 3. **Output**: Result is mathematically equivalent to 2^input
 *
 * **Mathematical Accuracy:**
 * The base conversion introduces minimal additional error:
 * - **ln(2) Precision**: Uses float constant with ~7 decimal digits accuracy
 * - **Total Error**: Base conversion error + underlying e^x approximation error
 * - **Range Safety**: Input range effectively scaled by ln(2) factor
 *
 * **Hardware Optimization:**
 * - **Multiplication**: Single FP32 multiply per element (32 lanes parallel)
 * - **Exponential**: Leverages all optimizations from ckernel_sfpu_exp.h
 * - **No FAST_APPROX**: Uses standard approximation mode (FAST_APPROX disabled)
 * - **Range Checking**: Full positive/negative range checking enabled
 *
 * **Template Parameters:**
 * - `APPROXIMATION_MODE`: Selects fast vs precise exponential algorithm
 * - `ITERATIONS`: Number of 32-element vectors to process
 *
 * **Input Range Considerations:**
 * After ln(2) scaling, effective input range becomes:
 * - **Safe Range**: [-42/ln(2), 89/ln(2)] ≈ [-60.6, 128.4]
 * - **Overflow**: Inputs > 128.4 may saturate to infinity
 * - **Underflow**: Inputs < -60.6 may saturate to zero
 *
 * @tparam APPROXIMATION_MODE If true, uses fast bit manipulation; if false, uses precise polynomial
 * @tparam ITERATIONS Number of 32-element vectors to process in sequence
 *
 * @note Reuses exponential infrastructure for optimal performance and accuracy
 * @note Base conversion factor ln(2) computed at compile time for efficiency
 * @note All exponential safety features (range checking, saturation) preserved
 * @note Input scaling does not affect overall algorithm complexity
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_exp2_()
{
    // Configuration for underlying exponential function
    const bool SCALE_EN                  = false; // Exp2 does not use additional scaling
    const bool SKIP_POSITIVE_CHECK       = false; // Exp2 uses full range checking for safety
    const uint16_t exp_base_scale_factor = p_sfpu::kCONST_1_FP16B; // Unity scale (1.0f in BF16)

    // Process each 32-element vector in sequence
    for (int d = 0; d < ITERATIONS; d++)
    {
        // Load 32-lane input vector from destination register
        sfpi::vFloat v = sfpi::dst_reg[0];
        
        // Base conversion: 2^x = e^(x * ln(2))
        // ln(2) ≈ 0.6931471805599453 (natural logarithm of 2)
        v = v * 0.6931471805f;
        
        // Compute exponential using optimized e^x implementation
        // This leverages all hardware optimizations and range safety features
        sfpi::vFloat exp = _calculate_exponential_piecewise_<APPROXIMATION_MODE, SCALE_EN, SKIP_POSITIVE_CHECK>(v, exp_base_scale_factor);
        
        // Store result back to destination register  
        sfpi::dst_reg[0] = exp;
        
        // Advance to next 32-element vector
        sfpi::dst_reg++;
    }
}

/**
 * @brief Initialize constants and configuration for base-2 exponential computation
 *
 * @details Sets up the underlying exponential function infrastructure for optimal
 * base-2 exponential computation. This initialization configures the exponential
 * algorithms, loads necessary constants, and prepares hardware for efficient 2^x
 * calculation.
 *
 * **Initialization Strategy:**
 * - **No FAST_APPROX**: Uses standard exponential modes for reliability
 * - **Unity Scaling**: No additional input scaling beyond ln(2) conversion
 * - **Full Safety**: Enables all range checking and saturation features
 * - **Algorithm Selection**: Respects APPROXIMATION_MODE for speed vs accuracy trade-off
 *
 * **Underlying Configuration:**
 * ```cpp
 * EXP_BASE_SCALE_FACTOR = 0x3F800000;  // 1.0f in IEEE-754 format
 * FAST_APPROX = false;                 // Use standard approximation modes
 * SCALE_EN = false;                    // No additional scaling in exponential
 * ```
 *
 * **Hardware Setup:**
 * Depending on APPROXIMATION_MODE, this configures:
 * - **Approximation Mode**: Fast bit manipulation constants (ln2_recip, format conversion)
 * - **Precise Mode**: Polynomial coefficients and range reduction constants  
 * - **No Macro Instructions**: FAST_APPROX disabled, uses standard processing
 *
 * **Why No FAST_APPROX for exp2:**
 * - **Complexity**: FAST_APPROX requires extensive macro instruction programming
 * - **Overhead**: Base conversion already adds one multiplication step
 * - **Reliability**: Standard modes provide better error handling and debugging
 * - **Performance**: Single ln(2) multiplication overhead is minimal vs macro setup
 *
 * **Comparison with Natural Exponential:**
 * | Feature | exp(x) | exp2(x) |
 * |---------|--------|---------|
 * | FAST_APPROX | Available | Disabled |
 * | Range Checking | Configurable | Always Enabled |
 * | Input Scaling | Configurable | Fixed (ln(2)) |
 * | Macro Instructions | Optional | Not Used |
 *
 * @tparam APPROXIMATION_MODE Algorithm selection for underlying exponential computation
 *
 * @note Must be called before any exp2 computations
 * @note Initializes underlying exponential infrastructure with appropriate settings
 * @note FAST_APPROX intentionally disabled for simplicity and reliability
 * @note Unity scale factor ensures no additional scaling beyond ln(2) conversion
 */
template <bool APPROXIMATION_MODE>
inline void _init_exp2_()
{
    // Scale factor: 1.0f in IEEE-754 FP32 format (no additional scaling)
    const uint32_t EXP_BASE_SCALE_FACTOR = 0x3F800000;
    
    // Disable fast approximation mode for exp2 (prioritize reliability)
    const bool FAST_APPROX               = false;
    
    // Initialize underlying exponential infrastructure
    // This sets up constants, algorithms, and hardware configuration
    // based on the selected approximation mode and scaling settings
    _init_exponential_<APPROXIMATION_MODE, FAST_APPROX, EXP_BASE_SCALE_FACTOR>();
}

} // namespace ckernel::sfpu
