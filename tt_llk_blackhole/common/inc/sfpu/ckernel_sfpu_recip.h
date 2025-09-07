// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
// SPDX-FileCopyrightText: © 2025 Jason Davies <jason@jasondavies.com>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu_rsqrt_compat.h"
#include "llk_defs.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

// Computes the reciprocal of a floating point value x.
template <int max_iter = 2>
sfpi_inline sfpi::vFloat _sfpu_reciprocal_(const sfpi::vFloat x)
{
    // sfpi::approx_recip(x) will return ±0 for x = ±inf or x ≥ ±2**126, and ±inf for x = ±0.
    sfpi::vFloat y = sfpi::approx_recip(x);

    // Optionally improve the approximation using Newton-Raphson.
    if (max_iter > 0)
    {
        // Normally, t = 2.0 - x * y, but we negate this (and negate again using y = y * -t later).
        // On Blackhole, when x=0 and y=infinity (and vice versa), t=+NaN regardless of the operand signs.
        // Negating the meaning of t makes it easier to detect NaN using a trivial sign check t>=0.
        // Equivalently, we could use v_if (t >= 2.0) instead, but SFPI doesn't support SFPLE/SFPGT at the moment.
        sfpi::vFloat t = x * y - sfpi::vConstFloatPrgm0;

        if (max_iter > 1)
        {
            sfpi::vFloat y1 = y * -t - sfpi::vConst0;
            // If t=NaN, then t>=0.  This check consumes the SFPNOP slot of the preceding SFPMAD.
            v_if (t < 0)
            {
                t = x * y1 - sfpi::vConstFloatPrgm0;
                y = y1 * -t - sfpi::vConst0;
            }
            v_endif;
        }
        else
        {
            // If t=NaN, then t>=0.  This check cannot be hidden in a SFPNOP slot as it depends on the result of the preceding SFPMAD.
            v_if (t < 0)
            {
                y = y * -t - sfpi::vConst0;
            }
            v_endif;
        }
    }

    return y;
}

template <ApproximationMode APPROX_MODE, int ITERATIONS, bool is_fp32_dest_acc_en>
inline void _calculate_reciprocal_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat in = sfpi::dst_reg[0];

        if constexpr (APPROX_MODE)
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

template <ApproximationMode APPROX_MODE, int ITERATIONS, bool is_fp32_dest_acc_en, bool legacy_compat = false>
inline void _calculate_reciprocal_(const int iterations)
{
    if constexpr (legacy_compat)
    {
        _calculate_reciprocal_compat_<APPROX_MODE, ITERATIONS, is_fp32_dest_acc_en>(iterations);
    }
    else
    {
        _calculate_reciprocal_internal_<APPROX_MODE, ITERATIONS, is_fp32_dest_acc_en>(iterations);
    }
}

template <ApproximationMode APPROX_MODE, bool legacy_compat = false>
inline void _init_reciprocal_()
{
    if constexpr (!APPROX_MODE && !legacy_compat)
    {
        sfpi::vConstFloatPrgm0 = 2.0f;
    }
}

} // namespace sfpu
} // namespace ckernel
