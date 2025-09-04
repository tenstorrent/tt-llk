// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ckernel_trisc_common.h"
#include "cmath_common.h"

namespace ckernel
{
namespace sfpu
{
// Calculates RECIP for number of rows of output SFPU ops (Quasar = 2 rows)
template <bool APPROXIMATION_MODE>
inline void _calculate_reciprocal_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0); // load from dest into lreg[0], uses ADDR_MOD_7 (set to all zeroes)

    // SFPARECIP, approx version of reciprocal
    if constexpr (APPROXIMATION_MODE)
    {
        TTI_SFPARECIP(0, p_sfpu::LREG0, p_sfpu::LREG1, p_sfparecip::RECIP_MODE); // Read value from lreg[0], approximate recip, load back into lreg[1]
    }

    // Store from lreg[1] into dest register
    TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0);
}

template <bool APPROXIMATION_MODE>
inline void _calculate_reciprocal_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_reciprocal_sfp_rows_<APPROXIMATION_MODE>();
        _incr_counters_<0x0, 0x0, SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
}

} // namespace sfpu
} // namespace ckernel
