// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_pack.h"
#include "unified_operand.hpp"
#include "unified_state.hpp"

inline void _unified_pack_init_(UnifiedOperand dst)
{
    _llk_pack_init_<ckernel::p_pacr::PACK0>(dst.hint, 1);
}

inline void _unified_pack_(uint32_t dest_index, [[maybe_unused]] UnifiedOperand dst, uint32_t l1_index)
{
    _llk_pack_<ckernel::p_pacr::PACK0>(dest_index, l1_index);
}

inline void _unified_pack_uninit_([[maybe_unused]] UnifiedOperand dst)
{
}
