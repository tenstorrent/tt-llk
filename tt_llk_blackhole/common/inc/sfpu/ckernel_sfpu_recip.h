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

    // Handle y = nan.  This happens if:
    // 1. x = ±0:   t = ±0 * ∓inf + 1.0 = nan
    // 2. x = ±inf: t = ±inf * ∓0 + 1.0 = nan
    // For performance reasons we use the conditional exponent >= 255, which is
    // also true for y = ±inf, but in those cases the initial approximation
    // should also be correct.
    v_if (sfpi::exexp_nodebias(y) >= 255)
    {
        // Replace with the initial approximation, i.e. ±inf or 0.
        y = sfpi::approx_recip(x);
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
}

} // namespace sfpu
} // namespace ckernel
