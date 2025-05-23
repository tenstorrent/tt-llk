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
    sfpi::vInt i = sfpi::reinterpret<sfpi::vInt>(sfpi::reinterpret<sfpi::vUInt>(x) >> 1);
    sfpi::vFloat y = sfpi::reinterpret<sfpi::vFloat>(sfpi::vConstIntPrgm0 - i);

    if constexpr (APPROXIMATE)
    {
        // Algorithm SQRT_10-bits, with modifications for reciprocal.
        sfpi::vFloat c = x * y;
        sfpi::vFloat negative_y = -y;
        if constexpr (RECIPROCAL) {
            y = y * (sfpi::vConstFloatPrgm1 + negative_y * c);
        }
        else
        {
            y = c * (sfpi::vConstFloatPrgm1 + negative_y * c);
        }
    }
    else
    {
        // Algorithm SQRT_23-bits, with modifications for reciprocal.
        sfpi::vFloat negative_x = -x;
        sfpi::vFloat c = negative_x * y * y;
        y = y * (sfpi::vConstFloatPrgm1 + c * (sfpi::vConstFloatPrgm2 + c));
        sfpi::vFloat half_y = sfpi::addexp(y, -1);

        if constexpr (RECIPROCAL) {
            y = y + (sfpi::vConst1 + negative_x * y * y) * half_y;
        }
        else
        {
            half_y = -half_y;
            y = x * y;
            y = y + (y * y + negative_x) * half_y;
        }
    }

    return y;
}

template <bool APPROXIMATION_MODE, int ITERATIONS, bool RECIPROCAL>
inline void _calculate_sqrt_(int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        sfpi::dst_reg[0] = _sfpu_sqrt_<APPROXIMATION_MODE, RECIPROCAL>(sfpi::dst_reg[0]);
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
inline void _init_sqrt_()
{
    if (APPROXIMATION_MODE)
    {
        sfpi::vConstIntPrgm0 = 0x5f0b3892;
        sfpi::vConstFloatPrgm1 = 1.89099014875f;
    }
    else
    {
        sfpi::vConstIntPrgm0 = 0x5f1110a0;
        sfpi::vConstFloatPrgm1 = 2.2825186f;
        sfpi::vConstFloatPrgm2 = 2.2533049f;
    }
}

} // namespace sfpu
} // namespace ckernel
