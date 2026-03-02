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

    // Record PACR instructions for one face into the replay buffer.
    // Within a face: pacr_per_face - 1 PACRs with ADDR_MOD_0 (advance y += 4),
    // then 1 PACR with ADDR_MOD_2 (clear y, increment z to move to next face).
    const std::uint32_t replay_buf_len = pacr_per_face;

    lltt::record<lltt::NoExec>(0, replay_buf_len);
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

    // Each A slot replays the full face sequence. With halo enabled,
    // one MOP iteration executes A0-A3 = 4 face replays = 1 tile.
    // The zmask-based loop in _llk_pack_ repeats this for num_tiles.
    const std::uint32_t replay = lltt::replay_insn(0, replay_buf_len);

    ckernel::ckernel_unpack_template tmp(
        false,                                 // unpackB
        true,                                  // halo enable (A0-A3 all execute per iteration)
        TT_OP_REPLAY(0, replay_buf_len, 0, 0), // A0 — face 0
        TT_OP_REPLAY(0, replay_buf_len, 0, 0), // A1 — face 1
        TT_OP_REPLAY(0, replay_buf_len, 0, 0), // A2 — face 2
        TT_OP_REPLAY(0, replay_buf_len, 0, 0), // A3 — face 3
        TT_OP_NOP,                             // skipA

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
    set_dst_write_addr(tile_index);

    program_packer_destination(address);

    // zmask must have bits 0..num_tiles-1 set so every iteration takes
    // the A0-A3 (replay) path rather than the skip path.
    // const std::uint32_t zmask = (1u << num_tiles) ;
    const std::uint32_t zmask = 0xFFFFFFFF;

    ckernel::ckernel_unpack_template::run(static_cast<std::uint8_t>(num_tiles), zmask);

    TT_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0101);
}
