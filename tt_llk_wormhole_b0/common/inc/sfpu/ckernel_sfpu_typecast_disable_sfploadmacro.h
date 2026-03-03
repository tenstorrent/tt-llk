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

// fp32 → uint16: clamp to [0, 65535] then stochastic round
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_fp32_to_uint16_()
{
    constexpr int v = p_sfpu::LREG0;
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 0);
        TTI_SFPSWAP(0, p_sfpu::LCONST_0, v, 0xf); // v = max(v, 0.0)
        TTI_SFP_STOCH_RND(0, 0, 0, 0, v, 6);       // v L16 = stochrnd_fp32_to_uint16(v)
        TTI_SFPSTORE(v, InstrModLoadStore::LO16, ADDR_MOD_2, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

// uint16 → fp16b: cast integer to float then stochastic round to fp16b
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_uint16_to_fp16b_()
{
    constexpr int v = p_sfpu::LREG0;
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::LO16, ADDR_MOD_3, 0);
        TTI_SFPCAST(v, v, 0);                       // v = float(v)
        TTI_SFP_STOCH_RND(0, 0, 0, 0, v, 1);        // v L16 = stochrnd_fp32_to_fp16b(v)
        TTI_SFPSTORE(v, InstrModLoadStore::DEFAULT, ADDR_MOD_2, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

// int32 → fp16b: abs → shift-sign → cast → setsgn → mad → stochrnd
// Uses: LREG0=0.0, LREG1=-2^31, LREG12=-31 (from _init_typecast_int32_to_fp16b_)
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_int32_to_fp16b_()
{
    constexpr int v = p_sfpu::LREG2;
    constexpr int t = p_sfpu::LREG4;

    TTI_SFPLOADI(p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_USHORT, 0);
    TTI_SFPLOADI(p_sfpu::LREG1, sfpi::SFPLOADI_MOD0_FLOATB, 0xcf00); // -2^31

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::INT32, ADDR_MOD_3, 0);
        TT_SFPABS(0, v, t, 0);                                           // t = abs(v)
        TTI_SFPSHFT2(t, p_sfpu::LREG12, p_sfpu::LREG7, 5);             // L7 = t >> 31 (SHFT2_MOD1_SHFT_LREG)
        TTI_SFPCAST(t, t, 0);                                            // t = float(t)
        TT_SFPSETSGN(0, t, v, 0);                                        // v = setsgn(t, v)
        TTI_SFPMAD(0, p_sfpu::LCONST_1, v, v, 4);                       // v = L[L7]*1.0 + v (SFPMAD_MOD1_INDIRECT_VA)
        TTI_SFPNOP;                                                       // SFPMAD has 2-cycle latency
        TTI_SFP_STOCH_RND(0, 0, 0, v, v, 1);                            // v L16 = stochrnd_fp32_to_fp16b(v)
        TTI_SFPSTORE(v, InstrModLoadStore::DEFAULT, ADDR_MOD_2, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

// fp32 → fp16b: round-to-nearest using integer arithmetic on raw bits
// Adds 0x7fff + rounding bit (LSB of upper word) to the raw bits, then truncates.
// Uses: LREG12=1, LREG13=0x7fff (from _init_typecast_fp32_to_fp16b_)
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_fp32_to_fp16b_()
{
    constexpr int a = p_sfpu::LREG0;
    constexpr int b = p_sfpu::LREG2;

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(a, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 0);
        // a_hi = a >> 16 (upper 16 bits, used as round bit extraction)
        TTI_SFPSHFT2(-16 & 0xfff, 0, a, 6);                           // a >>= 16 (SHFT2_MOD1_SHFT_IMM)
        // round_bit = a & 1 (LSB of upper half)
        TTI_SFPAND(0, p_sfpu::LREG12, a, 0);                            // a &= 1 (extract rounding bit)
        // b = original + 0x7fff
        TTI_SFPLOAD(b, InstrModLoadStore::DEFAULT, ADDR_MOD_3, 0);     // reload original (no addr advance)
        TTI_SFPIADD(0, p_sfpu::LREG13, b, sfpi::SFPIADD_MOD1_CC_NONE); // b += 0x7fff
        TTI_SFPIADD(0, a, b, sfpi::SFPIADD_MOD1_CC_NONE);              // b += round bit
        TTI_SFPSTORE(b, InstrModLoadStore::FP16B, ADDR_MOD_2, 0);      // store fp16b
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

// uint16 → fp32: cast integer to float, store as fp32
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_uint16_to_fp32_()
{
    constexpr int v = p_sfpu::LREG0;
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::LO16, ADDR_MOD_3, 0);
        TTI_SFPCAST(v, v, 0);                         // v = float(v)
        TTI_SFPSTORE(v, InstrModLoadStore::FP32, ADDR_MOD_2, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

// int32 → fp32: abs → shift-sign → cast → setsgn → mad → store fp32
// Uses: LREG0=0.0, LREG1=-2^31, LREG12=-31 (from _init_typecast_int32_to_fp32_)
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_int32_to_fp32_()
{
    constexpr int v = p_sfpu::LREG2;
    constexpr int t = p_sfpu::LREG4;

    TTI_SFPLOADI(p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_USHORT, 0);
    TTI_SFPLOADI(p_sfpu::LREG1, sfpi::SFPLOADI_MOD0_FLOATB, 0xcf00); // -2^31

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::INT32, ADDR_MOD_3, 0);
        TT_SFPABS(0, v, t, 0);
        TTI_SFPSHFT2(t, p_sfpu::LREG12, p_sfpu::LREG7, 5);           // L7 = t >> 31
        TTI_SFPCAST(t, t, 0);
        TT_SFPSETSGN(0, t, v, 0);
        TTI_SFPMAD(0, p_sfpu::LCONST_1, v, v, 4);                     // v = L[L7]*1.0 + v
        TTI_SFPSTORE(v, InstrModLoadStore::FP32, ADDR_MOD_2, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

// uint32 → fp16b: shift-sign → setsgn → cast → mad → stochrnd
// Uses: LREG0=0.0, LREG1=2^31 (from _init_typecast_uint32_to_fp16b_)
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_uint32_to_fp16b_()
{
    constexpr int v = p_sfpu::LREG2;

    TTI_SFPLOADI(p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_USHORT, 0);
    TTI_SFPLOADI(p_sfpu::LREG1, sfpi::SFPLOADI_MOD0_FLOATB, 0x4f00); // 2^31

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::INT32, ADDR_MOD_3, 0);
        TTI_SFPSHFT2(v, p_sfpu::LREG12, p_sfpu::LREG7, 5);           // L7 = v >> 31
        TT_SFPSETSGN(0, v, v, 1);                                     // v = setsgn(v, 0) (clear sign bit)
        TTI_SFPCAST(v, v, 0);
        TTI_SFPMAD(0, p_sfpu::LCONST_1, v, v, 4);                     // v = L[L7]*1.0 + v
        TTI_SFPNOP;                                                     // SFPMAD has 2-cycle latency
        TTI_SFP_STOCH_RND(0, 0, 0, v, v, 1);                          // v L16 = stochrnd_fp32_to_fp16b(v)
        TTI_SFPSTORE(v, InstrModLoadStore::DEFAULT, ADDR_MOD_2, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

// uint32 → fp32: setsgn(0) → cast → shift-sign → mad → store fp32
// Uses: LREG0=0.0, LREG1=2^31, LREG12=-31 (via sfpi::vConstIntPrgm0=-31)
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_uint32_to_fp32_()
{
    constexpr int a  = p_sfpu::LREG2;
    constexpr int b  = p_sfpu::LREG3;
    constexpr int L7 = p_sfpu::LREG7;

    TTI_SFPLOADI(p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_USHORT, 0);
    TTI_SFPLOADI(p_sfpu::LREG1, sfpi::SFPLOADI_MOD0_FLOATB, 0x4f00); // 2^31

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(a, InstrModLoadStore::INT32, ADDR_MOD_3, 0);
        TTI_SFPSHFT2(a, p_sfpu::LREG12, L7, 5);                       // L7 = a >> 31
        TT_SFPSETSGN(0, a, a, 1);                                      // a = abs(a) (clear sign)
        TTI_SFPCAST(a, b, 0);                                          // b = float(a)
        TTI_SFPMAD(0, p_sfpu::LCONST_1, b, b, 4);                     // b = L[L7]*1.0 + b
        TTI_SFPSTORE(b, InstrModLoadStore::FP32, ADDR_MOD_2, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

// uint16 → uint32: load LO16, store as INT32 (zero-extended)
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_uint16_to_uint32_()
{
    constexpr int v = p_sfpu::LREG0;
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::LO16, ADDR_MOD_3, 0);
        TTI_SFPSTORE(v, InstrModLoadStore::INT32, ADDR_MOD_2, 0);
    }
    TTI_SFPNOP;
}

// uint32 → uint16: load full 32-bit value, store lower 16 bits (truncation)
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_uint32_to_uint16_()
{
    constexpr int v = p_sfpu::LREG0;
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::INT32, ADDR_MOD_3, 0);
        TTI_SFPSTORE(v, InstrModLoadStore::LO16, ADDR_MOD_2, 0);
    }
    TTI_SFPNOP;
}

// int32 → uint16: cast to fp32, clamp to ≥0, stochastic round to uint16
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_int32_to_uint16_()
{
    constexpr int a = p_sfpu::LREG0;
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(a, InstrModLoadStore::INT32, ADDR_MOD_3, 0);
        TTI_SFPCAST(a, a, 0);                         // a = float(a)
        TTI_SFPSWAP(0, p_sfpu::LCONST_0, a, 0xf);    // a = max(0.0, a)
        TTI_SFPNOP;
        TTI_SFP_STOCH_RND(0, 0, 0, 0, a, 6);          // a L16 = stochrnd_fp32_to_uint16(a)
        TTI_SFPSTORE(a, InstrModLoadStore::LO16, ADDR_MOD_2, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

// Init functions: set only the scalar constants needed by respective calculate functions.
// No InstructionTemplate/Macro/SFPCONFIG programming (LOADMACRO-specific).

template <bool APPROXIMATION_MODE>
inline void _init_typecast_fp32_to_fp16b_()
{
    sfpi::vConstIntPrgm0 = 1;
    sfpi::vConstIntPrgm1 = 0x7fff;
}

template <bool APPROXIMATION_MODE>
inline void _init_typecast_uint16_to_uint32_()
{
}

template <bool APPROXIMATION_MODE>
inline void _init_typecast_uint32_to_fp32_()
{
    sfpi::vConstIntPrgm0 = -31; // LREG12 = -31 for SFPSHFT2 shift amount
}

template <bool APPROXIMATION_MODE>
inline void _init_typecast_int32_to_fp32_()
{
    sfpi::vConstIntPrgm0 = -31;
}

template <bool APPROXIMATION_MODE>
inline void _init_typecast_int32_to_fp16b_()
{
    sfpi::vConstIntPrgm0 = -31;
}

template <bool APPROXIMATION_MODE>
inline void _init_typecast_uint16_to_fp32_()
{
}

template <bool APPROXIMATION_MODE>
inline void _init_typecast_uint16_to_fp16b_()
{
}

template <bool APPROXIMATION_MODE>
inline void _init_typecast_uint32_to_fp16b_()
{
    sfpi::vConstIntPrgm0 = -31; // LREG12 = -31 for SFPSHFT2 shift amount
}

template <bool APPROXIMATION_MODE>
inline void _init_typecast_fp32_to_uint16_()
{
}

template <bool APPROXIMATION_MODE>
inline void _init_typecast_uint32_to_uint16_()
{
}

template <bool APPROXIMATION_MODE>
inline void _init_typecast_int32_to_uint16_()
{
}

} // namespace sfpu
} // namespace ckernel
