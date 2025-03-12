// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_clamp_(const int iterations, uint param0, uint param1, uint param2)
{
    // All params are in FP16 format
    // param0 = min
    // param1 = max

    // uint format = (param0 >> 16)&0x1;
    s2sfpi::vFloat16::Format format = s2sfpi::vFloat16::fp16a;

    // SFPU microcode
    sfpi::vFloat min = s2sfpi::vFloat16(param0, format);
    sfpi::vFloat max = s2sfpi::vFloat16(param1, format);
#pragma GCC unroll 0
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat val = sfpi::dst_reg[0];

        v_if (val < min)
        {
            val = s2sfpi::vFloat16(param0, format);
        }
        v_elseif (val >= max)
        {
            val = s2sfpi::vFloat16(param1, format);
        }
        v_endif;

        sfpi::dst_reg[0] = val + s2sfpi::vFloat16b(param2); // 12 bits

        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
