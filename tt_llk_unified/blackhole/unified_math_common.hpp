// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_defs.h"
#include "llk_math_common.h"
#include "unified_operand.hpp"
#include "unified_state.hpp"

// This API is affected by the following feature flags:
// TODO LP
inline void _unified_math_hw_configure_(UnifiedOperand src_a, UnifiedOperand src_b, bool is_32bit_dest, [[maybe_unused]] StochRndType rounding_mode)
{
    _llk_math_hw_configure_<false, false>((uint)src_a.src_format, (uint)src_b.src_format);
    _unified_state_math_is_32bit_dest_ = is_32bit_dest;
}

inline void _unified_math_sync_init_(bool full_sync)
{
    if (full_sync)
    {
        _llk_math_pack_sync_init_<ckernel::DstSync::SyncFull, false>();
    }
    else
    {
        _llk_math_pack_sync_init_<ckernel::DstSync::SyncHalf, false>();
    }
    _unified_state_math_full_sync_ = full_sync;
}

inline void _unified_math_acquire_dest_()
{
    math_dest_wait();
}

inline void _unified_math_release_dest_()
{
    if (_unified_state_math_full_sync_)
    {
        _llk_math_dest_section_done_<ckernel::DstSync::SyncFull, false>();
    }
    else
    {
        _llk_math_dest_section_done_<ckernel::DstSync::SyncHalf, false>();
    }
}
