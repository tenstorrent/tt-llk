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
// Calculates threshold for number of rows of output SFPU ops (Quasar = 2 rows)
// If input <= threshold, replace with value; otherwise keep input unchanged
inline void _calculate_threshold_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0); // load from dest into lreg[0]

    // Set CC where LREG2 > LREG0 (threshold > input, i.e., input < threshold)
    TTI_SFPGT(0, p_sfpu::LREG0, p_sfpu::LREG2, 0x1 /*cc mode*/);

    // Conditionally copy replacement value from LREG3 into LREG0 where CC is set
    TTI_SFPMOV(p_sfpu::LREG3 /*src*/, p_sfpu::LREG0 /*dest*/, 0);

    TTI_SFPENCC(0, 0); // clear cc result reg

    // Store from lreg0 into dest register
    TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0);
}

// Implements threshold: returns value when input < threshold, otherwise returns input
inline void _calculate_threshold_(const int iterations, const std::uint32_t threshold, const std::uint32_t value)
{
    TTI_SFPLOADI(p_sfpu::LREG2, 0 /*Float16_b*/, (threshold >> 16)); // store threshold in LREG2
    TTI_SFPLOADI(p_sfpu::LREG3, 0 /*Float16_b*/, (value >> 16));     // store replacement value in LREG3
    TTI_SFPENCC(1, 2);                                                // enable cc
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_threshold_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
    TTI_SFPENCC(0, 2); // disable cc
}

} // namespace sfpu
} // namespace ckernel
