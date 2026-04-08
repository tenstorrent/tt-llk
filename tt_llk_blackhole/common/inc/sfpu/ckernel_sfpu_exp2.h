// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "sfpi.h"

// exp2 implementation removed — depends on ckernel_sfpu_exp.h (Family 1 primitive)
// Generator must implement from SFPI instructions

namespace ckernel::sfpu
{

template <bool APPROXIMATION_MODE /*unused*/, bool is_fp32_dest_acc_en = false, int ITERATIONS = 8>
inline void _calculate_exp2_() {}

template <bool APPROXIMATION_MODE /*unused*/>
inline void _init_exp2_() {}

} // namespace ckernel::sfpu
