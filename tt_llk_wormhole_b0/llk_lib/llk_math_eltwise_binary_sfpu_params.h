// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <utility>

#include "llk_math_eltwise_binary_sfpu.h"
#include "llk_sfpu_types.h"

// Internal implementation - takes a nullary callable that wraps the actual sfpu invocation
template <typename InvokeFunc>
inline void _llk_math_eltwise_binary_sfpu_params_impl_(InvokeFunc&& invoke, int vector_mode)
{
    _llk_math_eltwise_binary_sfpu_start_<DST_SYNC_MODE>(0);

    VectorMode mode = static_cast<VectorMode>(vector_mode);

    if (mode == VectorMode::R)
    {
        // Do a row vector, Face0 + Face1 -- first iteration (first row)
#pragma GCC unroll 0
        for (int face = 0; face < 2; face++)
        {
            invoke();
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
        }
        // Skip the next 2 faces
        TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
        TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
        TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
        TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
    }
    else if (mode == VectorMode::C)
    {
        // Do a column vector, Face0 + Face2 -- All iterations for full face
#pragma GCC unroll 0
        for (int face = 0; face < 2; face++)
        {
            invoke();
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
        }
    }
    else if (mode == VectorMode::RC)
    {
        // Do all four faces, and iterate through all 4 blocks of 4 rows each
#pragma GCC unroll 0
        for (int face = 0; face < 4; face++)
        {
            invoke();
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
        }
    }
    else
    {
        invoke();
    }

    _llk_math_eltwise_binary_sfpu_done_();
}

// Overload for callables that take (dst_index_in0, dst_index_in1, dst_index_out, args...)
template <bool APPROXIMATE, typename Callable, typename... Args>
inline void _llk_math_eltwise_binary_sfpu_params_(
    Callable&& sfpu_func, uint dst_index_in0, uint dst_index_in1, uint dst_index_out, int vector_mode = static_cast<int>(VectorMode::RC), Args&&... args)
{
    _llk_math_eltwise_binary_sfpu_params_impl_([&]() { sfpu_func(dst_index_in0, dst_index_in1, dst_index_out, std::forward<Args>(args)...); }, vector_mode);
}

// Overload for callables that take (dst_index_in0, dst_index_in1, args...) - no dst_index_out
template <bool APPROXIMATE, typename Callable, typename... Args>
inline void _llk_math_eltwise_binary_sfpu_params_(Callable&& sfpu_func, uint dst_index_in0, uint dst_index_in1, int vector_mode, Args&&... args)
{
    _llk_math_eltwise_binary_sfpu_params_impl_([&]() { sfpu_func(dst_index_in0, dst_index_in1, std::forward<Args>(args)...); }, vector_mode);
}

// Overload for callables that only take (dst_index_in0, dst_index_in1) - no args, default vector_mode
template <bool APPROXIMATE, typename Callable>
inline void _llk_math_eltwise_binary_sfpu_params_(Callable&& sfpu_func, uint dst_index_in0, uint dst_index_in1)
{
    _llk_math_eltwise_binary_sfpu_params_impl_([&]() { sfpu_func(dst_index_in0, dst_index_in1); }, static_cast<int>(VectorMode::RC));
}
