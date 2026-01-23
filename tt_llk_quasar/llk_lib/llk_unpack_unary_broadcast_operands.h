// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_unpack_common.h"
using namespace ckernel;

/**
 * @brief MOP configuration for unpack of unary operations with broadcasts
 * @details Sets up MOP for unpacking unary operands tile by tile with broadcast support
 * @tparam UNP_SEL: Selects which unpacker resource to use, values = p_unpacr::UNP_A/p_unpacr::UNP_B
 * @tparam BROADCAST_TYPE: Sets the broadcast type, values = [SCALAR, COL, ROW]
 * @param buf_desc_id: The buffer descriptor ID where the buffer information is
 * stored in the buffer descriptor table, values = 0 - 16
 * @param num_tiles: number of tiles to unpack at a time
 */
template <uint32_t UNP_SEL, BroadcastType BROADCAST_TYPE>
inline void _llk_unpack_unary_broadcast_operands_mop_config_(const uint32_t buf_desc_id, const uint32_t num_tiles)
{
    static_assert((UNP_SEL == p_unpacr::UNP_A) || (UNP_SEL == p_unpacr::UNP_B), "UNP_SEL can only be set to p_unpacr::UNP_A/UNP_B");
    static_assert((BROADCAST_TYPE != BroadcastType::NONE), "Broadcast type cannot be NONE for this operation");

    const uint32_t MOP_OUTER_LOOP        = num_tiles;
    constexpr uint32_t MOP_INNER_LOOP    = 1;
    constexpr static uint replay_buf_len = (BROADCAST_TYPE == BroadcastType::SCALAR) ? 1 : 2;

    // Select the appropriate tile increment instruction based on unpacker
    uint unpack_tile_inc;
    if constexpr (UNP_SEL == p_unpacr::UNP_A)
    {
        unpack_tile_inc = TT_OP_UNPACR0_TILE_INC(0, 1 /*Src Tile Idx*/, buf_desc_id, 1 /*Set Dvalid*/);
    }
    else if constexpr (UNP_SEL == p_unpacr::UNP_B)
    {
        unpack_tile_inc = TT_OP_UNPACR1_TILE_INC(0, 1 /*Src Tile Idx*/, buf_desc_id, 1 /*Set Dvalid*/);
    }

    // Load replay buffer with broadcast pattern based on unpacker selection and broadcast type
    if constexpr (UNP_SEL == p_unpacr::UNP_A)
    {
        if constexpr (BROADCAST_TYPE == BroadcastType::SCALAR)
        {
            load_replay_buf<0, replay_buf_len>(
                [buf_desc_id]
                {
                    // Unpack Face 0, Row 0 → absolute row 0 (contains scalar at column 0)
                    TT_UNPACR0_ROW(0 /*Dst_Row_Idx*/, 0 /*Src_Row_Idx*/, 0 /*Dst_Face_Idx*/, 0 /*Src_Face_Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                });
        }
        else if constexpr (BROADCAST_TYPE == BroadcastType::ROW)
        {
            load_replay_buf<0, replay_buf_len>(
                [buf_desc_id]
                {
                    // Unpack Face 0, Row 0 → absolute row 0 (Dst_Face_Idx=0, Dst_Row_Idx=0)
                    TT_UNPACR0_ROW(0 /*Dst_Row_Idx*/, 0 /*Src_Row_Idx*/, 0 /*Dst_Face_Idx*/, 0 /*Src_Face_Idx*/, 0, 0, buf_desc_id, 0 /*SetDatValid*/);
                    // Unpack Face 1, Row 0 → absolute row 16 (Dst_Face_Idx=1, Dst_Row_Idx=0) (last instruction sets dvalid)
                    TT_UNPACR0_ROW(0 /*Dst_Row_Idx*/, 0 /*Src_Row_Idx*/, 1 /*Dst_Face_Idx*/, 1 /*Src_Face_Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                });
        }
        else if constexpr (BROADCAST_TYPE == BroadcastType::COL)
        {
            load_replay_buf<0, replay_buf_len>(
                [buf_desc_id]
                {
                    // Unpack Face 0 (entire face) → column will be extracted in math section
                    TT_UNPACR0_FACE(0 /*Dst Face Idx*/, 0 /*Src Face Idx*/, 0, 0, buf_desc_id, 0 /*SetDatValid*/);
                    // Unpack Face 2 (entire face) → column will be extracted in math section (last instruction sets dvalid)
                    TT_UNPACR0_FACE(0 /*Dst Face Idx*/, 2 /*Src Face Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                });
        }
    }
    else if constexpr (UNP_SEL == p_unpacr::UNP_B)
    {
        if constexpr (BROADCAST_TYPE == BroadcastType::SCALAR)
        {
            load_replay_buf<0, replay_buf_len>(
                [buf_desc_id]
                {
                    // Unpack Face 0, Row 0 → absolute row 0 (contains scalar at column 0)
                    TT_UNPACR1_ROW(0 /*Dst_Row_Idx*/, 0 /*Src_Row_Idx*/, 0 /*Dst_Face_Idx*/, 0 /*Src_Face_Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                });
        }
        else if constexpr (BROADCAST_TYPE == BroadcastType::ROW)
        {
            load_replay_buf<0, replay_buf_len>(
                [buf_desc_id]
                {
                    // Unpack Face 0, Row 0 → absolute row 0 (Dst_Face_Idx=0, Dst_Row_Idx=0)
                    TT_UNPACR1_ROW(0 /*Dst_Row_Idx*/, 0 /*Src_Row_Idx*/, 0 /*Dst_Face_Idx*/, 0 /*Src_Face_Idx*/, 0, 0, buf_desc_id, 0 /*SetDatValid*/);
                    // Unpack Face 1, Row 0 → absolute row 16 (Dst_Face_Idx=1, Dst_Row_Idx=0) (last instruction sets dvalid)
                    TT_UNPACR1_ROW(0 /*Dst_Row_Idx*/, 0 /*Src_Row_Idx*/, 1 /*Dst_Face_Idx*/, 1 /*Src_Face_Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                });
        }
        else if constexpr (BROADCAST_TYPE == BroadcastType::COL)
        {
            load_replay_buf<0, replay_buf_len>(
                [buf_desc_id]
                {
                    // Unpack Face 0 (entire face) → column will be extracted in math section
                    TT_UNPACR1_FACE(0 /*Dst Face Idx*/, 0 /*Src Face Idx*/, 0, 0, buf_desc_id, 0 /*SetDatValid*/);
                    // Unpack Face 2 (entire face) → column will be extracted in math section (last instruction sets dvalid)
                    TT_UNPACR1_FACE(0 /*Dst Face Idx*/, 2 /*Src Face Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                });
        }
    }

    // Select the appropriate increment instruction based on unpacker
    // Increment by 1 tile because UNPACR_ROW and UNPACR_FACE instructions do not increment counters
    uint inc_src_tile_instrn;
    if constexpr (UNP_SEL == p_unpacr::UNP_A)
    {
        inc_src_tile_instrn = TT_OP_INC_SRC_TILE_FACE_ROW_IDX(p_set_inc_sel::TILE_SEL, p_unpacr::UNP_A, 1);
    }
    else if constexpr (UNP_SEL == p_unpacr::UNP_B)
    {
        inc_src_tile_instrn = TT_OP_INC_SRC_TILE_FACE_ROW_IDX(p_set_inc_sel::TILE_SEL, p_unpacr::UNP_B, 1);
    }

    ckernel_template temp(MOP_OUTER_LOOP, MOP_INNER_LOOP, TT_OP_REPLAY(0, replay_buf_len, 0, 0, 0, 0), inc_src_tile_instrn);

    temp.set_start_op(unpack_tile_inc);

    temp.program_bank0_sw_cntl(instrn_buffer);
}

/**
 * @brief Initialization for unpack of unary operations with broadcasts
 * @details Sets up MOP for unpacking unary operands tile by tile with broadcast support
 * @tparam UNP_SEL: Selects which unpacker resource to use, values = p_unpacr::UNP_A/p_unpacr::UNP_B
 * @tparam BROADCAST_TYPE: Sets the broadcast type, values = [SCALAR, COL, ROW]
 * @param buf_desc_id: The buffer descriptor ID where the buffer information is
 * stored in the buffer descriptor table, values = 0 - 16
 * @param num_tiles: number of tiles to unpack at a time
 */
template <uint32_t UNP_SEL, BroadcastType BROADCAST_TYPE>
inline void _llk_unpack_unary_broadcast_operands_init_(const uint32_t buf_desc_id, const uint32_t num_tiles)
{
    _llk_unpack_unary_broadcast_operands_mop_config_<UNP_SEL, BROADCAST_TYPE>(buf_desc_id, num_tiles);
}

/**
 * @brief Unpacks unary operands with broadcast support
 * @param start_l1_tile_idx: Start tile index into the L1 buffer
 * @tparam UNP_SEL: Selects which unpacker resource to use, values = p_unpacr::UNP_A/p_unpacr::UNP_B
 */
template <uint32_t UNP_SEL>
inline void _llk_unpack_unary_broadcast_operands_(const uint start_l1_tile_idx)
{
    // RT: for the best performance, setting counters should be placed in a REPLAY buffer
    // in the mop_config, but for back compatibility with APIs, the counter functions must
    // be programmable with users input offset idx

    // Reset Dest counters for Unpacker to 0
    // Set Source counter to L1 base + offset
    TT_SET_SRC_TILE_FACE_ROW_IDX(p_set_inc_sel::TILE_SEL, UNP_SEL, start_l1_tile_idx);
    TTI_SET_DST_TILE_FACE_ROW_IDX(p_set_inc_sel::TILE_SEL, UNP_SEL, 0);

    // Runs MOP
    ckernel::ckernel_template::run_bank0_sw_cntl(instrn_buffer);
}
