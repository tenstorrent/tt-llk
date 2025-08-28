// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"
#include "llk_defs.h"
namespace ckernel
{
namespace sfpu
{

template <ApproximationMode APPROX_MODE, int ITERATIONS>
inline void _calculate_power_(const int iterations, uint exponent)
{
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat in     = sfpi::dst_reg[0];
        sfpi::vFloat result = in * in;
        for (uint i = 2; i < exponent; i++)
        {
            result *= in;
        }

        sfpi::dst_reg[0] = result;

        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
