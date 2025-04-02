// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <type_traits>

#include "ckernel_globals.h"
#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "ckernel_sfpu.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_math_common.h"

// local function declarations
inline void eltwise_binary_sfpu_configure_addrmod()
{
    // NOTE: this kernel is typically used in conjunction with
    //       A2D, which is using ADDR_MOD_0 and ADDR_MOD_2, so use one
    //       that doesn't conflict!

    ckernel::addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_7);
}

inline void eltwise_binary_sfpu_configure_mop();

template <ckernel::DstSync Dst>
inline void _llk_math_eltwise_binary_sfpu_start_(const uint dst_index)
{
    if constexpr ((Dst == ckernel::DstSync::SyncTile16) || (Dst == ckernel::DstSync::SyncTile2))
    {
        ckernel::math::set_dst_write_addr<ckernel::DstTileLayout::Default, ckernel::DstTileShape::Tile32x32>(math_sync_tile_dst_index);
    }
    else
    {
        ckernel::math::set_dst_write_addr<ckernel::DstTileLayout::Default, ckernel::DstTileShape::Tile32x32>(dst_index);
    }
    TTI_STALLWAIT(ckernel::p_stall::STALL_SFPU, ckernel::p_stall::MATH);
}

inline void _llk_math_eltwise_binary_sfpu_done_()
{
    ckernel::math::clear_dst_reg_addr();
}

inline void _llk_math_eltwise_binary_sfpu_inc_dst_face_addr_()
{
    ckernel::math::inc_dst_addr<8>();
    ckernel::math::inc_dst_addr<8>();
}

template <SfpuType sfpu_op>
inline void _llk_math_eltwise_binary_sfpu_init_()
{
    ckernel::sfpu::_init_sfpu_config_reg();
    eltwise_binary_sfpu_configure_addrmod();
    ckernel::math::reset_counters(ckernel::p_setrwc::SET_ABD_F);
}
