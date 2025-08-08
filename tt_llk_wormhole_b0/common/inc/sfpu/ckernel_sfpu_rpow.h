// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"
#include "sfpu/ckernel_sfpu_exp.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_rpow_(const uint32_t log_val)
{
    sfpi::vFloat log_val_v = Converter::as_float(log_val);
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat in     = sfpi::dst_reg[0];
        sfpi::vFloat result = _calculate_exponential_body_<APPROXIMATION_MODE>(in * log_val_v);

        sfpi::dst_reg[0] = result;

        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
