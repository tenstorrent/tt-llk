// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_unpack_common.h"
using namespace ckernel;

/**
 * @brief MOP configuration for unpack of unary operations with broadcasts
 * @tparam UNP_SEL Unpacker resource selector used only when unpack_to_dest=false; in that case it must be
 * p_unpacr::UNP_B (see static_assert). When unpack_to_dest=true, this function internally uses
 * p_unpacr::UNP_A regardless of the value of UNP_SEL.
 * @tparam BROADCAST_TYPE Broadcast type, values = [SCALAR, COL, ROW]
 * @tparam unpack_to_dest If true, unpack to dest using UNPACKER0 (srcA / p_unpacr::UNP_A); otherwise unpack to
 * srcB using UNPACKER1 (p_unpacr::UNP_B).
 */
template <uint32_t UNP_SEL, BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false>
inline void _llk_unpack_unary_broadcast_operands_mop_config_(const uint32_t buf_desc_id, const uint32_t num_tiles)
{
    static_assert(
        unpack_to_dest || (UNP_SEL == p_unpacr::UNP_B),
        "UNP_SEL must be p_unpacr::UNP_B when unpack_to_dest is false - movA2D broadcast is not working on Quasar");
    static_assert((BROADCAST_TYPE != BroadcastType::NONE), "Broadcast type cannot be NONE for this operation");

    const uint32_t MOP_OUTER_LOOP     = num_tiles;
    constexpr uint32_t MOP_INNER_LOOP = 1;
    // Replay buffer length: SCALAR=1, ROW=4 (unpack_to_dest=false) or 2 (unpack_to_dest=true), COL=4 (unpack_to_dest=false) or 2 (unpack_to_dest=true)
    constexpr static uint replay_buf_len =
        (BROADCAST_TYPE == BroadcastType::SCALAR)
            ? 1
            : ((BROADCAST_TYPE == BroadcastType::ROW && !unpack_to_dest) ? 4 : ((BROADCAST_TYPE == BroadcastType::COL && !unpack_to_dest) ? 4 : 2));

    uint unpack_tile_inc;
    if constexpr (unpack_to_dest)
    {
        unpack_tile_inc = TT_OP_UNPACR_DEST_TILE_INC(1, 1 /*Src Tile Idx*/, buf_desc_id, 0 /*Set Dvalid*/);
    }
    else
    {
        // For SCALAR, we only need face 0, so skip start_op and only use replay buffer
        if constexpr (BROADCAST_TYPE == BroadcastType::SCALAR)
        {
            unpack_tile_inc = TT_OP_NOP;
        }
        else
        {
            unpack_tile_inc = TT_OP_UNPACR1_TILE_INC(0, 1 /*Src Tile Idx*/, buf_desc_id, 1 /*Set Dvalid*/);
        }
    }
    if constexpr (BROADCAST_TYPE == BroadcastType::SCALAR)
    {
        load_replay_buf<0, replay_buf_len>(
            [buf_desc_id]
            {
                if constexpr (unpack_to_dest)
                {
                    TT_UNPACR_DEST_FACE(0 /*Dst Face Idx*/, 0 /*Src Face Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                }
                else
                {
                    TT_UNPACR1_FACE(0 /*Dst Face Idx*/, 0 /*Src Face Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                }
            });
    }
    else if constexpr (BROADCAST_TYPE == BroadcastType::ROW)
    {
        load_replay_buf<0, replay_buf_len>(
            [buf_desc_id]
            {
                if constexpr (unpack_to_dest)
                {
                    TT_UNPACR_DEST_ROW(0 /*Dst_Row_Idx*/, 0 /*Src_Row_Idx*/, 0 /*Dst_Face_Idx*/, 0 /*Src_Face_Idx*/, 0, 0, buf_desc_id, 0 /*SetDatValid*/);
                    TT_UNPACR_DEST_ROW(0 /*Dst_Row_Idx*/, 0 /*Src_Row_Idx*/, 1 /*Dst_Face_Idx*/, 1 /*Src_Face_Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                }
                else
                {
                    // ROW: unpack F0 row, F1 row, F0 row, F1 row (like binary broadcast - all set dvalid)
                    // Pattern: F0, F1, F0, F1 to match golden generator (output faces 0,1,2,3 use F0 row, F1 row, F0 row, F1 row)
                    TT_UNPACR1_ROW(0 /*Dst_Row_Idx*/, 0 /*Src_Row_Idx*/, 0 /*Dst_Face_Idx*/, 0 /*Src_Face_Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                    TT_UNPACR1_ROW(0 /*Dst_Row_Idx*/, 0 /*Src_Row_Idx*/, 0 /*Dst_Face_Idx*/, 1 /*Src_Face_Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                    TT_UNPACR1_ROW(0 /*Dst_Row_Idx*/, 0 /*Src_Row_Idx*/, 0 /*Dst_Face_Idx*/, 0 /*Src_Face_Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                    TT_UNPACR1_ROW(0 /*Dst_Row_Idx*/, 0 /*Src_Row_Idx*/, 0 /*Dst_Face_Idx*/, 1 /*Src_Face_Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                }
            });
    }
    else if constexpr (BROADCAST_TYPE == BroadcastType::COL)
    {
        load_replay_buf<0, replay_buf_len>(
            [buf_desc_id]
            {
                if constexpr (unpack_to_dest)
                {
                    TT_UNPACR_DEST_FACE(0 /*Dst Face Idx*/, 0 /*Src Face Idx*/, 0, 0, buf_desc_id, 0 /*SetDatValid*/);
                    TT_UNPACR_DEST_FACE(2 /*Dst Face Idx*/, 2 /*Src Face Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                }
                else
                {
                    // COL: unpack F0, F0, F2, F2 all to srcB Face 0 (like binary broadcast - all set dvalid)
                    TT_UNPACR1_FACE(0 /*Dst Face Idx*/, 0 /*Src Face Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                    TT_UNPACR1_FACE(0 /*Dst Face Idx*/, 0 /*Src Face Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                    TT_UNPACR1_FACE(0 /*Dst Face Idx*/, 2 /*Src Face Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                    TT_UNPACR1_FACE(0 /*Dst Face Idx*/, 2 /*Src Face Idx*/, 0, 0, buf_desc_id, 1 /*SetDatValid*/);
                }
            });
    }

    uint inc_src_tile_instrn;
    if constexpr (unpack_to_dest)
    {
        inc_src_tile_instrn = TT_OP_INC_SRC_TILE_FACE_ROW_IDX(p_set_inc_sel::TILE_SEL, p_unpacr::UNP_A, 1);
    }
    else
    {
        inc_src_tile_instrn = TT_OP_INC_SRC_TILE_FACE_ROW_IDX(p_set_inc_sel::TILE_SEL, UNP_SEL, 1);
    }

    ckernel_template temp(MOP_OUTER_LOOP, MOP_INNER_LOOP, TT_OP_REPLAY(0, replay_buf_len, 0, 0, 0, 0), inc_src_tile_instrn);

    temp.set_start_op(unpack_tile_inc);

    temp.program_bank0_sw_cntl(instrn_buffer);
}

/**
 * @brief Initialization for unpack of unary operations with broadcasts
 */
template <uint32_t UNP_SEL, BroadcastType BROADCAST_TYPE, bool unpack_to_dest = false>
inline void _llk_unpack_unary_broadcast_operands_init_(const uint32_t buf_desc_id, const uint32_t num_tiles)
{
    _llk_unpack_unary_broadcast_operands_mop_config_<UNP_SEL, BROADCAST_TYPE, unpack_to_dest>(buf_desc_id, num_tiles);
}

/**
 * @brief Unpacks unary operands with broadcast support
 */
template <uint32_t UNP_SEL, bool unpack_to_dest = false>
inline void _llk_unpack_unary_broadcast_operands_(const uint start_l1_tile_idx)
{
    static_assert(
        unpack_to_dest || (UNP_SEL == p_unpacr::UNP_B),
        "UNP_SEL must be p_unpacr::UNP_B when unpack_to_dest is false - movA2D broadcast is not working on Quasar");

    constexpr uint32_t counter_unp_sel = unpack_to_dest ? p_unpacr::UNP_A : UNP_SEL;
    TT_SET_SRC_TILE_FACE_ROW_IDX(p_set_inc_sel::TILE_SEL, counter_unp_sel, start_l1_tile_idx);
    TTI_SET_DST_TILE_FACE_ROW_IDX(p_set_inc_sel::TILE_SEL, counter_unp_sel, 0);
    ckernel::ckernel_template::run_bank0_sw_cntl(instrn_buffer);
}
