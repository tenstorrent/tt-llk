// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_addrmod.h"
#include "ckernel_instr_params.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

inline void _calculate_sum_tile_columns_(const uint tile_idx)
{
    // each face is 16 rows
    constexpr uint face_0_offset = 0;
    constexpr uint face_1_offset = 16;
    constexpr uint face_2_offset = 32;
    constexpr uint face_3_offset = 48;

    // Suppress unused parameter warning
    (void)tile_idx;

    // ===== FACES 0 & 1 =====
    TT_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset); // even cols face 0 row 0-7
    TT_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset + 4);
    TT_SFPLOAD(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset + 2); // odd cols face 0 row 0-7
    TT_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset + 4 + 2);

    TT_SFPLOAD(p_sfpu::LREG4, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset); // even cols f1 row 0-3
    TT_SFPLOAD(p_sfpu::LREG5, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset + 4);
    TT_SFPLOAD(p_sfpu::LREG6, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset + 2); // odd cols f1 row 0-7
    TT_SFPLOAD(p_sfpu::LREG7, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset + 4 + 2);

    // sum 4 columns of 4 rows () for faces 0 & 1
    lltt::replay(0, 21);

    // LREGS 0, 2, 4, 6 hold sum for rows 0-7 of face 0 & face 1
    TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset);
    TT_SFPSTORE(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset + 2);
    TT_SFPSTORE(p_sfpu::LREG4, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset);
    TT_SFPSTORE(p_sfpu::LREG6, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset + 2);

    TT_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset + 8); // even cols face 0 row 8-15
    TT_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset + 12);
    TT_SFPLOAD(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset + 8 + 2); // odd cols face 0 row 8-15
    TT_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset + 12 + 2);

    TT_SFPLOAD(p_sfpu::LREG4, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset + 8); // even cols face 1 row 8-15
    TT_SFPLOAD(p_sfpu::LREG5, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset + 12);
    TT_SFPLOAD(p_sfpu::LREG6, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset + 8 + 2); // odd cols face 1 row 8-15
    TT_SFPLOAD(p_sfpu::LREG7, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset + 12 + 2);

    // sum 4 columns of 4 rows (8-15) for faces 0 & 1
    lltt::replay(0, 21);

    // lregs 0, 2, 4, 6 hold sum for rows 8-15 of face 0 & face 1
    // we want to add them with sums for columns 0-7 of face 0 & face 1
    TT_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset); // cols face 0 row 0-3
    TT_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset + 2);
    TT_SFPLOAD(p_sfpu::LREG5, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset); // cols face 1 row 0-3
    TT_SFPLOAD(p_sfpu::LREG7, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset + 2);

    TTI_SFPADD(10, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LREG0, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpu::LREG2, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG4, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, p_sfpu::LREG6, p_sfpu::LREG7, p_sfpu::LREG6, 0);
    TTI_SFPNOP;

    TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset);
    TT_SFPSTORE(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset + 2);
    TT_SFPSTORE(p_sfpu::LREG4, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset);
    TT_SFPSTORE(p_sfpu::LREG6, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset + 2);
    // now all columns of face 0 & face 1 are summed

    // ===== FACES 2 & 3 =====
    TT_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_2_offset); // even cols face 2 row 0-7
    TT_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_2_offset + 4);
    TT_SFPLOAD(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_2_offset + 2); // odd cols face 2 row 0-7
    TT_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_2_offset + 4 + 2);

    TT_SFPLOAD(p_sfpu::LREG4, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_3_offset); // even cols f1 row 0-3
    TT_SFPLOAD(p_sfpu::LREG5, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_3_offset + 4);
    TT_SFPLOAD(p_sfpu::LREG6, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_3_offset + 2); // odd cols f1 row 0-7
    TT_SFPLOAD(p_sfpu::LREG7, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_3_offset + 4 + 2);

    // sum 4 columns of 4 rows () for faces 2 & 3
    lltt::replay(0, 21);

    // LREGS 0, 2, 4, 6 hold sum for rows 0-7 of face 2 & face 3
    TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_2_offset);
    TT_SFPSTORE(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_2_offset + 2);
    TT_SFPSTORE(p_sfpu::LREG4, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_3_offset);
    TT_SFPSTORE(p_sfpu::LREG6, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_3_offset + 2);

    TT_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_2_offset + 8); // even cols face 2 row 8-15
    TT_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_2_offset + 12);
    TT_SFPLOAD(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_2_offset + 8 + 2); // odd cols face 2 row 8-15
    TT_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_2_offset + 12 + 2);

    TT_SFPLOAD(p_sfpu::LREG4, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_3_offset + 8); // even cols face 3 row 8-15
    TT_SFPLOAD(p_sfpu::LREG5, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_3_offset + 12);
    TT_SFPLOAD(p_sfpu::LREG6, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_3_offset + 8 + 2); // odd cols face 3 row 8-15
    TT_SFPLOAD(p_sfpu::LREG7, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_3_offset + 12 + 2);

    // sum 4 columns of 4 rows (8-15) for faces 0 & 1
    lltt::replay(0, 21);

    // lregs 0, 2, 4, 6 hold sum for rows 8-15 of face 0 & face 1
    // we want to add them with sums for columns 0-7 of face 0 & face 1
    TT_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_2_offset); // cols face 2 row 0-3
    TT_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_2_offset + 2);
    TT_SFPLOAD(p_sfpu::LREG5, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_3_offset); // cols face 3 row 0-3
    TT_SFPLOAD(p_sfpu::LREG7, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_3_offset + 2);

    TTI_SFPADD(10, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LREG0, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpu::LREG2, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG4, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, p_sfpu::LREG6, p_sfpu::LREG7, p_sfpu::LREG6, 0);
    TTI_SFPNOP;

    // Now all columns of face 2 & face 3 are summed in lregs 0, 2, 4, 6
    // extract sum from faces 0 & 1
    TT_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset);
    TT_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset + 2);
    TT_SFPLOAD(p_sfpu::LREG5, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset);
    TT_SFPLOAD(p_sfpu::LREG7, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset + 2);

    TTI_SFPADD(10, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LREG0, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpu::LREG2, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG4, 0);
    TTI_SFPNOP;
    TTI_SFPADD(10, p_sfpu::LREG6, p_sfpu::LREG7, p_sfpu::LREG6, 0);
    TTI_SFPNOP;

    // Now all columns of faces are summed in lregs 0, 2, 4, 6
    TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset);
    TT_SFPSTORE(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_0_offset + 2);
    TT_SFPSTORE(p_sfpu::LREG4, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset);
    TT_SFPSTORE(p_sfpu::LREG6, InstrModLoadStore::DEFAULT, ADDR_MOD_7, face_1_offset + 2);
}

inline void _init_sum_tile_columns_()
{
    // Program replay buffer
    load_replay_buf(
        0,
        21,
        []
        {
            // Values have been loaded such that 4 rows of Dest occupy the 4 lanes of each LREG
            // To sum those 4 rows, we transpose the SFPU LREGs  to put elements of 4 rows of each column into separate LREGs of each unit of SFPU
            TTI_SFPTRANSP(0, 0, 0, 0);

            // sum rows
            TTI_SFPADD(10, p_sfpu::LREG3, p_sfpu::LREG2, p_sfpu::LREG2, 0);
            TTI_SFPNOP;
            TTI_SFPADD(10, p_sfpu::LREG2, p_sfpu::LREG1, p_sfpu::LREG1, 0);
            TTI_SFPNOP;
            TTI_SFPADD(10, p_sfpu::LREG1, p_sfpu::LREG0, p_sfpu::LREG0, 0);
            TTI_SFPNOP;

            TTI_SFPADD(10, p_sfpu::LREG7, p_sfpu::LREG6, p_sfpu::LREG6, 0);
            TTI_SFPNOP;
            TTI_SFPADD(10, p_sfpu::LREG6, p_sfpu::LREG5, p_sfpu::LREG5, 0);
            TTI_SFPNOP;
            TTI_SFPADD(10, p_sfpu::LREG5, p_sfpu::LREG4, p_sfpu::LREG4, 0);
            TTI_SFPNOP;

            TTI_SFPTRANSP(0, 0, 0, 0);

            //
            TTI_SFPADD(10, p_sfpu::LREG1, p_sfpu::LREG0, p_sfpu::LREG0, 0);
            TTI_SFPNOP;
            TTI_SFPADD(10, p_sfpu::LREG5, p_sfpu::LREG4, p_sfpu::LREG4, 0);
            TTI_SFPNOP;
            TTI_SFPADD(10, p_sfpu::LREG3, p_sfpu::LREG2, p_sfpu::LREG2, 0);
            TTI_SFPNOP;
            TTI_SFPADD(10, p_sfpu::LREG7, p_sfpu::LREG6, p_sfpu::LREG6, 0);
            TTI_SFPNOP;
        });
}

} // namespace sfpu
} // namespace ckernel
