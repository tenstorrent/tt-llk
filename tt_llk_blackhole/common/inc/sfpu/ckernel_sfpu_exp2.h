// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_sfpu_exp.h"
#include "llk_defs.h"
#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel::sfpu
{

template <ckernel::ApproximationMode APPROX_MODE, int ITERATIONS>
inline void _calculate_exp2_()
{
    static_assert(
        (APPROX_MODE == ckernel::ApproximationMode::Precise) || (APPROX_MODE == ckernel::ApproximationMode::Approximate),
        "Exp2 only supports Precise or Approximate modes");
    const std::uint16_t exp_base_scale_factor = p_sfpu::kCONST_1_FP16B;

    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat v = sfpi::dst_reg[0];
        // log(2) = 0.6931471805;
        v = v * 0.6931471805f;
        // exp = e^(v)
        sfpi::vFloat exp = _calculate_exponential_piecewise_<APPROX_MODE, false, false>(v, exp_base_scale_factor);
        sfpi::dst_reg[0] = exp;
        sfpi::dst_reg++;
    }
}

template <ckernel::ApproximationMode APPROX_MODE>
inline void _init_exp2_()
{
    const std::uint32_t EXP_BASE_SCALE_FACTOR = 0x3F800000;
    // Exp2 does not use fast approximation.
    static_assert(
        (APPROX_MODE == ckernel::ApproximationMode::Precise) || (APPROX_MODE == ckernel::ApproximationMode::Approximate),
        "Exp2 only supports Precise or Approximate modes");
    _init_exponential_<APPROX_MODE, EXP_BASE_SCALE_FACTOR>();
}

} // namespace ckernel::sfpu
