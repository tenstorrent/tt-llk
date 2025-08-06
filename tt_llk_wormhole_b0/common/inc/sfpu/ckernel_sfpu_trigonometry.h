// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <limits>

#include "ckernel_sfpu_log.h"
#include "ckernel_sfpu_sqrt.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

// Mathematical constants for trigonometric functions
namespace trig_constants
{
constexpr float PI     = 3.141592653589793f;
constexpr float INV_PI = 0.318309886183791f; // 1/PI

// Maclaurin series coefficients
namespace sine
{
constexpr float COEFF_X3  = -0.166666666f;   // -1/3!
constexpr float COEFF_X5  = 0.0083333333f;   // 1/5!
constexpr float COEFF_X7  = -0.0001984126f;  // -1/7!
constexpr float COEFF_X9  = 0.0000027557f;   // 1/9!
constexpr float COEFF_X11 = -0.00000002505f; // -1/11!
} // namespace sine

namespace cosine
{
constexpr float COEFF_X2  = -0.5f;          // -1/2!
constexpr float COEFF_X4  = 0.0416666666f;  // 1/4!
constexpr float COEFF_X6  = -0.0013888888f; // -1/6!
constexpr float COEFF_X8  = 0.0000248015f;  // 1/8!
constexpr float COEFF_X10 = -0.0000002755f; // -1/10!
} // namespace cosine
} // namespace trig_constants

// Helper function: Reduce input to [-pi, pi] range and handle periodicity
sfpi_inline sfpi::vFloat _reduce_to_pi_range_(sfpi::vFloat input, sfpi::vInt& period_count)
{
    // Convert input to units of pi: input_in_pi = input / pi
    sfpi::vFloat input_in_pi = trig_constants::INV_PI * input;

    // Extract integer part (number of pi periods)
    period_count              = float_to_int16(input_in_pi, 0);
    sfpi::vFloat period_float = int32_to_float(period_count, 0);

    // Get fractional part and convert back to radians: frac * pi
    sfpi::vFloat fractional_part = input_in_pi - period_float;
    return fractional_part * trig_constants::PI;
}

// Sine Maclaurin series implementation
template <bool APPROXIMATION_MODE>
sfpi_inline sfpi::vFloat _sfpu_sine_maclaurin_series_(sfpi::vFloat val)
{
    // Maclaurin series for sin(x): x - x^3/3! + x^5/5! - x^7/7! + x^9/9! - x^11/11!
    // Valid for [-pi, pi] range

    sfpi::vFloat x_squared = val * val;
    sfpi::vFloat term      = val;
    sfpi::vFloat result    = term; // x term

    // x^3/3! term
    term *= x_squared;
    result += trig_constants::sine::COEFF_X3 * term;

    // x^5/5! term
    term *= x_squared;
    result += trig_constants::sine::COEFF_X5 * term;

    // x^7/7! term
    term *= x_squared;
    result += trig_constants::sine::COEFF_X7 * term;

    if constexpr (not APPROXIMATION_MODE)
    {
        // Higher precision terms for accurate mode
        // x^9/9! term
        term *= x_squared;
        result += trig_constants::sine::COEFF_X9 * term;

        // x^11/11! term
        term *= x_squared;
        result += trig_constants::sine::COEFF_X11 * term;
    }

    return result;
}

// Cosine Maclaurin series implementation
template <bool APPROXIMATION_MODE>
sfpi_inline sfpi::vFloat _sfpu_cosine_maclaurin_series_(sfpi::vFloat val)
{
    // Maclaurin series for cos(x): 1 - x^2/2! + x^4/4! - x^6/6! + x^8/8! - x^10/10!
    // Valid for [-pi, pi] range

    sfpi::vFloat x_squared = val * val;
    sfpi::vFloat term      = x_squared;
    sfpi::vFloat result    = 1.0f; // Constant term

    // x^2/2! term
    result += trig_constants::cosine::COEFF_X2 * term;

    // x^4/4! term
    term *= x_squared;
    result += trig_constants::cosine::COEFF_X4 * term;

    // x^6/6! term
    term *= x_squared;
    result += trig_constants::cosine::COEFF_X6 * term;

    if constexpr (not APPROXIMATION_MODE)
    {
        // Higher precision terms for accurate mode
        // x^8/8! term
        term *= x_squared;
        result += trig_constants::cosine::COEFF_X8 * term;

        // x^10/10! term
        term *= x_squared;
        result += trig_constants::cosine::COEFF_X10 * term;
    }

    return result;
}

// Unified trigonometric function implementation
// Handles both sine and cosine with template parameter
template <bool IS_SINE, bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_trigonometric_(const int iterations)
{
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat input = sfpi::dst_reg[0];

        // Reduce input to [-pi, pi] range and track periods
        sfpi::vInt period_count;
        sfpi::vFloat reduced_input = _reduce_to_pi_range_(input, period_count);

        // Calculate the trigonometric function value
        sfpi::vFloat result;
        if constexpr (IS_SINE)
        {
            result = _sfpu_sine_maclaurin_series_<APPROXIMATION_MODE>(reduced_input);
        }
        else
        {
            result = _sfpu_cosine_maclaurin_series_<APPROXIMATION_MODE>(reduced_input);
        }

        // Handle sign changes due to periodicity
        // For sine: odd periods flip sign, for cosine: odd periods flip sign
        sfpi::vInt is_odd_period = period_count & 0x1;
        v_if (is_odd_period != 0)
        {
            result *= -1.0f; // Flip sign for odd periods
        }
        v_endif;

        sfpi::dst_reg[0] = result;
        sfpi::dst_reg++;
    }
}

// Sine function - wrapper for unified implementation
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_sine_(const int iterations)
{
    _calculate_trigonometric_<true, APPROXIMATION_MODE, ITERATIONS>(iterations);
}

// Cosine function - wrapper for unified implementation
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_cosine_(const int iterations)
{
    _calculate_trigonometric_<false, APPROXIMATION_MODE, ITERATIONS>(iterations);
}

// Helper function: Calculate acosh for valid input (x >= 1)
// Formula: acosh(x) = log(x + sqrt(x^2 - 1))
// Helper function is not used in the code, but it is here for reference.
sfpi_inline sfpi::vFloat _calculate_acosh_core_(sfpi::vFloat x)
{
    sfpi::vFloat x_squared_minus_one = x * x - sfpi::vConst1;
    sfpi::vFloat sqrt_term           = _calculate_sqrt_body_<false, 2>(x_squared_minus_one);
    sfpi::vFloat log_arg             = x + sqrt_term;
    return _calculate_log_body_no_init_(log_arg);
}

// Inverse hyperbolic cosine: acosh(x) = log(x + sqrt(x^2 - 1))
// Domain: x >= 1, Range: [0, +∞)
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_acosh_()
{
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat input = sfpi::dst_reg[0];

        // Handle edge cases and domain validation
        v_if (input < sfpi::vConst1)
        {
            sfpi::dst_reg[0] = std::numeric_limits<float>::quiet_NaN(); // Undefined for x < 1
        }
        v_elseif (input == sfpi::vConst1)
        {
            sfpi::dst_reg[0] = sfpi::vConst0; // acosh(1) = 0
        }
        v_else
        {
            // Standard calculation
            sfpi::vFloat x_squared_minus_one = input * input - sfpi::vConst1;
            sfpi::vFloat sqrt_term           = _calculate_sqrt_body_<APPROXIMATION_MODE, 2>(x_squared_minus_one);
            sfpi::vFloat log_arg             = input + sqrt_term;
            sfpi::dst_reg[0]                 = _calculate_log_body_no_init_(log_arg);
        }
        v_endif;
        sfpi::dst_reg++;
    }
}

// Inverse hyperbolic sine: asinh(x) = log(x + sqrt(x^2 + 1))
// Domain: all real numbers, Range: all real numbers
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_asinh_()
{
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat input = sfpi::dst_reg[0];

        // Calculate sqrt(x^2 + 1)
        sfpi::vFloat x_squared_plus_one = input * input + sfpi::vConst1;
        sfpi::vFloat sqrt_term          = _calculate_sqrt_body_<APPROXIMATION_MODE, 2>(x_squared_plus_one);

        // Use |x| + sqrt(x^2 + 1) for numerical stability
        sfpi::vFloat log_arg = sfpi::abs(input) + sqrt_term;
        sfpi::vFloat result  = _calculate_log_body_no_init_(log_arg);

        // Preserve sign: asinh(-x) = -asinh(x)
        v_if (input < sfpi::vConst0)
        {
            result = -result;
        }
        v_endif;

        sfpi::dst_reg[0] = result;
        sfpi::dst_reg++;
    }
}

// Helper function: Calculate atanh for valid input (|x| < 1)
// Formula: atanh(x) = 0.5 * ln((1 + x) / (1 - x))
template <bool APPROXIMATION_MODE, bool is_fp32_dest_acc_en>
sfpi_inline sfpi::vFloat _calculate_atanh_core_(sfpi::vFloat x)
{
    sfpi::vFloat numerator   = sfpi::vConst1 + x; // 1 + x
    sfpi::vFloat denominator = sfpi::vConst1 - x; // 1 - x

    // Calculate reciprocal of denominator for division
    sfpi::vFloat recip = _sfpu_reciprocal_<APPROXIMATION_MODE ? 2 : 3>(denominator);
    recip              = sfpi::setsgn(recip, denominator); // Preserve sign

    // Handle precision based on accumulation mode
    if constexpr (is_fp32_dest_acc_en || APPROXIMATION_MODE)
    {
        denominator = recip;
    }
    else
    {
        denominator = sfpi::reinterpret<sfpi::vFloat>(float_to_fp16b(recip, 0));
    }

    // Calculate (1 + x) / (1 - x)
    sfpi::vFloat ratio = numerator * denominator;

    // Calculate ln(ratio) and multiply by 0.5
    sfpi::vFloat log_result = _calculate_log_body_no_init_(ratio);
    return 0.5f * log_result;
}

// Inverse hyperbolic tangent: atanh(x) = 0.5 * ln((1 + x) / (1 - x))
// Domain: (-1, 1), Range: all real numbers
template <bool APPROXIMATION_MODE, bool is_fp32_dest_acc_en, int ITERATIONS>
inline void _calculate_atanh_()
{
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat input     = sfpi::dst_reg[0];
        sfpi::vFloat abs_input = sfpi::abs(input);
        sfpi::vFloat result;

        // Handle edge cases and domain validation
        v_if (abs_input > sfpi::vConst1)
        {
            result = std::numeric_limits<float>::quiet_NaN(); // Undefined for |x| > 1
        }
        v_elseif (abs_input == sfpi::vConst1)
        {
            // atanh(±1) = ±∞
            sfpi::vFloat inf = std::numeric_limits<float>::infinity();
            result           = sfpi::setsgn(inf, input);
        }
        v_else
        {
            result = _calculate_atanh_core_<APPROXIMATION_MODE, is_fp32_dest_acc_en>(input);
        }
        v_endif;

        sfpi::dst_reg[0] = result;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
void _init_inverse_hyperbolic_()
{
    _init_sqrt_<APPROXIMATION_MODE>();
}

template <bool APPROXIMATION_MODE>
void _init_atanh_()
{
    _init_reciprocal_<APPROXIMATION_MODE>();
}

} // namespace sfpu
} // namespace ckernel
