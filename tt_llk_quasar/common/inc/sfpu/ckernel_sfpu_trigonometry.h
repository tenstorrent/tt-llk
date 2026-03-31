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

// Calculates sin(x) using Maclaurin series for number of rows of output SFPU ops (Quasar = 2 rows)
// Algorithm: range reduce x to [-pi, pi] via half-period counting, evaluate Maclaurin series,
// then correct sign based on parity of half-period count.
// Expects LREG7 = 1/pi and LREG6 = pi to be preloaded by the outer function.
template <bool APPROXIMATION_MODE>
inline void _calculate_sine_maclaurin_series_sfp_rows_()
{
    // Load input from Dest into LREG0
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

    // --- Range Reduction ---
    // scaled = x * (1/pi) to get number of half-periods
    TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG7, p_sfpu::LCONST_0, p_sfpu::LREG1, 0); // LREG1 = x * (1/pi)

    // Convert to integer (FP32 -> SMAG32 via RTNE)
    TTI_SFPCAST(p_sfpu::LREG1, p_sfpu::LREG2, 4); // LREG2 = SMAG32(round(LREG1))

    // Save integer for odd/even check later
    TTI_SFPMOV(p_sfpu::LREG2, p_sfpu::LREG3, 0); // LREG3 = integer n (SMAG32 copy)

    // Convert integer back to float (SMAG32 -> FP32)
    TTI_SFPCAST(p_sfpu::LREG2, p_sfpu::LREG2, 0); // LREG2 = float(n)

    // Fractional part: frac = scaled - float(n)
    // SFPMAD: dest = a * b + c; with InstrMod bit 1 set, SrcC is negated
    // LREG1 = LREG1 * 1.0 + (-LREG2) = scaled - float(n)
    TTI_SFPMAD(p_sfpu::LREG1, p_sfpu::LCONST_1, p_sfpu::LREG2, p_sfpu::LREG1, 0x2);

    // Scale fractional part by pi: f_rad = frac * pi
    TTI_SFPMUL(p_sfpu::LREG1, p_sfpu::LREG6, p_sfpu::LCONST_0, p_sfpu::LREG1, 0); // LREG1 = frac * pi = f_rad

    // === Maclaurin Series: sin(x) = x - x^3/3! + x^5/5! - x^7/7! ... ===

    // Initialize output = x (first term of series)
    TTI_SFPMOV(p_sfpu::LREG1, p_sfpu::LREG0, 0); // LREG0 = f_rad (output accumulator)

    // x^2 (needed for building higher powers)
    TTI_SFPMUL(p_sfpu::LREG1, p_sfpu::LREG1, p_sfpu::LCONST_0, p_sfpu::LREG2, 0); // LREG2 = x^2

    // --- Term: -x^3/3! ---
    TTI_SFPMUL(p_sfpu::LREG1, p_sfpu::LREG2, p_sfpu::LCONST_0, p_sfpu::LREG4, 0); // LREG4 = x^3
    TTI_SFPLOADI(p_sfpu::LREG5, 0, 0xBE2A);                                        // LREG5 = -1/6 (BF16)
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG0, 0);    // LREG0 += x^3 * (-1/6)

    // --- Term: +x^5/5! ---
    TTI_SFPMUL(p_sfpu::LREG4, p_sfpu::LREG2, p_sfpu::LCONST_0, p_sfpu::LREG4, 0); // LREG4 = x^5
    TTI_SFPLOADI(p_sfpu::LREG5, 0, 0x3C08);                                        // LREG5 = 1/120 (BF16)
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG0, 0);    // LREG0 += x^5 * (1/120)

    // --- Term: -x^7/7! ---
    TTI_SFPMUL(p_sfpu::LREG4, p_sfpu::LREG2, p_sfpu::LCONST_0, p_sfpu::LREG4, 0); // LREG4 = x^7
    TTI_SFPLOADI(p_sfpu::LREG5, 0, 0xB950);                                        // LREG5 = -1/5040 (BF16)
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG0, 0);    // LREG0 += x^7 * (-1/5040)

    if constexpr (!APPROXIMATION_MODE)
    {
        // --- Term: +x^9/9! ---
        TTI_SFPMUL(p_sfpu::LREG4, p_sfpu::LREG2, p_sfpu::LCONST_0, p_sfpu::LREG4, 0); // LREG4 = x^9
        TTI_SFPLOADI(p_sfpu::LREG5, 0, 0x3638);                                        // LREG5 = 1/362880 (BF16)
        TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG0, 0);    // LREG0 += x^9 * (1/362880)

        // --- Term: -x^11/11! ---
        TTI_SFPMUL(p_sfpu::LREG4, p_sfpu::LREG2, p_sfpu::LCONST_0, p_sfpu::LREG4, 0); // LREG4 = x^11
        TTI_SFPLOADI(p_sfpu::LREG5, 0, 0xB2D7);                                        // LREG5 = -1/39916800 (BF16)
        TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG0, 0);    // LREG0 += x^11 * (-1/39916800)
    }

    // === Sign Correction (negate if n is odd) ===
    // Convert SMAG32 integer n to 2's complement for bitwise AND
    TTI_SFPCAST(p_sfpu::LREG3, p_sfpu::LREG3, 2); // LREG3 = INT32 (2's complement)

    // Load integer 1 mask for AND
    TTI_SFPLOADI(p_sfpu::LREG5, 0x8, 0x0000); // LREG5 upper 16 = 0
    TTI_SFPLOADI(p_sfpu::LREG5, 0xA, 0x0001); // LREG5 lower 16 = 1 -> LREG5 = 0x00000001

    // AND to isolate bit 0
    TTI_SFPAND(p_sfpu::LREG5, p_sfpu::LREG3); // LREG3 = LREG3 & 0x1

    // Set CC where n is odd (bit 0 = 1, value != 0)
    TTI_SFPSETCC(0, p_sfpu::LREG3, 2); // InstrMod=2: CC.Res = (LREG3 != 0)

    // Negate where CC is set (odd n means flip sign)
    TTI_SFPMOV(p_sfpu::LREG0, p_sfpu::LREG0, 1); // LREG0 = -LREG0 (sign inversion)

    // Clear CC
    TTI_SFPENCC(0, 0);

    // Store result to Dest
    TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0);
}

// Calculates cos(x) using Maclaurin series for number of rows of output SFPU ops (Quasar = 2 rows)
// Same range reduction as sine, but evaluates cosine Maclaurin series.
// Expects LREG7 = 1/pi and LREG6 = pi to be preloaded by the outer function.
template <bool APPROXIMATION_MODE>
inline void _calculate_cosine_maclaurin_series_sfp_rows_()
{
    // Load input from Dest into LREG0
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

    // --- Range Reduction (identical to sine) ---
    TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG7, p_sfpu::LCONST_0, p_sfpu::LREG1, 0); // LREG1 = x * (1/pi)
    TTI_SFPCAST(p_sfpu::LREG1, p_sfpu::LREG2, 4);                                  // LREG2 = SMAG32(round(LREG1))
    TTI_SFPMOV(p_sfpu::LREG2, p_sfpu::LREG3, 0);                                   // LREG3 = integer n (SMAG32 copy)
    TTI_SFPCAST(p_sfpu::LREG2, p_sfpu::LREG2, 0);                                  // LREG2 = float(n)
    TTI_SFPMAD(p_sfpu::LREG1, p_sfpu::LCONST_1, p_sfpu::LREG2, p_sfpu::LREG1, 0x2); // LREG1 = scaled - float(n) = frac
    TTI_SFPMUL(p_sfpu::LREG1, p_sfpu::LREG6, p_sfpu::LCONST_0, p_sfpu::LREG1, 0);   // LREG1 = frac * pi = f_rad

    // === Maclaurin Series: cos(x) = 1 - x^2/2! + x^4/4! - x^6/6! ... ===

    // Initialize output = 1.0
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0x3F80); // LREG0 = 1.0 (BF16)

    // x^2
    TTI_SFPMUL(p_sfpu::LREG1, p_sfpu::LREG1, p_sfpu::LCONST_0, p_sfpu::LREG2, 0); // LREG2 = x^2
    TTI_SFPMOV(p_sfpu::LREG2, p_sfpu::LREG4, 0);                                   // LREG4 = x^2 (current power)

    // --- Term: -x^2/2! ---
    TTI_SFPLOADI(p_sfpu::LREG5, 0, 0xBF00);                                        // LREG5 = -0.5 (BF16)
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG0, 0);    // LREG0 += x^2 * (-0.5)

    // --- Term: +x^4/4! ---
    TTI_SFPMUL(p_sfpu::LREG4, p_sfpu::LREG2, p_sfpu::LCONST_0, p_sfpu::LREG4, 0); // LREG4 = x^4
    TTI_SFPLOADI(p_sfpu::LREG5, 0, 0x3D2A);                                        // LREG5 = 1/24 (BF16)
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG0, 0);    // LREG0 += x^4 * (1/24)

    // --- Term: -x^6/6! ---
    TTI_SFPMUL(p_sfpu::LREG4, p_sfpu::LREG2, p_sfpu::LCONST_0, p_sfpu::LREG4, 0); // LREG4 = x^6
    TTI_SFPLOADI(p_sfpu::LREG5, 0, 0xBAB6);                                        // LREG5 = -1/720 (BF16)
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG0, 0);    // LREG0 += x^6 * (-1/720)

    if constexpr (!APPROXIMATION_MODE)
    {
        // --- Term: +x^8/8! ---
        TTI_SFPMUL(p_sfpu::LREG4, p_sfpu::LREG2, p_sfpu::LCONST_0, p_sfpu::LREG4, 0); // LREG4 = x^8
        TTI_SFPLOADI(p_sfpu::LREG5, 0, 0x37D0);                                        // LREG5 = 1/40320 (BF16)
        TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG0, 0);    // LREG0 += x^8 * (1/40320)

        // --- Term: -x^10/10! ---
        TTI_SFPMUL(p_sfpu::LREG4, p_sfpu::LREG2, p_sfpu::LCONST_0, p_sfpu::LREG4, 0); // LREG4 = x^10
        TTI_SFPLOADI(p_sfpu::LREG5, 0, 0xB493);                                        // LREG5 = -1/3628800 (BF16)
        TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG0, 0);    // LREG0 += x^10 * (-1/3628800)
    }

    // === Sign Correction (identical to sine) ===
    TTI_SFPCAST(p_sfpu::LREG3, p_sfpu::LREG3, 2);    // SMAG32 -> INT32 (2's complement)
    TTI_SFPLOADI(p_sfpu::LREG5, 0x8, 0x0000);         // LREG5 upper 16 = 0
    TTI_SFPLOADI(p_sfpu::LREG5, 0xA, 0x0001);         // LREG5 lower 16 = 1 -> LREG5 = 0x00000001
    TTI_SFPAND(p_sfpu::LREG5, p_sfpu::LREG3);         // LREG3 = LREG3 & 0x1
    TTI_SFPSETCC(0, p_sfpu::LREG3, 2);                // CC.Res = (LREG3 != 0) -> odd n
    TTI_SFPMOV(p_sfpu::LREG0, p_sfpu::LREG0, 1);     // LREG0 = -LREG0 (sign flip where CC set)
    TTI_SFPENCC(0, 0);                                 // Clear CC

    // Store result to Dest
    TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0, 0);
}

// Implements sin(x) for all rows in the tile
template <bool APPROXIMATION_MODE>
inline void _calculate_sine_(const int iterations)
{
    // Load persistent constants (full 32-bit precision for range reduction accuracy)
    TTI_SFPLOADI(p_sfpu::LREG7, 0x8, 0x3EA2); // 1/pi upper 16 bits
    TTI_SFPLOADI(p_sfpu::LREG7, 0xA, 0xF983); // 1/pi lower 16 bits -> LREG7 = 0.31830988...
    TTI_SFPLOADI(p_sfpu::LREG6, 0x8, 0x4049); // pi upper 16 bits
    TTI_SFPLOADI(p_sfpu::LREG6, 0xA, 0x0FDB); // pi lower 16 bits -> LREG6 = 3.14159265...

    TTI_SFPENCC(1, 2); // enable conditional execution
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_sine_maclaurin_series_sfp_rows_<APPROXIMATION_MODE>();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
    TTI_SFPENCC(0, 2); // disable conditional execution
}

// Implements cos(x) for all rows in the tile
template <bool APPROXIMATION_MODE>
inline void _calculate_cosine_(const int iterations)
{
    // Load persistent constants (full 32-bit precision for range reduction accuracy)
    TTI_SFPLOADI(p_sfpu::LREG7, 0x8, 0x3EA2); // 1/pi upper 16 bits
    TTI_SFPLOADI(p_sfpu::LREG7, 0xA, 0xF983); // 1/pi lower 16 bits -> LREG7 = 0.31830988...
    TTI_SFPLOADI(p_sfpu::LREG6, 0x8, 0x4049); // pi upper 16 bits
    TTI_SFPLOADI(p_sfpu::LREG6, 0xA, 0x0FDB); // pi lower 16 bits -> LREG6 = 3.14159265...

    TTI_SFPENCC(1, 2); // enable conditional execution
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_cosine_maclaurin_series_sfp_rows_<APPROXIMATION_MODE>();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
    TTI_SFPENCC(0, 2); // disable conditional execution
}

// No-op initialization for inverse hyperbolic functions on Quasar.
// Blackhole initializes software sqrt constants, but Quasar uses
// hardware SFPNONLINEAR SQRT_MODE which requires no setup.
// Log polynomial coefficients are loaded inline per-iteration.
inline void _init_inverse_hyperbolic_()
{
}

// Calculates acosh(x) = log(x + sqrt(x^2 - 1)) for number of rows of output SFPU ops (Quasar = 2 rows)
// Returns NaN for x < 1, 0 for x == 1, computed result for x > 1.
// Computes main result for all lanes, then overwrites boundary lanes.
inline void _calculate_acosh_sfp_rows_()
{
    // Load input x from Dest into LREG0
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

    // Compute (x - 1.0) for boundary detection, stored in LREG3
    // SFPMAD: LREG3 = LREG0 * 1.0 + (-1.0) = x - 1
    TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LCONST_1, p_sfpu::LCONST_1, p_sfpu::LREG3, 0x2);

    // === Main computation (all lanes, boundary lanes overwritten later) ===

    // LREG1 = x^2
    TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG0, p_sfpu::LCONST_0, p_sfpu::LREG1, 0);

    // LREG1 = x^2 - 1.0 (via LREG1 * 1.0 + (-1.0))
    TTI_SFPMAD(p_sfpu::LREG1, p_sfpu::LCONST_1, p_sfpu::LCONST_1, p_sfpu::LREG1, 0x2);

    // LREG2 = sqrt(x^2 - 1) via hardware sqrt
    TTI_SFPNONLINEAR(p_sfpu::LREG1, p_sfpu::LREG2, p_sfpnonlinear::SQRT_MODE);

    // LREG1 = sqrt(x^2 - 1) + x (via 1.0 * LREG2 + LREG0)
    TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG2, p_sfpu::LREG0, p_sfpu::LREG1, 0);

    // === Inline log(LREG1) -> result in LREG2 ===

    // Normalize mantissa to [1, 2) by setting exponent to 127
    TTI_SFPSETEXP(127, p_sfpu::LREG1, p_sfpu::LREG4, 1); // LREG4 = normalized mantissa

    // Load coefficient A = 0.1058 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG5, 0, 0x3DD8); // LREG5 = A

    // Load coefficient B = -0.7116 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0xBF36); // LREG0 = B (clobbers input x, no longer needed)

    // Horner polynomial: x*(x*(x*A + B) + C) + D
    // Step 1: x*A + B -> LREG5
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG5, 0);

    // Load coefficient C = 2.0871 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0x4005); // LREG0 = C

    // Step 2: x*(x*A+B) + C -> LREG5
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG5, 0);

    // Load coefficient D = -1.4753 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0xBFBC); // LREG0 = D

    // Step 3: x*(x*(x*A+B)+C) + D = series_result -> LREG5
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG5, 0);

    // Extract unbiased exponent as two's complement integer
    TTI_SFPEXEXP(p_sfpu::LREG1, p_sfpu::LREG4, 0); // LREG4 = exponent (2's complement)

    // Convert two's complement -> sign-magnitude
    TTI_SFPCAST(p_sfpu::LREG4, p_sfpu::LREG4, 2);

    // Convert sign-magnitude -> FP32
    TTI_SFPCAST(p_sfpu::LREG4, p_sfpu::LREG4, 0); // LREG4 = float(exponent)

    // Load ln(2) = 0.6929 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0x3F31); // LREG0 = ln(2)

    // Combine: result = exponent * ln(2) + series_result -> LREG2
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG0, p_sfpu::LREG5, p_sfpu::LREG2, 0);

    // Handle log(0) = -inf: check if log input was zero
    TTI_SFPSETCC(0, p_sfpu::LREG1, 0x6);    // CC = 1 where log input is zero
    TTI_SFPLOADI(p_sfpu::LREG2, 0, 0xFF80);  // -infinity (BF16)
    TTI_SFPENCC(0, 0);                        // clear CC

    // === Boundary: overwrite where x == 1.0 (LREG3 == 0, i.e. x - 1 == 0) ===
    TTI_SFPSETCC(0, p_sfpu::LREG3, 0x6);     // CC = 1 where (x-1) is zero
    TTI_SFPLOADI(p_sfpu::LREG2, 0, 0x0000);  // result = 0.0 for x==1 lanes
    TTI_SFPENCC(0, 0);                        // clear CC

    // === Boundary: overwrite where x < 1.0 (LREG3 < 0, i.e. x - 1 < 0) ===
    TTI_SFPSETCC(0, p_sfpu::LREG3, 0);       // CC = 1 where (x-1) is negative
    TTI_SFPLOADI(p_sfpu::LREG2, 0, 0x7FC0);  // result = NaN for x<1 lanes
    TTI_SFPENCC(0, 0);                        // clear CC

    // Store result to Dest
    TTI_SFPSTORE(p_sfpu::LREG2, 0, ADDR_MOD_7, 0, 0);
}

// Implements acosh(x) for all rows in the tile
inline void _calculate_acosh_(const int iterations)
{
    TTI_SFPENCC(1, 2); // enable conditional execution
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_acosh_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
    TTI_SFPENCC(0, 2); // disable conditional execution
}

// Calculates asinh(x) = sign(x) * log(|x| + sqrt(x^2 + 1)) for number of rows of output SFPU ops (Quasar = 2 rows)
// Defined for all real x. Uses abs + log + sign restoration.
inline void _calculate_asinh_sfp_rows_()
{
    // Load input x from Dest into LREG0
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

    // Save original x for sign restoration later
    TTI_SFPMOV(p_sfpu::LREG0, p_sfpu::LREG3, 0); // LREG3 = x (copy for sign check)

    // |x| in LREG0 (FP32 abs, clears sign bit)
    TTI_SFPABS(p_sfpu::LREG0, p_sfpu::LREG0, 1);

    // LREG1 = |x|^2
    TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG0, p_sfpu::LCONST_0, p_sfpu::LREG1, 0);

    // LREG1 = |x|^2 + 1.0 (via 1.0 * LREG1 + 1.0)
    TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG1, p_sfpu::LCONST_1, p_sfpu::LREG1, 0);

    // LREG2 = sqrt(|x|^2 + 1) via hardware sqrt
    TTI_SFPNONLINEAR(p_sfpu::LREG1, p_sfpu::LREG2, p_sfpnonlinear::SQRT_MODE);

    // LREG1 = sqrt(|x|^2 + 1) + |x| (via 1.0 * LREG2 + LREG0)
    TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG2, p_sfpu::LREG0, p_sfpu::LREG1, 0);

    // === Inline log(LREG1) -> result in LREG2 ===

    // Normalize mantissa to [1, 2) by setting exponent to 127
    TTI_SFPSETEXP(127, p_sfpu::LREG1, p_sfpu::LREG4, 1); // LREG4 = normalized mantissa

    // Load coefficient A = 0.1058 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG5, 0, 0x3DD8); // LREG5 = A

    // Load coefficient B = -0.7116 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0xBF36); // LREG0 = B (clobbers |x|, no longer needed)

    // Horner polynomial: x*(x*(x*A + B) + C) + D
    // Step 1: x*A + B -> LREG5
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG5, 0);

    // Load coefficient C = 2.0871 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0x4005); // LREG0 = C

    // Step 2: x*(x*A+B) + C -> LREG5
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG5, 0);

    // Load coefficient D = -1.4753 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0xBFBC); // LREG0 = D

    // Step 3: x*(x*(x*A+B)+C) + D = series_result -> LREG5
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG5, 0);

    // Extract unbiased exponent as two's complement integer
    TTI_SFPEXEXP(p_sfpu::LREG1, p_sfpu::LREG4, 0); // LREG4 = exponent (2's complement)

    // Convert two's complement -> sign-magnitude
    TTI_SFPCAST(p_sfpu::LREG4, p_sfpu::LREG4, 2);

    // Convert sign-magnitude -> FP32
    TTI_SFPCAST(p_sfpu::LREG4, p_sfpu::LREG4, 0); // LREG4 = float(exponent)

    // Load ln(2) = 0.6929 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0x3F31); // LREG0 = ln(2)

    // Combine: result = exponent * ln(2) + series_result -> LREG2
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG0, p_sfpu::LREG5, p_sfpu::LREG2, 0);

    // Handle log(0) = -inf special case (should not occur for asinh since arg >= 1, but safe)
    TTI_SFPSETCC(0, p_sfpu::LREG1, 0x6);    // CC = 1 where log input is zero
    TTI_SFPLOADI(p_sfpu::LREG2, 0, 0xFF80);  // -infinity (BF16)
    TTI_SFPENCC(0, 0);                        // clear CC

    // === Sign correction: negate result where original x was negative ===
    TTI_SFPSETCC(0, p_sfpu::LREG3, 0);       // CC = 1 where original x < 0
    TTI_SFPMOV(p_sfpu::LREG2, p_sfpu::LREG2, 1); // negate LREG2 where CC is set
    TTI_SFPENCC(0, 0);                        // clear CC

    // Store result to Dest
    TTI_SFPSTORE(p_sfpu::LREG2, 0, ADDR_MOD_7, 0, 0);
}

// Implements asinh(x) for all rows in the tile
inline void _calculate_asinh_(const int iterations)
{
    TTI_SFPENCC(1, 2); // enable conditional execution
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_asinh_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
    TTI_SFPENCC(0, 2); // disable conditional execution
}

// No-op initialization for atanh on Quasar.
// Blackhole initializes software reciprocal constants, but Quasar uses
// hardware SFPNONLINEAR RECIP_MODE which requires no setup.
inline void _init_atanh_()
{
}

// Calculates atanh(x) = 0.5 * ln((1 + x) / (1 - x)) for number of rows of output SFPU ops (Quasar = 2 rows)
// Returns NaN for |x| > 1, signed infinity for |x| == 1, computed result for |x| < 1.
// Computes main result for all lanes, then overwrites boundary lanes.
inline void _calculate_atanh_sfp_rows_()
{
    // Load input x from Dest into LREG0
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);

    // Save copy of original x for sign extraction in boundary handling
    TTI_SFPMOV(p_sfpu::LREG0, p_sfpu::LREG3, 0); // LREG3 = x

    // Compute |x| and save for boundary checks after main computation
    TTI_SFPABS(p_sfpu::LREG0, p_sfpu::LREG0, 1); // LREG0 = |x|
    TTI_SFPMOV(p_sfpu::LREG0, p_sfpu::LREG7, 0); // LREG7 = |x| (preserved across log)

    // === Main computation (all lanes, boundary lanes overwritten later) ===

    // num = 1.0 + x -> LREG1
    // SFPADD: LREG1 = 1.0 * LREG3 + 1.0 = x + 1
    TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG3, p_sfpu::LCONST_1, p_sfpu::LREG1, 0);

    // den = 1.0 - x -> LREG2
    // First compute x - 1: LREG2 = LREG3 * 1.0 + (-1.0) = x - 1
    TTI_SFPMAD(p_sfpu::LREG3, p_sfpu::LCONST_1, p_sfpu::LCONST_1, p_sfpu::LREG2, 0x2);
    // Negate: LREG2 = -(x - 1) = 1 - x
    TTI_SFPMOV(p_sfpu::LREG2, p_sfpu::LREG2, 1);

    // Save den for sign correction after reciprocal
    TTI_SFPMOV(p_sfpu::LREG2, p_sfpu::LREG6, 0); // LREG6 = den = 1 - x

    // Compute 1/den via hardware reciprocal -> LREG4
    TTI_SFPNONLINEAR(p_sfpu::LREG2, p_sfpu::LREG4, p_sfpnonlinear::RECIP_MODE);

    // Sign correction: set sign of recip(den) to sign of den
    // SFPSETSGN(0, lreg_c, lreg_dest, 0): sets sign of RG[VC] to sign of RG[VD], result in RG[VD]
    // lreg_c = LREG4 (magnitude source = |recip|), lreg_dest = LREG6 (sign source = den, also receives result)
    TTI_SFPSETSGN(0, p_sfpu::LREG4, p_sfpu::LREG6, 0);
    // LREG6 now holds |recip| with sign of den; move to LREG4
    TTI_SFPMOV(p_sfpu::LREG6, p_sfpu::LREG4, 0); // LREG4 = sign-corrected 1/den

    // Compute num * (1/den) = (1+x)/(1-x) -> LREG1
    TTI_SFPMUL(p_sfpu::LREG1, p_sfpu::LREG4, p_sfpu::LCONST_0, p_sfpu::LREG1, 0);

    // === Inline log(LREG1) -> result in LREG2 ===

    // Normalize mantissa to [1, 2) by setting exponent to 127
    TTI_SFPSETEXP(127, p_sfpu::LREG1, p_sfpu::LREG4, 1); // LREG4 = normalized mantissa

    // Load coefficient A = 0.1058 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG5, 0, 0x3DD8); // LREG5 = A

    // Load coefficient B = -0.7116 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0xBF36); // LREG0 = B

    // Horner polynomial: x*(x*(x*A + B) + C) + D
    // Step 1: x*A + B -> LREG5
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG5, 0);

    // Load coefficient C = 2.0871 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0x4005); // LREG0 = C

    // Step 2: x*(x*A+B) + C -> LREG5
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG5, 0);

    // Load coefficient D = -1.4753 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0xBFBC); // LREG0 = D

    // Step 3: x*(x*(x*A+B)+C) + D = series_result -> LREG5
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG0, p_sfpu::LREG5, 0);

    // Extract unbiased exponent as two's complement integer
    TTI_SFPEXEXP(p_sfpu::LREG1, p_sfpu::LREG4, 0); // LREG4 = exponent (2's complement)

    // Convert two's complement -> sign-magnitude
    TTI_SFPCAST(p_sfpu::LREG4, p_sfpu::LREG4, 2);

    // Convert sign-magnitude -> FP32
    TTI_SFPCAST(p_sfpu::LREG4, p_sfpu::LREG4, 0); // LREG4 = float(exponent)

    // Load ln(2) = 0.6929 (BF16)
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0x3F31); // LREG0 = ln(2)

    // Combine: result = exponent * ln(2) + series_result -> LREG2
    TTI_SFPMAD(p_sfpu::LREG4, p_sfpu::LREG0, p_sfpu::LREG5, p_sfpu::LREG2, 0);

    // Handle log(0) = -inf: check if log input was zero
    TTI_SFPSETCC(0, p_sfpu::LREG1, 0x6);    // CC = 1 where log input is zero
    TTI_SFPLOADI(p_sfpu::LREG2, 0, 0xFF80);  // -infinity (BF16)
    TTI_SFPENCC(0, 0);                        // clear CC

    // === Multiply by 0.5 ===
    TTI_SFPLOADI(p_sfpu::LREG0, 0, 0x3F00);  // LREG0 = 0.5 (BF16)
    TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG2, p_sfpu::LCONST_0, p_sfpu::LREG2, 0); // LREG2 = 0.5 * log_result

    // === Boundary overwrite ===

    // Compute (|x| - 1.0) for boundary detection -> LREG0
    // SFPMAD: LREG0 = LREG7 * 1.0 + (-1.0) = |x| - 1
    TTI_SFPMAD(p_sfpu::LREG7, p_sfpu::LCONST_1, p_sfpu::LCONST_1, p_sfpu::LREG0, 0x2);

    // Boundary: |x| > 1.0 -> NaN
    // CC = 1 where (|x| - 1) > 0 (positive, which includes +0.0 but that's corrected next)
    TTI_SFPSETCC(0, p_sfpu::LREG0, 0x8);     // CC = 1 where LREG0 positive (sign bit clear)
    TTI_SFPLOADI(p_sfpu::LREG2, 0, 0x7FC0);  // NaN (BF16)
    TTI_SFPENCC(0, 0);                        // clear CC

    // Boundary: |x| == 1.0 -> signed infinity (sign matches original x)
    // Load +infinity, then set its sign to match original x
    TTI_SFPLOADI(p_sfpu::LREG4, 0, 0x7F80);  // LREG4 = +inf (BF16)
    // SFPSETSGN: set sign of LREG4 (magnitude=+inf) to sign of LREG3 (original x), result in LREG3
    TTI_SFPSETSGN(0, p_sfpu::LREG4, p_sfpu::LREG3, 0);
    // Now LREG3 = sign(x) * inf
    // CC = 1 where (|x| - 1) is zero, i.e. |x| == 1
    TTI_SFPSETCC(0, p_sfpu::LREG0, 0x6);     // CC = 1 where (|x|-1) == 0
    TTI_SFPMOV(p_sfpu::LREG3, p_sfpu::LREG2, 0); // copy signed infinity into result for those lanes
    TTI_SFPENCC(0, 0);                        // clear CC

    // Store result to Dest
    TTI_SFPSTORE(p_sfpu::LREG2, 0, ADDR_MOD_7, 0, 0);
}

// Implements atanh(x) for all rows in the tile
inline void _calculate_atanh_(const int iterations)
{
    TTI_SFPENCC(1, 2); // enable conditional execution
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_atanh_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
    TTI_SFPENCC(0, 2); // disable conditional execution
}

} // namespace sfpu
} // namespace ckernel
