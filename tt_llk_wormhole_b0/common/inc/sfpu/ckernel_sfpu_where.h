// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_debug.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

/*
Expects following input in Dst register:
Index 0 ( Tile 0 ) -> condition tensor
Index 32 ( Tile 1 ) -> true tensor
Index 64 ( Tile 2 ) -> false tensor

*/

template <bool APPROXIMATION_MODE, int ITERATIONS = 32>
inline void _calculate_where_()
{
    constexpr uint dst_tile_size = 32;

    sfpi::vFloat output_tensor = 0;
    sfpi::vFloat true_tensor   = 0;
    sfpi::vFloat false_tensor  = 0;
    sfpi::vFloat cond          = sfpi::dst_reg[0];

    for (int i = 0; i < ITERATIONS; i++)
    {
        cond         = sfpi::dst_reg[0];
        true_tensor  = sfpi::dst_reg[dst_tile_size];
        false_tensor = sfpi::dst_reg[dst_tile_size * 2];

        v_if (cond == 0.0f)
        {
            // output_tensor = false_tensor;
            TTI_SFPLOAD(p_sfpu::LREG3, 6, 0, 128);
            TTI_SFPLOAD(p_sfpu::LREG3, 6, 0, 130);
        }
        v_else
        {
            // output_tensor = true_tensor;
            TTI_SFPLOAD(p_sfpu::LREG3, 6, 0, 64);
            TTI_SFPLOAD(p_sfpu::LREG3, 6, 0, 66);
        }
        v_endif;

        // sfpi::dst_reg[0] = output_tensor;
        TTI_SFPSTORE(p_sfpu::LREG3, 6, 0, 0);
        TTI_SFPSTORE(p_sfpu::LREG3, 6, 0, 2);

        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
