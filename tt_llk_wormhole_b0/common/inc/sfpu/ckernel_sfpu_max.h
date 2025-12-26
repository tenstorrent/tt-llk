// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "lltt.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

constexpr int REPLAY_INSTR_CNT_ELWMAX = 5;

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

inline void _initialize_max_()
{
    eltwise_max_configure_addr_mod();

    // Record replay buffer that processes 4 rows at a time
    lltt::record<lltt::NoExec>(0, REPLAY_INSTR_CNT_ELWMAX);

    // Load from dest[0] (tile A) and dest[1] (tile B)
    TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::FP16B, ADDR_MOD_3, 0);   // Tile A, F0R0 even cols
    TTI_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::FP16B, ADDR_MOD_3, 64);  // Tile B, F0R0 even cols
    TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpswap::ALL_ROWS_MAX); // Max operation
    TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP16B, ADDR_MOD_3, 0);  // Store MAX to F0R0 even cols
    TTI_INCRWC(0, 2, 0, 0);
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_max_(const int iterations)
{
    for (int i = 0; i < iterations; i++)
    {
        lltt::replay(0, REPLAY_INSTR_CNT_ELWMAX);
    }
}

inline void _calculate_max_row_0_()
{
    eltwise_max_configure_addr_mod();

    // Face 0 Row 0 only
    TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::FP16B, ADDR_MOD_3, 0);   // Tile A, F0R0 even cols
    TTI_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::FP16B, ADDR_MOD_3, 64);  // Tile B, F0R0 even cols
    TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpswap::ALL_ROWS_MAX); // Max operation
    TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP16B, ADDR_MOD_3, 0);  // Store MAX to F0R0 even cols

    TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::FP16B, ADDR_MOD_3, 2);   // Tile A, F0R0 odd cols
    TTI_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::FP16B, ADDR_MOD_3, 66);  // Tile B, F0R0 odd cols
    TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpswap::ALL_ROWS_MAX); // Max operation
    TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP16B, ADDR_MOD_3, 2);  // Store MAX to F0R0 odd cols

    // // Face 1 Row 0 only

    TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::FP16B, ADDR_MOD_3, 16);  // Tile A, F1R0 even cols
    TTI_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::FP16B, ADDR_MOD_3, 80);  // Tile B, F1R0 even cols
    TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpswap::ALL_ROWS_MAX); // Max operation
    TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP16B, ADDR_MOD_3, 16); // Store MAX to F0R0 even cols

    TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::FP16B, ADDR_MOD_3, 18);  // Tile A, F1R0 odd cols
    TTI_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::FP16B, ADDR_MOD_3, 82);  // Tile B, F1R0 odd cols
    TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpswap::ALL_ROWS_MAX); // Max operation
    TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP16B, ADDR_MOD_3, 18); // Store MAX to F1R0 odd cols
}

} // namespace sfpu
} // namespace ckernel
