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
// Calculates EXP2 (2^x) for number of rows of output SFPU ops (Quasar = 2 rows)
// Algorithm: 2^x = e^(x * ln(2)), using SFPMUL for the ln(2) scaling and SFPNONLINEAR for e^x
template <bool APPROXIMATION_MODE>
inline void _calculate_exp2_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0); // load from dest into lreg[0], uses ADDR_MOD_7 (set to all zeroes)

    if constexpr (APPROXIMATION_MODE)
    {
        // SFPNONLINEAR EXP_MODE computes e^x. For 2^x we need e^(x*ln(2)).
        // Multiply x by ln(2) first, then apply SFPNONLINEAR.
        // Load ln(2) constant into LREG2 as FP16_B (0x3F31 ~ 0.6914 ~ ln(2))
        TTI_SFPLOADI(p_sfpu::LREG2, 0 /*FP16_B*/, 0x3F31);
        // lreg[0] = lreg[0] * lreg[2] + 0.0 = x * ln(2)
        TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG2, p_sfpu::LCONST_0, p_sfpu::LREG0, 0);
        // SFPMUL is a 2-cycle MAD instruction; SFPNONLINEAR runs in the Simple EXU.
        // Insert a NOP to ensure the multiply result is available before SFPNONLINEAR reads it.
        TTI_SFPNOP(0, 0, 0);
        TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE); // e^(x*ln2)=2^x
    }

    // Store from lreg[1] into dest register
    TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_7, 0, 0);
}

template <bool APPROXIMATION_MODE>
inline void _calculate_exp2_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_exp2_sfp_rows_<APPROXIMATION_MODE>();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
}

} // namespace sfpu
} // namespace ckernel
