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
// Calculates ELU for number of rows of output SFPU ops (Quasar = 2 rows)
// For x >= 0: output = x (identity, preserved in LREG0 since CC prevents modification)
// For x < 0:  output = alpha * (exp(x) - 1)
inline void _calculate_elu_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0); // load from dest into lreg[0]

    TTI_SFPSETCC(0, p_sfpu::LREG0, 0); // set CC.Res = 1 for lanes where LREG0 is negative (x < 0)

    TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::EXP_MODE); // exp(x) -> LREG1 [only for negative lanes]

    TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG1, p_sfpu::LCONST_neg1, p_sfpu::LREG1, 0); // 1.0 * LREG1 + (-1.0) = exp(x) - 1 -> LREG1

    TTI_SFPMUL(p_sfpu::LREG2, p_sfpu::LREG1, p_sfpu::LCONST_0, p_sfpu::LREG0, 0); // LREG2 * LREG1 + 0 = alpha * (exp(x)-1) -> LREG0

    TTI_SFPENCC(0, 0); // reset CC.Res = 1 for all lanes

    TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0); // store LREG0 to dest (all lanes now active)
}

// Implements ELU activation: x when x >= 0, alpha * (exp(x) - 1) when x < 0
inline void _calculate_elu_(const int iterations, const std::uint32_t slope)
{
    TTI_SFPLOADI(p_sfpu::LREG2, 0 /*Float16_b*/, (slope >> 16)); // store alpha in LREG2
    TTI_SFPENCC(1, 2);                                           // enable cc
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_elu_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
    TTI_SFPENCC(0, 2); // disable cc
}

} // namespace sfpu
} // namespace ckernel
