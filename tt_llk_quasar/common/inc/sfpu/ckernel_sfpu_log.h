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
// Load polynomial coefficients for ln(x) Chebyshev approximation into LREGs
// Coefficients for 3rd-order Horner form: x*(x*(x*A + B) + C) + D
// where x is the mantissa normalized to [1, 2)
inline void _init_log_()
{
    // Coefficient B = -0.7116 as BF16 into LREG7 (persistent across iterations)
    TTI_SFPLOADI(p_sfpu::LREG7, 0, 0xBF36);

    // Coefficient C = 2.0871 as BF16 into LREG4
    TTI_SFPLOADI(p_sfpu::LREG4, 0, 0x4005);

    // Coefficient D = -1.4753 as BF16 into LREG5
    TTI_SFPLOADI(p_sfpu::LREG5, 0, 0xBFBC);

    // ln(2) = 0.6929 as BF16 into LREG6
    TTI_SFPLOADI(p_sfpu::LREG6, 0, 0x3F31);
}

// Calculates natural logarithm for number of rows of output SFPU ops (Quasar = 2 rows)
// Algorithm: ln(x) = ln(mantissa) + exponent * ln(2)
// where ln(mantissa) is a 3rd-order Chebyshev polynomial in Horner form
inline void _calculate_log_sfp_rows_()
{
    // Load input from Dest into LREG0
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

    // Normalize mantissa to [1, 2) by setting exponent to 127 (bias)
    TTI_SFPSETEXP(127, p_sfpu::LREG0, p_sfpu::LREG1, 1); // LREG1 = normalized x

    // Load coefficient A = 0.1058 inline as BF16
    TTI_SFPLOADI(p_sfpu::LREG2, 0, 0x3DD8); // LREG2 = coeff A

    // Horner polynomial evaluation: x * (x * (x * A + B) + C) + D
    // Step 1: x*A + B -> LREG2
    TTI_SFPMAD(p_sfpu::LREG1, p_sfpu::LREG2, p_sfpu::LREG7, p_sfpu::LREG2, 0);

    // Step 2: x*(x*A+B) + C -> LREG2
    TTI_SFPMAD(p_sfpu::LREG1, p_sfpu::LREG2, p_sfpu::LREG4, p_sfpu::LREG2, 0);

    // Step 3: x*(x*(x*A+B)+C) + D -> LREG2 (series_result)
    TTI_SFPMAD(p_sfpu::LREG1, p_sfpu::LREG2, p_sfpu::LREG5, p_sfpu::LREG2, 0);

    // Extract unbiased exponent as two's complement integer
    TTI_SFPEXEXP(p_sfpu::LREG0, p_sfpu::LREG3, 0); // LREG3 = exponent (two's complement int32)

    // Convert two's complement int32 to sign-magnitude int32 using SFPCAST mode 2
    // This replaces the manual SFPNOT/SFPIADD/SFPSETSGN chain which had a CC corruption bug:
    // SFPIADD with mod=0 updates CC based on result sign, clobbering the negative-exponent CC
    TTI_SFPCAST(p_sfpu::LREG3, p_sfpu::LREG3, 2); // LREG3 = sign-magnitude int32

    // Convert sign-magnitude int32 to FP32
    TTI_SFPCAST(p_sfpu::LREG3, p_sfpu::LREG3, 0); // LREG3 = float(exponent)

    // Combine: result = exponent * ln(2) + series_result
    TTI_SFPMAD(p_sfpu::LREG3, p_sfpu::LREG6, p_sfpu::LREG2, p_sfpu::LREG2, 0);

    // Handle special case: ln(0) = -infinity
    TTI_SFPSETCC(0, p_sfpu::LREG0, 0x6);    // CC = 1 where input is zero
    TTI_SFPLOADI(p_sfpu::LREG2, 0, 0xFF80);  // -infinity (BF16 encoding: sign=1, exp=0xFF, mantissa=0)
    TTI_SFPENCC(0, 0);                        // clear CC

    // Store result to Dest
    TTI_SFPSTORE(p_sfpu::LREG2, 0, ADDR_MOD_7, 0, 0);
}

// Implements natural logarithm (ln) for all rows
inline void _calculate_log_(const int iterations)
{
    TTI_SFPENCC(1, 2); // enable conditional execution
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_log_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
    TTI_SFPENCC(0, 2); // disable conditional execution
}

} // namespace sfpu
} // namespace ckernel
