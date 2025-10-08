// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_pack.h"
#include "unified_operand.hpp"
#include "unified_state.hpp"

inline void _unified_pack_init_(UnifiedOperand dst)
{
    _llk_pack_init_((uint)dst.l1_format, dst.face_height, dst.tile_width(), dst.num_faces(), dst.partial_face(), dst.narrow_tile());
}

inline void _unified_pack_(uint32_t dest_index, UnifiedOperand dst, uint32_t l1_index)
{
    _llk_pack_<ckernel::DstSync::SyncHalf, false>(dest_index, dst.l1_address - 1 + l1_index * dst.tile_size());
}

inline void _unified_pack_uninit_([[maybe_unused]] UnifiedOperand dst)
{
}
