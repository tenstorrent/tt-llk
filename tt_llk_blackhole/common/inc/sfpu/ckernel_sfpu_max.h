// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "lltt.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

constexpr int REPLAY_INTRUCTIONS = 6;

inline void eltwise_max_configure_addr_mod()
{
    // ADDR_MOD_7: No increment for initial load
    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_7);
}

inline void _calculate_max_init_()
{
    // Size of each tile in Dest is 64 rows
    constexpr uint dst_tile_size          = 64;
    constexpr uint values_tile_offset0    = 0 * dst_tile_size;
    constexpr uint values_tile_offset1    = 1 * dst_tile_size;
    constexpr uint values_tile_offset_out = 0 * dst_tile_size;

    lltt::record<lltt::NoExec>(0, REPLAY_INTRUCTIONS);
    // Load values from first destination register location into LREG0
    TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_7, values_tile_offset0);

    // Load values from second destination register location into LREG1
    TTI_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_7, values_tile_offset1);

    // Perform element-wise max comparison using ALL_ROWS_MAX mode
    TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpswap::ALL_ROWS_MAX);
    TTI_SFPNOP;

    // Store maximum results from LREG0 back to destination register
    TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_7, values_tile_offset_out);

    // Increment destination register for next iteration
    // sfpi::dst_reg++;
    TTI_INCRWC(0, 2, 0, 0);
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_max_(int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        lltt::replay(0, REPLAY_INTRUCTIONS);
    }
}

/*************************************************************************************
SDPA specific implementation - just do max on rows 0
*************************************************************************************/

inline void _calculate_max_row_0_()
{
    eltwise_max_configure_addr_mod();

    // Face 0
    TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 0);  // Tile A, F0R0 even cols
    TTI_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 64); // Tile B, F0R0 even cols
    TTI_SFPLOAD(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 2);  // Tile A, F0R0 odd cols
    TTI_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 66); // Tile B, F0R0 odd cols

    // Start first swap
    TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpswap::ALL_ROWS_MAX); // Max operation (even)

    // Start second swap
    TTI_SFPSWAP(0, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpswap::ALL_ROWS_MAX); // Max operation (odd)

    // Now store results (swaps are done, no explicit NOPs needed)
    TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 0); // Store MAX to F0R0 even cols
    TTI_SFPSTORE(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 2); // Store MAX to F0R0 odd cols

    // Face 1
    TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 16); // Tile A, F1R0 even cols
    TTI_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 80); // Tile B, F1R0 even cols
    TTI_SFPLOAD(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 18); // Tile A, F1R0 odd cols
    TTI_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 82); // Tile B, F1R0 odd cols

    // Start first swap
    TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpswap::ALL_ROWS_MAX); // Max operation (even)

    // Start second swap
    TTI_SFPSWAP(0, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpswap::ALL_ROWS_MAX); // Max operation (odd)

    // Now store results (swaps are done, no explicit NOPs needed)
    TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 16); // Store MAX to F0R0 even cols
    TTI_SFPSTORE(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 18); // Store MAX to F0R0 odd cols
}

} // namespace sfpu
} // namespace ckernel
