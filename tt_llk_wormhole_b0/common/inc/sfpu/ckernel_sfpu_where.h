// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <int ITERATIONS = 32>
inline void _calculate_where_(int dst_offset, sfpi::vFloat true_value, sfpi::vFloat false_value)
{
    constexpr uint dst_tile_size = 32;
    sfpi::vFloat input_tensor    = sfpi::dst_reg[dst_offset * dst_tile_size];
    sfpi::vFloat output_tensor   = 0.0f;

    for (int i = 0; i < ITERATIONS; i++)
    {
        sfpi::vFloat cond = sfpi::dst_reg[dst_offset * dst_tile_size]; // input_tensor;
        v_if (cond != 0.0f)
        {
            output_tensor = true_value;
        }
        v_else
        {
            output_tensor = false_value;
        }
        v_endif;
        sfpi::dst_reg[0] = output_tensor;
        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
