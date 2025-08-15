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
template <bool APPROXIMATE = false>
sfpi_inline sfpi::vFloat _sfpu_reciprocal_(const sfpi::vFloat x)
{
    sfpi::vFloat y      = sfpi::approx_recip(x);
    sfpi::vInt exponent = sfpi::exexp_nodebias(y);

    // Ensure that y is not ±0 or ±infinity for Newton-Raphson.
    // If exponent = 0, then y = ±0.
    // If exponent = 255, then y = ±infinity.

    v_if (exponent >= 1)
    {
        // One iteration of Newton-Raphson.
        sfpi::vFloat t = y * -x + sfpi::vConstFloatPrgm0;

        // For performance reasons, we exclude exponent = 255 here, to hide the
        // `sfpnop` that would otherwise be generated.
        v_and(exponent < 255);

        y = y * t;

        if constexpr (!APPROXIMATE)
        {
            // 2nd iteration of Newton-Raphson
            t = y * -x + sfpi::vConstFloatPrgm0;
            y = y * t;
        }
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
    sfpi::vConstFloatPrgm0 = 2.0f;
}

} // namespace sfpu
} // namespace ckernel
