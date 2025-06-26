// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
// SPDX-FileCopyrightText: © 2025 Jason Davies <jason@jasondavies.com>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel
{
namespace sfpu
{

// See: Kokosiński, Z., Gepner, P., Moroz, L. et al.
// Fast and accurate approximation algorithms for computing floating point square root. Numer Algor (2024).
// https://doi.org/10.1007/s11075-024-01932-7

template <bool APPROXIMATE = false, bool RECIPROCAL = false>
sfpi_inline sfpi::vFloat _sfpu_sqrt_(const sfpi::vFloat x)
{
    sfpi::vInt i   = sfpi::reinterpret<sfpi::vInt>(sfpi::reinterpret<sfpi::vUInt>(x) >> 1);
    sfpi::vFloat y = sfpi::reinterpret<sfpi::vFloat>(sfpi::vConstIntPrgm0 - i);

    if constexpr (APPROXIMATE)
    {
        // Algorithm SQRT_10-bits, with modifications for reciprocal.
        sfpi::vFloat c            = x * y;
        sfpi::vFloat negative_y   = -y;
        sfpi::vFloat k1_minus_xyy = sfpi::vConstFloatPrgm1 + negative_y * c;

        if constexpr (RECIPROCAL)
        {
            y = y * k1_minus_xyy;
        }
        else
        {
            y = c * k1_minus_xyy;
        }
    }
    else
    {
        // Algorithm SQRT_23-bits, with modifications for reciprocal.
        sfpi::vFloat xy            = x * y;
        sfpi::vFloat negative_y    = -y;
        sfpi::vFloat c             = negative_y * xy;
        y                          = y * (sfpi::vConstFloatPrgm1 + c * (sfpi::vConstFloatPrgm2 + c));
        xy                         = x * y;
        negative_y                 = -y;
        sfpi::vFloat one_minus_xyy = sfpi::vConst1 + (negative_y * xy);

        if constexpr (RECIPROCAL)
        {
            sfpi::vFloat half_y = 0.5f * y;
            y                   = one_minus_xyy * half_y + y;
        }
        else
        {
            sfpi::vFloat half_xy = 0.5f * xy;
            y                    = one_minus_xyy * half_xy + xy;
        }
    }

    return y;
}

template <bool APPROXIMATION_MODE, int ITERATIONS, bool RECIPROCAL>
inline void _calculate_sqrt_internal_(int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat out = _sfpu_sqrt_<APPROXIMATION_MODE, RECIPROCAL>(sfpi::dst_reg[0]);
        sfpi::dst_reg[0] = out;
        //sfpi::dst_reg[0] = sfpi::reinterpret<sfpi::vFloat>(float_to_fp16b(out, 0));
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_sqrt_(int iterations)
{
    _calculate_sqrt_internal_<APPROXIMATION_MODE, ITERATIONS, false>(iterations);
}

template <bool APPROXIMATION_MODE>
inline void _init_sqrt_()
{
    if (APPROXIMATION_MODE)
    {
        sfpi::vConstIntPrgm0   = 0x5f0b3892;
        sfpi::vConstFloatPrgm1 = 1.89099014875f;
    }
    else
    {
        sfpi::vConstIntPrgm0   = 0x5f1110a0;
        sfpi::vConstFloatPrgm1 = 2.2825186f;
        sfpi::vConstFloatPrgm2 = 2.2533049f;
    }
}

} // namespace sfpu
} // namespace ckernel
