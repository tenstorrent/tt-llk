// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ckernel_trisc_common.h"
#include "cmath_common.h"
#include "llk_defs.h"

namespace ckernel
{
namespace sfpu
{
// Calculates EXP for number of rows of output SFPU ops (Quasar = 2 rows)
template <ApproximationMode APPROX_MODE>
inline void _calculate_exp_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0); // load from dest into lreg[0], uses ADDR_MOD_7 (set to all zeroes)

    // SFPARECIP, approx version of reciprocal
    if constexpr (APPROX_MODE == ApproximationMode::Fast)
    {
        TTI_SFPARECIP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfparecip::EXP_MODE); // Read value from lreg[0], approximate recip, load back into lreg[1]
    }

    // Store from lreg[1] into dest register
    TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0);
}

template <ApproximationMode APPROX_MODE>
inline void _calculate_exp_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_exp_sfp_rows_<APPROX_MODE>();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
}

} // namespace sfpu
} // namespace ckernel
