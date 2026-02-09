// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_math_common.h"
using namespace ckernel;
using namespace ckernel::trisc;
using namespace ckernel::math;

/**
 * @brief Sets up address modifiers for elementwise unary broadcast operations
 * @tparam BROADCAST_TYPE: Sets the broadcast type, values = [COL, ROW, SCALAR]
 */
template <BroadcastType BROADCAST_TYPE>
inline void _llk_math_eltwise_unary_broadcast_addrmod_()
{
    static_assert((BROADCAST_TYPE != BroadcastType::NONE), "Broadcast type cannot be NONE for this operation");

    constexpr uint8_t num_srb_rows_inc = (BROADCAST_TYPE == BroadcastType::COL) ? ELTWISE_MATH_ROWS : 0;

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
 * @brief Sets up mop config for elementwise unary broadcast operations
 * @tparam BROADCAST_TYPE: Sets the broadcast type, values = [COL, ROW, SCALAR]
 * @tparam unpack_to_dest: If true, unpacker writes directly to dest register (workaround for 32-bit formats)
 * @param tile_shape: Contains all the information of the tile shape: num faces, face row/col dim, etc
 */
template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false>
inline void _llk_math_eltwise_unary_broadcast_mop_config_(const TileShape& tile_shape)
{
    static_assert((BROADCAST_TYPE != BroadcastType::NONE), "Broadcast type cannot be NONE for this operation");

    if constexpr (unpack_to_dest)
    {
        // Workaround: movA2D doesn't support broadcast, so dest→srcB→dest via MOVD2B then MOVB2D
        _llk_math_eltwise_unary_broadcast_addrmod_<BROADCAST_TYPE>();
    }

    // For SCALAR: unpacker unpacks 1 row with 1 dvalid -> math processes entire tile (64 rows) using row broadcast mode
    // For ROW/COL: unpacker unpacks multiple rows/faces with multiple dvalids -> math processes 1 face (16 rows) per outer loop iteration
    const uint32_t num_rows_per_matrix    = (BROADCAST_TYPE == BroadcastType::SCALAR) ? tile_shape.num_faces * tile_shape.face_r_dim : tile_shape.face_r_dim;
    const uint32_t num_dvalids_outer_loop = (BROADCAST_TYPE == BroadcastType::SCALAR) ? 1 : tile_shape.num_faces;

    const uint32_t num_rows_per_move_instrn = ELTWISE_MATH_ROWS;
    const uint32_t MOP_INNER_LOOP           = num_rows_per_matrix >> math_rows_log2(num_rows_per_move_instrn);
    const uint32_t mov_rows_instn           = p_mov_src_to_dest::MOV_8_ROWS;
    const uint32_t MOP_OUTER_LOOP           = num_dvalids_outer_loop;

    constexpr uint32_t bcast_datum0 = (BROADCAST_TYPE == BroadcastType::SCALAR || BROADCAST_TYPE == BroadcastType::COL) ? 1 : 0;
    constexpr uint32_t dst_addr     = (BROADCAST_TYPE == BroadcastType::SCALAR || BROADCAST_TYPE == BroadcastType::ROW) ? 1 : 0;

    const auto movb2d_func = [mov_rows_instn, bcast_datum0, dst_addr](uint8_t addr_mod)
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
    else // ROW
    {
        temp.set_last_inner_loop_instr(movb2d_func(ADDR_MOD_0));
        temp.set_last_outer_loop_instr(movb2d_func(ADDR_MOD_2));
    }

    temp.program_bank0_sw_cntl(instrn_buffer);
}

/**
 * @brief Sets up mop config for dest→srcB phase (workaround for unpack_to_dest)
 * @param tile_shape: Contains all the information of the tile shape: num faces, face row/col dim, etc
 * @note This function transfers data from dest register to srcB register.
 * For unpack_to_dest=true: unpacker unpacks SCALAR=1 row, ROW=2 rows, COL=2 faces to dest.
 * This function copies that data from dest to srcB for the broadcast operation.
 */
template <BroadcastType BROADCAST_TYPE>
inline void _llk_math_eltwise_unary_broadcast_d2b_mop_config_(const TileShape& tile_shape)
{
    const uint32_t num_rows_per_move_instrn = ELTWISE_MATH_ROWS;
    const uint32_t mov_rows_instn           = p_mov_src_to_dest::MOV_8_ROWS;

    // For unpack_to_dest=true: unpacker unpacks different amounts based on broadcast type
    // SCALAR: 1 row, ROW: 2 rows (F0, F1), COL: 2 faces (F0, F2) = 32 rows total
    // Calculate MOP loops based on what needs to be transferred from dest to srcB
    uint32_t MOP_INNER_LOOP_VAL;
    uint32_t MOP_OUTER_LOOP_VAL;

    if constexpr (BROADCAST_TYPE == BroadcastType::SCALAR)
    {
        // SCALAR: 1 row needs to be transferred (but MOVD2B moves 8 rows, so we use 1 instruction)
        MOP_INNER_LOOP_VAL = 1;
        MOP_OUTER_LOOP_VAL = 1;
    }
    else if constexpr (BROADCAST_TYPE == BroadcastType::ROW)
    {
        // ROW: 2 rows need to be transferred (but MOVD2B moves 8 rows, so we use 1 instruction)
        MOP_INNER_LOOP_VAL = 1;
        MOP_OUTER_LOOP_VAL = 1;
    }
    else // COL
    {
        // COL: 2 faces = 32 rows need to be transferred
        // 32 rows / 8 rows per instruction = 4 instructions
        const uint32_t num_rows_to_transfer = tile_shape.face_r_dim * 2; // 2 faces
        MOP_INNER_LOOP_VAL                  = num_rows_to_transfer >> math_rows_log2(num_rows_per_move_instrn);
        MOP_OUTER_LOOP_VAL                  = 1;
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
 * @tparam BROADCAST_TYPE: Sets the broadcast type, values = [COL, ROW, SCALAR]
 * @tparam unpack_to_dest: If true, unpacker writes directly to dest register (workaround for 32-bit formats)
 * @param tile_shape: Contains all the information of the tile shape: num faces, face row/col dim, etc
 */
template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false>
inline void _llk_math_eltwise_unary_broadcast_init_(const TileShape& tile_shape)
{
    if constexpr (unpack_to_dest)
    {
        _llk_math_eltwise_unary_broadcast_d2b_mop_config_<BROADCAST_TYPE>(tile_shape);
        _llk_math_eltwise_unary_broadcast_addrmod_<BROADCAST_TYPE>();
        _llk_math_eltwise_unary_broadcast_mop_config_<BROADCAST_TYPE, unpack_to_dest>(tile_shape);
    }
    else
    {
        _llk_math_eltwise_unary_broadcast_addrmod_<BROADCAST_TYPE>();
        _llk_math_eltwise_unary_broadcast_mop_config_<BROADCAST_TYPE, unpack_to_dest>(tile_shape);
    }
    _reset_counters_<p_setrwc::SET_ABD_F>();
}

/**
 * @brief Perform an elementwise unary broadcast operation
 * @tparam BROADCAST_TYPE: Sets the broadcast type, values = [COL, ROW, SCALAR]
 * @tparam unpack_to_dest: If true, unpacker writes directly to dest register (workaround for 32-bit formats)
 * @param tile_idx: Tile index into the destination register.
 * If dest reg in float16 mode -> values = [0 - 8] in double buffering mode, values = [0 - 16] in full mode
 * If dest reg in float32 mode -> values = [0 - 4] in double buffering mode, values = [0 - 8] in full mode
 */
template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false>
inline void _llk_math_eltwise_unary_broadcast_(const uint32_t tile_idx)
{
    if constexpr (unpack_to_dest)
    {
        ckernel::ckernel_template::run_bank0_sw_cntl(instrn_buffer);
        _reset_counters_<p_setrwc::SET_ABD_F>();
    }

    _set_dst_write_addr_<DstTileShape::Tile32x32>(tile_idx);

    // Run MOP
    ckernel::ckernel_template::run_bank0_sw_cntl(instrn_buffer);

    // Reset all counters
    _reset_counters_<p_setrwc::SET_ABD_F>();
}
