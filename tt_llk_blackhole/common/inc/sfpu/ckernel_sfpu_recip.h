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
    sfpi::vFloat y = sfpi::approx_recip(x);

    // One iteration of Newton-Raphson.
    sfpi::vFloat t = y * -x + sfpi::vConst1;
    y              = y * t + y;

    if constexpr (!APPROXIMATE)
    {
        // 2nd iteration of Newton-Raphson
        t = y * -x + sfpi::vConst1;
        y = y * t + y;
    }

    // Handle x = 0, y = ±inf, which results in y * -x + 1.0 = nan.
    v_if (sfpi::exexp_nodebias(y) >= 255)
    {
        // Converts ±nan to ±inf (preserving sign).
        y = sfpi::setman(y, 0);
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
