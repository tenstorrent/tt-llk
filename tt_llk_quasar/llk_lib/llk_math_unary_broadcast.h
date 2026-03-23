// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "llk_math_common.h"
using namespace ckernel;
using namespace ckernel::trisc;
using namespace ckernel::math;

template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false>
inline void _llk_math_eltwise_unary_broadcast_addrmod_(const TileShape& tile_shape)
{
    static_assert(BROADCAST_TYPE != BroadcastType::NONE, "Broadcast type cannot be NONE");

    constexpr std::uint16_t row_step = 8;
    const std::uint8_t srcb_col_inc  = (BROADCAST_TYPE == BroadcastType::COL) ? static_cast<std::uint8_t>(8) : static_cast<std::uint8_t>(0);

    addr_mod_t {.srcb = {.incr = srcb_col_inc}, .dest = {.incr = row_step}}.set(ADDR_MOD_0);
    addr_mod_t {.srcb = {.clr = 1}, .dest = {.incr = row_step}}.set(ADDR_MOD_1);

    if constexpr (BROADCAST_TYPE == BroadcastType::COL)
    {
        addr_mod_t {.srcb = {.clr = 1}, .dest = {.incr = row_step}}.set(ADDR_MOD_2);
    }
    else if constexpr (BROADCAST_TYPE == BroadcastType::ROW)
    {
        addr_mod_t {.srcb = {.clr = 1}, .dest = {.incr = row_step}}.set(ADDR_MOD_1);
        addr_mod_t {
            .srcb = {.incr = static_cast<std::uint8_t>(tile_shape.face_r_dim)},
            .dest = {.incr = row_step},
        }
            .set(ADDR_MOD_2);
    }

    if constexpr (unpack_to_dest)
    {
        if constexpr (BROADCAST_TYPE == BroadcastType::ROW)
        {
            addr_mod_t {
                .srcb = {.incr = static_cast<std::uint8_t>(tile_shape.face_r_dim)},
                .dest = {.incr = static_cast<std::uint16_t>(tile_shape.face_r_dim)},
            }
                .set(ADDR_MOD_3);
        }
        else
        {
            addr_mod_t {.srcb = {.incr = static_cast<std::uint8_t>(8)}, .dest = {.incr = static_cast<std::uint16_t>(8)}}.set(ADDR_MOD_3);
        }
    }
}

template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false>
inline void _llk_math_eltwise_unary_broadcast_mop_config_(const TileShape& tile_shape)
{
    static_assert(BROADCAST_TYPE != BroadcastType::NONE, "Broadcast type cannot be NONE");

    if constexpr (BROADCAST_TYPE == BroadcastType::ROW && unpack_to_dest)
    {
        constexpr std::uint32_t replay_buf_start = 1;
        constexpr std::uint32_t replay_buf_len   = 8;
        load_replay_buf<replay_buf_start, replay_buf_len>(
            []
            {
                TTI_MOVB2D(0, 0, ADDR_MOD_0, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
                TTI_MOVB2D(0, 0, ADDR_MOD_2, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
                TTI_MOVB2D(0, 0, ADDR_MOD_0, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
                TTI_MOVB2D(0, 0, ADDR_MOD_1, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
                TTI_MOVB2D(0, 0, ADDR_MOD_0, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
                TTI_MOVB2D(0, 0, ADDR_MOD_2, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
                TTI_MOVB2D(0, 0, ADDR_MOD_0, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
                TTI_MOVB2D(0, 0, ADDR_MOD_2, p_mov_src_to_dest::MOV_8_ROWS, 0, 1);
            });

        ckernel_template temp(1, 1, TT_OP_REPLAY(replay_buf_start, replay_buf_len, 0, 0, 0, 0));
        temp.set_end_op(TT_OP_CLEARDVALID(p_cleardvalid::CLR_SRCB_VLD, 0, 0, 0, 0, 0));
        temp.program_bank0_sw_cntl(instrn_buffer);
    }
    else
    {
        const std::uint32_t num_rows = (BROADCAST_TYPE == BroadcastType::SCALAR) ? tile_shape.num_faces * tile_shape.face_r_dim : tile_shape.face_r_dim;
        const std::uint32_t outer    = (BROADCAST_TYPE == BroadcastType::SCALAR) ? 1U : static_cast<std::uint32_t>(tile_shape.num_faces);
        const std::uint32_t inner    = num_rows >> 3;

        const std::uint32_t dst_lo     = (BROADCAST_TYPE != BroadcastType::COL) ? 1U : 0U;
        constexpr std::uint32_t bcast0 = (BROADCAST_TYPE == BroadcastType::COL || BROADCAST_TYPE == BroadcastType::SCALAR) ? 1U : 0U;

        const auto movb2d = [bcast0, dst_lo](std::uint8_t am) { return TT_OP_MOVB2D(0, 0, am, p_mov_src_to_dest::MOV_8_ROWS, bcast0, dst_lo); };

        ckernel_template temp(outer, inner, movb2d(ADDR_MOD_0));
        temp.set_end_op(TT_OP_CLEARDVALID(p_cleardvalid::CLR_SRCB_VLD, 0, 0, 0, 0, 0));

        if constexpr (BROADCAST_TYPE == BroadcastType::SCALAR)
        {
            temp.set_last_outer_loop_instr(movb2d(ADDR_MOD_1));
        }
        else if constexpr (BROADCAST_TYPE == BroadcastType::COL)
        {
            temp.set_last_inner_loop_instr(movb2d(ADDR_MOD_2));
        }
        else
        {
            temp.set_last_inner_loop_instr(movb2d(ADDR_MOD_0));
            temp.set_last_outer_loop_instr(movb2d(ADDR_MOD_2));
        }

        temp.program_bank0_sw_cntl(instrn_buffer);
    }
}

template <BroadcastType BROADCAST_TYPE>
inline void _llk_math_eltwise_unary_broadcast_d2b_mop_config_(const TileShape& tile_shape)
{
    if constexpr (BROADCAST_TYPE == BroadcastType::ROW)
    {
        const auto f = [](std::uint8_t am, std::uint32_t d32) { return TT_OP_MOVD2B(d32, 0, am, p_movd2b::MOV_1_ROW, 0, 0); };

        constexpr std::uint32_t replay_buf_len = 1;
        load_replay_buf<0, replay_buf_len>([] { TTI_MOVD2B(0, 0, ADDR_MOD_3, p_movd2b::MOV_1_ROW, 0, 0); });

        ckernel_template temp(1, 1, TT_OP_REPLAY(0, replay_buf_len, 0, 0, 0, 0), f(ADDR_MOD_3, 0));
        temp.set_last_inner_loop_instr(f(ADDR_MOD_3, 1));
        temp.program_bank0_sw_cntl(instrn_buffer);
    }
    else
    {
        const std::uint32_t rows_sel  = (BROADCAST_TYPE == BroadcastType::SCALAR) ? tile_shape.num_faces * tile_shape.face_r_dim : tile_shape.face_r_dim * 2;
        const std::uint32_t inner_d2b = (BROADCAST_TYPE == BroadcastType::SCALAR) ? 1U : (rows_sel >> 3);

        const auto f = [](std::uint8_t am, std::uint32_t d32) { return TT_OP_MOVD2B(d32, 0, am, p_mov_src_to_dest::MOV_8_ROWS, 0, 0); };

        constexpr std::uint32_t replay_buf_len = 1;
        load_replay_buf<0, replay_buf_len>([] { TTI_MOVD2B(0, 0, ADDR_MOD_3, p_mov_src_to_dest::MOV_8_ROWS, 0, 0); });

        ckernel_template temp(1, inner_d2b * 2, TT_OP_REPLAY(0, replay_buf_len, 0, 0, 0, 0), f(ADDR_MOD_3, 0));
        temp.set_last_inner_loop_instr(f(ADDR_MOD_3, 1));
        temp.program_bank0_sw_cntl(instrn_buffer);
    }
}

template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false, bool is_fp32_dest_acc_en = false>
inline void _llk_math_eltwise_unary_broadcast_init_(const TileShape& tile_shape)
{
    (void)is_fp32_dest_acc_en;
    _llk_math_eltwise_unary_broadcast_addrmod_<BROADCAST_TYPE, unpack_to_dest>(tile_shape);
    if constexpr (!unpack_to_dest)
    {
        _llk_math_eltwise_unary_broadcast_mop_config_<BROADCAST_TYPE, false>(tile_shape);
    }
    _reset_counters_<p_setrwc::SET_ABD_F>();
}

template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false, bool is_fp32_dest_acc_en = false>
inline void _llk_math_eltwise_unary_broadcast_(const std::uint32_t tile_idx, const TileShape& tile_shape)
{
    (void)is_fp32_dest_acc_en;
    _set_dst_write_addr_<DstTileShape::Tile32x32>(tile_idx);

    if constexpr (unpack_to_dest)
    {
        _llk_math_eltwise_unary_broadcast_d2b_mop_config_<BROADCAST_TYPE>(tile_shape);
        ckernel::ckernel_template::run_bank0_sw_cntl(instrn_buffer);
        _reset_counters_<p_setrwc::SET_ABD_F>();
        _llk_math_eltwise_unary_broadcast_mop_config_<BROADCAST_TYPE, true>(tile_shape);
    }
    else
    {
        (void)tile_shape;
    }

    ckernel::ckernel_template::run_bank0_sw_cntl(instrn_buffer);
    _reset_counters_<p_setrwc::SET_ABD_F>();
}
