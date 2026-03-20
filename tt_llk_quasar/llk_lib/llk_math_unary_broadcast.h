// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "llk_math_common.h"
using namespace ckernel;
using namespace ckernel::trisc;
using namespace ckernel::math;

/**
 * @brief Address modifiers for eltwise unary broadcast.
 * @tparam BROADCAST_TYPE COL, ROW, or SCALAR.
 */
template <BroadcastType BROADCAST_TYPE>
inline void _llk_math_eltwise_unary_broadcast_addrmod_()
{
    static_assert((BROADCAST_TYPE != BroadcastType::NONE), "Broadcast type cannot be NONE for this operation");

    constexpr std::uint8_t num_srb_rows_inc = (BROADCAST_TYPE == BroadcastType::COL) ? ELTWISE_MATH_ROWS : 0;

    addr_mod_t {
        .srcb = {.incr = num_srb_rows_inc},
        .dest = {.incr = ELTWISE_MATH_ROWS},
    }
        .set(ADDR_MOD_0);

    addr_mod_t {
        .srcb = {.clr = 1},
        .dest = {.incr = ELTWISE_MATH_ROWS},
    }
        .set(ADDR_MOD_1);

    if constexpr (BROADCAST_TYPE == BroadcastType::COL)
    {
        addr_mod_t {
            .srcb = {.clr = 1},
            .dest = {.incr = ELTWISE_MATH_ROWS},
        }
            .set(ADDR_MOD_2);
    }
    else if constexpr (BROADCAST_TYPE == BroadcastType::ROW)
    {
        addr_mod_t {
            .srcb = {.clr = 1},
            .dest = {.incr = ELTWISE_MATH_ROWS},
        }
            .set(ADDR_MOD_1);
        addr_mod_t {
            .srcb = {.incr = FACE_R_DIM},
            .dest = {.incr = ELTWISE_MATH_ROWS},
        }
            .set(ADDR_MOD_2);
    }
}

/**
 * @brief MOP config for eltwise unary broadcast B2D.
 * @tparam BROADCAST_TYPE COL, ROW, or SCALAR.
 * @tparam unpack_to_dest If true, unpacker writes to dest (32-bit path).
 */
template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false>
inline void _llk_math_eltwise_unary_broadcast_mop_config_(const TileShape& tile_shape)
{
    static_assert((BROADCAST_TYPE != BroadcastType::NONE), "Broadcast type cannot be NONE for this operation");

    if constexpr (BROADCAST_TYPE == BroadcastType::ROW && unpack_to_dest)
    {
        // ROW unpack_to_dest: replay buffer for alternating srcB reads (face 0/2 from srcB[0], face 1/3 from srcB[FACE_R_DIM]).
        constexpr std::uint32_t b2d_replay_offset = 1;
        constexpr std::uint32_t b2d_replay_len    = 8;

        load_replay_buf<b2d_replay_offset, b2d_replay_len>(
            []
            {
                // Face 0: broadcast srcb[0] to 16 dest rows
                TTI_MOVB2D(0, 0, ADDR_MOD_0, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
                TTI_MOVB2D(0, 0, ADDR_MOD_2, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
                // Face 1: broadcast srcb[FACE_R_DIM] to 16 dest rows
                TTI_MOVB2D(0, 0, ADDR_MOD_0, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
                TTI_MOVB2D(0, 0, ADDR_MOD_1, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
                // Face 2: broadcast srcb[0] to 16 dest rows
                TTI_MOVB2D(0, 0, ADDR_MOD_0, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
                TTI_MOVB2D(0, 0, ADDR_MOD_2, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
                // Face 3: broadcast srcb[FACE_R_DIM] to 16 dest rows
                TTI_MOVB2D(0, 0, ADDR_MOD_0, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
                TTI_MOVB2D(0, 0, ADDR_MOD_2, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
            });

        ckernel_template temp(1, 1, TT_OP_REPLAY(b2d_replay_offset, b2d_replay_len, 0, 0, 0, 0));
        temp.set_end_op(TT_OP_CLEARDVALID(p_cleardvalid::CLR_SRCB_VLD, 0, 0, 0, 0, 0));
        temp.program_bank0_sw_cntl(instrn_buffer);
    }
    else
    {
        const std::uint32_t num_rows_per_matrix =
            (BROADCAST_TYPE == BroadcastType::SCALAR) ? tile_shape.num_faces * tile_shape.face_r_dim : tile_shape.face_r_dim;
        const std::uint32_t num_dvalids_outer_loop = (BROADCAST_TYPE == BroadcastType::SCALAR) ? 1 : tile_shape.num_faces;

        const std::uint32_t num_rows_per_move_instrn = ELTWISE_MATH_ROWS;
        const std::uint32_t MOP_INNER_LOOP           = num_rows_per_matrix >> math_rows_log2(num_rows_per_move_instrn);
        const std::uint32_t mov_rows_instn           = p_mov_src_to_dest::MOV_8_ROWS;
        const std::uint32_t MOP_OUTER_LOOP           = num_dvalids_outer_loop;

        constexpr std::uint32_t bcast_datum0 = (BROADCAST_TYPE == BroadcastType::SCALAR || BROADCAST_TYPE == BroadcastType::COL) ? 1 : 0;
        constexpr std::uint32_t dst_addr     = (BROADCAST_TYPE == BroadcastType::SCALAR || BROADCAST_TYPE == BroadcastType::ROW) ? 1 : 0;

        const auto movb2d_func = [mov_rows_instn, bcast_datum0, dst_addr](std::uint8_t addr_mod)
        { return TT_OP_MOVB2D(0, 0, addr_mod, mov_rows_instn, bcast_datum0, dst_addr); };

        ckernel_template temp(MOP_OUTER_LOOP, MOP_INNER_LOOP, movb2d_func(ADDR_MOD_0));

        temp.set_end_op(TT_OP_CLEARDVALID(p_cleardvalid::CLR_SRCB_VLD, 0, 0, 0, 0, 0));

        if constexpr (BROADCAST_TYPE == BroadcastType::SCALAR)
        {
            temp.set_last_outer_loop_instr(movb2d_func(ADDR_MOD_1));
        }
        else if constexpr (BROADCAST_TYPE == BroadcastType::COL)
        {
            temp.set_last_inner_loop_instr(movb2d_func(ADDR_MOD_2));
        }
        else // ROW (non-unpack_to_dest)
        {
            temp.set_last_inner_loop_instr(movb2d_func(ADDR_MOD_0));
            temp.set_last_outer_loop_instr(movb2d_func(ADDR_MOD_2));
        }

        temp.program_bank0_sw_cntl(instrn_buffer);
    }
}

/**
 * @brief MOP config for dest→srcB (unpack_to_dest path). Copies dest to srcB for broadcast.
 */
template <BroadcastType BROADCAST_TYPE>
inline void _llk_math_eltwise_unary_broadcast_d2b_mop_config_(const TileShape& tile_shape)
{
    if constexpr (BROADCAST_TYPE == BroadcastType::ROW)
    {
        addr_mod_t {
            .srcb = {.incr = FACE_R_DIM},
            .dest = {.incr = FACE_R_DIM},
        }
            .set(ADDR_MOD_3);

        const auto movd2b_row_func = [](std::uint8_t addr_mod, std::uint32_t dest_32b_lo)
        { return TT_OP_MOVD2B(dest_32b_lo, 0, addr_mod, p_movd2b::MOV_1_ROW, 0, 0); };

        constexpr std::uint32_t replay_buf_len = 1;
        load_replay_buf<0, replay_buf_len>([] { TTI_MOVD2B(0, 0, ADDR_MOD_3, p_movd2b::MOV_1_ROW, 0, 0); });

        ckernel_template temp(1, 1, TT_OP_REPLAY(0, replay_buf_len, 0, 0, 0, 0), movd2b_row_func(ADDR_MOD_3, 0));
        temp.set_last_inner_loop_instr(movd2b_row_func(ADDR_MOD_3, 1));

        temp.program_bank1_sw_cntl(instrn_buffer);
    }
    else
    {
        const std::uint32_t num_rows_per_move_instrn = ELTWISE_MATH_ROWS;
        const std::uint32_t mov_rows_instn           = p_mov_src_to_dest::MOV_8_ROWS;

        std::uint32_t MOP_INNER_LOOP_VAL;
        std::uint32_t MOP_OUTER_LOOP_VAL;

        if constexpr (BROADCAST_TYPE == BroadcastType::SCALAR)
        {
            MOP_INNER_LOOP_VAL = 1;
            MOP_OUTER_LOOP_VAL = 1;
        }
        else // COL
        {
            const std::uint32_t num_rows_to_transfer = tile_shape.face_r_dim * 2;
            MOP_INNER_LOOP_VAL                       = num_rows_to_transfer >> math_rows_log2(num_rows_per_move_instrn);
            MOP_OUTER_LOOP_VAL                       = 1;
        }

        addr_mod_t {
            .srcb = {.incr = ELTWISE_MATH_ROWS},
            .dest = {.incr = ELTWISE_MATH_ROWS},
        }
            .set(ADDR_MOD_3);

        const auto movd2b_func = [mov_rows_instn](std::uint8_t addr_mod, std::uint32_t dest_32b_lo)
        { return TT_OP_MOVD2B(dest_32b_lo, 0, addr_mod, mov_rows_instn, 0, 0); };

        constexpr std::uint32_t replay_buf_len = 1;
        load_replay_buf<0, replay_buf_len>([movd2b_func] { TTI_MOVD2B(0, 0, ADDR_MOD_3, p_mov_src_to_dest::MOV_8_ROWS, 0, 0); });

        ckernel_template temp(MOP_OUTER_LOOP_VAL, MOP_INNER_LOOP_VAL * 2, TT_OP_REPLAY(0, replay_buf_len, 0, 0, 0, 0), movd2b_func(ADDR_MOD_3, 0));
        temp.set_last_inner_loop_instr(movd2b_func(ADDR_MOD_3, 1));

        temp.program_bank1_sw_cntl(instrn_buffer);
    }
}

/**
 * @brief Init for eltwise unary broadcast.
 * @tparam BROADCAST_TYPE COL, ROW, or SCALAR.
 * @tparam unpack_to_dest Unpacker writes to dest when true (32-bit path).
 * @tparam is_fp32_dest_acc_en Dest accumulation in float32.
 */
template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false, bool is_fp32_dest_acc_en = false>
inline void _llk_math_eltwise_unary_broadcast_init_(const TileShape& tile_shape)
{
    _llk_math_eltwise_unary_broadcast_addrmod_<BROADCAST_TYPE>();
    if constexpr (unpack_to_dest)
    {
        _llk_math_eltwise_unary_broadcast_d2b_mop_config_<BROADCAST_TYPE>(tile_shape);
    }
    _llk_math_eltwise_unary_broadcast_mop_config_<BROADCAST_TYPE, unpack_to_dest>(tile_shape);
    _reset_counters_<p_setrwc::SET_ABD_F>();
}

/**
 * @brief Performs one-tile eltwise unary broadcast.
 * @tparam BROADCAST_TYPE COL, ROW, or SCALAR.
 * @tparam unpack_to_dest Unpacker writes to dest when true.
 * @tparam is_fp32_dest_acc_en Dest accumulation in float32.
 * @param tile_idx Destination tile index.
 * @param tile_shape Tile shape (unused in thin path).
 */
template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false, bool is_fp32_dest_acc_en = false>
inline void _llk_math_eltwise_unary_broadcast_(const std::uint32_t tile_idx, const TileShape& /*tile_shape*/)
{
    if constexpr (unpack_to_dest)
    {
        ckernel::ckernel_template::run_bank1_sw_cntl(instrn_buffer);
        _reset_counters_<p_setrwc::SET_ABD_F>();
    }

    _set_dst_write_addr_<DstTileShape::Tile32x32>(tile_idx);
    ckernel::ckernel_template::run_bank0_sw_cntl(instrn_buffer);
    _reset_counters_<p_setrwc::SET_ABD_F>();
}
