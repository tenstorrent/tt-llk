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

using namespace ckernel;
using namespace ckernel::packer;

// WARNING: Experimental API for SDPA optimizations only.
// This header has no corresponding tests in the llk-test infrastructure.
// Do not use outside of SDPA optimization workflows.

template <bool untilize = false, bool zero_output = false, bool tilize = false>
inline void _llk_pack_mop_config_custom_(
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

    {
        addr_mod_pack_t {
            .y_src = {.incr = 4},
            .y_dst = {.incr = 4},
        }
            .set(ADDR_MOD_0);

        addr_mod_pack_t {
            .y_src = {.incr = 0, .clr = 1, .cr = 0},
            .y_dst = {.incr = 0, .clr = 1, .cr = 0},
            .z_src = {.incr = 0, .clr = 0},
            .z_dst = {.incr = 0, .clr = 0},
        }
            .set(ADDR_MOD_1);

        addr_mod_pack_t {
            .y_src = {.incr = 4, .clr = 0, .cr = 0},
            .y_dst = {.incr = 0, .clr = 1, .cr = 0},
            .z_src = {.incr = 0, .clr = 0},
        }
            .set(ADDR_MOD_2);
    }

    constexpr std::uint32_t ZERO_OUTPUT_FLAG = zero_output ? p_pacr::P_ZERO_OUTPUT_ENABLED : p_pacr::P_ZERO_OUTPUT_DISABLED;

    const std::uint32_t PACK_INTF_SEL = face_r_dim == 1 ? p_pacr::SINGLE_INTF_ACTIVE : (face_r_dim == 2 ? p_pacr::TWO_INTFS_ACTIVE : p_pacr::ALL_INTF_ACTIVE);

    const std::uint32_t MOP_INNER_LOOP = (face_r_dim < 4) ? 1 : (face_r_dim >> 2) * num_faces;
    const std::uint32_t MOP_OUTER_LOOP = num_tiles;

    ckernel::ckernel_template tmp(
        MOP_OUTER_LOOP,
        MOP_INNER_LOOP,
        TT_OP_PACR(
            p_pacr::CFG_CTXT_0,
            p_pacr::NO_ROW_PAD_ZERO,
            p_pacr::DST_ACCESS_NORMAL_MODE,
            ADDR_MOD_0,
            p_pacr::ADDR_CNT_CTXT_0,
            ZERO_OUTPUT_FLAG,
            PACK_INTF_SEL,
            0,
            0,
            0,
            0,
            0));
    // tmp.set_s tart_op(TT_OP_SETADCXY(p_setadc::PAC, 0, 0, 0, 0, 0b1000));
    tmp.set_last_inner_loop_instr(TT_OP_PACR(
        p_pacr::CFG_CTXT_0,
        p_pacr::NO_ROW_PAD_ZERO,
        p_pacr::DST_ACCESS_NORMAL_MODE,
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
        p_pacr::DST_ACCESS_NORMAL_MODE,
        ADDR_MOD_1,
        p_pacr::ADDR_CNT_CTXT_0,
        ZERO_OUTPUT_FLAG,
        PACK_INTF_SEL,
        0,
        0,
        0,
        0,
        1));

    tmp.program();
}

// NOTE: DO NOT TRY TO USE IN KERNELS OTHER THAN SDPA.CPP.
// HIGHLY OPTIMIZED FOR A SPECIFIC USE CASE, NOT TESTED, DOES NOT RESPECT ANY CONTRACT OR PROGRAMMING MODEL.
template <bool out_of_order_output = false, bool is_fp32_dest_acc_en = false>
inline void _llk_pack_w_acc_custom_(const std::uint32_t tile_index, const std::uint32_t address, const std::uint32_t num_tiles)
{
    program_packer_destination(address);

    for (std::uint32_t i = 0; i < num_tiles; i++)
    {
        set_dst_write_addr(tile_index + i);
        ckernel::ckernel_template::run();
        //     TT_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0101); // reset z counters
    }
}
