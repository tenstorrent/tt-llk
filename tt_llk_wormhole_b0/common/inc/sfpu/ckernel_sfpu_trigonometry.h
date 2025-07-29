// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_trigonometry.h
 * @brief SFPU Trigonometric and Inverse Hyperbolic Functions for Wormhole B0
 *
 * @details This file implements hardware-accelerated trigonometric and inverse hyperbolic
 * functions using the Wormhole B0 SFPU's 32-lane SIMD architecture. The implementation
 * combines Maclaurin series approximation for trigonometric functions with composite
 * mathematical operations leveraging logarithm and square root primitives for inverse
 * hyperbolic functions.
 *
 * **Implemented Functions:**
 * - **Trigonometric**: sin(x), cos(x) using Maclaurin series
 * - **Inverse Hyperbolic**: acosh(x), asinh(x), atanh(x) using logarithmic identities
 *
 * **Mathematical Foundations:**
 *
 * **Trigonometric Functions (Maclaurin Series):**
 * ```
 * sin(x) = Σ(-1)ⁿ × x^(2n+1) / (2n+1)!  for n = 0,1,2,...
 *        = x - x³/6 + x⁵/120 - x⁷/5040 + x⁹/362880 - x¹¹/39916800 + ...
 * ```
 *
 * **Coefficient Values:**
 * - **x¹**: 1.0 (identity term)
 * - **x³**: -1/6 ≈ -0.166666666 
 * - **x⁵**: 1/120 ≈ 0.0083333333
 * - **x⁷**: -1/5040 ≈ -0.0001984126
 * - **x⁹**: 1/362880 ≈ 0.0000027557 (high precision only)
 * - **x¹¹**: -1/39916800 ≈ -0.00000002505 (high precision only)
 *
 * **Convergence Properties:**
 * - **Rapid Convergence**: Near x=0, series converges quickly
 * - **Range Limitation**: Best accuracy for |x| ≤ π
 * - **Error Bound**: Approximation error decreases factorially with term count
 */

#pragma once

#include <limits>

#include "ckernel_sfpu_log.h"
#include "ckernel_sfpu_sqrt.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Compute sine using Maclaurin series approximation
 * @details Evaluates sin(x) using Taylor series expansion around x=0 optimized for
 * the range [-π, π] with configurable precision via term count selection.
 *
 * @tparam APPROXIMATION_MODE Precision control
 *   - true: 4-term series (x - x³/3! + x⁵/5! - x⁷/7!)
 *   - false: 6-term series (adds x⁹/9! - x¹¹/11! terms)
 * @param val Input angle in radians (should be in range [-π, π])
 * @return sin(val) approximation
 *
 * **Maclaurin Series Expansion:**
 * ```
 * sin(x) = Σ(-1)ⁿ × x^(2n+1) / (2n+1)!  for n = 0,1,2,...
 *        = x - x³/6 + x⁵/120 - x⁷/5040 + x⁹/362880 - x¹¹/39916800 + ...
 * ```
 *
 * **Coefficient Values:**
 * - **x¹**: 1.0 (identity term)
 * - **x³**: -1/6 ≈ -0.166666666 
 * - **x⁵**: 1/120 ≈ 0.0083333333
 * - **x⁷**: -1/5040 ≈ -0.0001984126
 * - **x⁹**: 1/362880 ≈ 0.0000027557 (high precision only)
 * - **x¹¹**: -1/39916800 ≈ -0.00000002505 (high precision only)
 *
 * **Convergence Properties:**
 * - **Rapid Convergence**: Near x=0, series converges quickly
 * - **Range Limitation**: Best accuracy for |x| ≤ π
 * - **Error Bound**: Approximation error decreases factorially with term count
 */
template <bool APPROXIMATION_MODE>
sfpi_inline sfpi::vFloat _sfpu_sine_maclaurin_series_(sfpi::vFloat val)
{
    // Good for [-pi:pi]
    // Mclauren series = x - x^3/3! + x^5/5! - x^7/7! + x^9/9! - x^11/11!
    sfpi::vFloat tmp = val;
    // x
    sfpi::vFloat output = tmp;
    // x^3/3!
    tmp = tmp * val * val;
    output += -0.166666666 * tmp;
    // x^5/5!
    tmp = tmp * val * val;
    output += 0.0083333333 * tmp;
    // x^7/7!
    tmp = tmp * val * val;
    output += -0.0001984126 * tmp;
    if constexpr (not APPROXIMATION_MODE)
    {
        // x^9/9!
        tmp = tmp * val * val;
        output += 0.0000027557 * tmp;
        // x^11/11!
        tmp = tmp * val * val;
        output += -0.00000002505 * tmp;
    }

    // Write out output
    return output;
}

/**
 * @brief Compute cosine using Maclaurin series approximation
 * @details Evaluates cos(x) using Taylor series expansion around x=0 optimized for
 * the range [-π, π] with configurable precision via term count selection.
 *
 * @tparam APPROXIMATION_MODE Precision control
 *   - true: 3-term series (1 - x²/2! + x⁴/4! - x⁶/6!)
 *   - false: 5-term series (adds x⁸/8! - x¹⁰/10! terms)
 * @param val Input angle in radians (should be in range [-π, π])
 * @return cos(val) approximation
 *
 * **Maclaurin Series Expansion:**
 * ```
 * cos(x) = Σ(-1)ⁿ × x^(2n) / (2n)!  for n = 0,1,2,...
 *        = 1 - x²/2 + x⁴/24 - x⁶/720 + x⁸/40320 - x¹⁰/3628800 + ...
 * ```
 *
 * **Coefficient Values:**
 * - **x⁰**: 1.0 (constant term)
 * - **x²**: -1/2 = -0.5
 * - **x⁴**: 1/24 ≈ 0.0416666666
 * - **x⁶**: -1/720 ≈ -0.0013888888
 * - **x⁸**: 1/40320 ≈ 0.0000248015 (high precision only)
 * - **x¹⁰**: -1/3628800 ≈ -0.0000002755 (high precision only)
 *
 * **Mathematical Properties:**
 * - **Even Function**: cos(-x) = cos(x), series contains only even powers
 * - **Periodic**: cos(x + 2π) = cos(x), requires range reduction for large inputs
 * - **Bounded**: Output always in range [-1, 1]
 */
template <bool APPROXIMATION_MODE>
sfpi_inline sfpi::vFloat _sfpu_cosine_maclaurin_series_(sfpi::vFloat val)
{
    // Good for [-pi:pi]
    // Mclauren series = 1 - x^2/2! + x^4/4! - x^6/6! + x^8/8! - x^10/10! + x^12/12!
    // 1
    sfpi::vFloat output = 1.0f;
    // x^2/2!
    sfpi::vFloat tmp = val * val;
    output += -0.5 * tmp;
    // x^4/4!
    tmp = tmp * val * val;
    output += 0.0416666666 * tmp;
    // x^6/6!
    tmp = tmp * val * val;
    output += -0.0013888888 * tmp;
    if constexpr (not APPROXIMATION_MODE)
    {
        // x^8/8!
        tmp = tmp * val * val;
        output += 0.0000248015 * tmp;
        // x^10/10!
        tmp = tmp * val * val;
        output += -0.0000002755 * tmp;
    }

    // Write out output
    return output;
}

/**
 * @brief Compute sine function for DEST register data with range reduction
 * @details Processes DEST register faces computing sin(x) using range reduction
 * to [-π, π] followed by Maclaurin series evaluation. Handles arbitrary input ranges.
 *
 * @tparam APPROXIMATION_MODE Series precision control
 * @tparam ITERATIONS Number of DEST faces to process
 * @param iterations Runtime iteration count (should match template parameter)
 *
 * **Range Reduction Algorithm:**
 * ```cpp
 * 1. Normalize: x' = x / π          // Convert to multiples of π
 * 2. Extract: n = floor(x')         // Integer multiple
 * 3. Fraction: frac = x' - n        // Fractional part ∈ [0,1)
 * 4. Scale: reduced = frac × π       // Map to [0, π]
 * 5. Quadrant: sign = (-1)^n        // Handle sign based on quadrant
 * ```
 *
 * **Mathematical Correctness:**
 * - **Periodicity**: sin(x + 2πn) = sin(x) for integer n
 * - **Symmetry**: sin(x + π) = -sin(x) handled by sign correction
 * - **Precision**: Range reduction maintains floating-point precision
 *
 * **Performance Considerations:**
 * - **Range Reduction Cost**: Additional ~3-4 cycles per face
 * - **Series Evaluation**: 4-6 terms depending on precision mode
 * - **Branch Efficiency**: Conditional sign correction optimized for SFPU
 *
 * @note Legacy implementation marked for potential future removal
 */
// Legacy implementation.
// Candidate for removal in future versions. See https://github.com/tenstorrent/tt-llk/issues/225 for more details.
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_sine_(const int iterations)
{
    // SFPU microcode
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat v             = sfpi::dst_reg[0];
        v                          = 0.318309886183791f * v; // *1/pi to get number of pi rads.
        sfpi::vInt whole_v         = float_to_int16(v, 0);
        sfpi::vFloat whole_v_float = int32_to_float(whole_v, 0);
        v                          = v - whole_v_float;
        v *= 3.141592653589793f; // fractional * pi to get it in [-pi:pi]
        v       = _sfpu_sine_maclaurin_series_<APPROXIMATION_MODE>(v);
        whole_v = whole_v & 0x1;
        v_if (whole_v != 0)
        {
            // odd so flip the sign
            v *= -1;
        }
        v_endif;
        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}

/**
 * @brief Compute cosine function for DEST register data with range reduction
 * @details Processes DEST register faces computing cos(x) using range reduction
 * to [-π, π] followed by Maclaurin series evaluation. Handles arbitrary input ranges.
 *
 * @tparam APPROXIMATION_MODE Series precision control  
 * @tparam ITERATIONS Number of DEST faces to process
 * @param iterations Runtime iteration count (should match template parameter)
 *
 * **Range Reduction Algorithm:**
 * Same as sine function - uses identical modular arithmetic approach:
 * ```cpp
 * reduced_angle = (x mod 2π) mapped to [-π, π]
 * quadrant_correction = (-1)^floor(x/π)
 * ```
 *
 * **Cosine-Specific Considerations:**
 * - **Even Function**: cos(-x) = cos(x), but range reduction handles this automatically
 * - **Phase Relationship**: cos(x) = sin(x + π/2), but direct series evaluation used
 * - **Quadrant Behavior**: cos(x + π) = -cos(x) handled by sign correction
 *
 * **Numerical Stability:**
 * - **Range Reduction**: Keeps series evaluation in optimal convergence region
 * - **Floating-Point Precision**: Maintains precision through modular arithmetic
 * - **Coefficient Accuracy**: Uses high-precision factorial coefficients
 *
 * @note Legacy implementation marked for potential future removal
 */
// Legacy implementation.
// Candidate for removal in future versions. See https://github.com/tenstorrent/tt-llk/issues/225 for more details.
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_cosine_(const int iterations)
{
    // SFPU microcode
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat v             = sfpi::dst_reg[0];
        v                          = 0.318309886183791f * v; // *1/pi to get number of pi rads.
        sfpi::vInt whole_v         = float_to_int16(v, 0);
        sfpi::vFloat whole_v_float = int32_to_float(whole_v, 0);
        v                          = v - whole_v_float;
        v *= 3.141592653589793f; // fractional * pi to get it in [-pi:pi]
        v       = _sfpu_cosine_maclaurin_series_<APPROXIMATION_MODE>(v);
        whole_v = whole_v & 0x1;
        v_if (whole_v != 0)
        {
            // odd so flip the sign
            v *= -1;
        }
        v_endif;
        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}

/**
 * @brief Compute inverse hyperbolic cosine using logarithmic identity
 * @details Implements acosh(x) = ln(x + √(x² - 1)) using SFPU composite operations.
 * Leverages sqrt and log primitives for accurate computation with proper domain handling.
 *
 * @tparam APPROXIMATION_MODE Precision mode for sqrt and log operations
 * @tparam ITERATIONS Number of DEST faces to process
 *
 * **Mathematical Identity:**
 * ```
 * acosh(x) = ln(x + √(x² - 1))  for x ≥ 1
 * ```
 *
 * **Algorithm Steps:**
 * ```cpp
 * 1. Domain check: if x < 1 return NaN
 * 2. Special case: if x = 1 return 0  
 * 3. Compute: temp = √(x² - 1)
 * 4. Add: temp = temp + x
 * 5. Result: acosh(x) = ln(temp)
 * ```
 *
 * **Domain and Range:**
 * - **Domain**: x ≥ 1 (real-valued results)
 * - **Range**: [0, +∞) (all non-negative reals)
 * - **Special Values**: acosh(1) = 0, acosh(∞) = ∞
 *
 * **Error Sources:**
 * - **Square Root**: Inherited from SFPU sqrt approximation
 * - **Logarithm**: Inherited from SFPU log Chebyshev approximation  
 * - **Subtraction**: x² - 1 may have precision loss for x ≈ 1
 * - **Composition**: Errors accumulate through operation chain
 *
 * **Numerical Considerations:**
 * - **Near Unity**: For x ≈ 1, uses √(x² - 1) ≈ √(2(x-1)) expansion
 * - **Large Values**: For x >> 1, acosh(x) ≈ ln(2x) asymptotically
 * - **Precision**: Maintains ~1e-5 relative accuracy typical of SFPU operations
 */
// https://en.wikipedia.org/wiki/Inverse_hyperbolic_functions#Definitions_in_terms_of_logarithms
// acosh(x) = log(x + sqrt(x^2 - 1))
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_acosh_()
{
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat inp = sfpi::dst_reg[0];
        v_if (inp < sfpi::vConst1)
        {
            sfpi::dst_reg[0] = std::numeric_limits<float>::quiet_NaN();
        }
        v_elseif (inp == sfpi::vConst1)
        {
            sfpi::dst_reg[0] = sfpi::vConst0;
        }
        v_else
        {
            sfpi::vFloat tmp = inp * inp;
            tmp              = tmp - sfpi::vConst1;
            tmp              = _calculate_sqrt_body_<APPROXIMATION_MODE, 2>(tmp);
            tmp              = tmp + inp;
            sfpi::dst_reg[0] = _calculate_log_body_no_init_(tmp);
        }
        v_endif;
        sfpi::dst_reg++;
    }
}

/**
 * @brief Compute inverse hyperbolic sine using logarithmic identity
 * @details Implements asinh(x) = ln(x + √(x² + 1)) using SFPU composite operations.
 * Handles all real inputs with proper sign preservation and numerical stability.
 *
 * @tparam APPROXIMATION_MODE Precision mode for sqrt and log operations
 * @tparam ITERATIONS Number of DEST faces to process
 *
 * **Mathematical Identity:**
 * ```
 * asinh(x) = ln(x + √(x² + 1))  for all real x
 * ```
 *
 * **Algorithm Steps:**
 * ```cpp
 * 1. Compute: temp = √(x² + 1)     // Always real and ≥ 1
 * 2. Add: temp = temp + |x|        // Use absolute value for stability
 * 3. Log: result = ln(temp)        // Logarithm of positive value
 * 4. Sign: if x < 0, negate result // Restore original sign
 * ```
 *
 * **Odd Function Property:**
 * ```
 * asinh(-x) = -asinh(x)
 * ```
 * Implementation exploits this by computing asinh(|x|) then applying sign correction.
 *
 * **Domain and Range:**
 * - **Domain**: All real numbers (-∞, +∞)
 * - **Range**: All real numbers (-∞, +∞)  
 * - **Monotonicity**: Strictly increasing function
 *
 * **Numerical Stability:**
 * - **No Cancellation**: x² + 1 ≥ 1 prevents subtraction issues
 * - **Large Values**: For |x| >> 1, asinh(x) ≈ ln(2|x|) + sign(x)
 * - **Small Values**: For |x| << 1, asinh(x) ≈ x with high precision
 */
// asinh(x) = log(x + sqrt(x^2 + 1))
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_asinh_()
{
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat inp = sfpi::dst_reg[0];
        sfpi::vFloat tmp = inp * inp + sfpi::vConst1;
        tmp              = _calculate_sqrt_body_<APPROXIMATION_MODE, 2>(tmp);
        tmp              = tmp + sfpi::abs(inp);
        sfpi::dst_reg[0] = _calculate_log_body_no_init_(tmp);
        v_if (inp < sfpi::vConst0)
        {
            sfpi::dst_reg[0] = -sfpi::dst_reg[0];
        }
        v_endif;
        sfpi::dst_reg++;
    }
}

/**
 * @brief Compute inverse hyperbolic tangent using logarithmic identity
 * @details Implements atanh(x) = (1/2) × ln((1+x)/(1-x)) using SFPU reciprocal and log.
 * Includes comprehensive domain checking and singularity handling.
 *
 * @tparam APPROXIMATION_MODE Precision mode for reciprocal and log operations
 * @tparam is_fp32_dest_acc_en Output format control (FP32 vs FP16B)
 * @tparam ITERATIONS Number of DEST faces to process
 *
 * **Mathematical Identity:**
 * ```
 * atanh(x) = (1/2) × ln((1+x)/(1-x))  for |x| < 1
 * ```
 *
 * **Algorithm Steps:**
 * ```cpp
 * 1. Domain check: if |x| > 1 return NaN
 * 2. Singularity: if |x| = 1 return ±∞
 * 3. Numerator: num = 1 + x
 * 4. Denominator: den = 1 - x  
 * 5. Division: ratio = num / den using reciprocal
 * 6. Logarithm: result = (1/2) × ln(ratio)
 * ```
 *
 * **Domain and Range:**
 * - **Domain**: |x| < 1 (open interval (-1, 1))
 * - **Range**: All real numbers (-∞, +∞)
 * - **Singularities**: atanh(±1) = ±∞
 *
 * **Numerical Considerations:**
 * - **Near Singularity**: For x ≈ ±1, ratio becomes very large/small
 * - **Precision Loss**: Subtraction 1-x may lose precision for x ≈ 1
 * - **Reciprocal Accuracy**: Division implemented via SFPU reciprocal with 2-3 iterations
 * - **Format Conversion**: Configurable FP32/FP16B output for memory efficiency
 *
 * **Special Value Handling:**
 * - **|x| > 1**: Returns quiet NaN (domain error)
 * - **x = ±1**: Returns ±∞ with proper sign
 * - **x = 0**: Returns 0 (exact)
 */
// atanh[x] = 0.5 * ln((1 + x) / (1 - x))
template <bool APPROXIMATION_MODE, bool is_fp32_dest_acc_en, int ITERATIONS>
inline void _calculate_atanh_()
{
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat inp     = sfpi::dst_reg[0];
        sfpi::vFloat abs_inp = sfpi::abs(inp);
        v_if (abs_inp > sfpi::vConst1)
        {
            sfpi::dst_reg[0] = std::numeric_limits<float>::quiet_NaN();
        }
        v_elseif (abs_inp == sfpi::vConst1)
        {
            sfpi::vFloat inf = std::numeric_limits<float>::infinity();
            sfpi::dst_reg[0] = sfpi::setsgn(inf, inp);
        }
        v_else
        {
            sfpi::vFloat num = sfpi::vConst1 + inp;
            sfpi::vFloat den = sfpi::vConst1 - inp;
            sfpi::vFloat tmp = _sfpu_reciprocal_<APPROXIMATION_MODE ? 2 : 3>(den);
            tmp              = sfpi::setsgn(tmp, den);
            if constexpr (is_fp32_dest_acc_en || APPROXIMATION_MODE)
            {
                den = tmp;
            }
            else
            {
                den = sfpi::reinterpret<sfpi::vFloat>(float_to_fp16b(tmp, 0));
            }
            num              = num * den;
            den              = _calculate_log_body_no_init_(num);
            sfpi::dst_reg[0] = 0.5f * den;
        }
        v_endif;
        sfpi::dst_reg++;
    }
}

/**
 * @brief Initialize SFPU constants for inverse hyperbolic functions
 * @details Sets up square root algorithm constants required for acosh and asinh computation.
 * Must be called before using inverse hyperbolic functions.
 *
 * @tparam APPROXIMATION_MODE Precision mode for sqrt initialization
 *
 * **Dependencies:**
 * - **Square Root**: Required for √(x² ± 1) computation in acosh/asinh
 * - **Logarithm**: Uses existing log initialization (no additional setup needed)
 *
 * **Initialization Scope:**
 * - **acosh**: Requires sqrt for √(x² - 1)
 * - **asinh**: Requires sqrt for √(x² + 1)
 * - **atanh**: Uses only reciprocal and log (separate initialization)
 */
template <bool APPROXIMATION_MODE>
void _init_inverse_hyperbolic_()
{
    _init_sqrt_<APPROXIMATION_MODE>();
}

/**
 * @brief Initialize SFPU constants for inverse hyperbolic tangent
 * @details Sets up reciprocal algorithm constants required for atanh division operation.
 * Separate from general inverse hyperbolic initialization.
 *
 * @tparam APPROXIMATION_MODE Precision mode for reciprocal initialization
 *
 * **Dependencies:**
 * - **Reciprocal**: Required for (1+x)/(1-x) division in atanh
 * - **Logarithm**: Uses existing log constants (no additional setup needed)
 *
 * **Usage Pattern:**
 * ```cpp
 * _init_atanh_<false>();           // Initialize reciprocal constants
 * _calculate_atanh_<false, true, 8>(); // Compute atanh with FP32 output
 * ```
 */
template <bool APPROXIMATION_MODE>
void _init_atanh_()
{
    _init_reciprocal_<APPROXIMATION_MODE>();
}

} // namespace sfpu
} // namespace ckernel
