// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <utility>

#include "llk_math_eltwise_ternary_sfpu.h"
#include "llk_sfpu_types.h"

template <bool APPROXIMATE, typename Callable, typename... Args>
inline void _llk_math_eltwise_ternary_sfpu_params_(
    Callable&& sfpu_func, uint dst_index_in0, uint dst_index_in1, uint dst_index_in2, uint dst_index_out, int vector_mode = (int)VectorMode::RC, Args&&... args)
{
    _llk_math_eltwise_ternary_sfpu_start_(0); // Reuse same sync primitive

    if (vector_mode == (int)VectorMode::R)
    {
        // Row vector - Face0 + Face1
        for (int face = 0; face < 2; face++)
        {
            std::forward<Callable>(sfpu_func)(dst_index_in0, dst_index_in1, dst_index_in2, dst_index_out, std::forward<Args>(args)...);
            _llk_math_eltwise_ternary_sfpu_inc_dst_face_addr_();
        }
        // Skip next 2 faces
        _llk_math_eltwise_ternary_sfpu_inc_dst_face_addr_();
        _llk_math_eltwise_ternary_sfpu_inc_dst_face_addr_();
    }
    else if (vector_mode == (int)VectorMode::C)
    {
        // Column vector - Face0 + Face2
        for (int face = 0; face < 2; face++)
        {
            std::forward<Callable>(sfpu_func)(dst_index_in0, dst_index_in1, dst_index_in2, dst_index_out, std::forward<Args>(args)...);
            _llk_math_eltwise_ternary_sfpu_inc_dst_face_addr_();
            _llk_math_eltwise_ternary_sfpu_inc_dst_face_addr_();
        }
    }
    else if (vector_mode == (int)VectorMode::RC)
    {
        // All 4 faces
        for (int face = 0; face < 4; face++)
        {
            std::forward<Callable>(sfpu_func)(dst_index_in0, dst_index_in1, dst_index_in2, dst_index_out, std::forward<Args>(args)...);
            _llk_math_eltwise_ternary_sfpu_inc_dst_face_addr_();
        }
    }
    else
    {
        // Default: single face pass-through
        std::forward<Callable>(sfpu_func)(dst_index_in0, dst_index_in1, dst_index_in2, dst_index_out, std::forward<Args>(args)...);
    }
    _llk_math_eltwise_ternary_sfpu_done_(); // Finalize
}
