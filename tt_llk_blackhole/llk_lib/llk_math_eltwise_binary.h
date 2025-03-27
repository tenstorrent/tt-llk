// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_math_common.h"

// local function declarations
inline void eltwise_binary_configure_addrmod();

template <ckernel::EltwiseBinaryReuseDestType binary_reuse_dest = ckernel::EltwiseBinaryReuseDestType::NONE>
inline void eltwise_binary_reuse_dest_as_src()
{
    if constexpr (binary_reuse_dest == ckernel::EltwiseBinaryReuseDestType::DEST_TO_SRCA)
    {
        ckernel::math::move_d2a_fixed_face(ckernel::ADDR_MOD_1);
    }
    else if constexpr (binary_reuse_dest == ckernel::EltwiseBinaryReuseDestType::DEST_TO_SRCB)
    {
        ckernel::math::move_d2b_fixed_face(ckernel::ADDR_MOD_1);
    }
}

template <
    ckernel::EltwiseBinaryType eltwise_binary_type,
    ckernel::BroadcastType src_b_bcast_type,
    ckernel::DstSync Dst,
    int NUM_FIDELITY_PHASES                               = 0,
    ckernel::EltwiseBinaryReuseDestType binary_reuse_dest = ckernel::EltwiseBinaryReuseDestType::NONE,
    bool is_fp32_dest_acc_en                              = false>
inline void _llk_math_eltwise_binary_(const std::uint32_t num_faces, uint dst_index, const bool clear_fp32_dst_acc)
{
    constexpr bool high_fidelity     = (NUM_FIDELITY_PHASES > 0);
    constexpr uint32_t ZERO_ACC_MODE = ckernel::p_zeroacc::CLR_16;

    if constexpr ((Dst == ckernel::DstSync::SyncTile16) || (Dst == ckernel::DstSync::SyncTile2))
    {
        ckernel::math::set_dst_write_addr<ckernel::DstTileLayout::Default, ckernel::DstTileShape::Tile32x32>(math_sync_tile_dst_index);

        if constexpr (eltwise_binary_type == ckernel::ELWMUL)
        {
            if (is_fp32_dest_acc_en && clear_fp32_dst_acc)
            {
#pragma GCC unroll 0
                for (std::uint32_t i = 0; i < 8; i++)
                {
                    TT_ZEROACC(ZERO_ACC_MODE, is_fp32_dest_acc_en, 0, ckernel::ADDR_MOD_1, (math_sync_tile_dst_index << 3) + i);
                }
            }
            else
            {
#pragma GCC unroll 0
                for (std::uint32_t i = 0; i < 4; i++)
                {
                    TT_ZEROACC(ZERO_ACC_MODE, is_fp32_dest_acc_en, 0, ckernel::ADDR_MOD_1, (math_sync_tile_dst_index << 2) + i);
                }
            }
        }
        else if constexpr (binary_reuse_dest != ckernel::EltwiseBinaryReuseDestType::NONE)
        {
            static_assert(
                !(binary_reuse_dest != ckernel::EltwiseBinaryReuseDestType::NONE && (Dst == ckernel::DstSync::SyncTile16) ||
                  (Dst == ckernel::DstSync::SyncTile2)),
                "Dst clear in DstSync::SyncTile16 or DstSync::SyncTile2 dst sync mode is not supported!");
            /*
            if (clear_dest_acc) {
                if constexpr (is_fp32_dest_acc_en) {
                    #pragma GCC unroll 0
                    for(std::uint32_t i = 0; i < 8; i++) {
                        TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, (math_sync_tile_dst_index << 3) + i);
                    }
                } else {
                    #pragma GCC unroll 0
                    for(std::uint32_t i = 0; i < 4; i++) {
                        TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, (math_sync_tile_dst_index << 2) + i);
                    }
                }
            }
            */
        }
    }
    else
    {
        ckernel::math::set_dst_write_addr<ckernel::DstTileLayout::Default, ckernel::DstTileShape::Tile32x32>(dst_index);
    }

    if constexpr ((eltwise_binary_type == ckernel::ELWADD) || (eltwise_binary_type == ckernel::ELWSUB))
    {
        if constexpr (src_b_bcast_type == ckernel::BroadcastType::COL)
        {
            // Mop for col broadcast only does 2 outerloops.  Needs to clear B manually and call twice
            constexpr uint32_t outerloop = (binary_reuse_dest != ckernel::EltwiseBinaryReuseDestType::NONE) ? 2 : 1;
#pragma GCC unroll 0
            for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
            {
                eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                ckernel::ckernel_template::run(ckernel::instrn_buffer);
            }
            TTI_SETRWC(ckernel::p_setrwc::CLR_B, 0, 0, 0, 0, 0);
#pragma GCC unroll 0
            for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
            {
                eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                ckernel::ckernel_template::run(ckernel::instrn_buffer);
            }
            TTI_SETRWC(ckernel::p_setrwc::CLR_B, 0, 0, 0, 0, 0);
        }
        else
        {
            constexpr uint32_t outerloop = (binary_reuse_dest != ckernel::EltwiseBinaryReuseDestType::NONE) ? 4 : 1;
#pragma GCC unroll 0
            for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
            {
                eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                ckernel::ckernel_template::run(ckernel::instrn_buffer);
            }
            // Manually clear B once mop is done for scaler bcast
            if constexpr (src_b_bcast_type == ckernel::BroadcastType::SCALAR)
            {
                TTI_SETRWC(ckernel::p_setrwc::CLR_B, 0, 0, 0, 0, ckernel::p_setrwc::SET_D);
            }
        }
    }
    else if constexpr (eltwise_binary_type == ckernel::ELWMUL)
    {
        if constexpr (src_b_bcast_type == ckernel::BroadcastType::COL)
        {
            // Mop for col broadcast only does 2 outerloops.  Needs to clear B manually and call twice
            constexpr uint32_t outerloop = (binary_reuse_dest != ckernel::EltwiseBinaryReuseDestType::NONE) ? 2 : 1;
            if constexpr (high_fidelity)
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < 2; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != ckernel::EltwiseBinaryReuseDestType::NONE)
                    {
                        // We clear the DEST face-by-face, given the DEST base, tile index and face index
                        if (is_fp32_dest_acc_en && clear_fp32_dst_acc)
                        {
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                1 /*clear fp32*/,
                                0,
                                ckernel::ADDR_MOD_1,
                                (ckernel::math::get_dest_buffer_base_32b() +
                                 ckernel::math::get_dest_index_in_faces(dst_index, (0 + face_num)))); // Clear faces 0 & 1
                        }
                        else
                        {
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                0 /*clear fp32*/,
                                0,
                                ckernel::ADDR_MOD_1,
                                (ckernel::math::get_dest_buffer_base_16b() +
                                 ckernel::math::get_dest_index_in_faces(dst_index, (0 + face_num)))); // Clear faces 0 & 1
                        }
                    }
                    ckernel::ckernel_template::run(ckernel::instrn_buffer);
                }
            }
            else
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != ckernel::EltwiseBinaryReuseDestType::NONE)
                    {
                        // We clear the DEST face-by-face, given the DEST base, tile index and face index
                        if (is_fp32_dest_acc_en && clear_fp32_dst_acc)
                        {
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                1 /*clear fp32*/,
                                0,
                                ckernel::ADDR_MOD_1,
                                (ckernel::math::get_dest_buffer_base_32b() +
                                 ckernel::math::get_dest_index_in_faces(dst_index, (0 + face_num)))); // Clear faces 0 & 1
                        }
                        else
                        {
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                0 /*clear fp32*/,
                                0,
                                ckernel::ADDR_MOD_1,
                                (ckernel::math::get_dest_buffer_base_16b() +
                                 ckernel::math::get_dest_index_in_faces(dst_index, (0 + face_num)))); // Clear faces 0 & 1
                        }
                    }
                    ckernel::ckernel_template::run(ckernel::instrn_buffer);
                }
            }
            TTI_SETRWC(ckernel::p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            if constexpr (high_fidelity)
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < 2; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != ckernel::EltwiseBinaryReuseDestType::NONE)
                    {
                        // We clear the DEST face-by-face, given the DEST base, tile index and face index
                        if (is_fp32_dest_acc_en && clear_fp32_dst_acc)
                        {
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                1 /*clear fp32*/,
                                0,
                                ckernel::ADDR_MOD_1,
                                (ckernel::math::get_dest_buffer_base_32b() +
                                 ckernel::math::get_dest_index_in_faces(dst_index, (2 + face_num)))); // Clear faces 2 & 3
                        }
                        else
                        {
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                0 /*clear fp32*/,
                                0,
                                ckernel::ADDR_MOD_1,
                                (ckernel::math::get_dest_buffer_base_16b() +
                                 ckernel::math::get_dest_index_in_faces(dst_index, (2 + face_num)))); // Clear faces 2 & 3
                        }
                    }
                    ckernel::ckernel_template::run(ckernel::instrn_buffer);
                }
            }
            else
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != ckernel::EltwiseBinaryReuseDestType::NONE)
                    {
                        // We clear the DEST face-by-face, given the DEST base, tile index and face index
                        if (is_fp32_dest_acc_en && clear_fp32_dst_acc)
                        {
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                1 /*clear fp32*/,
                                0,
                                ckernel::ADDR_MOD_1,
                                (ckernel::math::get_dest_buffer_base_32b() +
                                 ckernel::math::get_dest_index_in_faces(dst_index, (2 + face_num)))); // Clear faces 2 & 3
                        }
                        else
                        {
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                0 /*clear fp32*/,
                                0,
                                ckernel::ADDR_MOD_1,
                                (ckernel::math::get_dest_buffer_base_16b() +
                                 ckernel::math::get_dest_index_in_faces(dst_index, (2 + face_num)))); // Clear faces 2 & 3
                        }
                    }
                    ckernel::ckernel_template::run(ckernel::instrn_buffer);
                }
            }
            TTI_SETRWC(ckernel::p_setrwc::CLR_B, 0, 0, 0, 0, 0);
        }
        else
        {
            // Row and no broadcasted behaves similarly
            const uint32_t outerloop = (binary_reuse_dest != ckernel::EltwiseBinaryReuseDestType::NONE) ? num_faces : 1;
            if constexpr (high_fidelity)
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < num_faces; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != ckernel::EltwiseBinaryReuseDestType::NONE)
                    {
                        // We clear the DEST face-by-face, given the DEST base, tile index and face index
                        if (is_fp32_dest_acc_en && clear_fp32_dst_acc)
                        {
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                1 /*clear fp32*/,
                                0,
                                ckernel::ADDR_MOD_1,
                                (ckernel::math::get_dest_buffer_base_32b() + ckernel::math::get_dest_index_in_faces(dst_index, face_num)));
                        }
                        else
                        {
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                0 /*clear fp32*/,
                                0,
                                ckernel::ADDR_MOD_1,
                                (ckernel::math::get_dest_buffer_base_16b() + ckernel::math::get_dest_index_in_faces(dst_index, face_num)));
                        }
                    }
                    ckernel::ckernel_template::run(ckernel::instrn_buffer);
                }
            }
            else
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != ckernel::EltwiseBinaryReuseDestType::NONE)
                    {
                        // We clear the DEST face-by-face, given the DEST base, tile index and face index
                        if (is_fp32_dest_acc_en && clear_fp32_dst_acc)
                        {
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                1 /*clear fp32*/,
                                0,
                                ckernel::ADDR_MOD_1,
                                (ckernel::math::get_dest_buffer_base_32b() + ckernel::math::get_dest_index_in_faces(dst_index, face_num)));
                        }
                        else
                        {
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                0 /*clear fp32*/,
                                0,
                                ckernel::ADDR_MOD_1,
                                (ckernel::math::get_dest_buffer_base_16b() + ckernel::math::get_dest_index_in_faces(dst_index, face_num)));
                        }
                    }
                    ckernel::ckernel_template::run(ckernel::instrn_buffer);
                }
            }
            if constexpr (src_b_bcast_type == ckernel::BroadcastType::SCALAR)
            {
                TTI_SETRWC(ckernel::p_setrwc::CLR_B, 0, 0, 0, 0, ckernel::p_setrwc::SET_D);
            }
        }
    }
    else
    {
        FWASSERT("Unsupported op!", false);
    }
    ckernel::math::clear_dst_reg_addr();
}

template <ckernel::EltwiseBinaryType eltwise_binary_type, ckernel::BroadcastType bcast_type, std::uint32_t FIDELITY_INCREMENT>
inline void eltwise_binary_configure_addrmod()
{
    // Use srcA for data movement
    if constexpr ((eltwise_binary_type == ckernel::ELWADD) || (eltwise_binary_type == ckernel::ELWSUB) || (eltwise_binary_type == ckernel::ELWMUL))
    {
        if constexpr (bcast_type == ckernel::BroadcastType::NONE || bcast_type == ckernel::BroadcastType::COL)
        {
            ckernel::addr_mod_t {
                .srca = {.incr = 8},
                .srcb = {.incr = 8},
                .dest = {.incr = 8},
            }
                .set(ckernel::ADDR_MOD_0);
        }
        else if constexpr (bcast_type == ckernel::BroadcastType::ROW || bcast_type == ckernel::BroadcastType::SCALAR)
        {
            ckernel::addr_mod_t {
                .srca = {.incr = 8},
                .srcb = {.incr = 0},
                .dest = {.incr = 8},
            }
                .set(ckernel::ADDR_MOD_0);
        }
        ckernel::addr_mod_t {
            .srca = {.incr = 0},
            .srcb = {.incr = 0},
            .dest = {.incr = 0},
        }
            .set(ckernel::ADDR_MOD_1);

        ckernel::addr_mod_t {
            .srca = {.incr = 0, .clr = 1}, .srcb = {.incr = 0, .clr = 1}, .dest = {.incr = 0, .clr = 0, .cr = 1}, .fidelity = {.incr = FIDELITY_INCREMENT}}
            .set(ckernel::ADDR_MOD_2);

        ckernel::addr_mod_t {
            .srca     = {.incr = 0, .clr = 1},
            .srcb     = {.incr = 0, .clr = 1},
            .dest     = {.incr = 8, .clr = 0, .cr = 0, .c_to_cr = 1},
            .fidelity = {.incr = 0, .clr = 1}}
            .set(ckernel::ADDR_MOD_3);
    }
}

template <
    ckernel::EltwiseBinaryType eltwise_binary_type,
    ckernel::BroadcastType bcast_type,
    int NUM_FIDELITY_PHASES                               = 0,
    ckernel::EltwiseBinaryReuseDestType binary_reuse_dest = ckernel::EltwiseBinaryReuseDestType::NONE>
inline void eltwise_binary_configure_mop(const std::uint32_t acc_to_dest = 0, const std::uint32_t num_faces = 4)
{
    constexpr bool high_fidelity = (NUM_FIDELITY_PHASES > 0);
    const uint addr_mod          = ckernel::ADDR_MOD_0;
    constexpr uint innerloop     = 16 >> 3; // 8 rows per eltwise op at a time.
    uint outerloop               = num_faces;
    auto broadcast_type          = ckernel::p_elwise::SRCB_NO_BCAST;
    if constexpr (bcast_type == ckernel::BroadcastType::COL)
    {
        // The mop only runs for 2 outer loops and mop is called twice for col broadcast
        outerloop      = 2;
        broadcast_type = ckernel::p_elwise::SRCB_BCAST_COL;
    }
    else if constexpr (bcast_type == ckernel::BroadcastType::ROW)
    {
        broadcast_type = ckernel::p_elwise::SRCB_BCAST_ROW;
    }
    else if constexpr (bcast_type == ckernel::BroadcastType::SCALAR)
    {
        broadcast_type = ckernel::p_elwise::SRCB_BCAST_ALL;
    }

    if constexpr (binary_reuse_dest != ckernel::EltwiseBinaryReuseDestType::NONE)
    {
        outerloop = 1;
    }

    // Scalar and Col broadcast should not Clear B within a mop.  This is controlled outside of MOP.
    if constexpr (bcast_type == ckernel::BroadcastType::COL || bcast_type == ckernel::BroadcastType::SCALAR)
    {
        if constexpr (eltwise_binary_type == ckernel::ELWADD)
        {
            ckernel::ckernel_template tmp(outerloop, innerloop, TT_OP_ELWADD(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(ckernel::p_setrwc::CLR_A, ckernel::p_setrwc::CR_AB, 0, 0, 0, ckernel::p_setrwc::SET_AB));
            tmp.program(ckernel::instrn_buffer);
        }
        else if constexpr (eltwise_binary_type == ckernel::ELWSUB)
        {
            ckernel::ckernel_template tmp(outerloop, innerloop, TT_OP_ELWSUB(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(ckernel::p_setrwc::CLR_A, ckernel::p_setrwc::CR_AB, 0, 0, 0, ckernel::p_setrwc::SET_AB));
            tmp.program(ckernel::instrn_buffer);
        }
        else if constexpr (eltwise_binary_type == ckernel::ELWMUL)
        {
            ckernel::ckernel_template tmp(high_fidelity ? NUM_FIDELITY_PHASES : outerloop, innerloop, TT_OP_ELWMUL(0, 0, broadcast_type, addr_mod, 0));
            if constexpr (high_fidelity)
            {
                tmp.set_last_inner_loop_instr(TT_OP_ELWMUL(0, 0, broadcast_type, ckernel::ADDR_MOD_2, 0)); // Incr fidelity last inst of inner loop
                tmp.set_last_outer_loop_instr(TT_OP_ELWMUL(ckernel::p_setrwc::CLR_A, 0, broadcast_type, ckernel::ADDR_MOD_3, 0));
            }
            else
            {
                tmp.set_end_op(TT_OP_SETRWC(ckernel::p_setrwc::CLR_A, ckernel::p_setrwc::CR_AB, 0, 0, 0, ckernel::p_setrwc::SET_AB));
            }
            tmp.program(ckernel::instrn_buffer);
        }
    }
    else
    {
        if constexpr (eltwise_binary_type == ckernel::ELWADD)
        {
            ckernel::ckernel_template tmp(outerloop, innerloop, TT_OP_ELWADD(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(ckernel::p_setrwc::CLR_AB, ckernel::p_setrwc::CR_AB, 0, 0, 0, ckernel::p_setrwc::SET_AB));
            tmp.program(ckernel::instrn_buffer);
        }
        else if constexpr (eltwise_binary_type == ckernel::ELWSUB)
        {
            ckernel::ckernel_template tmp(outerloop, innerloop, TT_OP_ELWSUB(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(ckernel::p_setrwc::CLR_AB, ckernel::p_setrwc::CR_AB, 0, 0, 0, ckernel::p_setrwc::SET_AB));
            tmp.program(ckernel::instrn_buffer);
        }
        else if constexpr (eltwise_binary_type == ckernel::ELWMUL)
        {
            ckernel::ckernel_template tmp(high_fidelity ? NUM_FIDELITY_PHASES : outerloop, innerloop, TT_OP_ELWMUL(0, 0, broadcast_type, addr_mod, 0));
            if constexpr (high_fidelity)
            {
                tmp.set_last_inner_loop_instr(TT_OP_ELWMUL(0, 0, broadcast_type, ckernel::ADDR_MOD_2, 0)); // Incr fidelity last inst of inner loop
                tmp.set_last_outer_loop_instr(TT_OP_ELWMUL(ckernel::p_setrwc::CLR_AB, 0, broadcast_type, ckernel::ADDR_MOD_3, 0));
            }
            else
            {
                tmp.set_end_op(TT_OP_SETRWC(ckernel::p_setrwc::CLR_AB, ckernel::p_setrwc::CR_AB, 0, 0, 0, ckernel::p_setrwc::SET_AB));
            }
            tmp.program(ckernel::instrn_buffer);
        }
    }
}

template <
    ckernel::EltwiseBinaryType eltwise_binary_type,
    ckernel::BroadcastType src_b_bcast_type,
    int MATH_FIDELITY_DESC                                = 0,
    ckernel::EltwiseBinaryReuseDestType binary_reuse_dest = ckernel::EltwiseBinaryReuseDestType::NONE>
inline void _llk_math_eltwise_binary_init_(const std::uint32_t num_faces, const std::uint32_t transpose, const std::uint32_t acc_to_dest)
{
    constexpr int MATH_FIDELITY_PHASES    = ckernel::math::get_math_num_fidelity_phases(MATH_FIDELITY_DESC);
    constexpr int MATH_FIDELITY_INCREMENT = ckernel::math::get_math_fidelity_increment(MATH_FIDELITY_DESC);

    eltwise_binary_configure_addrmod<eltwise_binary_type, src_b_bcast_type, MATH_FIDELITY_INCREMENT>();

    if constexpr ((eltwise_binary_type == ckernel::ELWADD) || (eltwise_binary_type == ckernel::ELWSUB) || (eltwise_binary_type == ckernel::ELWMUL))
    {
        eltwise_binary_configure_mop<eltwise_binary_type, src_b_bcast_type, MATH_FIDELITY_PHASES, binary_reuse_dest>(acc_to_dest, num_faces);
    }
    else
    {
        FWASSERT("Unsupported op!", false);
    }

    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);

    ckernel::math::reset_counters(ckernel::p_setrwc::SET_ABD_F);
}
