// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_defs.h"
#include "llk_pack.h"
#include "unified_operand.hpp"
#include "unified_state.hpp"

inline void _unified_pack_hw_configure_(UnifiedOperand dst, bool is_32bit_dest, [[maybe_unused]] StochRndType rounding_mode)
{
    if (is_32bit_dest)
    {
        configure_pack<true, false>(
            (uint)dst.dest_format, (uint)dst.l1_format, 0, dst.face_height, dst.tile_width(), dst.num_faces(), dst.partial_face(), dst.narrow_tile());
    }
    else
    {
        configure_pack<false, false>(
            (uint)dst.dest_format, (uint)dst.l1_format, 0, dst.face_height, dst.tile_width(), dst.num_faces(), dst.partial_face(), dst.narrow_tile());
    }
    _unified_state_pack_is_32bit_dest_ = is_32bit_dest;
}

inline void _unified_pack_sync_init_(bool full_sync)
{
    if (full_sync)
    {
        _llk_pack_dest_init_<ckernel::DstSync::SyncFull, false>();
    }
    else
    {
        _llk_pack_dest_init_<ckernel::DstSync::SyncHalf, false>();
    }
    _unified_state_pack_full_sync_ = full_sync;
}

inline void _unified_pack_acquire_dest_()
{
    _llk_packer_wait_for_math_done_();
}

inline void _unified_pack_release_dest_()
{
    uint8_t mask = _unified_state_pack_full_sync_ ? 0b10 : 0b00;
    mask |= _unified_state_pack_is_32bit_dest_ ? 0b01 : 0b00;

    switch (mask)
    {
        case 0b00:
            _llk_pack_dest_section_done_<ckernel::DstSync::SyncHalf, false>();
            break;
        case 0b01:
            _llk_pack_dest_section_done_<ckernel::DstSync::SyncHalf, true>();
            break;
        case 0b10:
            _llk_pack_dest_section_done_<ckernel::DstSync::SyncFull, false>();
            break;
        case 0b11:
            _llk_pack_dest_section_done_<ckernel::DstSync::SyncFull, true>();
            break;
    }
}
