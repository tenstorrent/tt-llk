// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_unpack_AB_matmul.h"
#include "unified_operand.hpp"
#include "unified_state.hpp"

inline void _unified_unpack_matmul_init_(UnifiedOperand src_a, UnifiedOperand src_b)
{
    _llk_unpack_AB_matmul_init_(
        0,
        matmul_ct_dim(src_a, src_b),
        matmul_rt_dim(src_a, src_b),
        matmul_kt_dim(src_a, src_b),
        src_a.face_height,
        src_b.face_height,
        src_a.num_faces(),
        src_b.num_faces(),
        src_a.partial_face(),
        src_b.partial_face(),
        src_a.tile_size(),
        src_b.tile_size());
}

inline void _unified_unpack_matmul_(UnifiedOperand src_a, UnifiedOperand src_b, int idx_a, int idx_b)
{
    _llk_unpack_AB_matmul_(
        src_a.l1_address - 1,
        src_b.l1_address - 1,
        idx_a,
        idx_b,
        src_a.tile_size(),
        src_b.tile_size(),
        src_a.face_height,
        src_b.face_height,
        src_a.partial_face(),
        src_b.partial_face(),
        matmul_ct_dim(src_a, src_b),
        matmul_rt_dim(src_a, src_b),
        matmul_kt_dim(src_a, src_b));
}

inline void _unified_unpack_matmul_uninit_([[maybe_unused]] UnifiedOperand src_a, [[maybe_unused]] UnifiedOperand src_b)
{
}
