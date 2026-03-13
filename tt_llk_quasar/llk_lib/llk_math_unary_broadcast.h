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
 * @brief Sets up address modifiers for elementwise unary broadcast operations
 * @tparam BROADCAST_TYPE: Sets the broadcast type, values = [COL, ROW, SCALAR]
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
 * @brief Sets up mop config for elementwise unary broadcast operations
 * @tparam BROADCAST_TYPE: Sets the broadcast type, values = [COL, ROW, SCALAR]
 * @tparam unpack_to_dest: If true, unpacker writes directly to dest register (workaround for 32-bit formats)
 * @param tile_shape: Contains all the information of the tile shape: num faces, face row/col dim, etc
 */
template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false>
inline void _llk_math_eltwise_unary_broadcast_mop_config_(const TileShape& tile_shape)
{
    static_assert((BROADCAST_TYPE != BroadcastType::NONE), "Broadcast type cannot be NONE for this operation");

    // Addrmod is set by _llk_math_eltwise_unary_broadcast_init_ before calling this (and before d2b when unpack_to_dest).

    if constexpr (BROADCAST_TYPE == BroadcastType::ROW && unpack_to_dest)
    {
        // For unpack_to_dest ROW: D2B placed face 0 data at srcB[0] and face 1 data at srcB[FACE_R_DIM].
        // B2D must alternate srcB reads: srcB[0] for faces 0/2, srcB[FACE_R_DIM] for faces 1/3.
        // Use a replay buffer with 8 explicit MOVB2D instructions to achieve the alternating pattern.
        //
        // Addr_mods (set by _llk_math_eltwise_unary_broadcast_addrmod_ for ROW):
        //   ADDR_MOD_0: srcb.incr=0,          dest.incr=8  → srcb stays, dest advances
        //   ADDR_MOD_1: srcb.clr=1,           dest.incr=8  → srcb resets to 0, dest advances
        //   ADDR_MOD_2: srcb.incr=FACE_R_DIM, dest.incr=8  → srcb jumps by 16, dest advances
        //
        // Sequence (starting from srcb=0, dest=tile_base):
        //   Face 0: MOVB2D(MOD_0) srcb[0],  MOVB2D(MOD_2) srcb[0]→+16  = srcb becomes 16
        //   Face 1: MOVB2D(MOD_0) srcb[16], MOVB2D(MOD_1) srcb[16]→clr = srcb becomes 0
        //   Face 2: MOVB2D(MOD_0) srcb[0],  MOVB2D(MOD_2) srcb[0]→+16  = srcb becomes 16
        //   Face 3: MOVB2D(MOD_0) srcb[16], MOVB2D(MOD_2) srcb[16]→+16 = srcb becomes 32 (don't care)
        constexpr std::uint32_t b2d_replay_offset = 1; // D2B uses replay offset 0
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

        // MOP: outer=1, inner=1, replays all 8 MOVB2D instructions in one shot
        ckernel_template temp(1, 1, TT_OP_REPLAY(b2d_replay_offset, b2d_replay_len, 0, 0, 0, 0));
        temp.set_end_op(TT_OP_CLEARDVALID(p_cleardvalid::CLR_SRCB_VLD, 0, 0, 0, 0, 0));
        temp.program_bank0_sw_cntl(instrn_buffer);
    }
    else
    {
        // Standard B2D path (non-unpack_to_dest, or SCALAR/COL with unpack_to_dest)
        // For SCALAR: unpacker unpacks 1 row with 1 dvalid -> math processes entire tile (64 rows) using row broadcast mode
        // For ROW/COL: unpacker unpacks multiple rows/faces with multiple dvalids -> math processes 1 face (16 rows) per outer loop iteration
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
 * @brief Sets up mop config for dest→srcB phase (workaround for unpack_to_dest)
 * @param tile_shape: Contains all the information of the tile shape: num faces, face row/col dim, etc
 * @note This function transfers data from dest register to srcB register.
 * For unpack_to_dest=true: unpacker unpacks SCALAR=1 row, ROW=2 rows, COL=2 faces to dest.
 * This function copies that data from dest to srcB for the broadcast operation.
 */
template <BroadcastType BROADCAST_TYPE>
inline void _llk_math_eltwise_unary_broadcast_d2b_mop_config_(const TileShape& tile_shape)
{
    if constexpr (BROADCAST_TYPE == BroadcastType::ROW)
    {
        // ROW: unpacker writes 2 seed rows to dest: row 0 (face 0) and row FACE_R_DIM (face 1).
        // Use MOV_1_ROW to transfer exactly these 2 rows from dest to srcB.
        // ADDR_MOD_3: increment by FACE_R_DIM to jump from face 0 row to face 1 row.
        addr_mod_t {
            .srcb = {.incr = FACE_R_DIM},
            .dest = {.incr = FACE_R_DIM},
        }
            .set(ADDR_MOD_3);

        const auto movd2b_row_func = [](std::uint8_t addr_mod, std::uint32_t dest_32b_lo)
        { return TT_OP_MOVD2B(dest_32b_lo, 0, addr_mod, p_movd2b::MOV_1_ROW, 0, 0); };

        constexpr std::uint32_t replay_buf_len = 1;
        load_replay_buf<0, replay_buf_len>([] { TTI_MOVD2B(0, 0, ADDR_MOD_3, p_movd2b::MOV_1_ROW, 0, 0); });

        // MOP: outer=1, inner=1 → executes (OP0, OP1) once = 2 MOVD2B instructions:
        //   OP0 = REPLAY(hi, MOV_1_ROW) → dest[0] hi → srcB[0], then srcb/dest += FACE_R_DIM
        //   OP1 = MOVD2B(hi, MOV_1_ROW) → dest[FACE_R_DIM] hi → srcB[FACE_R_DIM]
        ckernel_template temp(1, 1, TT_OP_REPLAY(0, replay_buf_len, 0, 0, 0, 0), movd2b_row_func(ADDR_MOD_3, 0));
        temp.set_last_inner_loop_instr(movd2b_row_func(ADDR_MOD_3, 1));

        // Program D2B into bank1 so it doesn't overwrite B2D in bank0
        temp.program_bank1_sw_cntl(instrn_buffer);
    }
    else
    {
        // SCALAR and COL: use MOV_8_ROWS with ADDR_MOD_3 incrementing by ELTWISE_MATH_ROWS
        const std::uint32_t num_rows_per_move_instrn = ELTWISE_MATH_ROWS;
        const std::uint32_t mov_rows_instn           = p_mov_src_to_dest::MOV_8_ROWS;

        std::uint32_t MOP_INNER_LOOP_VAL;
        std::uint32_t MOP_OUTER_LOOP_VAL;

        if constexpr (BROADCAST_TYPE == BroadcastType::SCALAR)
        {
            // SCALAR: 1 seed row, MOVD2B moves 8 rows (includes garbage beyond row 0, but only row 0 matters)
            MOP_INNER_LOOP_VAL = 1;
            MOP_OUTER_LOOP_VAL = 1;
        }
        else // COL
        {
            // COL: 2 faces = 32 rows need to be transferred, 32/8 = 4 instructions
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

        // Program D2B into bank1 so it doesn't overwrite B2D in bank0
        temp.program_bank1_sw_cntl(instrn_buffer);
    }
}

/**
 * @brief Sets up initialization for elementwise unary broadcast operation
 * @tparam BROADCAST_TYPE: Sets the broadcast type, values = [COL, ROW, SCALAR]
 * @tparam unpack_to_dest: If true, unpacker writes directly to dest register (workaround for 32-bit formats)
 * @tparam is_fp32_dest_acc_en: If true, dest accumulation uses float32 (for compatibility with test harness)
 * @param tile_shape: Contains all the information of the tile shape: num faces, face row/col dim, etc
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
 * @brief Perform an elementwise unary broadcast operation
 * @tparam BROADCAST_TYPE: Sets the broadcast type, values = [COL, ROW, SCALAR]
 * @tparam unpack_to_dest: If true, unpacker writes directly to dest register (workaround for 32-bit formats)
 * @tparam is_fp32_dest_acc_en: If true, dest accumulation uses float32 (for compatibility with test harness)
 * @param tile_idx: Tile index into the destination register.
 * If dest reg in float16 mode -> values = [0 - 8] in double buffering mode, values = [0 - 16] in full mode
 * If dest reg in float32 mode -> values = [0 - 4] in double buffering mode, values = [0 - 8] in full mode
 * @param tile_shape: Tile shape (for compatibility with test harness; unused in thin path)
 */
template <BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false, bool is_fp32_dest_acc_en = false>
inline void _llk_math_eltwise_unary_broadcast_(const std::uint32_t tile_idx, const TileShape& /*tile_shape*/)
{
    if constexpr (unpack_to_dest)
    {
        // D2B: move seed data from dest → srcB (bank1)
        ckernel::ckernel_template::run_bank1_sw_cntl(instrn_buffer);
        _reset_counters_<p_setrwc::SET_ABD_F>();
    }

    _set_dst_write_addr_<DstTileShape::Tile32x32>(tile_idx);

    // B2D: broadcast from srcB → dest (bank0)
    ckernel::ckernel_template::run_bank0_sw_cntl(instrn_buffer);

    // Reset all counters
    _reset_counters_<p_setrwc::SET_ABD_F>();
}
