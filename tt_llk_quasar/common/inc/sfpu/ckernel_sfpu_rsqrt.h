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
// Initialization for RSQRT operation - no-op for Quasar (uses hardware instructions, no polynomial setup needed)
template <bool APPROXIMATION_MODE>
inline void _init_rsqrt_()
{
    // Quasar uses hardware SFPNONLINEAR instructions directly, no initialization needed
}

// Calculates RSQRT (reciprocal square root) for number of rows of output SFPU ops (Quasar = 2 rows)
// Computes 1/sqrt(x) by first computing sqrt(x) then taking its reciprocal
template <bool APPROXIMATION_MODE>
inline void _calculate_rsqrt_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0); // load from dest into lreg[0], uses ADDR_MOD_7 (set to all zeroes)

    // First compute sqrt(x) into LREG1
    if constexpr (APPROXIMATION_MODE)
    {
        TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::SQRT_MODE); // Read value from lreg[0], approximate sqrt, load back into lreg[1]

        // TTI_SFPNOP(0, 0, 0); // NOP to ensure sqrt completes before recip operation
        //  Then compute 1/sqrt(x) = recip(sqrt(x)) into LREG2
        TTI_SFPNONLINEAR(p_sfpu::LREG1, p_sfpu::LREG2, p_sfpnonlinear::RECIP_MODE); // Read value from lreg[1], approximate recip, load back into lreg[2]
    }

    // Store from lreg[2] into dest register
    TTI_SFPSTORE(p_sfpu::LREG2, 0, ADDR_MOD_7, 0, 0);
}

template <bool APPROXIMATION_MODE>
inline void _calculate_rsqrt_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_rsqrt_sfp_rows_<APPROXIMATION_MODE>();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
}

} // namespace sfpu
} // namespace ckernel
