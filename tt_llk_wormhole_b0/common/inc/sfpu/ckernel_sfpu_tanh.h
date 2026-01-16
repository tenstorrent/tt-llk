// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <sys/_stdint.h>

#include "ckernel_sfpu_load_config.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_tanh_(const int iterations)
{
    sfpi::vFloat C0 = sfpi::vConst1;
    sfpi::vFloat C1 = -1.0f / 3.0f;
    sfpi::vFloat C2 = 2.0f / 15.0f;
    sfpi::vFloat C3 = -17.0f / 315.0f;

#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat x  = sfpi::dst_reg[0];
        sfpi::vFloat x2 = x * x;

        // sfpi::vFloat p = (C2 * x2 + C1) * x2 + C0;
        sfpi::vFloat p = ((C3 * x2 + C2) * x2 + C1) * x2 + C0;

        sfpi::vFloat y = x * p;

        sfpi::dst_reg[0] = y;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
inline void _init_tanh_()
{
}

} // namespace sfpu
} // namespace ckernel
