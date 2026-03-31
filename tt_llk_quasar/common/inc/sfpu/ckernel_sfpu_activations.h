// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>

#include "ckernel_trisc_common.h"
#include "cmath_common.h"

namespace ckernel
{
namespace sfpu
{
// Calculates CELU for number of rows of output SFPU ops (Quasar = 2 rows)
// For x >= 0: output = x (identity, preserved in LREG0 since CC prevents modification)
// For x < 0:  output = alpha * (exp(x / alpha) - 1)
inline void _calculate_celu_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0); // load from dest into lreg[0]

    TTI_SFPSETCC(0, p_sfpu::LREG0, 0); // set CC.Res = 1 for lanes where LREG0 is negative (x < 0)

    TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG3, p_sfpu::LCONST_0, p_sfpu::LREG1, 0); // LREG0 * LREG3 = x * alpha_recip = x / alpha -> LREG1

    // SFPMUL is a 2-cycle MAD instruction; SFPNONLINEAR runs in the Simple EXU.
    // Insert a NOP to ensure the multiply result in LREG1 is available before SFPNONLINEAR reads it.
    TTI_SFPNOP(0, 0, 0);

    TTI_SFPNONLINEAR(p_sfpu::LREG1, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE); // exp(x / alpha) -> LREG1

    TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG1, p_sfpu::LCONST_neg1, p_sfpu::LREG1, 0); // 1.0 * LREG1 + (-1.0) = exp(x/alpha) - 1 -> LREG1

    TTI_SFPMUL(p_sfpu::LREG2, p_sfpu::LREG1, p_sfpu::LCONST_0, p_sfpu::LREG0, 0); // LREG2 * LREG1 + 0 = alpha * (exp(x/alpha)-1) -> LREG0

    TTI_SFPENCC(0, 0); // reset CC.Res = 1 for all lanes

    TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0); // store LREG0 to dest (all lanes now active)
}

// Implements CELU activation: x when x >= 0, alpha * (exp(x / alpha) - 1) when x < 0
inline void _calculate_celu_(const int iterations, const std::uint32_t alpha, const std::uint32_t alpha_recip)
{
    TTI_SFPLOADI(p_sfpu::LREG2, 0 /*Float16_b*/, (alpha >> 16));       // store alpha in LREG2
    TTI_SFPLOADI(p_sfpu::LREG3, 0 /*Float16_b*/, (alpha_recip >> 16)); // store alpha_recip in LREG3
    TTI_SFPENCC(1, 2);                                                  // enable cc
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_celu_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
    TTI_SFPENCC(0, 2); // disable cc
}

// Load hardsigmoid constants into persistent LREGs
// slope = 1/6 = 0.166015625 (BF16: 0x3E2A)
// offset = 0.5 (BF16: 0x3F00)
// upper_clamp = 1.0 (BF16: 0x3F80)
inline void _init_hardsigmoid_()
{
    TTI_SFPLOADI(p_sfpu::LREG4, 0 /*FP16_B*/, 0x3E2A); // slope = 1/6
    TTI_SFPLOADI(p_sfpu::LREG5, 0 /*FP16_B*/, 0x3F00); // offset = 0.5
    TTI_SFPLOADI(p_sfpu::LREG6, 0 /*FP16_B*/, 0x3F80); // upper clamp = 1.0
}

// Calculates Hardsigmoid for number of rows of output SFPU ops (Quasar = 2 rows)
// hardsigmoid(x) = clamp(x * (1/6) + 0.5, 0, 1)
inline void _calculate_hardsigmoid_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0); // load from dest into lreg[0]

    // Compute x * slope + offset: LREG0 = LREG0 * LREG4 + LREG5
    TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, 0);

    // NOP to let SFPMAD result (2-cycle latency) settle before SFPGT reads LREG0
    TTI_SFPNOP(0, 0, 0);

    // Clamp upper bound: if LREG0 > 1.0, set LREG0 = 1.0
    TTI_SFPGT(0, p_sfpu::LREG6, p_sfpu::LREG0, 0x1);
    TTI_SFPMOV(p_sfpu::LREG6 /*src*/, p_sfpu::LREG0 /*dest*/, 0); // copy 1.0 where value > 1.0

    // Reset CC so all lanes are active before the lower-bound check.
    // Without this, SFPSETCC would only examine lanes where CC was already set
    // (from SFPGT), missing negative values in non-active lanes.
    TTI_SFPENCC(0, 0); // clear cc -- all lanes active

    // Clamp lower bound: if LREG0 < 0, set LREG0 = 0 (ReLU)
    TTI_SFPSETCC(0, p_sfpu::LREG0, 0); // CC.Res = 1 for negative lanes
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0); // load 0.0 into negative lanes

    TTI_SFPENCC(0, 0); // clear cc -- all lanes active

    TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0); // store LREG0 to dest
}

// Implements hardsigmoid: clamp(x * (1/6) + 0.5, 0, 1)
// Constants (slope, offset, upper_clamp) must be loaded by _init_hardsigmoid_() before calling
inline void _calculate_hardsigmoid_(const int iterations)
{
    TTI_SFPENCC(1, 2); // enable cc
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_hardsigmoid_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
    TTI_SFPENCC(0, 2); // disable cc
}

} // namespace sfpu
} // namespace ckernel
