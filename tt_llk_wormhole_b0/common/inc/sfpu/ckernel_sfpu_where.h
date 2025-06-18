// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

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

template <int ITERATIONS = 32>
inline void _calculate_where_()
{
    constexpr uint dst_tile_size = 32;

    sfpi::vUInt output_tensor = 0;
    sfpi::vUInt true_tensor   = 0;
    sfpi::vUInt false_tensor  = 0;
    // sfpi::vUInt sign_shift = 31;
    sfpi::vUInt cond = sfpi::dst_reg[0];

    for (int i = 0; i < ITERATIONS; i++)
    {
        // fetch next conditions, true and false values
        cond         = sfpi::dst_reg[0];
        true_tensor  = sfpi::dst_reg[dst_tile_size];
        false_tensor = sfpi::dst_reg[dst_tile_size * 2];

        v_if (cond != 0.0f)
        {
            output_tensor = true_tensor;
        }
        v_else
        {
            output_tensor = false_tensor;
        }
        v_endif;

        v_if (output_tensor == 0x80000000)
        {
            output_tensor = 55;
        }
        v_endif;

        sfpi::dst_reg[0]                 = output_tensor;
        sfpi::dst_reg[dst_tile_size * 3] = (output_tensor & 0x80000000); // >> sign_shift;
        sfpi::dst_reg++;
    }
    // for(int i = 0; i < ITERATIONS; i++)
    // {
    //     sfpi::dst_reg[0] = sfpi::dst_reg[0] + sfpi::dst_reg[3*dst_tile_size];
    // }
}

} // namespace sfpu
} // namespace ckernel
