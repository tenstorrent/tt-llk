// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "cmath_common.h"

// Uses ADDR_MOD_4 only ({srca=8, dest=8}), preserving matmul's ADDR_MOD 0,1,2
// and reduce's ADDR_MOD 2,3. ADDR_MOD_4 is a "don't care" — matmul restores it
// on next reinit.
template <DataCopyType type, DstSync Dst, bool is_fp32_dest_acc_en, BroadcastType src_b_bcast_type = BroadcastType::NONE, bool unpack_to_dest = false>
inline void _llk_math_eltwise_unary_datacopy_custom_(
    const std::uint32_t dst_index, const std::uint32_t src_format, const std::uint32_t dst_format, const std::uint32_t num_faces = 4)
{
    addr_mod_t {
        .srca = {.incr = 8},
        .srcb = {.incr = 0},
        .dest = {.incr = 8},
    }
        .set(ADDR_MOD_4);

    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);
    math::reset_counters(p_setrwc::SET_ABD_F);

    TTI_MOVA2D(0, 0, ADDR_MOD_4, p_mova2d::MOV_8_ROWS, 0);
    TTI_MOVA2D(0, 0, ADDR_MOD_4, p_mova2d::MOV_8_ROWS, 0);
    TTI_MOVA2D(0, 0, ADDR_MOD_4, p_mova2d::MOV_8_ROWS, 0);
    TTI_MOVA2D(0, 0, ADDR_MOD_4, p_mova2d::MOV_8_ROWS, 0);
    TTI_MOVA2D(0, 0, ADDR_MOD_4, p_mova2d::MOV_8_ROWS, 0);
    TTI_MOVA2D(0, 0, ADDR_MOD_4, p_mova2d::MOV_8_ROWS, 0);
    TTI_MOVA2D(0, 0, ADDR_MOD_4, p_mova2d::MOV_8_ROWS, 0);
    TTI_MOVA2D(0, 0, ADDR_MOD_4, p_mova2d::MOV_8_ROWS, 0);
    TTI_SETRWC(p_setrwc::CLR_A, 0, 0, 0, 0, p_setrwc::SET_AB);
}
