// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_ops.h"
#include "lltt.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE /*unused*/, int ITERATIONS /*unused*/>
inline void _calculate_cumsum_(const std::uint32_t dst_index_in, const std::uint32_t dst_index_out, const bool first)
{
    constexpr std::uint32_t dst_tile_size = 64;
    const std::uint32_t load_off          = dst_index_in * dst_tile_size;
    const std::uint32_t store_off         = dst_index_out * dst_tile_size;

    if (first)
    {
        // Clear context for F0
        TTI_SFPMOV(0, 9, 4, 0);
        TTI_SFPMOV(0, 9, 5, 0);
        TTI_SFPMOV(0, 9, 6, 0);
        TTI_SFPMOV(0, 9, 7, 0);
    }

    // F0,1 R0
    TT_SFPLOAD(0, 0, ADDR_MOD_3, load_off + 0);
    TT_SFPLOAD(1, 0, ADDR_MOD_3, load_off + 2);
    TT_SFPLOAD(2, 0, ADDR_MOD_3, load_off + 0 + 16);
    TT_SFPLOAD(3, 0, ADDR_MOD_3, load_off + 2 + 16);

    TTI_SFPTRANSP(0, 0, 0, 0);
    lltt::replay(0, 8);
    TTI_SFPTRANSP(0, 0, 0, 0);

    TT_SFPSTORE(0, 0, ADDR_MOD_3, store_off + 0);
    TT_SFPSTORE(1, 0, ADDR_MOD_3, store_off + 2);
    TT_SFPSTORE(2, 0, ADDR_MOD_3, store_off + 0 + 16);
    TT_SFPSTORE(3, 0, ADDR_MOD_3, store_off + 2 + 16);

    // F0,1 R4
    TT_SFPLOAD(4, 0, ADDR_MOD_3, load_off + 4);
    TT_SFPLOAD(5, 0, ADDR_MOD_3, load_off + 6);
    TT_SFPLOAD(6, 0, ADDR_MOD_3, load_off + 4 + 16);
    TT_SFPLOAD(7, 0, ADDR_MOD_3, load_off + 6 + 16);

    TTI_SFPTRANSP(0, 0, 0, 0);
    lltt::replay(8, 8);
    TTI_SFPTRANSP(0, 0, 0, 0);

    TT_SFPSTORE(4, 0, ADDR_MOD_3, store_off + 4);
    TT_SFPSTORE(5, 0, ADDR_MOD_3, store_off + 6);
    TT_SFPSTORE(6, 0, ADDR_MOD_3, store_off + 4 + 16);
    TT_SFPSTORE(7, 0, ADDR_MOD_3, store_off + 6 + 16);

    // F0,1 R8
    TT_SFPLOAD(0, 0, ADDR_MOD_3, load_off + 8);
    TT_SFPLOAD(1, 0, ADDR_MOD_3, load_off + 10);
    TT_SFPLOAD(2, 0, ADDR_MOD_3, load_off + 8 + 16);
    TT_SFPLOAD(3, 0, ADDR_MOD_3, load_off + 10 + 16);

    TTI_SFPTRANSP(0, 0, 0, 0);
    lltt::replay(0, 8);
    TTI_SFPTRANSP(0, 0, 0, 0);

    TT_SFPSTORE(0, 0, ADDR_MOD_3, store_off + 8);
    TT_SFPSTORE(1, 0, ADDR_MOD_3, store_off + 10);
    TT_SFPSTORE(2, 0, ADDR_MOD_3, store_off + 8 + 16);
    TT_SFPSTORE(3, 0, ADDR_MOD_3, store_off + 10 + 16);

    // F0,1 R12
    TT_SFPLOAD(4, 0, ADDR_MOD_3, load_off + 12);
    TT_SFPLOAD(5, 0, ADDR_MOD_3, load_off + 14);
    TT_SFPLOAD(6, 0, ADDR_MOD_3, load_off + 12 + 16);
    TT_SFPLOAD(7, 0, ADDR_MOD_3, load_off + 14 + 16);

    TTI_SFPTRANSP(0, 0, 0, 0);
    lltt::replay(8, 8);
    TTI_SFPTRANSP(0, 0, 0, 0);

    TT_SFPSTORE(4, 0, ADDR_MOD_3, store_off + 12);
    TT_SFPSTORE(5, 0, ADDR_MOD_3, store_off + 14);
    TT_SFPSTORE(6, 0, ADDR_MOD_3, store_off + 12 + 16);
    TT_SFPSTORE(7, 0, ADDR_MOD_3, store_off + 14 + 16);

    // F2,3 R0
    TT_SFPLOAD(0, 0, ADDR_MOD_3, load_off + 0 + 32);
    TT_SFPLOAD(1, 0, ADDR_MOD_3, load_off + 2 + 32);
    TT_SFPLOAD(2, 0, ADDR_MOD_3, load_off + 0 + 16 + 32);
    TT_SFPLOAD(3, 0, ADDR_MOD_3, load_off + 2 + 16 + 32);

    TTI_SFPTRANSP(0, 0, 0, 0);
    lltt::replay(0, 8);
    TTI_SFPTRANSP(0, 0, 0, 0);

    TT_SFPSTORE(0, 0, ADDR_MOD_3, store_off + 0 + 32);
    TT_SFPSTORE(1, 0, ADDR_MOD_3, store_off + 2 + 32);
    TT_SFPSTORE(2, 0, ADDR_MOD_3, store_off + 0 + 16 + 32);
    TT_SFPSTORE(3, 0, ADDR_MOD_3, store_off + 2 + 16 + 32);

    // F2,3 R4
    TT_SFPLOAD(4, 0, ADDR_MOD_3, load_off + 4 + 32);
    TT_SFPLOAD(5, 0, ADDR_MOD_3, load_off + 6 + 32);
    TT_SFPLOAD(6, 0, ADDR_MOD_3, load_off + 4 + 16 + 32);
    TT_SFPLOAD(7, 0, ADDR_MOD_3, load_off + 6 + 16 + 32);

    TTI_SFPTRANSP(0, 0, 0, 0);
    lltt::replay(8, 8);
    TTI_SFPTRANSP(0, 0, 0, 0);

    TT_SFPSTORE(4, 0, ADDR_MOD_3, store_off + 4 + 32);
    TT_SFPSTORE(5, 0, ADDR_MOD_3, store_off + 6 + 32);
    TT_SFPSTORE(6, 0, ADDR_MOD_3, store_off + 4 + 16 + 32);
    TT_SFPSTORE(7, 0, ADDR_MOD_3, store_off + 6 + 16 + 32);

    // F2,3 R8
    TT_SFPLOAD(0, 0, ADDR_MOD_3, load_off + 8 + 32);
    TT_SFPLOAD(1, 0, ADDR_MOD_3, load_off + 10 + 32);
    TT_SFPLOAD(2, 0, ADDR_MOD_3, load_off + 8 + 16 + 32);
    TT_SFPLOAD(3, 0, ADDR_MOD_3, load_off + 10 + 16 + 32);

    TTI_SFPTRANSP(0, 0, 0, 0);
    lltt::replay(0, 8);
    TTI_SFPTRANSP(0, 0, 0, 0);

    TT_SFPSTORE(0, 0, ADDR_MOD_3, store_off + 8 + 32);
    TT_SFPSTORE(1, 0, ADDR_MOD_3, store_off + 10 + 32);
    TT_SFPSTORE(2, 0, ADDR_MOD_3, store_off + 8 + 16 + 32);
    TT_SFPSTORE(3, 0, ADDR_MOD_3, store_off + 10 + 16 + 32);

    // F2,3 R12
    TT_SFPLOAD(4, 0, ADDR_MOD_3, load_off + 12 + 32);
    TT_SFPLOAD(5, 0, ADDR_MOD_3, load_off + 14 + 32);
    TT_SFPLOAD(6, 0, ADDR_MOD_3, load_off + 12 + 16 + 32);
    TT_SFPLOAD(7, 0, ADDR_MOD_3, load_off + 14 + 16 + 32);

    TTI_SFPTRANSP(0, 0, 0, 0);
    lltt::replay(8, 8);
    TTI_SFPTRANSP(0, 0, 0, 0);

    TT_SFPSTORE(4, 0, ADDR_MOD_3, store_off + 12 + 32);
    TT_SFPSTORE(5, 0, ADDR_MOD_3, store_off + 14 + 32);
    TT_SFPSTORE(6, 0, ADDR_MOD_3, store_off + 12 + 16 + 32);
    TT_SFPSTORE(7, 0, ADDR_MOD_3, store_off + 14 + 16 + 32);
}

template <bool APPROXIMATION_MODE /*unused*/>
inline void _cumsum_init_()
{
    lltt::record(0, 16);
    // FIXME: These should all be TT_SFP...
    TTI_SFPADD(10, 7, 0, 0, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, 0, 1, 1, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, 1, 2, 2, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, 2, 3, 3, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, 3, 4, 4, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, 4, 5, 5, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, 5, 6, 6, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, 6, 7, 7, 0);
    TTI_SFPNOP;
}

} // namespace sfpu
} // namespace ckernel
