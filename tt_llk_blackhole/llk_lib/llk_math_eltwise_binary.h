// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_assert.h"
#include "llk_math_common.h"

using namespace ckernel;

// local function declarations
inline void eltwise_binary_configure_addrmod();

template <EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void eltwise_binary_reuse_dest_as_src()
{
    if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCA)
    {
        move_d2a_fixed_face(ADDR_MOD_1);
    }
    else if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCB)
    {
        move_d2b_fixed_face(ADDR_MOD_1);
    }
}

// Helper macros to generate compile-time constant MOV D2A instructions
#define MOVD2A_4_ROWS(base_offset, row_offset) \
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + (row_offset) * 4, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, (base_offset) + (row_offset) * 4)

#define MOVD2A_16_ROWS(base_offset) \
    MOVD2A_4_ROWS(base_offset, 0);  \
    MOVD2A_4_ROWS(base_offset, 1);  \
    MOVD2A_4_ROWS(base_offset, 2);  \
    MOVD2A_4_ROWS(base_offset, 3);  \
    MOVD2A_4_ROWS(base_offset, 4);  \
    MOVD2A_4_ROWS(base_offset, 5);  \
    MOVD2A_4_ROWS(base_offset, 6);  \
    MOVD2A_4_ROWS(base_offset, 7);  \
    MOVD2A_4_ROWS(base_offset, 8);  \
    MOVD2A_4_ROWS(base_offset, 9);  \
    MOVD2A_4_ROWS(base_offset, 10); \
    MOVD2A_4_ROWS(base_offset, 11); \
    MOVD2A_4_ROWS(base_offset, 12); \
    MOVD2A_4_ROWS(base_offset, 13); \
    MOVD2A_4_ROWS(base_offset, 14); \
    MOVD2A_4_ROWS(base_offset, 15)

template <EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void eltwise_binary_reuse_dest_as_src_tile(uint32_t idst = 0)
{
    if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCA)
    {
        // Reset destination target register to base address before moving data from destination to source A
        if (idst == 0)
        {
            TT_SETC16(DEST_TARGET_REG_CFG_MATH_Offset_ADDR32, 0);
            TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);
        }

        // Use switch to handle different idst values with compile-time constants
        switch (idst)
        {
            case 0:
                MOVD2A_16_ROWS(0);
                break;
            case 1:
                MOVD2A_16_ROWS(64);
                break;
            case 2:
                MOVD2A_16_ROWS(128);
                break;
            case 3:
                MOVD2A_16_ROWS(192);
                break;
            case 4:
                MOVD2A_16_ROWS(256);
                break;
            case 5:
                MOVD2A_16_ROWS(320);
                break;
            case 6:
                MOVD2A_16_ROWS(384);
                break;
            case 7:
                MOVD2A_16_ROWS(448);
                break;
            default:
                break;
        }
    }
    else if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCB)
    {
        // Explicitly reset D (dest read) and B (srcB write) counters to 0
        TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);
        TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_B);

        // Use compile-time constants for all 16 MOVD2B calls
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 0, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 0);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 4, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 4);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 8, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 8);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 12, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 12);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 16, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 16);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 20, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 20);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 24, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 24);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 28, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 28);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 32, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 32);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 36, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 36);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 40, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 40);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 44, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 44);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 48, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 48);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 52, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 52);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 56, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 56);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 60, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 60);
    }
}

template <
    EltwiseBinaryType eltwise_binary_type,
    BroadcastType src_b_bcast_type,
    DstSync Dst,
    bool is_fp32_dest_acc_en,
    int NUM_FIDELITY_PHASES                      = 0,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void _llk_math_eltwise_binary_(const std::uint32_t num_faces, uint dst_index, const bool clear_fp32_dst_acc)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    constexpr bool high_fidelity     = (NUM_FIDELITY_PHASES > 0);
    constexpr uint32_t ZERO_ACC_MODE = p_zeroacc::CLR_16;

    math::set_dst_write_addr<DstTileShape::Tile32x32, UnpackDestination::SrcRegs>(dst_index);

    if constexpr ((eltwise_binary_type == ELWADD) || (eltwise_binary_type == ELWSUB))
    {
        if constexpr (src_b_bcast_type == BroadcastType::COL)
        {
            // Mop for col broadcast only does 2 outerloops.  Needs to clear B manually and call twice for full tile size
            constexpr uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? 2 : 1;
#pragma GCC unroll 0
            for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
            {
                eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                ckernel_template::run();
            }
            TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            if (num_faces == 4)
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    ckernel_template::run();
                }
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            }
        }
        else
        {
            const uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? num_faces : 1;
#pragma GCC unroll 0
            for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
            {
                eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                ckernel_template::run();
            }
            // Manually clear B once mop is done for scaler bcast
            if constexpr (src_b_bcast_type == BroadcastType::SCALAR)
            {
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_D);
            }
        }
    }
    else if constexpr (eltwise_binary_type == ELWMUL)
    {
        if constexpr (src_b_bcast_type == BroadcastType::COL)
        {
            // Mop for col broadcast only does 2 outerloops.  Needs to clear B manually and call twice for full tile size
            constexpr uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? 2 : 1;
            if constexpr (high_fidelity)
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < 2; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                    {
                        // We clear the DEST face-by-face, given the DEST base, tile index and face index
                        int clear_fp32   = is_fp32_dest_acc_en && clear_fp32_dst_acc ? 1 : 0;
                        auto buffer_base = is_fp32_dest_acc_en && clear_fp32_dst_acc ? get_dest_buffer_base_32b() : get_dest_buffer_base_16b();
                        TT_ZEROACC(
                            ZERO_ACC_MODE, clear_fp32, 0, ADDR_MOD_1, (buffer_base + get_dest_index_in_faces(dst_index, (0 + face_num)))); // Clear faces 0 & 1
                    }
                    ckernel_template::run();
                }
            }
            else
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                    {
                        // We clear the DEST face-by-face, given the DEST base, tile index and face index
                        int clear_fp32   = is_fp32_dest_acc_en && clear_fp32_dst_acc ? 1 : 0;
                        auto buffer_base = is_fp32_dest_acc_en && clear_fp32_dst_acc ? get_dest_buffer_base_32b() : get_dest_buffer_base_16b();
                        TT_ZEROACC(
                            ZERO_ACC_MODE, clear_fp32, 0, ADDR_MOD_1, (buffer_base + get_dest_index_in_faces(dst_index, (0 + face_num)))); // Clear faces 0 & 1
                    }
                    ckernel_template::run();
                }
            }
            TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            if (num_faces == 4)
            {
                if constexpr (high_fidelity)
                {
#pragma GCC unroll 0
                    for (std::uint32_t face_num = 0; face_num < 2; face_num++)
                    {
                        eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                        if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                        {
                            // We clear the DEST face-by-face, given the DEST base, tile index and face index
                            int clear_fp32   = is_fp32_dest_acc_en && clear_fp32_dst_acc ? 1 : 0;
                            auto buffer_base = is_fp32_dest_acc_en && clear_fp32_dst_acc ? get_dest_buffer_base_32b() : get_dest_buffer_base_16b();
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                clear_fp32,
                                0,
                                ADDR_MOD_1,
                                (buffer_base + get_dest_index_in_faces(dst_index, (2 + face_num)))); // Clear faces 2 & 3
                        }
                        ckernel_template::run();
                    }
                }
                else
                {
#pragma GCC unroll 0
                    for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
                    {
                        eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                        if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                        {
                            // We clear the DEST face-by-face, given the DEST base, tile index and face index
                            int clear_fp32   = is_fp32_dest_acc_en && clear_fp32_dst_acc ? 1 : 0;
                            auto buffer_base = is_fp32_dest_acc_en && clear_fp32_dst_acc ? get_dest_buffer_base_32b() : get_dest_buffer_base_16b();
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                clear_fp32,
                                0,
                                ADDR_MOD_1,
                                (buffer_base + get_dest_index_in_faces(dst_index, (2 + face_num)))); // Clear faces 2 & 3
                        }
                        ckernel_template::run();
                    }
                }
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            }
        }
        else
        {
            // Row and no broadcasted behaves similarly
            const uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? num_faces : 1;
            if constexpr (high_fidelity)
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < num_faces; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                    {
                        // We clear the DEST face-by-face, given the DEST base, tile index and face index
                        int clear_fp32   = is_fp32_dest_acc_en && clear_fp32_dst_acc ? 1 : 0;
                        auto buffer_base = is_fp32_dest_acc_en && clear_fp32_dst_acc ? get_dest_buffer_base_32b() : get_dest_buffer_base_16b();
                        TT_ZEROACC(ZERO_ACC_MODE, clear_fp32, 0, ADDR_MOD_1, (buffer_base + get_dest_index_in_faces(dst_index, face_num)));
                    }
                    ckernel_template::run();
                }
            }
            else
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                    {
                        // We clear the DEST face-by-face, given the DEST base, tile index and face index
                        int clear_fp32   = is_fp32_dest_acc_en && clear_fp32_dst_acc ? 1 : 0;
                        auto buffer_base = is_fp32_dest_acc_en && clear_fp32_dst_acc ? get_dest_buffer_base_32b() : get_dest_buffer_base_16b();
                        TT_ZEROACC(ZERO_ACC_MODE, clear_fp32, 0, ADDR_MOD_1, (buffer_base + get_dest_index_in_faces(dst_index, face_num)));
                    }
                    ckernel_template::run();
                }
            }
            if constexpr (src_b_bcast_type == BroadcastType::SCALAR)
            {
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_D);
            }
        }
    }
    math::clear_dst_reg_addr();
}

template <EltwiseBinaryType eltwise_binary_type, BroadcastType bcast_type, std::uint32_t FIDELITY_INCREMENT>
inline void eltwise_binary_configure_addrmod()
{
    // Use srcA for data movement
    if constexpr ((eltwise_binary_type == ELWADD) || (eltwise_binary_type == ELWSUB) || (eltwise_binary_type == ELWMUL))
    {
        if constexpr (bcast_type == BroadcastType::NONE || bcast_type == BroadcastType::COL)
        {
            addr_mod_t {
                .srca = {.incr = 8},
                .srcb = {.incr = 8},
                .dest = {.incr = 8},
            }
                .set(ADDR_MOD_0);
        }
        else if constexpr (bcast_type == BroadcastType::ROW || bcast_type == BroadcastType::SCALAR)
        {
            addr_mod_t {
                .srca = {.incr = 8},
                .srcb = {.incr = 0},
                .dest = {.incr = 8},
            }
                .set(ADDR_MOD_0);
        }
        addr_mod_t {
            .srca = {.incr = 0},
            .srcb = {.incr = 0},
            .dest = {.incr = 0},
        }
            .set(ADDR_MOD_1);

        addr_mod_t {
            .srca = {.incr = 0, .clr = 1}, .srcb = {.incr = 0, .clr = 1}, .dest = {.incr = 0, .clr = 0, .cr = 1}, .fidelity = {.incr = FIDELITY_INCREMENT}}
            .set(ADDR_MOD_2);

        addr_mod_t {
            .srca     = {.incr = 0, .clr = 1},
            .srcb     = {.incr = 0, .clr = 1},
            .dest     = {.incr = 8, .clr = 0, .cr = 0, .c_to_cr = 1},
            .fidelity = {.incr = 0, .clr = 1}}
            .set(ADDR_MOD_3);
    }
}

template <
    EltwiseBinaryType eltwise_binary_type,
    BroadcastType bcast_type,
    int NUM_FIDELITY_PHASES                      = 0,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void eltwise_binary_configure_mop(const std::uint32_t acc_to_dest = 0, const std::uint32_t num_faces = 4)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    constexpr bool high_fidelity = (NUM_FIDELITY_PHASES > 0);
    const uint addr_mod          = ADDR_MOD_0;
    constexpr uint innerloop     = 16 >> 3; // 8 rows per eltwise op at a time.
    uint outerloop               = num_faces;
    auto broadcast_type          = p_elwise::SRCB_NO_BCAST;
    if constexpr (bcast_type == BroadcastType::COL)
    {
        // The mop only runs for 2 outer loops and mop is called twice for col broadcast
        outerloop      = 2;
        broadcast_type = p_elwise::SRCB_BCAST_COL;
    }
    else if constexpr (bcast_type == BroadcastType::ROW)
    {
        broadcast_type = p_elwise::SRCB_BCAST_ROW;
    }
    else if constexpr (bcast_type == BroadcastType::SCALAR)
    {
        broadcast_type = p_elwise::SRCB_BCAST_ALL;
    }

    if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
    {
        outerloop = 1;
    }

    // Scalar and Col broadcast should not Clear B within a mop.  This is controlled outside of MOP.
    if constexpr (bcast_type == BroadcastType::COL || bcast_type == BroadcastType::SCALAR)
    {
        if constexpr (eltwise_binary_type == ELWADD)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWADD(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_A, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            tmp.program();
        }
        else if constexpr (eltwise_binary_type == ELWSUB)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWSUB(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_A, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            tmp.program();
        }
        else if constexpr (eltwise_binary_type == ELWMUL)
        {
            ckernel_template tmp(high_fidelity ? NUM_FIDELITY_PHASES : outerloop, innerloop, TT_OP_ELWMUL(0, 0, broadcast_type, addr_mod, 0));
            if constexpr (high_fidelity)
            {
                tmp.set_last_inner_loop_instr(TT_OP_ELWMUL(0, 0, broadcast_type, ADDR_MOD_2, 0)); // Incr fidelity last inst of inner loop
                tmp.set_last_outer_loop_instr(TT_OP_ELWMUL(p_setrwc::CLR_A, 0, broadcast_type, ADDR_MOD_3, 0));
            }
            else
            {
                tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_A, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            }
            tmp.program();
        }
    }
    else
    {
        if constexpr (eltwise_binary_type == ELWADD)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWADD(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_AB, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            tmp.program();
        }
        else if constexpr (eltwise_binary_type == ELWSUB)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWSUB(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_AB, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            tmp.program();
        }
        else if constexpr (eltwise_binary_type == ELWMUL)
        {
            ckernel_template tmp(high_fidelity ? NUM_FIDELITY_PHASES : outerloop, innerloop, TT_OP_ELWMUL(0, 0, broadcast_type, addr_mod, 0));
            if constexpr (high_fidelity)
            {
                tmp.set_last_inner_loop_instr(TT_OP_ELWMUL(0, 0, broadcast_type, ADDR_MOD_2, 0)); // Incr fidelity last inst of inner loop
                tmp.set_last_outer_loop_instr(TT_OP_ELWMUL(p_setrwc::CLR_AB, 0, broadcast_type, ADDR_MOD_3, 0));
            }
            else
            {
                tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_AB, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            }
            tmp.program();
        }
    }
}

template <
    EltwiseBinaryType eltwise_binary_type,
    BroadcastType src_b_bcast_type,
    int MATH_FIDELITY_DESC                       = 0,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void _llk_math_eltwise_binary_init_(const std::uint32_t num_faces, const std::uint32_t acc_to_dest)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    constexpr int MATH_FIDELITY_PHASES    = get_math_num_fidelity_phases(MATH_FIDELITY_DESC);
    constexpr int MATH_FIDELITY_INCREMENT = get_math_fidelity_increment(MATH_FIDELITY_DESC);

    eltwise_binary_configure_addrmod<eltwise_binary_type, src_b_bcast_type, MATH_FIDELITY_INCREMENT>();

    if constexpr ((eltwise_binary_type == ELWADD) || (eltwise_binary_type == ELWSUB) || (eltwise_binary_type == ELWMUL))
    {
        eltwise_binary_configure_mop<eltwise_binary_type, src_b_bcast_type, MATH_FIDELITY_PHASES, binary_reuse_dest>(acc_to_dest, num_faces);
    }

    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);

    math::reset_counters(p_setrwc::SET_ABD_F);
}

inline void _llk_math_eltwise_binary_uninit_()
{
    // No state to restore - all states are transient or default
}

// Cleanup function for fused eltwise binary operations
inline void _fused_eltwise_binary_uninit_()
{
    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_BD);
}
