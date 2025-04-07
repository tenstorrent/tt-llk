// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel
{
namespace sfpu
{

sfpi_inline void _calculate_dummy_()
{
    TTI_SFPLOADI(p_sfpu::LREG3, 4, 4);
    TTI_SFPSETSGN(1, p_sfpu::LREG3, p_sfpu::LREG3, 1);
    TTI_SFPSTORE(p_sfpu::LREG3, 4, 0, 0);
    
    
    // sfpi::vInt input = 4;
    // sfpi::dst_reg[0] = sfpi::setsgn(input, -5);
    // sfpi::dst_reg[0] = input; // sfpi::setsgn(input, -5);
}

template <bool HAS_BASE_SCALING>
sfpi_inline void _calculate_log_body_(const uint log_base_scale_factor)
{
    ////////////////////////////
    // Load From dest + "normalize to calculation range"
    ////////////////////////////
    sfpi::vFloat in = sfpi::dst_reg[0];
    sfpi::vFloat x  = setexp(in, 127); // set exp to exp bias (put in range of 1-2)

    // XXXXXX ask Namal? if we can derive the coefficients below to higher precision
    ////////////////////////////
    // Calculate Cheby Approximation using Horner Form Multiplication: 3rd Order
    // x* ( x* (A*x + B) + C) + D
    // A :0.1058, B: -0.3942, C: 0.9813, D: 0.006
    // Run above on (x-1) so x is in ln(x+1), plug (x-1 into equation above to
    // save the subtract and get A',B',C',D'):
    // A' = A
    // B' = -3A + B
    // C' = 3a -2B + C
    // D' = -A + B - C + D
    // A':0.1058, B':-0.7116, C':2.0871, D':-1.4753
    ////////////////////////////
    sfpi::vFloat a = sfpi::vConstFloatPrgm1;
    sfpi::vFloat b = sfpi::vConstFloatPrgm2;
    // XXXXX try variants of the below: B'=.7122, C'=2.0869
    sfpi::vFloat series_result = x * (x * (x * a + b) + 2.0871) + -1.4753f;

    ////////////////////////////
    // Convert exponent to float
    ////////////////////////////
    sfpi::vInt exp = exexp(in);
    v_if (exp < 0)
    {
        exp = sfpi::setsgn(~exp + 1, 1);
    }
    v_endif;

    sfpi::vFloat expf      = int32_to_float(exp, 0);
    sfpi::vFloat vConstLn2 = sfpi::vConstFloatPrgm0;
    sfpi::vFloat result    = expf * vConstLn2 + series_result; // exp correction: ln(1+x) + exp*ln(2)

    if constexpr (HAS_BASE_SCALING)
    {
        result *= sfpi::s2vFloat16a(log_base_scale_factor);
    }

    ////////////////////////////
    // Base case when input is 0. ln(0) = -inf
    ////////////////////////////
    v_if (in == 0.0F)
    { // Reload for register pressure
        result = -std::numeric_limits<float>::infinity();
    }
    v_endif;

    sfpi::dst_reg[0] = result;
}

template <bool APPROXIMATION_MODE, bool HAS_BASE_SCALING, int ITERATIONS>
inline void _calculate_log_(const int iterations, uint log_base_scale_factor)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_log_body_<HAS_BASE_SCALING>(log_base_scale_factor);
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
inline void _init_log_()
{
    sfpi::vConstFloatPrgm0 = 0.692871f; // ln2

    // XXXXX could do these to higher precision
    sfpi::vConstFloatPrgm1 = 0.1058f;
    sfpi::vConstFloatPrgm2 = -0.7166f;
}

} // namespace sfpu
} // namespace ckernel
