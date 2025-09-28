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

inline void _calculate_sum_tile_columns_(const uint tile_idx)
{
    // each face is 16 rows
    constexpr uint face_offset = 16;
    // constexpr uint face_1_offset = 16;
    // constexpr uint face_2_offset = 32;
    // constexpr uint face_3_offset = 48;

    // Suppress unused parameter warning
    (void)tile_idx;
    for (int i = 0; i < 4; i++)
    {
        // ===== FACES 0 =====
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
        lltt::replay(0, 11);
        TT_SFPTRANSP(0, 0, 0, 0);
        lltt::replay(0, 11);

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
    TT_SFPSTORE(p_sfpu::LREG4, InstrModLoadStore::INT32, ADDR_MOD_7, 16);
    TT_SFPSTORE(p_sfpu::LREG5, InstrModLoadStore::INT32, ADDR_MOD_7, 16 + 2);
}

inline void _init_sum_tile_columns_()
{
    // Initialize SFPU configuration register
    _init_sfpu_config_reg();

    // Program replay buffer
    load_replay_buf(
        0,
        11,
        []
        {
            // Use TTI_SFPIADD with instruction modifier 4 for integer addition
            // Format: TTI_SFPIADD(imm12_math, lreg_c, lreg_dest, instr_mod1)
            // With imod=4: Does integer addition of lreg_dest + lreg_c → lreg_dest

            TTI_SFPIADD(0, p_sfpu::LREG3, p_sfpu::LREG2, 4); // LREG2 = LREG2 + LREG3 (integer add)
            TTI_SFPNOP;
            TTI_SFPIADD(0, p_sfpu::LREG2, p_sfpu::LREG1, 4); // LREG1 = LREG1 + LREG2 (integer add)
            TTI_SFPNOP;
            TTI_SFPIADD(0, p_sfpu::LREG1, p_sfpu::LREG0, 4); // LREG0 = LREG0 + LREG1 (integer add)
            TTI_SFPNOP;

            TTI_SFPIADD(0, p_sfpu::LREG7, p_sfpu::LREG6, 4); // LREG6 = LREG6 + LREG7 (integer add)
            TTI_SFPNOP;
            TTI_SFPIADD(0, p_sfpu::LREG6, p_sfpu::LREG5, 4); // LREG5 = LREG5 + LREG6 (integer add)
            TTI_SFPNOP;
            TTI_SFPIADD(0, p_sfpu::LREG5, p_sfpu::LREG4, 4); // LREG4 = LREG4 + LREG5 (integer add)
            TTI_SFPNOP;
        });
}

} // namespace sfpu
} // namespace ckernel
