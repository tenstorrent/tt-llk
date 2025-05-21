// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

namespace
{
constexpr double FRAC_1_PI = 0.3183098861837907;

template <bool APPROXIMATION_MODE>
static sfpi::vFloat sfpu_sinpi(sfpi::vFloat x);

template <>
sfpi_inline sfpi::vFloat sfpu_sinpi<true>(sfpi::vFloat x)
{
    sfpi::vFloat xx = x * x;

    return x * ((0x1.29cf02p+1f * xx - 0x1.4954d4p+2f) * xx + 0x1.92149p+1f);
}

template <>
sfpi_inline sfpi::vFloat sfpu_sinpi<false>(sfpi::vFloat x)
{
    sfpi::vFloat xx = x * x;

    return x * ((((0x1.406628p-4f * xx - 0x9.93f86p-4f) * xx + 0x2.8cd64p+0f) * xx - 0x5.2aef6p+0f) * xx + 0x3.243f6cp+0f);
}
} // namespace

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_sine_()
{
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat v     = sfpi::dst_reg[0] * FRAC_1_PI;
        sfpi::vInt whole_v = float_to_int16(v, 0);
        v -= int32_to_float(whole_v, 0);
        v = sfpu_sinpi<APPROXIMATION_MODE>(v);

        v_if (whole_v & 1)
        {
            v = -v;
        }
        v_endif;
        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_cosine_()
{
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat v     = sfpi::dst_reg[0] * FRAC_1_PI + 0.5f;
        sfpi::vInt whole_v = float_to_int16(v, 0);
        v -= int32_to_float(whole_v, 0);
        v = sfpu_sinpi<APPROXIMATION_MODE>(v);

        v_if (whole_v & 1)
        {
            v = -v;
        }
        v_endif;
        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
