// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_unpack_matmul.h"
#include "unified_operand.hpp"
#include "unified_state.hpp"

inline void _unified_unpack_matmul_init_(UnifiedOperand src_a, UnifiedOperand src_b)
{
    _llk_unpack_matmul_init_(src_a.hint, src_b.hint, 0, matmul_ct_dim(src_a, src_b), matmul_rt_dim(src_a, src_b), matmul_kt_dim(src_a, src_b));
}

inline void _unified_unpack_matmul_(UnifiedOperand src_a, UnifiedOperand src_b, int idx_a, int idx_b)
{
    _llk_unpack_matmul_(src_a.hint, src_b.hint, idx_a, idx_b, matmul_ct_dim(src_a, src_b), matmul_rt_dim(src_a, src_b), matmul_kt_dim(src_a, src_b));
}

inline void _unified_unpack_matmul_uninit_([[maybe_unused]] UnifiedOperand src_a, [[maybe_unused]] UnifiedOperand src_b)
{
}
