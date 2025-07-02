// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu_exp.h"
#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel::sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS = 8>
inline void _calculate_expm1_()
{
    const bool SCALE_EN            = false; // Expm1 does not use scale.
    const bool SKIP_POSITIVE_CHECK = false; // Expm1 does not skip positive check.

    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat v   = sfpi::dst_reg[0];
        v                = _calculate_exponential_body_improved_<APPROXIMATION_MODE, SCALE_EN, SKIP_POSITIVE_CHECK>(v);
        sfpi::dst_reg[0] = v - 1.0f;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
inline void _init_expm1_()
{
    const bool FAST_APPROX = false; // Expm1 does not use fast approximation.
    _init_exponential_<APPROXIMATION_MODE, FAST_APPROX>();
}

} // namespace ckernel::sfpu
