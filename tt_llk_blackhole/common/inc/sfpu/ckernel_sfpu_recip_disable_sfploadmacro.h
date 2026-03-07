// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
// SPDX-FileCopyrightText: © 2026 Jason Davies <jason@jasondavies.com>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_addrmod.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

// Forward declarations of functions defined later in ckernel_sfpu_recip.h
template <int max_iter>
sfpi_inline sfpi::vFloat _sfpu_reciprocal_(const sfpi::vFloat x);

template <bool APPROXIMATION_MODE>
inline void _init_sfpu_reciprocal_();

// ~7b precision reciprocal; sequential per-element (no macro pipelining).
inline void _calculate_reciprocal_fast_7b_(const int iterations)
{
    constexpr int v = p_sfpu::LREG0;
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        TTI_SFPLOAD(v, 0, ADDR_MOD_7, 0);                       // load as SRCB format
        TTI_SFPARECIP(0, v, v, 0);                              // v = arecip(v)
        TT_SFPSTORE(v, InstrModLoadStore::LO16, ADDR_MOD_6, 0); // store L16
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

// BF16 reciprocal with 8-bit correction; sequential per-element.
// Pseudocode: y=load; x=mad(0,0,y); y=arecip(y); y|=0x8000; y=x*y-1; t=hi16(y); y+=t; store
inline void _calculate_reciprocal_fast_8b_3c_(const int iterations)
{
    constexpr int y = p_sfpu::LREG3;
    constexpr int x = p_sfpu::LREG1;

    // L0 = 0x80000000 (BF16 sign bit for OR operation)
    TTI_SFPLOADI(p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_FLOATB, 0x8000);

#pragma GCC unroll 6
    for (int d = 0; d < iterations; d++)
    {
        TTI_SFPLOAD(y, 0, ADDR_MOD_7, 0);                        // y = load (SRCB mode)
        TTI_SFPMAD(p_sfpu::LCONST_0, p_sfpu::LCONST_0, y, x, 0); // x = 0*0 + y = y (copy)
        TTI_SFPARECIP(0, y, y, 0);                               // y = arecip(y)
        // Set bit 15 of y (equivalent to y |= 0x8000 in BF16 mantissa position)
        TTI_SFPOR(0, p_sfpu::LREG0, y, 0);           // y |= L0
        TTI_SFPMAD(x, y, p_sfpu::LCONST_neg1, y, 0); // y = x*y - 1
        // Extract upper 16 bits as correction: t = hi16(y) = y >> 16 via LO16 load of y
        TTI_SFPLOAD(x, InstrModLoadStore::LO16, ADDR_MOD_7, 0); // t = load LO16 of previous result (reuse x)
        // Actually we need hi16: use SFPSHFT2 to get upper 16 bits
        TTI_SFPSHFT2(-16 & 0xfff, 0, y, 6);                     // y >> 16 (SFPSHFT2_MOD1_SHFT_IMM)
        TTI_SFPIADD(0, y, x, sfpi::SFPIADD_MOD1_CC_NONE);       // x += y  (t = original_y + y>>16)
        TT_SFPSTORE(x, InstrModLoadStore::LO16, ADDR_MOD_6, 0); // store L16
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

// FP32 reciprocal (24b); per-element Newton-Raphson via existing _sfpu_reciprocal_<2>.
// _init_reciprocal_fast_24b_5c_ must call _init_sfpu_reciprocal_<false>() to set vConstFloatPrgm0=2.0f.
inline void _calculate_reciprocal_fast_24b_5c_(const int iterations)
{
#pragma GCC unroll 7
    for (int d = 0; d < iterations; d++)
    {
        const sfpi::vFloat x = sfpi::dst_reg[0];
        sfpi::dst_reg[0]     = _sfpu_reciprocal_<2>(x);
        sfpi::dst_reg++;
    }
}

inline void _init_reciprocal_fast_7b_()
{
    // No constants or macro sequences needed.
}

inline void _init_reciprocal_fast_8b_3c_()
{
    // No constants or macro sequences needed for sequential implementation.
}

inline void _init_reciprocal_fast_24b_5c_()
{
    // Set constant needed by _sfpu_reciprocal_<2>.
    _init_sfpu_reciprocal_<false>();
}

} // namespace sfpu
} // namespace ckernel
