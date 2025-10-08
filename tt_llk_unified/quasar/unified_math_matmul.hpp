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
    _llk_math_matmul_init_((ckernel::MathFidelity)fidelity, matmul_ct_dim(src_a, src_b), matmul_rt_dim(src_a, src_b), false, false);

    _unified_state_math_fidelity_ = fidelity;
}

template <int THROTTLE_LEVEL>
inline void _unified_math_matmul_(UnifiedOperand src_a, UnifiedOperand src_b, [[maybe_unused]] int dest_index)
{
    _llk_math_matmul_block_(matmul_ct_dim(src_a, src_b), matmul_rt_dim(src_a, src_b));
}

inline void _unified_math_matmul_uninit_([[maybe_unused]] UnifiedOperand src_a, [[maybe_unused]] UnifiedOperand src_b)
{
}
