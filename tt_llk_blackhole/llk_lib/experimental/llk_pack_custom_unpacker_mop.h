// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
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
#include "lltt.h"

using namespace ckernel;
using namespace ckernel::packer;

inline void _llk_pack_configure_addrmod_()
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

template <bool zero_output = false>
inline void _llk_pack_mop_config_(
    [[maybe_unused]] const std::uint32_t pack_dst_format, const std::uint32_t face_r_dim = FACE_R_DIM, const std::uint32_t num_faces = 4)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");

    constexpr std::uint32_t ZERO_OUTPUT_FLAG = zero_output ? p_pacr::P_ZERO_OUTPUT_ENABLED : p_pacr::P_ZERO_OUTPUT_DISABLED;

    const std::uint32_t PACK_INTF_SEL = face_r_dim == 1 ? p_pacr::SINGLE_INTF_ACTIVE : (face_r_dim == 2 ? p_pacr::TWO_INTFS_ACTIVE : p_pacr::ALL_INTF_ACTIVE);

    const std::uint32_t pacr_per_face = (face_r_dim < 4) ? 1 : face_r_dim >> 2;

    // Buffer 0: intermediate face — ADDR_MOD_2 at end (clear y, increment z)
    const std::uint32_t buf0_len = pacr_per_face;
    lltt::record<lltt::NoExec>(0, buf0_len);
    for (std::uint32_t i = 0; i < pacr_per_face - 1; i++)
    {
        TTI_PACR(
            p_pacr::CFG_CTXT_0,
            p_pacr::NO_ROW_PAD_ZERO,
            p_pacr::DST_ACCESS_NORMAL_MODE,
            ADDR_MOD_0,
            p_pacr::ADDR_CNT_CTXT_0,
            ZERO_OUTPUT_FLAG,
            PACK_INTF_SEL,
            0,
            0,
            p_pacr::NO_CTXT_CTRL,
            0,
            0);
    }
    TTI_PACR(
        p_pacr::CFG_CTXT_0,
        p_pacr::NO_ROW_PAD_ZERO,
        p_pacr::DST_ACCESS_NORMAL_MODE,
        ADDR_MOD_2,
        p_pacr::ADDR_CNT_CTXT_0,
        ZERO_OUTPUT_FLAG,
        PACK_INTF_SEL,
        0,
        0,
        p_pacr::NO_CTXT_CTRL,
        0,
        0);

    // Buffer 1: last face — ADDR_MOD_1 at end (clear all) + close_tile
    const std::uint32_t buf1_start = buf0_len;
    const std::uint32_t buf1_len   = pacr_per_face;
    lltt::record<lltt::NoExec>(buf1_start, buf1_len);
    for (std::uint32_t i = 0; i < pacr_per_face - 1; i++)
    {
        TTI_PACR(
            p_pacr::CFG_CTXT_0,
            p_pacr::NO_ROW_PAD_ZERO,
            p_pacr::DST_ACCESS_NORMAL_MODE,
            ADDR_MOD_0,
            p_pacr::ADDR_CNT_CTXT_0,
            ZERO_OUTPUT_FLAG,
            PACK_INTF_SEL,
            0,
            0,
            p_pacr::NO_CTXT_CTRL,
            0,
            0);
    }
    TTI_PACR(
        p_pacr::CFG_CTXT_0,
        p_pacr::NO_ROW_PAD_ZERO,
        p_pacr::DST_ACCESS_NORMAL_MODE,
        ADDR_MOD_1,
        p_pacr::ADDR_CNT_CTXT_0,
        ZERO_OUTPUT_FLAG,
        PACK_INTF_SEL,
        0,
        0,
        p_pacr::NO_CTXT_CTRL,
        0,
        1);

    const std::uint32_t replay_inter = lltt::replay_insn(0, buf0_len);
    const std::uint32_t replay_last  = lltt::replay_insn(buf1_start, buf1_len);

    std::uint32_t a0 = replay_last, a1 = TT_OP_NOP, a2 = TT_OP_NOP, a3 = TT_OP_NOP;
    if (num_faces == 2)
    {
        a0 = replay_inter;
        a1 = replay_last;
    }
    if (num_faces == 4)
    {
        a0 = replay_inter;
        a1 = replay_inter;
        a2 = replay_inter;
        a3 = replay_last;
    }

    ckernel::ckernel_unpack_template tmp(
        false, // unpackB
        true,  // halo enable (A0-A3 all execute per iteration)
        a0,
        a1,
        a2,
        a3,
        TT_OP_NOP, // skipA
        TT_OP_NOP, // B
        TT_OP_NOP  // skipB
    );

    tmp.program();
}

template <bool is_fp32_dest_acc_en>
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
    configure_pack<is_fp32_dest_acc_en, false, false>(
        pack_src_format, pack_dst_format, tile_size, face_r_dim, tile_c_dim, num_faces, partial_face, narrow_tile, relu_config);
}

template <bool zero_output = false>
inline void _llk_pack_init_(
    const std::uint32_t pack_src_format,
    const std::uint32_t pack_dst_format,
    const std::uint32_t face_r_dim,
    const std::uint32_t tile_c_dim,
    const std::uint32_t num_faces)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    _llk_pack_configure_addrmod_();
    _llk_pack_mop_config_<zero_output>(pack_dst_format, face_r_dim, num_faces);
    set_packer_strides<false, false>(pack_src_format, tile_c_dim);
    TT_SETADCXX(p_setadc::PAC, FACE_C_DIM - 1, 0x0);
}

inline void _llk_pack_uninit_()
{
    // No state to restore - Blackhole pack_init sets PAC X counter to FACE_C_DIM - 1 which is the default
}

template <DstSync Dst, bool is_fp32_dest_acc_en>
inline void _llk_pack_(const std::uint32_t tile_index, const std::uint32_t address, const std::uint32_t num_tiles = 1)
{
    program_packer_destination(address);
    set_dst_write_addr(0);

    ckernel::ckernel_unpack_template::run(num_tiles, 0b0000);

    TT_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0101);
}
