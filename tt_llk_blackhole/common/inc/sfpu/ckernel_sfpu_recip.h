// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
// SPDX-FileCopyrightText: © 2025 Jason Davies <jason@jasondavies.com>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

// See: Cezary J. Walczyk, Leonid V. Moroz, Volodymyr Samotyy, and Jan L. Cieśliński.
// Optimal Approximation of the 1/x Function using Chebyshev Polynomials and Magic Constants.
// https://doi.org/10.1145/3708472

// Computes the reciprocal of a floating point value x.
// Returns 0 if abs(x) > 0x1.6a09e6p+126, or if x is NaN.
template <bool APPROXIMATE = false>
sfpi_inline sfpi::vFloat _sfpu_reciprocal_(const sfpi::vFloat x)
{
    sfpi::vFloat abs_x = sfpi::abs(x);
    sfpi::vInt y0_bits = sfpi::vConstIntPrgm0 - sfpi::reinterpret<sfpi::vInt>(abs_x);
    sfpi::vFloat y;
    v_if (y0_bits >= 0)
    {
        y              = sfpi::setsgn(sfpi::reinterpret<sfpi::vFloat>(y0_bits), x);
        sfpi::vFloat t = y * -x + sfpi::vConstFloatPrgm2;
        y              = y * sfpi::vConstFloatPrgm1;
        y              = y * t;

        if constexpr (!APPROXIMATE)
        {
            // 2nd iteration of Newton-Raphson
            t = y * -x + sfpi::vConst1;
            y = y * t + y;
        }
    }
    v_else
    {
        // This occurs for a small portion of very large floats, infinity, and NaN.
        y = sfpi::vConst0;
    }
    v_endif;

    v_if (x == 0)
    {
        y = sfpi::s2vFloat16b(std::numeric_limits<float>::infinity());
    }
    v_endif;

    return y;
}

template <bool APPROXIMATION_MODE, int ITERATIONS, bool is_fp32_dest_acc_en>
inline void _calculate_reciprocal_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat in  = sfpi::dst_reg[0];
        sfpi::vFloat out = _sfpu_reciprocal_<APPROXIMATION_MODE>(in);

        if constexpr (is_fp32_dest_acc_en || APPROXIMATION_MODE)
        {
            sfpi::dst_reg[0] = out;
        }
        else
        {
            sfpi::dst_reg[0] = sfpi::reinterpret<sfpi::vFloat>(float_to_fp16b(out, 0));
        }

        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
inline void _init_reciprocal_()
{
    sfpi::vConstIntPrgm0   = 0x7eb504f3;
    sfpi::vConstFloatPrgm1 = 1.94090888923f;
    sfpi::vConstFloatPrgm2 = 1.43566017178f;
}

} // namespace sfpu
} // namespace ckernel
