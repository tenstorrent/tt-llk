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

inline void eltwise_max_configure_addr_mod()
{
    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_7);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 2},
    }
        .set(ADDR_MOD_6);
}

inline void _initialize_max_()
{
    eltwise_max_configure_addr_mod();

    lltt::record<lltt::NoExec>(0, 4);
    TTI_SFPLOAD(p_sfpu::LREG0, 2, ADDR_MOD_3, 0);
    TTI_SFPLOAD(p_sfpu::LREG1, 2, ADDR_MOD_3, 64);
    TTI_SFPSWAP(0 /*unused*/, p_sfpu::LREG0, p_sfpu::LREG1, 1);
    TTI_SFPSTORE(p_sfpu::LREG0, 2, ADDR_MOD_2, 0);

    // sfpi::dst_reg++;
    // TTI_INCRWC(0, 2, 0, 0);
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_max_(const int iterations)
{
    for (int d = 0; d < iterations; d++)
    {
        // sfpi::vFloat a = sfpi::dst_reg[0];
        // sfpi::vFloat b = sfpi::dst_reg[32];
        // v_if (a < b)
        // {
        //     sfpi::dst_reg[0] = b;
        // }
        // v_endif;

        // TTI_SFPLOAD(p_sfpu::LREG0, 2, ADDR_MOD_3, 0);
        // TTI_SFPLOAD(p_sfpu::LREG1, 2, ADDR_MOD_3, 64);
        // TTI_SFPSWAP(0 /*unused*/,p_sfpu::LREG0, p_sfpu::LREG1, 1);
        // TTI_SFPSTORE(p_sfpu::LREG0, 2, ADDR_MOD_3, 0);

        // // sfpi::dst_reg++;
        // TTI_INCRWC(0, 2, 0, 0);

        lltt::replay(0, 4);
    }
}

} // namespace sfpu
} // namespace ckernel
