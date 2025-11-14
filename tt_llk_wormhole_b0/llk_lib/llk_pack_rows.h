// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel.h"
#include "ckernel_globals.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "llk_defs.h"
#include "llk_pack_common.h"

inline void _llk_pack_rows_configure_addrmod_()
{
    ckernel::addr_mod_pack_t {
        .y_src = {.incr = 1},
    }
        .set(ADDR_MOD_0);

    ckernel::addr_mod_pack_t {
        .y_src = {.incr = 0, .clr = 1, .cr = 0},
    }
        .set(ADDR_MOD_1);

    ckernel::addr_mod_pack_t {
        .y_src = {.incr = 0, .clr = 1, .cr = 0},
    }
        .set(ADDR_MOD_2);
}

inline void _llk_pack_rows_mop_config_(const std::uint32_t num_rows)
{
    constexpr uint PACKCNT          = 1; // Use only packer 0
    constexpr uint MEGAROW          = 0; // Not using megarow mode
    constexpr uint ZERO_OUTPUT_FLAG = p_pacr::P_ZERO_OUTPUT_DISABLED;
    constexpr uint MOP_OUTER_LOOP   = 1;
    const uint MOP_INNER_LOOP       = num_rows;

    ckernel::ckernel_template tmp(MOP_OUTER_LOOP, MOP_INNER_LOOP, TT_OP_PACR(ADDR_MOD_0, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 0));

    tmp.set_last_inner_loop_instr(TT_OP_PACR(ADDR_MOD_1, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 0));

    tmp.program();
}

inline void _llk_pack_rows_configure_dest_offset_()
{
    TTI_STALLWAIT(p_stall::STALL_TDMA | p_stall::STALL_THCON, p_stall::PACK);

    TTI_SETDMAREG(0, 0, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 0));
    TTI_SETDMAREG(0, 0, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 0));
}

inline void _llk_pack_rows_init_(
    const std::uint32_t num_rows, // Total number of rows to pack
    const bool include_setup_calls = false)
{
    constexpr std::uint32_t row_num_datums = 16;

    _llk_pack_rows_configure_addrmod_();
    _llk_pack_rows_mop_config_(num_rows);

    _llk_pack_rows_configure_dest_offset_();

    if (include_setup_calls)
    {
        // Set the packer X counter to pack the specified number of datums per row
        TT_SETADCXX(p_setadc::PAC, row_num_datums - 1, 0x0);

        // Reset Z/W counters
        TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b1111);
    }
}

inline void _llk_pack_rows_(const std::uint32_t tile_index, const std::uint32_t address)
{
    // Set the tile index in dest to read from
    TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, tile_index);

    ckernel::packer::program_packer_destination(address);

    ckernel::ckernel_template::run();

    // Close the pack operation
    TTI_PACR(ADDR_MOD_2, 0, 0xf, 0, 0, 1, 1);
}
