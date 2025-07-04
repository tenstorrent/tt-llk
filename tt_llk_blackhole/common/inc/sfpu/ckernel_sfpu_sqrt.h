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

// Assumptions:
// - x is positive.
// - x is not subnormal.
template <bool APPROXIMATE = false, bool RECIPROCAL = false>
sfpi_inline sfpi::vFloat _sfpu_sqrt_(const sfpi::vFloat x)
{
    // this is just a hack so that zero becomes +inf, and +inf remains +inf
    //sfpi::vFloat tmp1 = sfpi::addexp(x, -1);
    //tmp1 = tmp1 * 2.0f;
    sfpi::vInt i   = sfpi::reinterpret<sfpi::vInt>(sfpi::reinterpret<sfpi::vUInt>(x) >> 1);
    sfpi::vFloat y = sfpi::reinterpret<sfpi::vFloat>(sfpi::vConstIntPrgm0 - i);
    // if x = 0, then y = some large number
    // if x = inf, then y = some small number

    if constexpr (APPROXIMATE || 0)
    {
        // Algorithm SQRT_10-bits, with modifications for reciprocal.
        sfpi::vFloat c          = x * y; // if x == 0, c == 0; if x == inf, c == inf
        sfpi::vFloat negative_y = -y;
        sfpi::vFloat infinity = sfpi::s2vFloat16b(std::numeric_limits<float>::infinity());
        sfpi::vInt infinity_bits = sfpi::reinterpret<sfpi::vInt>(infinity);
        sfpi::vFloat t          = sfpi::vConstFloatPrgm1 + negative_y * c; // if x == 0, t == k1; if x == inf, t == -inf
        if constexpr (RECIPROCAL)
        {
            sfpi::vInt x_bits = sfpi::reinterpret<sfpi::vInt>(x);
            sfpi::vInt _x_bits = infinity_bits - x_bits;
            v_if (x_bits != 0 && _x_bits != 0) {
                y = y * t; // y = y * (k1 - y * y * x)
            } v_else {
                // if e == 0, set ie = 255 (t=inf)
                // if e == 255, set ie = 0 (t=0)
                y = sfpi::reinterpret<sfpi::vFloat>(_x_bits);
            } v_endif;
        }
        else
        {
            y = c;
            v_if (sfpi::reinterpret<sfpi::vInt>(x) != infinity_bits) {
                y = y * t;
            } v_endif;
            // if x == 0, y = c * t = 0 * 0 = 0
            // if x == inf, y = c * t = inf * -inf = -inf
            //v_if (sfpi::reinterpret<sfpi::vInt>(x) != 0) {
            //    y = c * t;
            //} v_endif;
        }
    }
    else
    {
        // Algorithm SQRT_23-bits, with modifications for reciprocal.
        sfpi::vFloat xy            = x * y;
        sfpi::vFloat negative_y    = -y;
        sfpi::vFloat c             = negative_y * xy;
        sfpi::vFloat infinity = sfpi::s2vFloat16b(std::numeric_limits<float>::infinity());
        sfpi::vInt infinity_bits = sfpi::reinterpret<sfpi::vInt>(infinity);
        y                          = y * (sfpi::vConstFloatPrgm1 + c * (sfpi::vConstFloatPrgm2 + c));
        xy                         = x * y;
        negative_y                 = -y;
        sfpi::vFloat negative_xyy = negative_y * xy;
        sfpi::vFloat one_minus_xyy = sfpi::vConst1 + (negative_y * xy);

        if constexpr (RECIPROCAL)
        {
            sfpi::vFloat half_y = sfpi::addexp(y, -1);
            sfpi::vInt x_bits = sfpi::reinterpret<sfpi::vInt>(x);
            sfpi::vInt _x_bits = infinity_bits - x_bits;
            v_if (_x_bits != 0 && x_bits != 0) {
                y = one_minus_xyy * half_y + y;
            } v_else {
                y = sfpi::reinterpret<sfpi::vFloat>(_x_bits);
            } v_endif;
        }
        else
        {
            sfpi::vFloat half_xy = 0.5f * xy;
            // if xy is inf or nan, then y will already be inf or nan
            // if xy is inf, then we skip this step to avoid it getting converted to nan due to inf - inf
            v_if (sfpi::reinterpret<sfpi::vInt>(x) < infinity_bits) {
                y = one_minus_xyy * half_xy + xy;
            } v_endif;
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
        sfpi::dst_reg[0] = _sfpu_sqrt_<APPROXIMATION_MODE, RECIPROCAL>(sfpi::dst_reg[0]);
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
    if (APPROXIMATION_MODE || 0)
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
