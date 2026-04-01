// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel.h"
#include "ckernel_globals.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "llk_assert.h"
#include "llk_defs.h"
#include "llk_memory_checks.h"
#include "llk_pack_common.h"

using namespace ckernel;
using namespace ckernel::packer;

inline void _llk_pack_untilize_configure_addrmod_()
{
    addr_mod_pack_t {
        .y_src = {.incr = 0, .clr = 0},
    }
        .set(ADDR_MOD_0);
}

/*
block_ct_dim represents the number of input tiles in a block.
dense is used with num_faces == 2 and even block_ct_dim, where two 16x32 (or smaller) tiles are packed in a single 32x32 tile region in dest.
*/
template <std::uint32_t block_ct_dim, bool narrow_row = false, bool dense = false>
inline void _llk_pack_untilize_mop_config_(const std::uint32_t face_r_dim = FACE_R_DIM, const std::uint32_t num_faces = 4)
{
    static_assert(!dense || (block_ct_dim % 2 == 0), "block_ct_dim must be even when dense");
    static_assert(!dense || (!narrow_row), "narrow_row must be false when dense");
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    LLK_ASSERT(!dense || (num_faces == 2), "num_faces must be 2 when dense");

    constexpr bool use_2_contexts = !dense && !narrow_row;

    if constexpr (use_2_contexts)
    {
        if (num_faces == 4)
        {
            /*
            Path 1: 2-context PACR for 4-face tiles (non-dense, non-narrow).
            MOP outer loop = block_ct_dim (tiles), inner loop = 1.
            LOOP_OP0: PACR with CNTX0, interfaces 0+1 (TWO_INTFS_ACTIVE) for top faces (F0/F1).
            LOOP_OP1: PACR with CNTX1, interfaces 2+3 (_2nd_AND_3rd_INTF_ACTIVE) for bottom faces (F2/F3).
            END_OP:   INCADCZW to advance W counter to the next tile.
            The software loop in _llk_pack_untilize_ iterates over rows, calling MOP run per row.
            */
            ckernel::ckernel_template tmp(
                block_ct_dim,
                1,
                TT_OP_PACR(
                    p_pacr::CFG_CTXT_0,
                    p_pacr::NO_ROW_PAD_ZERO,
                    p_pacr::DST_ACCESS_STRIDED_MODE,
                    ADDR_MOD_0,
                    p_pacr::ADDR_CNT_CTXT_0,
                    0,
                    p_pacr::TWO_INTFS_ACTIVE,
                    0,
                    0,
                    p_pacr::NO_CTXT_CTRL,
                    0,
                    0),
                TT_OP_PACR(
                    p_pacr::CFG_CTXT_1,
                    p_pacr::NO_ROW_PAD_ZERO,
                    p_pacr::DST_ACCESS_STRIDED_MODE,
                    ADDR_MOD_0,
                    p_pacr::ADDR_CNT_CTXT_0,
                    0,
                    p_pacr::_2nd_AND_3rd_INTF_ACTIVE,
                    0,
                    0,
                    p_pacr::NO_CTXT_CTRL,
                    0,
                    0));

            tmp.set_end_op(TT_OP_INCADCZW(p_setadc::PAC, 0, 0, 1, 0));

            // In the last outer loop iteration, replace LOOP_OP1 (CNTX1 PACR) with Last=1
            // to close the row for context 1.
            tmp.set_last_outer_loop_instr(TT_OP_PACR(
                p_pacr::CFG_CTXT_1,
                p_pacr::NO_ROW_PAD_ZERO,
                p_pacr::DST_ACCESS_STRIDED_MODE,
                ADDR_MOD_0,
                p_pacr::ADDR_CNT_CTXT_0,
                0,
                p_pacr::_2nd_AND_3rd_INTF_ACTIVE,
                0,
                0,
                p_pacr::NO_CTXT_CTRL,
                0,
                1));

            // Replay buffer: update both L1 addresses (context 0 and context 1)
            constexpr std::uint32_t replay_buf_len = 7;
            load_replay_buf(
                ckernel::packer::replay_buf_offset,
                replay_buf_len,
                []
                {
                    // Update context 0 L1 address (top faces)
                    TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET);
                    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
                    TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
                    // Update context 1 L1 address (bottom faces)
                    TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR + 1, p_gpr_pack::OUTPUT_ADDR + 1, p_gpr_pack::OUTPUT_ADDR_OFFSET);
                    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
                    TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR + 1, 0, THCON_SEC0_REG8_L1_Dest_addr_ADDR32);
                    TTI_NOP;
                });

            tmp.program();
        }
        else
        {
            /*
            Path 2: Single-context PACR for num_faces <= 2 (non-dense, non-narrow).
            Preserved from existing code: outer loop = face_r_dim (rows), inner loop = block_ct_dim (tiles).
            */
            constexpr std::uint32_t MOP_INNER_LOOP = block_ct_dim;
            const std::uint32_t MOP_OUTER_LOOP     = face_r_dim;

            const std::uint32_t PACK_INTF_SEL = (num_faces == 1) ? p_pacr::SINGLE_INTF_ACTIVE : p_pacr::TWO_INTFS_ACTIVE;

            ckernel::ckernel_template tmp(
                MOP_OUTER_LOOP,
                MOP_INNER_LOOP,
                TT_OP_INCADCZW(p_setadc::PAC, 0, 0, 1, 0),
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

            tmp.set_start_op(TT_OP_ADDRCRZW(p_setadc::PAC, 0, 0, 0, 0, 0b0010 /*CH0_W*/));

            const std::uint32_t replay_buf_len = 4;
            load_replay_buf(
                ckernel::packer::replay_buf_offset,
                replay_buf_len,
                []
                {
                    TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET);
                    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
                    TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
                    TTI_NOP;
                });

            tmp.set_end_ops(TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0), lltt::replay_insn(ckernel::packer::replay_buf_offset, replay_buf_len));

            std::uint32_t last_loop_op = TT_OP_PACR(
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
                1);

            tmp.set_last_inner_loop_instr(last_loop_op);
            tmp.set_last_outer_loop_instr(last_loop_op);

            tmp.program();
        }
    }
    else
    {
        /*
        Path 3: Dense or narrow_row (existing behavior preserved).
        Dense uses ALL_INTF_ACTIVE, narrow uses SINGLE_INTF.
        */
        constexpr std::uint32_t MOP_INNER_LOOP = dense ? block_ct_dim / 2 : block_ct_dim;
        const std::uint32_t MOP_OUTER_LOOP     = face_r_dim;

        const std::uint32_t PACK_INTF_SEL = (dense)                          ? p_pacr::ALL_INTF_ACTIVE
                                            : (narrow_row || num_faces == 1) ? p_pacr::SINGLE_INTF_ACTIVE
                                                                             : p_pacr::TWO_INTFS_ACTIVE;

        ckernel::ckernel_template tmp(
            MOP_OUTER_LOOP,
            MOP_INNER_LOOP,
            TT_OP_INCADCZW(p_setadc::PAC, 0, 0, 1, 0),
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

        tmp.set_start_op(TT_OP_ADDRCRZW(p_setadc::PAC, 0, 0, 0, 0, 0b0010 /*CH0_W*/));

        const std::uint32_t replay_buf_len = 4;
        load_replay_buf(
            ckernel::packer::replay_buf_offset,
            replay_buf_len,
            []
            {
                TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET);
                TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
                TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
                TTI_NOP;
            });

        tmp.set_end_ops(TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0), lltt::replay_insn(ckernel::packer::replay_buf_offset, replay_buf_len));

        std::uint32_t last_loop_op = TT_OP_PACR(
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
            1);

        tmp.set_last_inner_loop_instr(last_loop_op);
        tmp.set_last_outer_loop_instr(last_loop_op);

        tmp.program();
    }
}

template <
    std::uint32_t block_ct_dim,
    std::uint32_t full_ct_dim    = block_ct_dim,
    bool narrow_row              = false,
    std::uint32_t row_num_datums = TILE_C_DIM,
    bool dense                   = false>
inline void _llk_pack_untilize_init_(
    const std::uint32_t pack_src_format, const std::uint32_t pack_dst_format, const std::uint32_t face_r_dim = FACE_R_DIM, const std::uint32_t num_faces = 4)
{
    static_assert(block_ct_dim <= (dense ? 16 : 8), "block_ct_dim must be <= 8 when not dense, <= 16 when dense");
    static_assert(!dense || (block_ct_dim % 2 == 0), "block_ct_dim must be even when dense");
    static_assert(!dense || (!narrow_row), "narrow_row must be false when dense");
    static_assert(full_ct_dim % block_ct_dim == 0, "full_ct_dim must be divisible by block_ct_dim");
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    LLK_ASSERT(!dense || (num_faces == 2), "num_faces must be 2 when dense");

    if constexpr (narrow_row)
    {
        // Changed to check against TILE_C_DIM instead of FACE_C_DIM until tt-metal#24095 is investigated.
        static_assert(row_num_datums < TILE_C_DIM, "row_num_datums must be set to less than TILE_C_DIM for narrow_row packing");
    }

    _llk_pack_untilize_configure_addrmod_();

    _llk_pack_untilize_mop_config_<block_ct_dim, narrow_row, dense>(face_r_dim, num_faces);

    // Set CH0 Zstride = 2x16x16 faces, .z_src = {.incr = 1} jumps 2 faces
    std::uint32_t x_stride       = (pack_src_format & 0x3) == to_underlying(DataFormat::Float32)   ? 4
                                   : (pack_src_format & 0x3) == to_underlying(DataFormat::Float16) ? 2
                                                                                                   : 1;
    std::uint32_t y_stride       = FACE_C_DIM * x_stride;
    const std::uint32_t z_stride = 2 * face_r_dim * y_stride;
    cfg_reg_rmw_tensix<PCK0_ADDR_CTRL_ZW_REG_0_Zstride_RMW>(z_stride);

    std::uint32_t output_addr_offset;
    if constexpr (narrow_row)
    {
        output_addr_offset = SCALE_DATUM_SIZE(pack_dst_format, full_ct_dim * row_num_datums);
    }
    else
    {
        output_addr_offset = SCALE_DATUM_SIZE(pack_dst_format, full_ct_dim * ((num_faces == 1) ? 1 : 2) * FACE_C_DIM);
    }

    // Store 16B aligned row offset address
    TT_SETDMAREG(0, LOWER_HALFWORD(output_addr_offset / 16), 0, LO_16(p_gpr_pack::OUTPUT_ADDR_OFFSET));

    // Always include setup calls for safety (as recommended by maintainer)
    // Program packer to pack out the correct number of datums per row
    if constexpr (narrow_row)
    {
        TTI_SETADCXX(p_setadc::PAC, row_num_datums - 1, 0x0);
    }
    else
    {
        TTI_SETADCXX(p_setadc::PAC, FACE_C_DIM - 1, 0x0);
    }
}

template <
    std::uint32_t block_ct_dim,
    std::uint32_t full_ct_dim        = block_ct_dim,
    bool narrow_row                  = false,
    std::uint32_t tile_dst_ct_offset = 0,
    bool dense                       = false>
inline void _llk_pack_untilize_(
    const std::uint32_t address,
    const std::uint32_t pack_dst_format,
    const std::uint32_t face_r_dim         = FACE_R_DIM,
    const std::uint32_t num_faces          = 4,
    const std::uint32_t tile_dst_rt_offset = 0)
{
    static_assert(block_ct_dim <= (dense ? 16 : 8), "block_ct_dim must be <= 8 when not dense, <= 16 when dense");
    static_assert(!dense || (block_ct_dim % 2 == 0), "block_ct_dim must be even when dense");
    static_assert(!dense || (!narrow_row), "narrow_row must be false when dense");
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    LLK_ASSERT(!dense || (num_faces == 2), "num_faces must be 2 when dense");

    constexpr bool use_2_contexts       = !dense && !narrow_row;
    const std::uint32_t tile_dst_offset = tile_dst_ct_offset + tile_dst_rt_offset;

    if constexpr (use_2_contexts)
    {
        if (num_faces == 4)
        {
            /*
            Path 1: 2-context PACR for 4-face tiles (non-dense, non-narrow).
            Program both L1 addresses: context 0 for top faces, context 1 for bottom faces.
            Software loop over rows, with per-row MOP run and dual L1 address updates via replay buffer.
            */
            const std::uint32_t bottom_face_offset = SCALE_DATUM_SIZE(pack_dst_format, full_ct_dim * TILE_C_DIM * face_r_dim);
            program_packer_untilized_destination(address, bottom_face_offset);

            constexpr std::uint32_t replay_buf_len = 7;
            for (std::uint32_t row = 0; row < face_r_dim; row++)
            {
                TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, tile_dst_offset);
                ckernel::ckernel_template::run();
                TTI_INCADCXY(p_setadc::PAC, 0, 0, 1, 0);
                lltt::replay(ckernel::packer::replay_buf_offset, replay_buf_len);
            }
        }
        else
        {
            /*
            Path 2: Single-context PACR for num_faces <= 2 (non-dense, non-narrow).
            Existing behavior preserved: face loop for top/bottom faces.
            */
            program_packer_destination(address);
            const std::uint32_t num_faces_per_rdim_tile = (num_faces > 2) ? 2 : 1;

            TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0001);
            TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, (15 + tile_dst_offset) & 0xF);
            TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0011);

            for (std::uint32_t face = 0; face < num_faces_per_rdim_tile; face++)
            {
                ckernel::ckernel_template::run();

                TTI_INCADCZW(p_setadc::PAC, 0, 0, 0, 1);
                TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0010);
            }

            TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0101);
            set_dst_write_addr(tile_dst_offset);
        }
    }
    else
    {
        /*
        Path 3: Dense or narrow_row (existing behavior preserved).
        */
        program_packer_destination(address);
        const std::uint32_t num_faces_per_rdim_tile = (num_faces > 2) ? 2 : 1;

        TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0001);
        TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, (15 + tile_dst_offset) & 0xF);
        TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0011);

        for (std::uint32_t face = 0; face < num_faces_per_rdim_tile; face++)
        {
            ckernel::ckernel_template::run();

            TTI_INCADCZW(p_setadc::PAC, 0, 0, 0, 1);
            TTI_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b0010);
        }

        TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0101);
        set_dst_write_addr(tile_dst_offset);
    }
}

inline void _llk_pack_untilize_uninit_(const std::uint32_t pack_src_format)
{
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::PACK);
    const std::uint32_t z_stride = SCALE_DATUM_SIZE(pack_src_format, FACE_R_DIM * FACE_C_DIM);
    cfg_reg_rmw_tensix<PCK0_ADDR_CTRL_ZW_REG_0_Zstride_RMW>(z_stride);
}
