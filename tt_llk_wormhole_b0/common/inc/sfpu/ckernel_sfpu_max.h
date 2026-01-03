// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_max_(int iterations)
{
    // Size of each tile in Dest is 64 rows
    constexpr uint dst_tile_size          = 64;
    constexpr uint values_tile_offset0    = 0 * dst_tile_size;
    constexpr uint values_tile_offset1    = 1 * dst_tile_size;
    constexpr uint values_tile_offset_out = 0 * dst_tile_size;

#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        // Load values from first destination register location into LREG0
        TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_3, values_tile_offset0);

        // Load values from second destination register location into LREG1
        TTI_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_3, values_tile_offset1);

        // Perform element-wise max comparison using ALL_ROWS_MAX mode
        TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpswap::ALL_ROWS_MAX);

        // Store maximum results from LREG0 back to destination register
        TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_3, values_tile_offset_out);

        // Increment destination register for next iteration
        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
