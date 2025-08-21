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

// Computes the reciprocal of a floating point value x.
// For max_iter = 2, this should take 20 cycles.
// For max_iter = 1, this should take 16 cycles.
template <int max_iter = 2>
sfpi_inline sfpi::vFloat _sfpu_reciprocal_(const sfpi::vFloat in)
{
    // Combines the sign and exponent of -1.0 with the mantissa of `in`.
    // Scale the input value to the range [1.0, 2.0), and make it negative.
    // If in ≠ ±0 and in ≠ ±inf, then x = in * 2**(127-in.Exp).
    // If in = ±0 or in = ±inf, then x = ±1.
    // Then negative_x = -x.
    sfpi::vFloat negative_x = sfpi::setman(sfpi::vConstNeg1, sfpi::reinterpret<sfpi::vInt>(in));

    // Quadratic initial estimate: y = 140/33 - 64/11 * x + 256/99 x**2.
    sfpi::vFloat y = sfpi::vConstFloatPrgm1 + sfpi::vConstFloatPrgm0 * negative_x;

    // Scale factor: we want 1/in = 1/x * scale.
    // For x ≠ ±0 and x ≠ ±inf, in = x * 2**-(127-in.Exp), so 1/in = 1/x * 2**(127-in.Exp).
    // Add float32 bias: scale.Exp = 127+127-in.Exp = 254-in.Exp.
    // For efficiency and handling of x = ±0 and x = ±inf, we set scale.Exp = 255-in.Exp = ~in.Exp.
    // This is efficiently computed with a single SFPNOT, followed by SFPSETMAN to clear the mantissa at the next opportunity.
    // The sign doesn't matter as we set the output sign to match the input at the end.
    // Not only is 255-in.Exp more efficient via SFPNOT, but it also ensures
    // that in.Exp == 0 results in ±inf, and in.Exp == 255 results in ±0.
    // See the scale factor adjustment via scale*0.5 below for further details.
    sfpi::vInt scale_bits = ~sfpi::reinterpret<sfpi::vUInt>(in);

    // Continue with quadratic estimate.
    y = sfpi::vConstFloatPrgm2 + y * negative_x;

    // Scale factor: set mantissa to zero.
    sfpi::vFloat scale = sfpi::setman(sfpi::reinterpret<sfpi::vFloat>(scale_bits), 0);

    // First iteration of Newton-Raphson: t = 1.0 - xy.
    sfpi::vFloat t = sfpi::vConst1 + negative_x * y;

    // Scale factor adjustment: scale = scale*0.5.
    // Note that scale = ±0 and scale = ±inf will be preserved.
    // If scale.Exp == 0, then scale = ±inf, so scale*0.5 == ±inf.
    // If scale.Exp == 255, then scale = ±0, so scale*0.5 == ±0.
    // Otherwise, scale.Exp = scale.Exp-1 = 255-in.Exp-1 = 254-in.Exp.
    scale *= 0.5f;

    // Continue Newton-Raphson: y = y + yt.
    y = y + y * t;

    if constexpr (max_iter > 1)
    {
        // Second iteration of Newton-Raphson: t = 1.0 - xy; y = y + yt.
        t = sfpi::vConst1 + negative_x * y;
        y = y + y * t;
    }

    // Apply scaling factor, and set sign to match input.
    y = y * scale;
    y = sfpi::setsgn(y, in);

    return y;
}

template <bool APPROXIMATION_MODE, int ITERATIONS, bool is_fp32_dest_acc_en>
inline void _calculate_reciprocal_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat in = sfpi::dst_reg[0];

        if constexpr (APPROXIMATION_MODE)
        {
            sfpi::dst_reg[0] = _sfpu_reciprocal_<0>(in);
        }
        else
        {
            if constexpr (is_fp32_dest_acc_en)
            {
                sfpi::dst_reg[0] = _sfpu_reciprocal_<2>(in);
            }
            else
            {
                sfpi::vFloat out = _sfpu_reciprocal_<1>(in);
                sfpi::dst_reg[0] = sfpi::reinterpret<sfpi::vFloat>(float_to_fp16b(out, 0));
            }
        }

        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
inline void _init_reciprocal_()
{
    sfpi::vConstFloatPrgm0 = 32.0f / 99.0f;
    sfpi::vConstFloatPrgm1 = 16.0f / 11.0f;
    sfpi::vConstFloatPrgm2 = 70.0f / 33.0f;
}

} // namespace sfpu
} // namespace ckernel
