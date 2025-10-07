// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_unpack_common.h"
#include "unified_operand.hpp"
#include "unified_state.hpp"

inline void _unified_unpack_hw_configure_(UnifiedOperand src_a, UnifiedOperand src_b, bool is_32bit_dest, StochRndType rounding_mode)
{
    uint8_t mask = is_32bit_dest ? 0b100 : 0b000;
    mask |= (rounding_mode == StochRndType::Fpu || rounding_mode == StochRndType::All) ? 0b010 : 0b000;
    mask |= (rounding_mode == StochRndType::Pack || rounding_mode == StochRndType::All) ? 0b001 : 0b000;

    switch (mask)
    {
        case 0b000:
            configure_unpack_AB<false, false, false, false>(
                (uint)src_a.l1_format,
                (uint)src_b.l1_format,
                (uint)src_a.src_format,
                (uint)src_b.src_format,
                src_a.face_height,
                src_b.face_height,
                false,
                src_a.num_faces(),
                src_b.num_faces());
            break;
        case 0b001:
            configure_unpack_AB<false, false, false, true>(
                (uint)src_a.l1_format,
                (uint)src_b.l1_format,
                (uint)src_a.src_format,
                (uint)src_b.src_format,
                src_a.face_height,
                src_b.face_height,
                false,
                src_a.num_faces(),
                src_b.num_faces());
            break;
        case 0b010:
            configure_unpack_AB<false, false, true, false>(
                (uint)src_a.l1_format,
                (uint)src_b.l1_format,
                (uint)src_a.src_format,
                (uint)src_b.src_format,
                src_a.face_height,
                src_b.face_height,
                false,
                src_a.num_faces(),
                src_b.num_faces());
            break;
        case 0b011:
            configure_unpack_AB<false, false, true, true>(
                (uint)src_a.l1_format,
                (uint)src_b.l1_format,
                (uint)src_a.src_format,
                (uint)src_b.src_format,
                src_a.face_height,
                src_b.face_height,
                false,
                src_a.num_faces(),
                src_b.num_faces());
            break;
        case 0b100:
            configure_unpack_AB<true, false, false, false>(
                (uint)src_a.l1_format,
                (uint)src_b.l1_format,
                (uint)src_a.src_format,
                (uint)src_b.src_format,
                src_a.face_height,
                src_b.face_height,
                false,
                src_a.num_faces(),
                src_b.num_faces());
            break;
        case 0b101:
            configure_unpack_AB<true, false, false, true>(
                (uint)src_a.l1_format,
                (uint)src_b.l1_format,
                (uint)src_a.src_format,
                (uint)src_b.src_format,
                src_a.face_height,
                src_b.face_height,
                false,
                src_a.num_faces(),
                src_b.num_faces());
            break;
        case 0b110:
            configure_unpack_AB<true, false, true, false>(
                (uint)src_a.l1_format,
                (uint)src_b.l1_format,
                (uint)src_a.src_format,
                (uint)src_b.src_format,
                src_a.face_height,
                src_b.face_height,
                false,
                src_a.num_faces(),
                src_b.num_faces());
            break;
        case 0b111:
            configure_unpack_AB<true, false, true, true>(
                (uint)src_a.l1_format,
                (uint)src_b.l1_format,
                (uint)src_a.src_format,
                (uint)src_b.src_format,
                src_a.face_height,
                src_b.face_height,
                false,
                src_a.num_faces(),
                src_b.num_faces());
            break;
    }

    _unified_state_unpack_is_32bit_dest_ = is_32bit_dest;
}

inline void _unified_unpack_sync_init_(bool full_sync)
{
    // Initialize synchronization for unpack operation
    _unified_state_unpack_full_sync_ = full_sync;
}
