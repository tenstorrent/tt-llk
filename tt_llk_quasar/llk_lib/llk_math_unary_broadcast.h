// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_math_common.h"
using namespace ckernel;
using namespace ckernel::trisc;
using namespace ckernel::math;

/**
 * @brief Sets up addrmods for elementwise unary broadcast operations
 */
template <BroadcastType BROADCAST_TYPE>
inline void _llk_math_eltwise_unary_broadcast_addrmod_()
{
    static_assert((BROADCAST_TYPE != BroadcastType::NONE), "Broadcast type cannot be NONE for this operation");

    constexpr uint8_t num_srb_rows_inc = 0;

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
}

/**
 * @brief Sets up mop config for elementwise unary broadcast operations
 */
template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false>
inline void _llk_math_eltwise_unary_broadcast_mop_config_(const uint32_t num_rows_inner_loop, const uint32_t num_dvalids_outer_loop)
{
    static_assert((BROADCAST_TYPE != BroadcastType::NONE), "Broadcast type cannot be NONE for this operation");

    if constexpr (unpack_to_dest)
    {
        // Workaround: movA2D doesn't support broadcast, so dest→srcB→dest via MOVD2B then MOVB2D
        _llk_math_eltwise_unary_broadcast_addrmod_<BROADCAST_TYPE>();
    }

    const uint32_t num_rows_per_move_instrn = ELTWISE_MATH_ROWS;
    const uint32_t MOP_INNER_LOOP           = num_rows_inner_loop >> math_rows_log2(num_rows_per_move_instrn);
    const uint32_t mov_rows_instn           = p_mov_src_to_dest::MOV_8_ROWS;
    const uint32_t MOP_OUTER_LOOP           = num_dvalids_outer_loop;

    constexpr uint32_t bcast_datum0 = (BROADCAST_TYPE == BroadcastType::SCALAR || BROADCAST_TYPE == BroadcastType::COL) ? 1 : 0;
    constexpr uint32_t dst_addr     = (BROADCAST_TYPE == BroadcastType::SCALAR || BROADCAST_TYPE == BroadcastType::ROW) ? 1 : 0;

    const auto movb2d_func = [mov_rows_instn, bcast_datum0, dst_addr](uint8_t addr_mod)
    { return TT_OP_MOVB2D(0, 0, addr_mod, mov_rows_instn, bcast_datum0, dst_addr); };

    ckernel_template temp(MOP_OUTER_LOOP, MOP_INNER_LOOP, movb2d_func(ADDR_MOD_0));

    temp.set_end_op(TT_OP_CLEARDVALID(p_cleardvalid::CLR_SRCB_VLD, 0, 0, 0, 0, 0));

    if constexpr (BROADCAST_TYPE != BroadcastType::SCALAR)
    {
        constexpr uint ADDR_MOD = (BROADCAST_TYPE == BroadcastType::COL) ? ADDR_MOD_2 : ADDR_MOD_0;
        temp.set_last_inner_loop_instr(movb2d_func(ADDR_MOD));
    }
    else
    {
        temp.set_last_inner_loop_instr(movb2d_func(ADDR_MOD_1));
    }

    temp.program_bank0_sw_cntl(instrn_buffer);
}

/**
 * @brief Sets up mop config for dest→srcB phase (workaround for unpack_to_dest)
 */
template <BroadcastType BROADCAST_TYPE>
inline void _llk_math_eltwise_unary_broadcast_d2b_mop_config_(const uint32_t num_rows_inner_loop, const uint32_t num_dvalids_outer_loop)
{
    const uint32_t num_rows_per_move_instrn = ELTWISE_MATH_ROWS;
    const uint32_t mov_rows_instn           = p_mov_src_to_dest::MOV_8_ROWS;

    uint32_t MOP_INNER_LOOP_VAL;
    uint32_t MOP_OUTER_LOOP_VAL;

    if constexpr (BROADCAST_TYPE == BroadcastType::SCALAR)
    {
        MOP_INNER_LOOP_VAL = 1;
        MOP_OUTER_LOOP_VAL = 1;
    }
    else if constexpr (BROADCAST_TYPE == BroadcastType::ROW)
    {
        MOP_INNER_LOOP_VAL = 1;
        MOP_OUTER_LOOP_VAL = 3;
    }
    else // COL
    {
        MOP_INNER_LOOP_VAL = 1;
        MOP_OUTER_LOOP_VAL = 6;
    }
    addr_mod_t {
        .srcb = {.incr = ELTWISE_MATH_ROWS},
        .dest = {.incr = ELTWISE_MATH_ROWS},
    }
        .set(ADDR_MOD_0);

    const auto movd2b_func = [mov_rows_instn](uint8_t addr_mod, uint32_t dest_32b_lo) { return TT_OP_MOVD2B(dest_32b_lo, 0, addr_mod, mov_rows_instn, 0, 0); };

    constexpr uint32_t replay_buf_len = 1;
    load_replay_buf<0, replay_buf_len>([movd2b_func] { TTI_MOVD2B(0, 0, ADDR_MOD_0, p_mov_src_to_dest::MOV_8_ROWS, 0, 0); });

    ckernel_template temp(MOP_OUTER_LOOP_VAL, MOP_INNER_LOOP_VAL * 2, TT_OP_REPLAY(0, replay_buf_len, 0, 0, 0, 0), movd2b_func(ADDR_MOD_0, 0));

    temp.set_last_inner_loop_instr(movd2b_func(ADDR_MOD_0, 1));

    temp.program_bank0_sw_cntl(instrn_buffer);
}

/**
 * @brief Sets up initialization for elementwise unary broadcast operation
 */
template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false>
inline void _llk_math_eltwise_unary_broadcast_init_(const uint num_rows_per_matrix, const uint num_matrices)
{
    if constexpr (unpack_to_dest)
    {
        _llk_math_eltwise_unary_broadcast_d2b_mop_config_<BROADCAST_TYPE>(num_rows_per_matrix, num_matrices);
        _llk_math_eltwise_unary_broadcast_addrmod_<BROADCAST_TYPE>();
        _llk_math_eltwise_unary_broadcast_mop_config_<BROADCAST_TYPE, unpack_to_dest>(num_rows_per_matrix, num_matrices);
    }
    else
    {
        _llk_math_eltwise_unary_broadcast_addrmod_<BROADCAST_TYPE>();
        _llk_math_eltwise_unary_broadcast_mop_config_<BROADCAST_TYPE, unpack_to_dest>(num_rows_per_matrix, num_matrices);
    }
    _reset_counters_<p_setrwc::SET_ABD_F>();
}

/**
 * @brief Perform an elementwise unary broadcast operation
 */
template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false>
inline void _llk_math_eltwise_unary_broadcast_(const uint32_t num_rows_per_tile, const uint32_t tile_idx)
{
    if constexpr (unpack_to_dest)
    {
        ckernel::ckernel_template::run_bank0_sw_cntl(instrn_buffer);
        _reset_counters_<p_setrwc::SET_ABD_F>();
    }

    _set_dst_write_addr_by_rows_(num_rows_per_tile, tile_idx);
    ckernel::ckernel_template::run_bank0_sw_cntl(instrn_buffer);

    _reset_counters_<p_setrwc::SET_ABD_F>();
}
