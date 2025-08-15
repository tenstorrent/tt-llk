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
    // sfpi::approx_recip(x) will return ±0 for x = ±inf or x ≥ ±2**126, and ±inf for x = ±0.
    sfpi::vFloat y = sfpi::approx_recip(x);

    // Note that x = ±2**126 is an edge case, because its reciprocal is not a
    // denormal number, unlike all larger numbers |x| > 2**126, but the initial
    // approximation returned by sfpi::approx_recip will be ±0.

    // Now we improve the approximation using Newton-Raphson.

    // Check x to ensure that it is not ±infinity.  For simplicity we also exclude exponent = 254.
    // We also check that it is not ±0 within the block below for performance reasons.
    sfpi::vInt exponent = sfpi::exexp_nodebias(x);
    v_if (exponent < 254)
    {
        // Now we handle the x = ±2**126 edge case.
        // Check for |x| ≥ 2**126, i.e. exponent = 253.
        v_if (exponent >= 253)
        {
            // Set y = ±2**-126 for all values.  For |x| > 2**126, the Newton-Raphson iteration will flush to zero.
            // For |x| = 2**126, the approximation is exactly correct and will be preserved.
            y = sfpi::reinterpret<sfpi::vFloat>(sfpi::reinterpret<sfpi::vInt>(x) & sfpi::vConstIntPrgm1);

            // The above should compile to a single instruction, but a compiler bug makes it generate 3 instructions:
            // https://github.com/tenstorrent/tt-metal/issues/26928
        }
        v_endif;

        // One iteration of Newton-Raphson.
        sfpi::vFloat t = y * -x + sfpi::vConstFloatPrgm0;

        // For performance reasons, we exclude exponent = 0 here, to hide the
        // `sfpnop` that would otherwise be generated.
        v_and(exponent >= 1);

        // -sfpi::vConst0 is used to preserve the sign of y = -0.0.
        y = y * t - sfpi::vConst0;

        if constexpr (!APPROXIMATE)
        {
            // 2nd iteration of Newton-Raphson.
            t = y * -x + sfpi::vConstFloatPrgm0;
            y = y * t - sfpi::vConst0;
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
    sfpi::vConstIntPrgm1   = 0x80800000;
}

} // namespace sfpu
} // namespace ckernel
