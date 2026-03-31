// SPDX-FileCopyrightText: (c) 2026 Tenstorrent AI ULC
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
// No-op init for sign (no LUT or special setup needed)
template <bool APPROXIMATION_MODE>
inline void _init_sign_()
{
}

// Calculates sign for number of rows of output SFPU ops (Quasar = 2 rows)
// sign(x) = +1.0 if x > 0, -1.0 if x < 0, 0.0 if x == 0
template <bool APPROXIMATION_MODE>
inline void _calculate_sign_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0); // load from dest into lreg[0]

    // Default result = +1.0 for all lanes (CC.Res = 1 for all lanes at this point)
    TTI_SFPLOADI(p_sfpu::LREG3, 0, 0x3F80); // LREG3 = +1.0 (BF16)

    // Check if x is negative: CC.Res = 1 where LREG0 < 0
    TTI_SFPSETCC(0, p_sfpu::LREG0, 0);

    // For negative lanes: result = -1.0
    TTI_SFPMOV(p_sfpu::LREG2, p_sfpu::LREG3, 0); // copy -1.0 into LREG3 where CC set

    TTI_SFPENCC(0, 0); // reset CC -- all lanes active

    // Check if x is zero: CC.Res = 1 where LREG0 == 0
    TTI_SFPSETCC(0, p_sfpu::LREG0, 0x6);

    // For zero lanes: result = 0.0 (overrides both +1.0 and -1.0)
    TTI_SFPLOADI(p_sfpu::LREG3, 0, 0x0000); // LREG3 = 0.0 where CC set

    TTI_SFPENCC(0, 0); // reset CC -- all lanes active

    // Store result from LREG3 to dest
    TTI_SFPSTORE(p_sfpu::LREG3, 0, ADDR_MOD_7, 0, 0);
}

// Implements element-wise sign: +1.0 for positive, -1.0 for negative, 0.0 for zero
template <bool APPROXIMATION_MODE>
inline void _calculate_sign_(const int iterations)
{
    TTI_SFPLOADI(p_sfpu::LREG2, 0, 0xBF80); // -1.0 (BF16) -- persistent constant
    TTI_SFPENCC(1, 2);                       // enable cc
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_sign_sfp_rows_<APPROXIMATION_MODE>();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
    TTI_SFPENCC(0, 2); // disable cc
}

} // namespace sfpu
} // namespace ckernel
