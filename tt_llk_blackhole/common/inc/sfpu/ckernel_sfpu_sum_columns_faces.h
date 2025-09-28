// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_addrmod.h"
#include "ckernel_instr_params.h"
#include "ckernel_sfpu_load_config.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <uint AVERAGE>
inline void _calculate_sum_tile_columns_faces_(const uint tile_idx)
{
    // each face is 16 rows
    constexpr uint face_offset = 16;

    // Suppress unused parameter warning
    (void)tile_idx;
    for (int i = 0; i < 4; i++)
    {
        TT_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::INT32, ADDR_MOD_7, face_offset * i); // even cols face 0 row 0-15
        TT_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::INT32, ADDR_MOD_7, face_offset * i + 4);
        TT_SFPLOAD(p_sfpu::LREG2, InstrModLoadStore::INT32, ADDR_MOD_7, face_offset * i + 8);
        TT_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::INT32, ADDR_MOD_7, face_offset * i + 12);

        TT_SFPLOAD(p_sfpu::LREG4, InstrModLoadStore::INT32, ADDR_MOD_7, face_offset * i + 2); // odd cols face 0 row 0-15
        TT_SFPLOAD(p_sfpu::LREG5, InstrModLoadStore::INT32, ADDR_MOD_7, face_offset * i + 4 + 2);
        TT_SFPLOAD(p_sfpu::LREG6, InstrModLoadStore::INT32, ADDR_MOD_7, face_offset * i + 8 + 2);
        TT_SFPLOAD(p_sfpu::LREG7, InstrModLoadStore::INT32, ADDR_MOD_7, face_offset * i + 12 + 2);

        // sum 4 columns of 4 rows () for faces 0 & 1
        TT_SFPTRANSP(0, 0, 0, 0);
        lltt::replay(0, 6);
        TT_SFPTRANSP(0, 0, 0, 0);
        lltt::replay(0, 6);

        TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::INT32, ADDR_MOD_7, face_offset * i);
        TT_SFPSTORE(p_sfpu::LREG4, InstrModLoadStore::INT32, ADDR_MOD_7, face_offset * i + 2);
    }
    // Load column sums from faces 0 & 2
    TT_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::INT32, ADDR_MOD_7, 0);
    TT_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::INT32, ADDR_MOD_7, 0 + 2);
    TT_SFPLOAD(p_sfpu::LREG2, InstrModLoadStore::INT32, ADDR_MOD_7, 32);
    TT_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::INT32, ADDR_MOD_7, 32 + 2);

    // Load column sums from faces 1 & 3
    TT_SFPLOAD(p_sfpu::LREG4, InstrModLoadStore::INT32, ADDR_MOD_7, 16);
    TT_SFPLOAD(p_sfpu::LREG5, InstrModLoadStore::INT32, ADDR_MOD_7, 16 + 2);
    TT_SFPLOAD(p_sfpu::LREG6, InstrModLoadStore::INT32, ADDR_MOD_7, 48);
    TT_SFPLOAD(p_sfpu::LREG7, InstrModLoadStore::INT32, ADDR_MOD_7, 48 + 2);

    // Sum column sums from faces 0 & 2
    TT_SFPIADD(0, p_sfpu::LREG2, p_sfpu::LREG0, 4);
    TT_SFPIADD(0, p_sfpu::LREG3, p_sfpu::LREG1, 4);
    TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::INT32, ADDR_MOD_7, 0);
    TT_SFPSTORE(p_sfpu::LREG1, InstrModLoadStore::INT32, ADDR_MOD_7, 0 + 2);

    // Sum column sums from faces 1 & 3
    TT_SFPIADD(0, p_sfpu::LREG7, p_sfpu::LREG5, 4);
    TT_SFPIADD(0, p_sfpu::LREG6, p_sfpu::LREG4, 4);

    // Apply averaging if requested
    if constexpr (AVERAGE == 32)
    {
        // For a 32x32 tile, each column sum represents the sum of exactly 32 values (one per row)
        // To compute the average, we divide by 32. Since 32 = 2^5, we can use a right shift by 5
        // This is much more efficient than division and works perfectly for power-of-2 divisors
        TTI_SFPSHFT(-5, p_sfpu::LREG0, p_sfpu::LREG0, 0b11); // Average columns 0-15
        TTI_SFPSHFT(-5, p_sfpu::LREG1, p_sfpu::LREG1, 0b11); // Average columns 1-15
        TTI_SFPSHFT(-5, p_sfpu::LREG4, p_sfpu::LREG4, 0b11); // Average columns 16-31
        TTI_SFPSHFT(-5, p_sfpu::LREG5, p_sfpu::LREG5, 0b11); // Average columns 17-31
    }

    TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::INT32, ADDR_MOD_7, 0);
    TT_SFPSTORE(p_sfpu::LREG1, InstrModLoadStore::INT32, ADDR_MOD_7, 0 + 2);
    TT_SFPSTORE(p_sfpu::LREG4, InstrModLoadStore::INT32, ADDR_MOD_7, 16);
    TT_SFPSTORE(p_sfpu::LREG5, InstrModLoadStore::INT32, ADDR_MOD_7, 16 + 2);
}

inline void _init_sum_tile_columns_faces_()
{
    // Initialize SFPU configuration register
    _init_sfpu_config_reg();

    // Program replay buffer
    load_replay_buf(
        0,
        6,
        []
        {
            // Use TTI_SFPIADD with instruction modifier 4 for integer addition
            // Format: TTI_SFPIADD(imm12_math, lreg_c, lreg_dest, instr_mod1)
            // With imod=4: Does integer addition of lreg_dest + lreg_c → lreg_dest

            TTI_SFPIADD(0, p_sfpu::LREG3, p_sfpu::LREG2, 4); // LREG2 = LREG2 + LREG3
            TTI_SFPIADD(0, p_sfpu::LREG2, p_sfpu::LREG1, 4); // LREG1 = LREG1 + LREG2
            TTI_SFPIADD(0, p_sfpu::LREG1, p_sfpu::LREG0, 4); // LREG0 = LREG0 + LREG1

            TTI_SFPIADD(0, p_sfpu::LREG7, p_sfpu::LREG6, 4); // LREG6 = LREG6 + LREG7
            TTI_SFPIADD(0, p_sfpu::LREG6, p_sfpu::LREG5, 4); // LREG5 = LREG5 + LREG6
            TTI_SFPIADD(0, p_sfpu::LREG5, p_sfpu::LREG4, 4); // LREG4 = LREG4 + LREG5
        });
}

} // namespace sfpu
} // namespace ckernel
