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
    TTI_SFPLOAD(p_sfpu::LREG0, 2, ADDR_MOD_3, 0);    // Tile A, current row
    TTI_SFPLOAD(p_sfpu::LREG1, 2, ADDR_MOD_3, 64);   // Tile B, current row (offset by 64)
    TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, 1); // Max operation
    TTI_SFPSTORE(p_sfpu::LREG0, 2, ADDR_MOD_3, 0);   // Store result, advance by 2 rows
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

} // namespace sfpu
} // namespace ckernel
