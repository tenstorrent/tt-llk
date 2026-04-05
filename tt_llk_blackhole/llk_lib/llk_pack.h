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
#include "llk_pack_common.h"

using namespace ckernel;
using namespace ckernel::packer;

template <bool untilize = false, bool tilize = false>
inline void _llk_pack_configure_addrmod_()
{
    if constexpr (untilize && !tilize)
    {
        /*  Y src & Y dest inc by 1 to give strided increments:
            Rows: 0, 16, 1, 17, 2, 18, ........ 15, 31
        */
        addr_mod_pack_t {.y_src = {.incr = 1}, .y_dst = {.incr = 1}, .z_src = {.incr = 0}, .z_dst = {.incr = 0}}.set(ADDR_MOD_0);

        /* Increment Faces by 2 to give next 2 faces:
            Rows: 32, 48, 33, 49, 34, 50........47, 63
        */
        addr_mod_pack_t {.y_src = {.incr = 0, .clr = 1}, .y_dst = {.incr = 0, .clr = 1}, .z_src = {.incr = 1}, .z_dst = {.incr = 0}}.set(ADDR_MOD_1);

        addr_mod_pack_t {.y_src = {.incr = 0, .clr = 1}, .y_dst = {.incr = 0, .clr = 1}, .z_src = {.incr = 0, .clr = 1}, .z_dst = {.incr = 0, .clr = 1}}.set(
            ADDR_MOD_2);
    }
    else if constexpr (tilize && !untilize)
    {
        addr_mod_pack_t {.y_src = {.incr = 4}, .y_dst = {.incr = 2}, .z_src = {.incr = 0}, .z_dst = {.incr = 0}}.set(ADDR_MOD_0);

        addr_mod_pack_t {.y_src = {.incr = 0, .clr = 1}, .y_dst = {.incr = 0, .clr = 1}, .z_src = {.incr = 0}, .z_dst = {.incr = 0}}.set(ADDR_MOD_1);

        // Increment faces by 2 (jump 2 dest address 32)
        addr_mod_pack_t {.y_src = {.incr = 0, .clr = 1}, .y_dst = {.incr = 0, .clr = 1}, .z_src = {.incr = 1}, .z_dst = {.incr = 0}}.set(ADDR_MOD_2);
    }
    else
    {
        addr_mod_pack_t {
            .y_src = {.incr = 4},
            .y_dst = {.incr = 4},
        }
            .set(ADDR_MOD_0);

        addr_mod_pack_t {
            .y_src = {.incr = 0, .clr = 1, .cr = 0},
            .y_dst = {.incr = 0, .clr = 1, .cr = 0},
            .z_src = {.incr = 0, .clr = 1},
            .z_dst = {.incr = 0, .clr = 0},
        }
            .set(ADDR_MOD_1);

        addr_mod_pack_t {
            .y_src = {.incr = 0, .clr = 1, .cr = 0},
            .y_dst = {.incr = 4, .clr = 0, .cr = 0},
            .z_src = {.incr = 1, .clr = 0},
        }
            .set(ADDR_MOD_2);
    }
}

template <bool untilize = false, bool zero_output = false, bool tilize = false>
inline void _llk_pack_mop_config_(
    [[maybe_unused]] const std::uint32_t pack_dst_format,
    const std::uint32_t face_r_dim           = FACE_R_DIM,
    const std::uint32_t tile_c_dim           = TILE_C_DIM,
    const std::uint32_t num_faces            = 4,
    [[maybe_unused]] const bool partial_face = false,
    [[maybe_unused]] const bool narrow_tile  = false,
    const std::uint32_t num_tiles            = 1)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    LLK_ASSERT(!partial_face, "partial_face: this parameter is unused");
    LLK_ASSERT(!narrow_tile, "narrow_tile: this parameter is unused");

    constexpr std::uint32_t MEGAROW          = 1;
    constexpr std::uint32_t ZERO_OUTPUT_FLAG = zero_output ? p_pacr::P_ZERO_OUTPUT_ENABLED : p_pacr::P_ZERO_OUTPUT_DISABLED;

    if constexpr (untilize && !tilize)
    {
        const std::uint32_t PACK_INTF_SEL  = (tile_c_dim < TILE_C_DIM) ? p_pacr::SINGLE_INTF_ACTIVE : p_pacr::TWO_INTFS_ACTIVE;
        const std::uint32_t MOP_INNER_LOOP = face_r_dim;
        const std::uint32_t MOP_OUTER_LOOP = (tile_c_dim < TILE_C_DIM) ? num_faces : (num_faces >> 1);

        ckernel::ckernel_template tmp(
            MOP_OUTER_LOOP,
            MOP_INNER_LOOP,
            TT_OP_PACR(
                p_pacr::CFG_CTXT_0,
                p_pacr::NO_ROW_PAD_ZERO,
                p_pacr::DST_ACCESS_STRIDED_MODE,
                ADDR_MOD_0,
                p_pacr::ADDR_CNT_CTXT_0,
                ZERO_OUTPUT_FLAG,
                PACK_INTF_SEL,
                0,
                MEGAROW,
                p_pacr::NO_CTXT_CTRL,
                0,
                0));

        tmp.set_last_inner_loop_instr(TT_OP_PACR(
            p_pacr::CFG_CTXT_0,
            p_pacr::NO_ROW_PAD_ZERO,
            p_pacr::DST_ACCESS_STRIDED_MODE,
            ADDR_MOD_1,
            p_pacr::ADDR_CNT_CTXT_0,
            ZERO_OUTPUT_FLAG,
            PACK_INTF_SEL,
            0,
            MEGAROW,
            p_pacr::NO_CTXT_CTRL,
            0,
            0));
        tmp.set_last_outer_loop_instr(TT_OP_PACR(
            p_pacr::CFG_CTXT_0,
            p_pacr::NO_ROW_PAD_ZERO,
            p_pacr::DST_ACCESS_STRIDED_MODE,
            ADDR_MOD_2,
            p_pacr::ADDR_CNT_CTXT_0,
            ZERO_OUTPUT_FLAG,
            PACK_INTF_SEL,
            0,
            0,
            p_pacr::NO_CTXT_CTRL,
            0,
            1));
        tmp.program();
    }
    else if constexpr (tilize && !untilize)
    {
        const std::uint32_t PACK_INTF_SEL_0 = 0b0101;
        const std::uint32_t PACK_INTF_SEL_1 = 0b1010;
        const std::uint32_t MOP_INNER_LOOP  = 1;
        const std::uint32_t MOP_OUTER_LOOP  = (num_faces > 1) ? (num_faces >> 1) : 1;

        // Last row of half-tile (face_r_dim rows) is different between halves, so can't be replayed.
        LLK_ASSERT(face_r_dim == 2 || face_r_dim == 4 || face_r_dim == 8 || face_r_dim == 16, "face_r_dim must be 2, 4, 8, or 16 for tilize");
        const std::uint32_t replay_buf_len = face_r_dim - 1;

        // This replay buffer finishes 2 faces
        load_replay_buf(
            0,
            replay_buf_len,
            // Lambda function to set up replay buffer
            [PACK_INTF_SEL_0, PACK_INTF_SEL_1, ZERO_OUTPUT_FLAG, MEGAROW, face_r_dim]
            {
                // Number of instructions per face (minus 1 for the last special instruction)
                const std::uint32_t num_instrs_per_face = (face_r_dim >> 1) - 1;

                // Face 0 -> mask rows 1010
                // First num_instrs_per_face instructions use ADDR_MOD_0
                for (std::uint32_t i = 0; i < num_instrs_per_face; i++)
                {
                    TTI_PACR(
                        p_pacr::CFG_CTXT_0,
                        p_pacr::NO_ROW_PAD_ZERO,
                        p_pacr::DST_ACCESS_STRIDED_MODE,
                        ADDR_MOD_0,
                        p_pacr::ADDR_CNT_CTXT_0,
                        ZERO_OUTPUT_FLAG,
                        PACK_INTF_SEL_0,
                        0,
                        MEGAROW,
                        p_pacr::NO_CTXT_CTRL,
                        0,
                        0);
                }
                // Last instruction for Face 0 uses ADDR_MOD_1
                TTI_PACR(
                    p_pacr::CFG_CTXT_0,
                    p_pacr::NO_ROW_PAD_ZERO,
                    p_pacr::DST_ACCESS_STRIDED_MODE,
                    ADDR_MOD_1,
                    p_pacr::ADDR_CNT_CTXT_0,
                    ZERO_OUTPUT_FLAG,
                    PACK_INTF_SEL_0,
                    0,
                    MEGAROW,
                    p_pacr::NO_CTXT_CTRL,
                    0,
                    0);

                // Face 1 -> mask rows 0101
                // num_instrs_per_face instructions use ADDR_MOD_0
                // The last instruction is handled separately outside the replay buffer
                for (std::uint32_t i = 0; i < num_instrs_per_face; i++)
                {
                    TTI_PACR(
                        p_pacr::CFG_CTXT_0,
                        p_pacr::NO_ROW_PAD_ZERO,
                        p_pacr::DST_ACCESS_STRIDED_MODE,
                        ADDR_MOD_0,
                        p_pacr::ADDR_CNT_CTXT_0,
                        ZERO_OUTPUT_FLAG,
                        PACK_INTF_SEL_1,
                        0,
                        MEGAROW,
                        p_pacr::NO_CTXT_CTRL,
                        0,
                        0);
                }
                // Last PACR instruction of the half-tile must go separately in the MOP. This is to be able to override it, to ensure that for the second half
                // the tile is closed correctly.
            });

        ckernel::ckernel_template tmp(
            MOP_OUTER_LOOP,
            MOP_INNER_LOOP,
            lltt::replay_insn(0, replay_buf_len),
            TT_OP_PACR(
                p_pacr::CFG_CTXT_0,
                p_pacr::NO_ROW_PAD_ZERO,
                p_pacr::DST_ACCESS_STRIDED_MODE,
                ADDR_MOD_2,
                p_pacr::ADDR_CNT_CTXT_0,
                ZERO_OUTPUT_FLAG,
                PACK_INTF_SEL_1,
                0,
                0,
                p_pacr::NO_CTXT_CTRL,
                0,
                0) // don't close tile
        );

        // Close the tile only when it is actually done.
        tmp.set_last_outer_loop_instr(TT_OP_PACR(
            p_pacr::CFG_CTXT_0,
            p_pacr::NO_ROW_PAD_ZERO,
            p_pacr::DST_ACCESS_STRIDED_MODE,
            ADDR_MOD_2,
            p_pacr::ADDR_CNT_CTXT_0,
            ZERO_OUTPUT_FLAG,
            PACK_INTF_SEL_1,
            0,
            0,
            p_pacr::NO_CTXT_CTRL,
            0,
            1));

        tmp.set_end_op(TT_OP_SETADCZW(p_setadc::PAC, 0, num_faces >> 1, 0, 0, 0b0100)); // ch0_z = 0, ch1_z = num_faces >> 1;

        tmp.program();
    }
    else
    {
        const std::uint32_t PACK_INTF_SEL =
            face_r_dim == 1 ? p_pacr::SINGLE_INTF_ACTIVE : (face_r_dim == 2 ? p_pacr::TWO_INTFS_ACTIVE : p_pacr::ALL_INTF_ACTIVE);

        const std::uint32_t MOP_INNER_LOOP = (face_r_dim < 4) ? 1 : face_r_dim >> 2;
        const std::uint32_t MOP_OUTER_LOOP = num_faces * num_tiles;

        ckernel::ckernel_template tmp(
            MOP_OUTER_LOOP,
            MOP_INNER_LOOP,
            TT_OP_PACR(
                p_pacr::CFG_CTXT_0,
                p_pacr::NO_ROW_PAD_ZERO,
                p_pacr::DST_ACCESS_STRIDED_MODE,
                ADDR_MOD_0,
                p_pacr::ADDR_CNT_CTXT_0,
                ZERO_OUTPUT_FLAG,
                PACK_INTF_SEL,
                0,
                0,
                0,
                0,
                0));
        tmp.set_last_inner_loop_instr(TT_OP_PACR(
            p_pacr::CFG_CTXT_0,
            p_pacr::NO_ROW_PAD_ZERO,
            p_pacr::DST_ACCESS_STRIDED_MODE,
            ADDR_MOD_2,
            p_pacr::ADDR_CNT_CTXT_0,
            ZERO_OUTPUT_FLAG,
            PACK_INTF_SEL,
            0,
            0,
            0,
            0,
            0));
        tmp.set_last_outer_loop_instr(TT_OP_PACR(
            p_pacr::CFG_CTXT_0,
            p_pacr::NO_ROW_PAD_ZERO,
            p_pacr::DST_ACCESS_STRIDED_MODE,
            ADDR_MOD_1,
            p_pacr::ADDR_CNT_CTXT_0,
            ZERO_OUTPUT_FLAG,
            PACK_INTF_SEL,
            0,
            0,
            0,
            0,
            1));

        // if (partial_face) {
        //     tmp.set_start_op(TT_OP_PACR(p_pacr::CFG_CTXT_0, p_pacr::NO_ROW_PAD_ZERO, p_pacr::DST_ACCESS_STRIDED_MODE, ADDR_MOD_0, p_pacr::ADDR_CNT_CTXT_0,
        //     ZERO_OUTPUT_FLAG, p_pacr::ALL_INTF_ACTIVE, 0, MEGAROW, 0, 0, 1)); // Don't close the tile, point to the next face
        //     tmp.set_loop_op0(TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0)); // Inc ch0_y+=1 (addr_mod_0 will increment by 15)
        //     tmp.set_loop_op1(TT_OP_PACR(p_pacr::CFG_CTXT_0, p_pacr::NO_ROW_PAD_ZERO, p_pacr::DST_ACCESS_STRIDED_MODE, ADDR_MOD_1, p_pacr::ADDR_CNT_CTXT_0,
        //     ZERO_OUTPUT_FLAG, p_pacr::ALL_INTF_ACTIVE, 0, MEGAROW, 0, 0, 1)); // Close the tile
        // }

        tmp.program();
    }
}

template <bool is_fp32_dest_acc_en, bool is_tile_dim_reconfig_en = false>
inline void _llk_pack_reconfig_data_format_(
    const std::uint32_t pack_src_format,
    const std::uint32_t pack_dst_format,
    const std::uint32_t tile_size,
    const std::uint32_t face_r_dim = FACE_R_DIM,
    const std::uint32_t tile_c_dim = TILE_C_DIM,
    const std::uint32_t num_faces  = 4,
    const bool partial_face        = false,
    const bool narrow_tile         = false,
    const std::uint32_t num_tiles  = 1)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    reconfig_packer_data_format<is_fp32_dest_acc_en>(pack_src_format, pack_dst_format, tile_size, face_r_dim, tile_c_dim, num_faces, partial_face);

    if constexpr (is_tile_dim_reconfig_en)
    {
        _llk_pack_mop_config_<false, false>(pack_dst_format, face_r_dim, tile_c_dim, num_faces, partial_face, narrow_tile, num_tiles);
    }
}

inline void _llk_pack_set_fp32_dest_acc_(bool enable)
{
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::PACK);
    cfg_reg_rmw_tensix<PCK_DEST_RD_CTRL_Read_32b_data_RMW>(enable);
}

// If using 8bit datums for unpack src. tilize must be set to false because we skip the blackhole workaround which involves unswizzling rows in the tile,
// and this unswizzling is not needed for 8bit datums as they are not affected by the blackhole issue.
template <bool is_fp32_dest_acc_en, bool untilize = false, bool tilize = false>
inline void _llk_pack_hw_configure_(
    const std::uint32_t pack_src_format,
    const std::uint32_t pack_dst_format,
    const std::uint32_t tile_size,
    const std::uint32_t face_r_dim  = FACE_R_DIM,
    const std::uint32_t tile_c_dim  = TILE_C_DIM,
    const std::uint32_t num_faces   = 4,
    const bool partial_face         = false,
    const bool narrow_tile          = false,
    const std::uint32_t relu_config = 0)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    configure_pack<is_fp32_dest_acc_en, untilize, tilize>(
        pack_src_format, pack_dst_format, tile_size, face_r_dim, tile_c_dim, num_faces, partial_face, narrow_tile, relu_config);
}

// TODO NC: Clean up as the part of tt-metal#34587
template <bool untilize = false, bool zero_output = false, bool tilize = false>
inline void _llk_pack_init_(
    const std::uint32_t pack_dst_format,
    const std::uint32_t face_r_dim = FACE_R_DIM,
    const std::uint32_t tile_c_dim = TILE_C_DIM,
    const std::uint32_t num_faces  = 4,
    const bool partial_face        = false,
    const bool narrow_tile         = false,
    const std::uint32_t num_tiles  = 1)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    _llk_pack_configure_addrmod_<untilize, tilize>();
    _llk_pack_mop_config_<untilize, zero_output, tilize>(pack_dst_format, face_r_dim, tile_c_dim, num_faces, partial_face, narrow_tile, num_tiles);
}

// TODO NC: Clean up as the part of tt-metal#34587
template <bool untilize = false, bool zero_output = false, bool tilize = false>
inline void _llk_pack_init_(
    const std::uint32_t pack_src_format,
    const std::uint32_t pack_dst_format,
    const std::uint32_t face_r_dim,
    const std::uint32_t tile_c_dim,
    const std::uint32_t num_faces,
    const bool partial_face              = false,
    const bool narrow_tile               = false,
    const std::uint32_t num_tiles        = 1,
    const bool skip_bh_tilize_workaround = false)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    const DataFormat src_format = static_cast<DataFormat>(pack_src_format);
    if (src_format == DataFormat::Float32)
    {
        LLK_ASSERT(num_tiles <= 4, "Max supported num_tiles for FLOAT32 is 4.");
    }
    else if ((src_format == DataFormat::Float16) || (src_format == DataFormat::Float16_b))
    {
        LLK_ASSERT(num_tiles <= 8, "Max supported num_tiles for FLOAT16 or FLOAT16_B is 8.");
    }

    // 8bit datums in the unpack src format are not affected by the blackhole issue,
    // so we can skip the workaround which involves unswizzling rows in the tile.
    if (skip_bh_tilize_workaround)
    {
        _llk_pack_configure_addrmod_<untilize, false /* tilize */>();
        _llk_pack_mop_config_<untilize, zero_output, false /* tilize */>(
            pack_dst_format, face_r_dim, tile_c_dim, num_faces, partial_face, narrow_tile, num_tiles);
        set_packer_strides<untilize, false /* tilize */>(pack_src_format, tile_c_dim);
    }
    else
    {
        _llk_pack_configure_addrmod_<untilize, tilize>();
        _llk_pack_mop_config_<untilize, zero_output, tilize>(pack_dst_format, face_r_dim, tile_c_dim, num_faces, partial_face, narrow_tile, num_tiles);
        set_packer_strides<untilize, tilize>(pack_src_format, tile_c_dim);
    }

    TTI_SETADCXX(p_setadc::PAC, FACE_C_DIM - 1, 0x0);
}

inline void _llk_pack_uninit_()
{
    // No state to restore - Blackhole pack_init sets PAC X counter to FACE_C_DIM - 1 which is the default
}

template <DstSync Dst, bool is_fp32_dest_acc_en, bool untilize = false>
inline void _llk_pack_(const std::uint32_t tile_index, const std::uint32_t address)
{
    set_dst_write_addr(tile_index);

    program_packer_destination(address);

    ckernel::ckernel_template::run();

    TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0101); // reset z counters
}

// ============================================================================
// BH Fast-Tilize Pack
//
// Reads 4 tiles from the interleaved DEST layout using DST_ACCESS_STRIDED_MODE.
// Strided mode reads 4 rows at stride 16 per PACR (matching the gap pattern).
// 4 PACRs per face, 4 faces per tile, 4 tiles per unit = 64 PACRs.
//
// DEST layout (from fast-tilize unpack+math):
//   Tile T, face F (F=0: cols 0-15, F=1: cols 16-31) top half:
//     DEST row = tensor_row * 16 + T * 2 + F
//     for tensor_row = 0..15 (top), 16..31 (bottom, add 256 offset)
// ============================================================================

__attribute__((noinline)) void _llk_pack_fast_tilize_configure_addrmod_()
{
    // MOP-based pack: Z handles left/right face (Z_stride=1 row), W handles top/bottom (W_stride=256 rows).
    // MOP outerloop=2 (face0+face1), innerloop=4 (4 PACRs per face).
    // Z toggles 0→1 at face boundary via last_inner, resets at last_outer.

    // ADDR_MOD_0: Y += 1 (advance to next 4-row group within a face)
    addr_mod_pack_t {
        .y_src = {.incr = 1},
    }
        .set(ADDR_MOD_0);

    // ADDR_MOD_2 (last_inner): Y clear, Z += 1 (face0→face1 within a half)
    addr_mod_pack_t {
        .y_src = {.incr = 0, .clr = 1},
        .z_src = {.incr = 1},
    }
        .set(ADDR_MOD_2);

    // ADDR_MOD_3 (last_outer): Y clear, Z clear (face-pair done, reset for next half or tile)
    addr_mod_pack_t {
        .y_src = {.incr = 0, .clr = 1},
        .z_src = {.incr = 0, .clr = 1},
    }
        .set(ADDR_MOD_3);
}

// L1 advance replay (only one needed, at packer replay offset)
constexpr std::uint32_t FAST_TILIZE_L1_OFFSET = ckernel::packer::replay_buf_offset;
constexpr std::uint32_t FAST_TILIZE_L1_LEN    = 4;

__attribute__((noinline)) void _llk_pack_fast_tilize_load_l1_advance_replay_()
{
    // L1 address advance after unit flush.
    load_replay_buf(
        FAST_TILIZE_L1_OFFSET,
        FAST_TILIZE_L1_LEN,
        []
        {
            TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET);
            TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
            TTI_WRCFG(p_gpr_pack::OUTPUT_ADDR, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32);
            TTI_NOP;
        });
}

inline void _llk_pack_fast_tilize_mop_config_()
{
    // MOP-based pack: outerloop=2 (face0+face1), innerloop=4 (4 PACRs per face).
    // Z handles left/right (Z_stride=1 row), W handles top/bottom (W_stride=256 rows).
    // Two run() calls per tile: one for top (face0+face1), one for bottom (face2+face3).
    // Eliminates all replay overhead — PACRs stream directly from MOP at 1/cycle.

    ckernel::ckernel_template tmp(
        2, // outerloop — 2 faces per run (face0+face1 or face2+face3)
        4, // innerloop — 4 PACRs per face (Y=0..3)
        TT_OP_PACR(
            p_pacr::CFG_CTXT_0,
            p_pacr::NO_ROW_PAD_ZERO,
            p_pacr::DST_ACCESS_STRIDED_MODE,
            ADDR_MOD_0,
            p_pacr::ADDR_CNT_CTXT_0,
            0,
            p_pacr::ALL_INTF_ACTIVE,
            0,
            0,
            p_pacr::NO_CTXT_CTRL,
            0,
            0));

    // Last inner: Y clear + Z+=1 (face0→face1 transition)
    tmp.set_last_inner_loop_instr(TT_OP_PACR(
        p_pacr::CFG_CTXT_0,
        p_pacr::NO_ROW_PAD_ZERO,
        p_pacr::DST_ACCESS_STRIDED_MODE,
        ADDR_MOD_2,
        p_pacr::ADDR_CNT_CTXT_0,
        0,
        p_pacr::ALL_INTF_ACTIVE,
        0,
        0,
        p_pacr::NO_CTXT_CTRL,
        0,
        0));

    // Last outer: Y clear + Z clear (face-pair done, no Last — accumulate across tiles)
    tmp.set_last_outer_loop_instr(TT_OP_PACR(
        p_pacr::CFG_CTXT_0,
        p_pacr::NO_ROW_PAD_ZERO,
        p_pacr::DST_ACCESS_STRIDED_MODE,
        ADDR_MOD_3,
        p_pacr::ADDR_CNT_CTXT_0,
        0,
        p_pacr::ALL_INTF_ACTIVE,
        0,
        0,
        p_pacr::NO_CTXT_CTRL,
        0,
        0));

    tmp.program();

    // L1 advance replay (1 per 4-tile unit)
    _llk_pack_fast_tilize_load_l1_advance_replay_();
}

template <DstSync Dst>
__attribute__((noinline)) void _llk_pack_fast_tilize_init_(
    [[maybe_unused]] const std::uint32_t use_32bit_dest,
    const std::uint32_t pack_dst_format,
    [[maybe_unused]] const std::uint32_t unit_dim,
    [[maybe_unused]] const std::uint32_t num_faces = 4)
{
    // L1 address advancement per unit (4 tiles). Output buffer accumulates, flushes once.
    const std::uint32_t pacr_l1_size = 4 * (SCALE_DATUM_SIZE(pack_dst_format, TILE_C_DIM * TILE_R_DIM) >> 4);
    TT_SETDMAREG(0, LOWER_HALFWORD(pacr_l1_size), 0, LO_16(p_gpr_pack::OUTPUT_ADDR_OFFSET));

    // DEST offset registers for SyncHalf
    TTI_SETDMAREG(0, 0x000, 0, LO_16(p_gpr_pack::DEST_OFFSET_LO + 0));
    TTI_SETDMAREG(0, DEST_REGISTER_HALF_SIZE, 0, LO_16(p_gpr_pack::DEST_OFFSET_HI + 0));

    select_packer_dest_registers<Dst>();

    // X counter: 16 datums per pack row
    TTI_SETADCXX(p_setadc::PAC, FACE_C_DIM - 1, 0x0);

    // NOTE: remap_addrs + swizzle_32b enabled in block function after math done

    // Compute bytes-per-datum for the DEST source format
    std::uint32_t x_stride = (pack_dst_format & 0x3) == to_underlying(DataFormat::Float32)   ? 4
                             : (pack_dst_format & 0x3) == to_underlying(DataFormat::Float16) ? 2
                                                                                             : 1;

    // Strides for MOP-based gap DEST layout:
    // Y stride: 64 DEST rows per Y step (within-face group: 4 tensor rows × 16 rows/group)
    // Z stride: 1 DEST row (left/right face column: face0→face1 at Z+=1)
    // W stride: 2 DEST rows (tile selection: W=T for top, W=T+128 for bottom)
    //   Face0: W=T, Z=0 → T*2. Face1: W=T, Z=1 → T*2+1.
    //   Face2: W=T+128, Z=0 → (T+128)*2 = T*2+256. Face3: W=T+128, Z=1 → T*2+257.
    std::uint32_t y_stride = 64 * FACE_C_DIM * x_stride; // 64 rows
    std::uint32_t z_stride = FACE_C_DIM * x_stride;      // 1 row
    std::uint32_t w_stride = 2 * FACE_C_DIM * x_stride;  // 2 rows

    TT_SETDMAREG(0, LOWER_HALFWORD(y_stride << PCK0_ADDR_CTRL_XY_REG_0_Ystride_SHAMT), 0, LO_16(p_gpr_pack::TMP0));
    TT_SETDMAREG(0, UPPER_HALFWORD(y_stride << PCK0_ADDR_CTRL_XY_REG_0_Ystride_SHAMT), 0, HI_16(p_gpr_pack::TMP0));
    TT_SETDMAREG(0, LOWER_HALFWORD(z_stride << PCK0_ADDR_CTRL_ZW_REG_0_Zstride_SHAMT), 0, LO_16(p_gpr_pack::TMP1));
    TT_SETDMAREG(0, UPPER_HALFWORD(w_stride << PCK0_ADDR_CTRL_ZW_REG_0_Wstride_SHAMT), 0, HI_16(p_gpr_pack::TMP1));
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
    TTI_WRCFG(p_gpr_pack::TMP0, p_cfg::WRCFG_32b, PCK0_ADDR_CTRL_XY_REG_0_Xstride_ADDR32);
    TTI_WRCFG(p_gpr_pack::TMP1, p_cfg::WRCFG_32b, PCK0_ADDR_CTRL_ZW_REG_0_Zstride_ADDR32);
    TTI_NOP;
    TTI_NOP;

    _llk_pack_fast_tilize_configure_addrmod_();
    _llk_pack_fast_tilize_mop_config_();
}

inline void _llk_pack_fast_tilize_block_(
    [[maybe_unused]] const std::uint32_t tile_index,
    const std::uint32_t address,
    [[maybe_unused]] const std::uint32_t unit_dim,
    const std::uint32_t num_units,
    [[maybe_unused]] const std::uint32_t num_faces = 4)
{
    // remap enabled by math init (_llk_math_reconfig_remap_)

    program_packer_destination(address);

    // MOP config pointer for toggling Last=1 on last tile
    volatile std::uint32_t *mop_cfg = reinterpret_cast<volatile std::uint32_t *>(TENSIX_MOP_CFG_BASE);

    for (std::uint32_t u = 0; u < num_units; u++)
    {
        // Pack 4 tiles: 2 run() per tile (top + bottom face-pairs).
        // Output buffer accumulates across tiles; tile 3 flushes via Last=1.
        for (std::uint32_t t = 0; t < 4; t++)
        {
            TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, t);
            ckernel::ckernel_template::run();

            TTI_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0001);
            TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, t + 128);

            if (t == 3)
            {
                mop_cfg[7] = TT_OP_PACR(
                    p_pacr::CFG_CTXT_0,
                    p_pacr::NO_ROW_PAD_ZERO,
                    p_pacr::DST_ACCESS_STRIDED_MODE,
                    ADDR_MOD_3,
                    p_pacr::ADDR_CNT_CTXT_0,
                    0,
                    p_pacr::ALL_INTF_ACTIVE,
                    0,
                    0,
                    p_pacr::NO_CTXT_CTRL,
                    0,
                    1);
            }

            ckernel::ckernel_template::run();
        }

        // Restore last_outer to no-Last for next unit
        mop_cfg[7] = TT_OP_PACR(
            p_pacr::CFG_CTXT_0,
            p_pacr::NO_ROW_PAD_ZERO,
            p_pacr::DST_ACCESS_STRIDED_MODE,
            ADDR_MOD_3,
            p_pacr::ADDR_CNT_CTXT_0,
            0,
            p_pacr::ALL_INTF_ACTIVE,
            0,
            0,
            p_pacr::NO_CTXT_CTRL,
            0,
            0);

        // L1 advance for all 4 tiles
        TTI_REPLAY(FAST_TILIZE_L1_OFFSET, FAST_TILIZE_L1_LEN, 0, 0);
    }
}

template <DstSync Dst, bool is_fp32_dest_acc_en>
inline void _llk_pack_fast_tilize_uninit_(
    const std::uint32_t pack_dst_format, [[maybe_unused]] const std::uint32_t face_r_dim, [[maybe_unused]] const std::uint32_t num_faces)
{
    // Wait for pack to finish before modifying config
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::PACK);

    // remap disabled by math uninit (_llk_math_reconfig_remap_)

    // Restore standard pack strides
    set_packer_strides(pack_dst_format, TILE_C_DIM);

    // Restore standard pack X counter
    TTI_SETADCXX(p_setadc::PAC, FACE_C_DIM - 1, 0x0);
    // Restore standard addr_mod
    _llk_pack_configure_addrmod_<false, false>();
    // Restore standard pack MOP
    _llk_pack_mop_config_<false, false, false>(pack_dst_format, FACE_R_DIM, TILE_C_DIM, 4, false, false, 1);
}

#include "llk_pack_untilize.h"
