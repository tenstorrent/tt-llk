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

using namespace ckernel;
using namespace ckernel::packer;

template <bool diagonal = false>
inline void _llk_pack_untilize_configure_addrmod_()
{
    static_assert(!diagonal, "Diagonal not supported");

    addr_mod_pack_t {
        .y_src = {.incr = 0, .clr = 0},
    }
        .set(ADDR_MOD_0);
}

template <std::uint32_t block_ct_dim, std::uint32_t full_ct_dim = block_ct_dim, bool diagonal = false>
inline void _llk_pack_untilize_mop_config_(
    const std::uint32_t face_r_dim = FACE_R_DIM, const std::uint32_t num_faces = 4, bool narrow_row = false, std::uint32_t row_num_datums = TILE_C_DIM)
{
    constexpr uint MEGAROW          = 1;
    constexpr uint ZERO_OUTPUT_FLAG = p_pacr::P_ZERO_OUTPUT_DISABLED;
    constexpr uint MOP_INNER_LOOP   = block_ct_dim;
    const uint MOP_OUTER_LOOP       = face_r_dim;

    const uint PACK_INTF_SEL = (narrow_row) ? p_pacr::SINGLE_INTF_ACTIVE : ((num_faces > 1) ? p_pacr::TWO_INTFS_ACTIVE : p_pacr::SINGLE_INTF_ACTIVE);
    /*
    Each pack instruction does 2x16 datums if (num_faces>1)
    Each row of 16 datums, has a stride of 16 from dest read
    Dest row read in inner loop:
    tile 0: row 0, row 16
    tile 1: row 64, row 80
    .
    tile block_ct_dim-1: row 64*(block_ct_dim-1), row 64*(block_ct_dim-1)+16
    */
    ckernel::ckernel_template tmp(
        MOP_OUTER_LOOP,
        MOP_INNER_LOOP,
        TT_OP_INCADCZW(p_setadc::PAC, 0, 0, 1, 0), // w cnt points to the next tile
        TT_OP_PACR(
            p_pacr::CFG_CTXT_0,
            p_pacr::NO_ROW_PAD_ZERO,
            p_pacr::DST_ACCESS_STRIDED_MODE,
            ADDR_MOD_0,
            p_pacr::ADDR_CNT_CTXT_0,
            0,
            PACK_INTF_SEL,
            0,
            0,
            p_pacr::NO_CTXT_CTRL,
            0,
            0));

    /*
    Since there are two inner loop operations, the instruction set by set_last_inner_loop_instr
    will replace the second inner loop operation (in the last iteration, call the PACR instuction
    with the Last bit set to 1 instead of 0 to close the row and allow L1 address to be updated).
    Therefore, by setting the W counter to maxium value (15) as a start operation,
    TT_OP_INCADCZW in the inner loop will reset it to 0 in the first iteration of the inner loop.
    */
    tmp.set_start_op(TT_OP_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, 15));

    const std::uint32_t replay_buf_len = 4;
    load_replay_buf(
        ckernel::packer::replay_buf_offset,
        replay_buf_len,
        false,
        []
        {
            // update l1 address
            TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET);
            TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
            TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
            TTI_NOP;
        });

    tmp.set_end_ops(
        TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0),                             // inc ch0_y counters
        TT_OP_REPLAY(ckernel::packer::replay_buf_offset, replay_buf_len, 0, 0) // update row address
    );

    tmp.set_last_inner_loop_instr(TT_OP_PACR(
        p_pacr::CFG_CTXT_0,
        p_pacr::NO_ROW_PAD_ZERO,
        p_pacr::DST_ACCESS_STRIDED_MODE,
        ADDR_MOD_0,
        p_pacr::ADDR_CNT_CTXT_0,
        0,
        PACK_INTF_SEL,
        0,
        0,
        p_pacr::NO_CTXT_CTRL,
        0,
        1));

    tmp.set_last_outer_loop_instr(TT_OP_PACR(
        p_pacr::CFG_CTXT_0,
        p_pacr::NO_ROW_PAD_ZERO,
        p_pacr::DST_ACCESS_STRIDED_MODE,
        ADDR_MOD_0,
        p_pacr::ADDR_CNT_CTXT_0,
        0,
        PACK_INTF_SEL,
        0,
        0,
        p_pacr::NO_CTXT_CTRL,
        0,
        1));

    tmp.program(instrn_buffer);
}

template <
    std::uint32_t block_ct_dim,
    std::uint32_t full_ct_dim    = block_ct_dim,
    bool diagonal                = false,
    bool narrow_row              = false,
    std::uint32_t row_num_datums = TILE_C_DIM>
inline void _llk_pack_untilize_init_(
    const std::uint32_t pack_src_format, const std::uint32_t pack_dst_format, const std::uint32_t face_r_dim = FACE_R_DIM, const std::uint32_t num_faces = 4)
{
    static_assert(!diagonal, "Diagonal not supported");
    static_assert(block_ct_dim <= 8, "block_ct_dim must be less than or equal to 8");

    _llk_pack_untilize_configure_addrmod_<diagonal>();

    _llk_pack_untilize_mop_config_<block_ct_dim, full_ct_dim, diagonal>(face_r_dim, num_faces, narrow_row, row_num_datums);

    // Set CH0 Zstride = 2x16x16 faces, .z_src = {.incr = 1} jumps 2 faces
    uint x_stride       = (uint)(pack_src_format & 0x3) == (uint)DataFormat::Float32 ? 4 : (uint)(pack_src_format & 0x3) == (uint)DataFormat::Float16 ? 2 : 1;
    uint y_stride       = FACE_C_DIM * x_stride;
    const uint z_stride = 2 * face_r_dim * y_stride;
    cfg_reg_rmw_tensix<PCK0_ADDR_CTRL_ZW_REG_0_Zstride_RMW>(z_stride);
    std::uint32_t output_addr_offset;

    if constexpr (narrow_row)
    {
        static_assert(row_num_datums < 16, "row_num_datums must be set to less than FACE_C_DIM for narrow_row packing");
        output_addr_offset = SCALE_DATUM_SIZE(pack_dst_format, full_ct_dim * row_num_datums);
    }
    else
    {
        output_addr_offset = SCALE_DATUM_SIZE(pack_dst_format, full_ct_dim * ((num_faces > 1) ? (num_faces >> 1) : 1) * FACE_C_DIM);
    }

    TT_SETDMAREG(0, LOWER_HALFWORD(output_addr_offset / 16), 0, LO_16(p_gpr_pack::OUTPUT_ADDR_OFFSET)); // store 16B aligned row offset address
}

template <
    std::uint32_t block_ct_dim,
    std::uint32_t full_ct_dim    = block_ct_dim,
    bool diagonal                = false,
    bool narrow_row              = false,
    std::uint32_t row_num_datums = TILE_C_DIM>
inline void _llk_pack_untilize_(
    const std::uint32_t address,
    const std::uint32_t pack_dst_format,
    const std::uint32_t face_r_dim      = FACE_R_DIM,
    const std::uint32_t num_faces       = 4,
    const std::uint32_t tile_dst_offset = 0)
{
    // program_packer_untilized_destination<block_ct_dim, full_ct_dim, diagonal>(address, pack_dst_format);
    program_packer_destination(address);
    const std::uint32_t num_faces_per_rdim_tile = (num_faces > 2) ? 2 : 1;
    const uint PACK_INTF_SEL = (narrow_row) ? p_pacr::SINGLE_INTF_ACTIVE : ((num_faces > 1) ? p_pacr::TWO_INTFS_ACTIVE : p_pacr::SINGLE_INTF_ACTIVE);

    TT_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0011); // reset ch0 zw counters
    TT_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0011); // reset ch0 xy counters

    for (std::uint32_t face = 0; face < num_faces_per_rdim_tile; face++)
    {
        ckernel::ckernel_template::run(instrn_buffer);

        TTI_INCADCZW(p_setadc::PAC, 0, 0, 0, 1);         // z cnt increments by 2xface_r_dimxFACE_C_DIM
        TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0010); // reset ch0_y counters
    }

    TT_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0101);               // reset z counters
    TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, 0); // reset w counter
}
