// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <limits>

#include "ckernel_sfpu_log.h"
#include "ckernel_sfpu_sqrt.h"
#include "sfpi.h"

namespace ckernel {
namespace sfpu {

// ======================================================
// Mathematical Constants and Series Coefficients
// ======================================================
namespace trig_constants {
    constexpr float PI     = 3.141592653589793f;
    constexpr float INV_PI = 0.318309886183791f; // 1/PI

    namespace sine {
        constexpr float COEFF_X3  = -0.166666666f;   // -1/3!
        constexpr float COEFF_X5  = 0.0083333333f;   // 1/5!
        constexpr float COEFF_X7  = -0.0001984126f;  // -1/7!
        constexpr float COEFF_X9  = 0.0000027557f;   // 1/9!
        constexpr float COEFF_X11 = -0.00000002505f; // -1/11!
    }

    namespace cosine {
        constexpr float COEFF_X2  = -0.5f;          // -1/2!
        constexpr float COEFF_X4  = 0.0416666666f;  // 1/4!
        constexpr float COEFF_X6  = -0.0013888888f; // -1/6!
        constexpr float COEFF_X8  = 0.0000248015f;  // 1/8!
        constexpr float COEFF_X10 = -0.0000002755f; // -1/10!
    }
}

// ======================================================
// Core Trigonometric Functions
// ======================================================

/**
 * Reduces input to [-π, π] range and tracks period count
 * 
 * @param input Angle in radians
 * @param period_count Output parameter for number of π periods
 * @return Angle reduced to [-π, π] range
 */
sfpi_inline sfpi::vFloat reduce_to_pi_range(sfpi::vFloat input, sfpi::vInt& period_count) {
    sfpi::vFloat input_in_pi = trig_constants::INV_PI * input;
    period_count = float_to_int16(input_in_pi, 0);
    sfpi::vFloat period_float = int32_to_float(period_count, 0);
    return (input_in_pi - period_float) * trig_constants::PI;
}

/**
 * Computes sine using Maclaurin series approximation
 * 
 * @tparam APPROXIMATION_MODE When true, uses fewer terms for faster computation
 * @param val Input angle in radians (must be in [-π, π] range)
 * @return Sine of input angle
 */
template <bool APPROXIMATION_MODE>
sfpi_inline sfpi::vFloat compute_sine_series(sfpi::vFloat val) {
    sfpi::vFloat x_squared = val * val;
    sfpi::vFloat term = val;
    sfpi::vFloat result = term; // x term

    // Series terms
    term *= x_squared;
    result += trig_constants::sine::COEFF_X3 * term; // x³/3!

    term *= x_squared;
    result += trig_constants::sine::COEFF_X5 * term; // x⁵/5!

    term *= x_squared;
    result += trig_constants::sine::COEFF_X7 * term; // x⁷/7!

    if constexpr (!APPROXIMATION_MODE) {
        term *= x_squared;
        result += trig_constants::sine::COEFF_X9 * term; // x⁹/9!

        term *= x_squared;
        result += trig_constants::sine::COEFF_X11 * term; // x¹¹/11!
    }

    return result;
}

/**
 * Computes cosine using Maclaurin series approximation
 * 
 * @tparam APPROXIMATION_MODE When true, uses fewer terms for faster computation
 * @param val Input angle in radians (must be in [-π, π] range)
 * @return Cosine of input angle
 */
template <bool APPROXIMATION_MODE>
sfpi_inline sfpi::vFloat compute_cosine_series(sfpi::vFloat val) {
    sfpi::vFloat x_squared = val * val;
    sfpi::vFloat term = x_squared;
    sfpi::vFloat result = 1.0f; // Constant term

    // Series terms
    result += trig_constants::cosine::COEFF_X2 * term; // x²/2!

    term *= x_squared;
    result += trig_constants::cosine::COEFF_X4 * term; // x⁴/4!

    term *= x_squared;
    result += trig_constants::cosine::COEFF_X6 * term; // x⁶/6!

    if constexpr (!APPROXIMATION_MODE) {
        term *= x_squared;
        result += trig_constants::cosine::COEFF_X8 * term; // x⁸/8!

        term *= x_squared;
        result += trig_constants::cosine::COEFF_X10 * term; // x¹⁰/10!
    }

    return result;
}

// ======================================================
// Unified Trigonometric Function Implementation
// ======================================================

/**
 * Unified implementation for sine and cosine functions
 * 
 * @tparam IS_SINE When true computes sine, otherwise cosine
 * @tparam APPROXIMATION_MODE When true, uses faster approximation
 * @tparam ITERATIONS Number of vector elements to process
 * @param iterations Actual iteration count (for runtime flexibility)
 */
template <bool IS_SINE, bool APPROXIMATION_MODE, int ITERATIONS>
inline void calculate_trigonometric(const int iterations) {
    for (int d = 0; d < iterations; d++) {
        sfpi::vFloat input = sfpi::dst_reg[0];
        sfpi::vInt period_count;
        sfpi::vFloat reduced_input = reduce_to_pi_range(input, period_count);

        // Compute sine or cosine based on template parameter
        sfpi::vFloat result = IS_SINE 
            ? compute_sine_series<APPROXIMATION_MODE>(reduced_input)
            : compute_cosine_series<APPROXIMATION_MODE>(reduced_input);

        // Flip sign for odd periods (both sin(x + kπ) and cos(x + kπ) = (-1)^k * f(x))
        sfpi::vInt is_odd_period = period_count & 0x1;
        v_if (is_odd_period != 0) {
            result *= -1.0f;
        }
        v_endif;

        sfpi::dst_reg[0] = result;
        sfpi::dst_reg++;
    }
}

// Sine function wrapper
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_sine_(const int iterations) {
    calculate_trigonometric<true, APPROXIMATION_MODE, ITERATIONS>(iterations);
}

// Cosine function wrapper
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_cosine_(const int iterations) {
    calculate_trigonometric<false, APPROXIMATION_MODE, ITERATIONS>(iterations);
}

// ======================================================
// Inverse Hyperbolic Functions
// ======================================================

/**
 * Computes inverse hyperbolic cosine (acosh)
 * 
 * @tparam APPROXIMATION_MODE When true, uses faster approximation
 * @tparam ITERATIONS Number of vector elements to process
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_acosh_() {
    for (int d = 0; d < ITERATIONS; d++) {
        sfpi::vFloat input = sfpi::dst_reg[0];

        v_if (input < sfpi::vConst1) {
            sfpi::dst_reg[0] = std::numeric_limits<float>::quiet_NaN();
        }
        v_elseif (input == sfpi::vConst1) {
            sfpi::dst_reg[0] = sfpi::vConst0; // acosh(1) = 0
        }
        v_else {
            sfpi::vFloat x_squared_minus_one = input * input - sfpi::vConst1;
            sfpi::vFloat sqrt_term = _calculate_sqrt_body_<APPROXIMATION_MODE, 2>(x_squared_minus_one);
            sfpi::vFloat log_arg = input + sqrt_term;
            sfpi::dst_reg[0] = _calculate_log_body_no_init_(log_arg);
        }
        v_endif;
        
        sfpi::dst_reg++;
    }
}

/**
 * Computes inverse hyperbolic sine (asinh)
 * 
 * @tparam APPROXIMATION_MODE When true, uses faster approximation
 * @tparam ITERATIONS Number of vector elements to process
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_asinh_() {
    for (int d = 0; d < ITERATIONS; d++) {
        sfpi::vFloat input = sfpi::dst_reg[0];
        sfpi::vFloat x_squared_plus_one = input * input + sfpi::vConst1;
        sfpi::vFloat sqrt_term = _calculate_sqrt_body_<APPROXIMATION_MODE, 2>(x_squared_plus_one);
        sfpi::vFloat log_arg = sfpi::abs(input) + sqrt_term;
        sfpi::vFloat result = _calculate_log_body_no_init_(log_arg);

        v_if (input < sfpi::vConst0) {
            result = -result; // Preserve sign: asinh(-x) = -asinh(x)
        }
        v_endif;

        sfpi::dst_reg[0] = result;
        sfpi::dst_reg++;
    }
}

/**
 * Core computation for inverse hyperbolic tangent (atanh)
 * 
 * @tparam APPROXIMATION_MODE When true, uses faster approximation
 * @tparam is_fp32_dest_acc_en Precision mode flag
 * @param x Input value in (-1, 1)
 * @return atanh(x)
 */
template <bool APPROXIMATION_MODE, bool is_fp32_dest_acc_en>
sfpi_inline sfpi::vFloat compute_atanh_core(sfpi::vFloat x) {
    sfpi::vFloat numerator = sfpi::vConst1 + x;
    sfpi::vFloat denominator = sfpi::vConst1 - x;
    sfpi::vFloat recip = _sfpu_reciprocal_<APPROXIMATION_MODE ? 2 : 3>(denominator);

    // Precision handling based on accumulation mode
    if constexpr (is_fp32_dest_acc_en || APPROXIMATION_MODE) {
        denominator = recip;
    } else {
        denominator = sfpi::reinterpret<sfpi::vFloat>(float_to_fp16b(recip, 0));
    }

    sfpi::vFloat ratio = numerator * denominator;
    sfpi::vFloat log_result = _calculate_log_body_no_init_(ratio);
    return 0.5f * log_result;
}

/**
 * Computes inverse hyperbolic tangent (atanh)
 * 
 * @tparam APPROXIMATION_MODE When true, uses faster approximation
 * @tparam is_fp32_dest_acc_en Precision mode flag
 * @tparam ITERATIONS Number of vector elements to process
 */
template <bool APPROXIMATION_MODE, bool is_fp32_dest_acc_en, int ITERATIONS>
inline void _calculate_atanh_() {
    for (int d = 0; d < ITERATIONS; d++) {
        sfpi::vFloat input = sfpi::dst_reg[0];
        sfpi::vFloat abs_input = sfpi::abs(input);
        sfpi::vFloat result;

        v_if (abs_input > sfpi::vConst1) {
            result = std::numeric_limits<float>::quiet_NaN(); // Domain error
        }
        v_elseif (abs_input == sfpi::vConst1) {
            sfpi::vFloat inf = std::numeric_limits<float>::infinity();
            result = sfpi::setsgn(inf, input); // atanh(±1) = ±∞
        }
        v_else {
            result = compute_atanh_core<APPROXIMATION_MODE, is_fp32_dest_acc_en>(input);
        }
        v_endif;

        sfpi::dst_reg[0] = result;
        sfpi::dst_reg++;
    }
}

// ======================================================
// Initialization Functions
// ======================================================

template <bool APPROXIMATION_MODE>
void _init_inverse_hyperbolic_() {
    _init_sqrt_<APPROXIMATION_MODE>();
}

template <bool APPROXIMATION_MODE>
void _init_atanh_() {
    _init_reciprocal_<APPROXIMATION_MODE>();
}

} // namespace sfpu
} // namespace ckernel