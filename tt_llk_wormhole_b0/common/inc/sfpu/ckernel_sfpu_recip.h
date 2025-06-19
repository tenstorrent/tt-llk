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

template <bool APPROXIMATE = false>
sfpi_inline sfpi::vFloat _sfpu_reciprocal_(const sfpi::vFloat in)
{

    // Set the sign bit to zero, making x positive.  This avoids issues when subtracting from the magic constant, which would complicate edge case detection.
    sfpi::vFloat x = setsgn(in, 0);

    // Note that SFPI's codegen is suboptimal here, since we can use SFPIADD to set the condition flags directly.
    sfpi::vFloat y = sfpi::reinterpret<sfpi::vFloat>(sfpi::vConstIntPrgm0 - sfpi::reinterpret<sfpi::vInt>(x));

    // Handle cases where the bit pattern x > magic constant.  This happens for:
    // - extremely large finite values, whose reciprocal will be zero.
    // - +inf, whose reciprocal will be zero.
    // - +nan, which we do not support, but zero is acceptable.
    v_if (y < 0)
    {
        x = sfpi::vConst0;
        y = sfpi::vConst0;
    }
    v_endif;

    sfpi::vFloat negative_x = -x;
    sfpi::vFloat t          = sfpi::vConstFloatPrgm2 + negative_x * y;
    sfpi::vFloat t1         = y * sfpi::vConstFloatPrgm1;
    y                       = t1 * t;

    if constexpr (!APPROXIMATE)
    {
        // 2nd iteration of Newton-Raphson
        t = y * negative_x + sfpi::vConst1;
        y = y * t + y;
    }

    // Restore sign bit.
    return setsgn(y, in);
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
