// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_defs.h"
#include "llk_math_common.h"
#include "unified_feature_flags.hpp"
#include "unified_operand.hpp"
#include "unified_state.hpp"

// This API is affected by the following feature flags:
// TODO LP
inline void _unified_math_hw_configure_(UnifiedOperand src_a, UnifiedOperand src_b, bool is_32bit_dest, [[maybe_unused]] StochRndType rounding_mode)
{
    _llk_math_srcAB_hw_configure_(
        _unified_feature_flag_math_use_implied_formats_, is_32bit_dest, false /*false for now, is int dest*/, src_a.src_format, src_b.src_format);
    _unified_state_math_is_32bit_dest_ = is_32bit_dest;
}

inline void _unified_math_sync_init_(bool full_sync)
{
    set_up_dest_dvalid_per_thread<dest_dvalid_client::FPU>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});
    _unified_state_math_full_sync_ = full_sync;
}

inline void _unified_math_acquire_dest_()
{
}

inline void _unified_math_release_dest_()
{
    _llk_math_set_dvalid_<p_cleardvalid::FPU>();
}
