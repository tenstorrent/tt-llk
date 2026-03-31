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
// Calculates hardtanh for number of rows of output SFPU ops (Quasar = 2 rows)
// Hardtanh(x) = clamp(x, min_val, max_val)
// LREG2 = min_val, LREG3 = max_val (loaded by outer function)
inline void _calculate_hardtanh_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0); // load from dest into lreg[0]

    // Upper clamp: if x > max_val, x = max_val
    TTI_SFPGT(0, p_sfpu::LREG3, p_sfpu::LREG0, 0x1 /*cc mode*/); // CC.Res = 1 where LREG0 > LREG3 (x > max_val)
    TTI_SFPMOV(p_sfpu::LREG3 /*src*/, p_sfpu::LREG0 /*dest*/, 0); // copy max_val where x > max_val

    TTI_SFPENCC(0, 0); // reset CC for all lanes

    // Lower clamp: if x < min_val, x = min_val
    TTI_SFPGT(0, p_sfpu::LREG0, p_sfpu::LREG2, 0x1 /*cc mode*/); // CC.Res = 1 where LREG2 > LREG0 (min_val > x)
    TTI_SFPMOV(p_sfpu::LREG2 /*src*/, p_sfpu::LREG0 /*dest*/, 0); // copy min_val where x < min_val

    TTI_SFPENCC(0, 0); // reset CC for all lanes

    // Store from lreg0 into dest register
    TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0);
}

// Implements hardtanh: clamp(x, min_val, max_val)
inline void _calculate_hardtanh_(const int iterations, const std::uint32_t min_val, const std::uint32_t max_val)
{
    TTI_SFPLOADI(p_sfpu::LREG2, 0 /*Float16_b*/, (min_val >> 16)); // store min_val in LREG2
    TTI_SFPLOADI(p_sfpu::LREG3, 0 /*Float16_b*/, (max_val >> 16)); // store max_val in LREG3
    TTI_SFPENCC(1, 2);                                              // enable cc
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_hardtanh_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
    TTI_SFPENCC(0, 2); // disable cc
}

} // namespace sfpu
} // namespace ckernel
