// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_math_matmul.h"
#include "unified_operand.hpp"
#include "unified_state.hpp"

template <int THROTTLE_LEVEL>
inline void _unified_math_matmul_init_(UnifiedOperand src_a, UnifiedOperand src_b, int fidelity)
{
    switch (fidelity & 0b11)
    {
        case 0b00:
            _llk_math_matmul_init_<0, DstTileFaceLayout::RowMajor, THROTTLE_LEVEL>(
                src_a.tile_height(),
                src_a.tile_width(),
                src_b.tile_height(),
                src_b.tile_width(),
                src_a.partial_face() || src_b.partial_face(),
                0,
                matmul_ct_dim(src_a, src_b),
                matmul_rt_dim(src_a, src_b),
                matmul_kt_dim(src_a, src_b));
            break;
        case 0b01:
            _llk_math_matmul_init_<1, DstTileFaceLayout::RowMajor, THROTTLE_LEVEL>(
                src_a.tile_height(),
                src_a.tile_width(),
                src_b.tile_height(),
                src_b.tile_width(),
                src_a.partial_face() || src_b.partial_face(),
                0,
                matmul_ct_dim(src_a, src_b),
                matmul_rt_dim(src_a, src_b),
                matmul_kt_dim(src_a, src_b));
            break;
        case 0b10:
            _llk_math_matmul_init_<2, DstTileFaceLayout::RowMajor, THROTTLE_LEVEL>(
                src_a.tile_height(),
                src_a.tile_width(),
                src_b.tile_height(),
                src_b.tile_width(),
                src_a.partial_face() || src_b.partial_face(),
                0,
                matmul_ct_dim(src_a, src_b),
                matmul_rt_dim(src_a, src_b),
                matmul_kt_dim(src_a, src_b));
            break;
        case 0b11:
            _llk_math_matmul_init_<3, DstTileFaceLayout::RowMajor, THROTTLE_LEVEL>(
                src_a.tile_height(),
                src_a.tile_width(),
                src_b.tile_height(),
                src_b.tile_width(),
                src_a.partial_face() || src_b.partial_face(),
                0,
                matmul_ct_dim(src_a, src_b),
                matmul_rt_dim(src_a, src_b),
                matmul_kt_dim(src_a, src_b));
            break;
    }

    _unified_state_math_fidelity_ = fidelity;
}

template <int THROTTLE_LEVEL>
inline void _unified_math_matmul_(UnifiedOperand src_a, UnifiedOperand src_b, int dest_index)
{
    switch (_unified_state_math_fidelity_ & 0b11)
    {
        case 0b00:
            _llk_math_matmul_<0, DstTileFaceLayout::RowMajor, THROTTLE_LEVEL>(
                dest_index, 0, matmul_ct_dim(src_a, src_b), matmul_rt_dim(src_a, src_b), matmul_kt_dim(src_a, src_b));
            break;
        case 0b01:
            _llk_math_matmul_<1, DstTileFaceLayout::RowMajor, THROTTLE_LEVEL>(
                dest_index, 0, matmul_ct_dim(src_a, src_b), matmul_rt_dim(src_a, src_b), matmul_kt_dim(src_a, src_b));
            break;
        case 0b10:
            _llk_math_matmul_<2, DstTileFaceLayout::RowMajor, THROTTLE_LEVEL>(
                dest_index, 0, matmul_ct_dim(src_a, src_b), matmul_rt_dim(src_a, src_b), matmul_kt_dim(src_a, src_b));
            break;
        case 0b11:
            _llk_math_matmul_<3, DstTileFaceLayout::RowMajor, THROTTLE_LEVEL>(
                dest_index, 0, matmul_ct_dim(src_a, src_b), matmul_rt_dim(src_a, src_b), matmul_kt_dim(src_a, src_b));
            break;
    }
}

inline void _unified_math_matmul_uninit_([[maybe_unused]] UnifiedOperand src_a, [[maybe_unused]] UnifiedOperand src_b)
{
}
