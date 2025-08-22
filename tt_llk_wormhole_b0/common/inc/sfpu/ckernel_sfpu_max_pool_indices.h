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

template <bool APPROXIMATION_MODE, bool is_fp32_dest_acc_en, int num_rows, int ITERATIONS>
inline void _calculate_max_pool_with_indices_(const uint idx_addr)
{
    static_assert(num_rows == 9, "num_rows must be one of: {9}");
    const uint idx_tile_offset        = idx_addr * 64;
    constexpr uint8_t instr_mod_index = is_fp32_dest_acc_en ? InstrModLoadStore::INT32 : InstrModLoadStore::LO16;
    constexpr uint face_offset        = 16;

    // F0
    // data
    TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 0); // even cols
    TTI_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 4);
    TTI_SFPLOAD(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 2); // odd cols
    TTI_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 6);
    // index
    TT_SFPLOAD(p_sfpu::LREG4, instr_mod_index, ADDR_MOD_3, idx_tile_offset); // even cols
    TT_SFPLOAD(p_sfpu::LREG5, instr_mod_index, ADDR_MOD_3, idx_tile_offset + 4);
    TT_SFPLOAD(p_sfpu::LREG6, instr_mod_index, ADDR_MOD_3, idx_tile_offset + 2); // odd cols
    TT_SFPLOAD(p_sfpu::LREG7, instr_mod_index, ADDR_MOD_3, idx_tile_offset + 6);

    // sort 4 rows
    lltt::replay(0, 7);

    // data
    TTI_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 8);  // even cols
    TTI_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 10); // odd cols
    // index
    TT_SFPLOAD(p_sfpu::LREG5, instr_mod_index, ADDR_MOD_3, idx_tile_offset + 8);  // even cols
    TT_SFPLOAD(p_sfpu::LREG7, instr_mod_index, ADDR_MOD_3, idx_tile_offset + 10); // odd cols

    TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpswap::ALL_ROWS_MAX);
    TTI_SFPSWAP(0, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpswap::ALL_ROWS_MAX);

    TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 0);
    TTI_SFPSTORE(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 2);
    TT_SFPSTORE(p_sfpu::LREG4, instr_mod_index, ADDR_MOD_3, idx_tile_offset);
    TT_SFPSTORE(p_sfpu::LREG6, instr_mod_index, ADDR_MOD_3, idx_tile_offset + 2);

    // F1
    // data
    TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_3, face_offset); // even cols
    TTI_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_3, face_offset + 4);
    TTI_SFPLOAD(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_3, face_offset + 2); // odd cols
    TTI_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::DEFAULT, ADDR_MOD_3, face_offset + 6);
    // index
    TT_SFPLOAD(p_sfpu::LREG4, instr_mod_index, ADDR_MOD_3, idx_tile_offset + face_offset); // even cols
    TT_SFPLOAD(p_sfpu::LREG5, instr_mod_index, ADDR_MOD_3, idx_tile_offset + face_offset + 4);
    TT_SFPLOAD(p_sfpu::LREG6, instr_mod_index, ADDR_MOD_3, idx_tile_offset + face_offset + 2); // odd cols
    TT_SFPLOAD(p_sfpu::LREG7, instr_mod_index, ADDR_MOD_3, idx_tile_offset + face_offset + 6);

    // sort 4 rows
    lltt::replay(0, 7);

    // data
    TTI_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::DEFAULT, ADDR_MOD_3, face_offset + 8);  // even cols
    TTI_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::DEFAULT, ADDR_MOD_3, face_offset + 10); // odd cols
    // index
    TT_SFPLOAD(p_sfpu::LREG5, instr_mod_index, ADDR_MOD_3, idx_tile_offset + face_offset + 8);  // even cols
    TT_SFPLOAD(p_sfpu::LREG7, instr_mod_index, ADDR_MOD_3, idx_tile_offset + face_offset + 10); // odd cols

    TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpswap::ALL_ROWS_MAX);
    TTI_SFPSWAP(0, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpswap::ALL_ROWS_MAX);

    TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::DEFAULT, ADDR_MOD_3, face_offset);
    TTI_SFPSTORE(p_sfpu::LREG2, InstrModLoadStore::DEFAULT, ADDR_MOD_3, face_offset + 2);
    TT_SFPSTORE(p_sfpu::LREG4, instr_mod_index, ADDR_MOD_3, idx_tile_offset + face_offset);
    TT_SFPSTORE(p_sfpu::LREG6, instr_mod_index, ADDR_MOD_3, idx_tile_offset + face_offset + 2);
}

inline void _init_max_pool_with_indices_()
{
    // Set bit [2] of the SFPU_CONTROL_REG to enable index tracking mode
    _sfpu_load_config32_(0xF, 0x0, 0x4);

    // Program replay buffer
    lltt::record(0, 7);

    // transpose to put Dest rows into separate LREGS
    TTI_SFPTRANSP(0, 0, 0, 0);

    // put max into LREG0 (index into LREG4)
    TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpswap::ALL_ROWS_MAX);
    TTI_SFPSWAP(0, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpswap::ALL_ROWS_MAX);
    TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG2, p_sfpswap::ALL_ROWS_MAX);

    // transpose back
    TTI_SFPTRANSP(0, 0, 0, 0);

    TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfpswap::ALL_ROWS_MAX);
    TTI_SFPSWAP(0, p_sfpu::LREG2, p_sfpu::LREG3, p_sfpswap::ALL_ROWS_MAX);
}

} // namespace sfpu
} // namespace ckernel
