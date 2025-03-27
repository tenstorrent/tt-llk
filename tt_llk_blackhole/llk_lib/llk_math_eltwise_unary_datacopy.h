// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_globals.h"
#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_math_common.h"

// local function declarations
inline void eltwise_unary_configure_addrmod();

template <
    ckernel::DataCopyType type,
    ckernel::DstSync Dst,
    ckernel::BroadcastType src_b_bcast_type = ckernel::BroadcastType::NONE,
    bool is_fp32_dest_acc_en                = false,
    bool unpack_to_dest                     = false>
inline void _llk_math_eltwise_unary_datacopy_(const std::uint32_t dst_index, const std::uint32_t src_format, const std::uint32_t dst_format)
{
    std::uint32_t constexpr num_faces = 4;

    // For 32bit data, each half of DEST can take 16 tiles. Since dest offset is returned as if 16bit data are used, we need to
    // adjust it to offset in faces for 32bit data.
    std::uint32_t dest_base_offset_in_faces = ckernel::get_dest_buffer_base() >> 5;
    std::uint32_t dst_index_in_faces        = dst_index << 2; // Each tile has 4 faces;

    if (unpack_to_dest && ckernel::math::is_32bit_input(src_format, dst_format))
    {
#if SKIP_UNP == 1
#else
        ckernel::math::math_unpack_to_dest_math_ready();
        ckernel::math::set_dst_write_addr<ckernel::DstTileLayout::Default, ckernel::DstTileShape::Tile32x32, true>(dst_index);
        ckernel::math::math_unpack_to_dest_tile_ready();

        // Due to bug in Blackhole Tensix (more details in budabackend/#2730) when an event with side effect of clearing DEST zero flags
        // (such as Unpack-to-dest or RISC-to-dest) and a ZEROACC instruction from packer occur in the same cycle,
        // zero flags clearing is dropped.
        // To mitigate that, we issue additional zero flag clear instruction immediatelly after unpack tile to dest is done.
        // RISC-to-dest event is not currently used.

#pragma GCC unroll 0
        for (std::uint32_t i = 0; i < num_faces; i++)
        {
            // Clears zero flags in DEST for one face.
            TT_ZEROACC(ckernel::p_zeroacc::CLR_16, 0, 1 /*clear zero flags*/, ckernel::ADDR_MOD_3, dest_base_offset_in_faces + dst_index_in_faces + i);
        }
#endif
    }
    else
    {
        if constexpr ((Dst == ckernel::DstSync::SyncTile16) || (Dst == ckernel::DstSync::SyncTile2))
        {
            ckernel::math::set_dst_write_addr<ckernel::DstTileLayout::Default, ckernel::DstTileShape::Tile32x32>(math_sync_tile_dst_index);
        }
        else
        {
            ckernel::math::set_dst_write_addr<ckernel::DstTileLayout::Default, ckernel::DstTileShape::Tile32x32>(dst_index);
        }

        if constexpr (type == ckernel::A2D)
        {
            ckernel::ckernel_template::run(ckernel::instrn_buffer);
        }
        else if constexpr (type == ckernel::B2D)
        {
            if constexpr (src_b_bcast_type == ckernel::BroadcastType::COL)
            {
                // Mop for col broadcast only does 2 outerloops.  Needs to clear B manually and call twice
                ckernel::ckernel_template::run(ckernel::instrn_buffer);
                TTI_SETRWC(ckernel::p_setrwc::CLR_B, 0, 0, 0, 0, 0);
                ckernel::ckernel_template::run(ckernel::instrn_buffer);
                TTI_SETRWC(ckernel::p_setrwc::CLR_AB, 0, 0, 0, 0, 0);
            }
            else
            {
                ckernel::ckernel_template::run(ckernel::instrn_buffer);
            }
        }
        else
        {
            FWASSERT("Unsupported op!", false);
        }

        ckernel::math::clear_dst_reg_addr();
    }
}

template <ckernel::DataCopyType type, ckernel::BroadcastType bcast_type = ckernel::BroadcastType::NONE>
inline void eltwise_unary_configure_addrmod()
{
    ckernel::addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ckernel::ADDR_MOD_3);

    // Use srcA for data movement
    if constexpr (type == ckernel::A2D)
    {
        ckernel::addr_mod_t {
            .srca = {.incr = 1},
            .srcb = {.incr = 0},
            .dest = {.incr = 1},
        }
            .set(ckernel::ADDR_MOD_0);

        // Just unpack into A and move to Dest
        ckernel::addr_mod_t {
            .srca = {.incr = 8},
            .srcb = {.incr = 0},
            .dest = {.incr = 8},
        }
            .set(ckernel::ADDR_MOD_2);
    }
    else
    {
        if constexpr (bcast_type == ckernel::BroadcastType::ROW || bcast_type == ckernel::BroadcastType::SCALAR)
        {
            ckernel::addr_mod_t {
                .srca = {.incr = 0},
                .srcb = {.incr = 0},
                .dest = {.incr = 1},
            }
                .set(ckernel::ADDR_MOD_0);

            // Just unpack into B and move to Dest
            ckernel::addr_mod_t {
                .srca = {.incr = 0},
                .srcb = {.incr = 0},
                .dest = {.incr = 8},
            }
                .set(ckernel::ADDR_MOD_2);
        }
        else
        {
            ckernel::addr_mod_t {
                .srca = {.incr = 0},
                .srcb = {.incr = 1},
                .dest = {.incr = 1},
            }
                .set(ckernel::ADDR_MOD_0);

            // Just unpack into B and move to Dest
            ckernel::addr_mod_t {
                .srca = {.incr = 0},
                .srcb = {.incr = 8},
                .dest = {.incr = 8},
            }
                .set(ckernel::ADDR_MOD_2);
        }
    }
}

template <
    ckernel::DataCopyType type,
    ckernel::BroadcastType bcast_type = ckernel::BroadcastType::NONE,
    bool tilize                       = false,
    bool is_fp32_dest_acc_en          = false,
    bool is_int_fpu_en                = false>
inline void eltwise_unary_configure_mop(uint rows_per_inst, uint total_rows, const uint num_faces, const uint dst_format)
{
    // always move 32x32 tile, packed as 16x16x4

    if constexpr (type == ckernel::A2D)
    {
        uint addr_mod  = (rows_per_inst == ckernel::p_mova2d::MOV_1_ROW) ? ckernel::ADDR_MOD_0 : ckernel::ADDR_MOD_2;
        uint innerloop = (rows_per_inst == ckernel::p_mova2d::MOV_1_ROW) ? total_rows : (total_rows >> 3);
        uint outerloop = tilize ? 1 : num_faces;

        if (((is_fp32_dest_acc_en || is_int_fpu_en) && !(dst_format == (uint)DataFormat::UInt16)) || (dst_format == (uint)DataFormat::UInt8))
        {
            // use elwadd to handle unpacking data into src A as fp16, but dest is in fp32 mode OR to handle uint8 datums
            ckernel::ckernel_template tmp(outerloop, innerloop, TT_OP_ELWADD(0, 0, ckernel::p_elwise::SRCB_NO_BCAST, ckernel::ADDR_MOD_2, 0));
            tmp.set_end_op(TT_OP_SETRWC(ckernel::p_setrwc::CLR_AB, 0, 0, 0, 0, ckernel::p_setrwc::SET_AB));
            tmp.program(ckernel::instrn_buffer);
        }
        else
        {
            ckernel::ckernel_template tmp(outerloop, innerloop, TT_OP_MOVA2D(0, 0, ckernel::ADDR_MOD_2, ckernel::p_mova2d::MOV_8_ROWS, 0));
            tmp.set_end_op(TT_OP_SETRWC(ckernel::p_setrwc::CLR_AB, 0, 0, 0, 0, ckernel::p_setrwc::SET_AB));
            tmp.program(ckernel::instrn_buffer);
        }
    }
    else if constexpr (type == ckernel::B2D)
    {
        uint addr_mod       = (rows_per_inst == ckernel::p_movb2d::MOV_1_ROW) ? ckernel::ADDR_MOD_0 : ckernel::ADDR_MOD_2;
        uint innerloop      = (rows_per_inst == ckernel::p_movb2d::MOV_1_ROW) ? total_rows : (total_rows >> 2);
        uint outerloop      = 4;
        auto broadcast_type = ckernel::p_movb2d::MOV_1_ROW; // No broadcast;

        if constexpr (bcast_type == ckernel::BroadcastType::COL)
        {
            innerloop = 16 >> 3; // elwadd produces 8 rows per op
            // The mop only runs for 2 outer loops and mop is called twice for col broadcast
            outerloop = 2;
            // broadcast_type = p_movb2d::MOV_8_ROW_BRCST_D0_BRCST;
            // MOVB2D with column broadcast doesn't work due to the bug in FPU tile
            // which masks dest write enable signals when instrn_mode[1:0] == 2'b01
            // ELTWADD with zeros will be used as a workaround
            broadcast_type = ckernel::p_elwise::SRCB_BCAST_COL;
        }
        else if constexpr (bcast_type == ckernel::BroadcastType::ROW)
        {
            innerloop      = (total_rows >> 3);
            broadcast_type = ckernel::p_movb2d::MOV_8_ROW_BRCST;
        }
        else if constexpr (bcast_type == ckernel::BroadcastType::SCALAR)
        {
            // ELTWADD with zeros will be used as a workaround
            outerloop      = 1;
            innerloop      = num_faces * (total_rows >> 3);
            broadcast_type = ckernel::p_elwise::SRCB_BCAST_ALL;
        }

        if constexpr (bcast_type == ckernel::BroadcastType::SCALAR)
        {
            ckernel::ckernel_template tmp(outerloop, innerloop, TT_OP_ELWADD(0, 0, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(ckernel::p_setrwc::CLR_AB, 0, 0, 0, 0, 0));
            tmp.program(ckernel::instrn_buffer);
        }
        else if constexpr (bcast_type == ckernel::BroadcastType::COL)
        {
            ckernel::ckernel_template tmp(outerloop, innerloop, TT_OP_ELWADD(0, 0, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(0, ckernel::p_setrwc::CR_B, 0, 0, 0, ckernel::p_setrwc::SET_B));
            tmp.program(ckernel::instrn_buffer);
        }
        else if constexpr (bcast_type == ckernel::BroadcastType::ROW)
        {
            ckernel::ckernel_template tmp(outerloop, innerloop, TT_OP_MOVB2D(0, 0, addr_mod, broadcast_type, 0));
            tmp.set_end_op(TT_OP_SETRWC(ckernel::p_setrwc::CLR_B, ckernel::p_setrwc::CR_B, 0, 0, 0, ckernel::p_setrwc::SET_B));
            tmp.program(ckernel::instrn_buffer);
        }
        else
        {
            ckernel::ckernel_template tmp(outerloop, innerloop, TT_OP_MOVB2D(0, 0, addr_mod, rows_per_inst, 0));
            tmp.set_end_op(TT_OP_SETRWC(ckernel::p_setrwc::CLR_B, ckernel::p_setrwc::CR_B, 0, 0, 0, ckernel::p_setrwc::SET_B));
            tmp.program(ckernel::instrn_buffer);
        }
    }
}

template <
    ckernel::DataCopyType type,
    ckernel::BroadcastType src_b_bcast_type = ckernel::BroadcastType::NONE,
    bool tilize                             = false,
    bool is_fp32_dest_acc_en                = false,
    bool is_int_fpu_en                      = false>
// within_face_16x16_transpose is used by unpacker, math does not transpose
inline void _llk_math_eltwise_unary_datacopy_init_(
    const std::uint32_t transpose_of_faces          = 0 /*unused*/,
    const std::uint32_t within_face_16x16_transpose = 0 /* unused */,
    const std::uint32_t num_faces                   = 4,
    const std::uint32_t dst_format                  = 255)
{
    eltwise_unary_configure_addrmod<type, src_b_bcast_type>();

    if constexpr (type == ckernel::A2D)
    {
        const uint num_rows = tilize ? 64 : 16;
        eltwise_unary_configure_mop<type, src_b_bcast_type, tilize, is_fp32_dest_acc_en, is_int_fpu_en>(
            ckernel::p_mova2d::MOV_8_ROWS, num_rows, num_faces, dst_format);
    }
    else if constexpr (type == ckernel::B2D)
    {
        eltwise_unary_configure_mop<type, src_b_bcast_type>(ckernel::p_movb2d::MOV_4_ROWS, 16, num_faces, dst_format);
    }
    else
    {
        FWASSERT("Unsupported op!", false);
    }

    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);

    ckernel::math::reset_counters(ckernel::p_setrwc::SET_ABD_F);
}
