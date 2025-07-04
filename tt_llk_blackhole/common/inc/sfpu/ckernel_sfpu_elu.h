// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu_converter.h"
#include "ckernel_sfpu_exp.h"
#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel::sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_elu_(uint slope)
{
    const bool SCALE_EN                  = false; // Elu does not use scale.
    const bool SKIP_POSITIVE_CHECK       = false; // Elu does not skip positive check.
    const uint16_t exp_base_scale_factor = 0x3F80;

    sfpi::vFloat s = Converter::as_float(slope);
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat v = sfpi::dst_reg[0];

        v_if (v < 0.0f)
        {
            sfpi::vFloat v_exp = _calculate_exponential_body_improved_<APPROXIMATION_MODE, SCALE_EN, SKIP_POSITIVE_CHECK>(v, exp_base_scale_factor);
            v                  = s * (v_exp - 1.0f);
        }
        v_endif;

        sfpi::dst_reg[0] = v;

        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
inline void _init_elu_()
{
    const bool FAST_APPROX = false; // Elu does not use fast approximation.
    _init_exponential_<APPROXIMATION_MODE, FAST_APPROX>();
}

} // namespace ckernel::sfpu
