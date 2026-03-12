// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
// SPDX-FileCopyrightText: © 2026 Jason Davies <jason@jasondavies.com>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_addrmod.h"
#include "sfpi.h"

// Blackhole version: uses ADDR_MOD_7 (load, auto-increment) and ADDR_MOD_6 (store, auto-increment).

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_fp32_to_uint16_()
{
    constexpr int v = p_sfpu::LREG0;
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::DEFAULT, ADDR_MOD_7, 0);
        TTI_SFPSWAP(0, p_sfpu::LCONST_0, v, 0xf);
        TTI_SFP_STOCH_RND(0, 0, 0, 0, v, 6);
        TTI_SFPSTORE(v, InstrModLoadStore::LO16, ADDR_MOD_6, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_uint16_to_fp16b_()
{
    constexpr int v = p_sfpu::LREG0;
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::LO16, ADDR_MOD_7, 0);
        TTI_SFPCAST(v, v, 0);
        TTI_SFP_STOCH_RND(0, 0, 0, 0, v, 1);
        TTI_SFPSTORE(v, InstrModLoadStore::DEFAULT, ADDR_MOD_6, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_int32_to_fp16b_()
{
    constexpr int v = p_sfpu::LREG2;
    constexpr int t = p_sfpu::LREG4;

    TTI_SFPLOADI(p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_USHORT, 0);
    TTI_SFPLOADI(p_sfpu::LREG1, sfpi::SFPLOADI_MOD0_FLOATB, 0xcf00);

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::INT32, ADDR_MOD_7, 0);
        TT_SFPABS(0, v, t, 0);
        TTI_SFPSHFT2(t, p_sfpu::LREG12, p_sfpu::LREG7, 5);
        TTI_SFPCAST(t, t, 0);
        TT_SFPSETSGN(0, t, v, 0);
        TTI_SFPMAD(0, p_sfpu::LCONST_1, v, v, 4);
        TTI_SFP_STOCH_RND(0, 0, 0, 0, v, 1);
        TTI_SFPSTORE(v, InstrModLoadStore::DEFAULT, ADDR_MOD_6, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_fp32_to_fp16b_()
{
    constexpr int a = p_sfpu::LREG0;
    constexpr int b = p_sfpu::LREG2;

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(a, InstrModLoadStore::DEFAULT, ADDR_MOD_7, 0);
        TTI_SFPSHFT2(-16 & 0xfff, 0, a, 6);
        TTI_SFPIADD(0, p_sfpu::LREG12, a, sfpi::SFPIADD_MOD1_CC_NONE);
        TTI_SFPLOAD(b, InstrModLoadStore::DEFAULT, ADDR_MOD_7, 0);
        TTI_SFPIADD(0, p_sfpu::LREG13, b, sfpi::SFPIADD_MOD1_CC_NONE);
        TTI_SFPSTORE(a, InstrModLoadStore::INT32, ADDR_MOD_6, 0);
        TTI_SFPIADD(0, a, b, sfpi::SFPIADD_MOD1_CC_NONE);
        TTI_SFPSTORE(b, InstrModLoadStore::FP16B, ADDR_MOD_6, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_uint16_to_fp32_()
{
    constexpr int v = p_sfpu::LREG0;
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::LO16, ADDR_MOD_7, 0);
        TTI_SFPCAST(v, v, 0);
        TTI_SFPSTORE(v, InstrModLoadStore::FP32, ADDR_MOD_6, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_int32_to_fp32_()
{
    constexpr int v = p_sfpu::LREG2;
    constexpr int t = p_sfpu::LREG4;

    TTI_SFPLOADI(p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_USHORT, 0);
    TTI_SFPLOADI(p_sfpu::LREG1, sfpi::SFPLOADI_MOD0_FLOATB, 0xcf00);

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::INT32, ADDR_MOD_7, 0);
        TT_SFPABS(0, v, t, 0);
        TTI_SFPSHFT2(t, p_sfpu::LREG12, p_sfpu::LREG7, 5);
        TTI_SFPCAST(t, t, 0);
        TT_SFPSETSGN(0, t, v, 0);
        TTI_SFPMAD(0, p_sfpu::LCONST_1, v, v, 4);
        TTI_SFPSTORE(v, InstrModLoadStore::FP32, ADDR_MOD_6, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_uint32_to_fp16b_()
{
    constexpr int v = p_sfpu::LREG2;

    TTI_SFPLOADI(p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_USHORT, 0);
    TTI_SFPLOADI(p_sfpu::LREG1, sfpi::SFPLOADI_MOD0_FLOATB, 0x4f00);

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::INT32, ADDR_MOD_7, 0);
        TTI_SFPSHFT2(v, p_sfpu::LREG12, p_sfpu::LREG7, 5);
        TT_SFPSETSGN(0, v, v, 1);
        TTI_SFPCAST(v, v, 0);
        TTI_SFPMAD(0, p_sfpu::LCONST_1, v, v, 4);
        TTI_SFP_STOCH_RND(0, 0, 0, 0, v, 1);
        TTI_SFPSTORE(v, InstrModLoadStore::DEFAULT, ADDR_MOD_6, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_uint32_to_fp32_()
{
    constexpr int a  = p_sfpu::LREG2;
    constexpr int b  = p_sfpu::LREG3;
    constexpr int L7 = p_sfpu::LREG7;

    TTI_SFPLOADI(p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_USHORT, 0);
    TTI_SFPLOADI(p_sfpu::LREG1, sfpi::SFPLOADI_MOD0_FLOATB, 0x4f00);

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(a, InstrModLoadStore::INT32, ADDR_MOD_7, 0);
        TTI_SFPSHFT2(a, p_sfpu::LREG12, L7, 5);
        TT_SFPSETSGN(0, a, a, 1);
        TTI_SFPCAST(a, b, 0);
        TTI_SFPMAD(0, p_sfpu::LCONST_1, b, b, 4);
        TTI_SFPSTORE(b, InstrModLoadStore::FP32, ADDR_MOD_6, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_uint16_to_uint32_()
{
    constexpr int v = p_sfpu::LREG0;
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(v, InstrModLoadStore::LO16, ADDR_MOD_7, 0);
        TTI_SFPSTORE(v, InstrModLoadStore::INT32, ADDR_MOD_6, 0);
    }
    TTI_SFPNOP;
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_uint32_to_uint16_()
{
    constexpr int a = p_sfpu::LREG0;
    constexpr int b = p_sfpu::LREG2;

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS + 1; d++)
    {
        if (d < ITERATIONS)
        {
            TTI_SFPLOAD(a, InstrModLoadStore::LO16, ADDR_MOD_7, 0);
        }
        else
        {
            TTI_SFPNOP;
        }

        if (d == 0)
        {
            TTI_SFPNOP;
        }
        else if (d < ITERATIONS)
        {
            TTI_SFPIADD(0, p_sfpu::LCONST_0, a, sfpi::SFPIADD_MOD1_ARG_2SCOMP_LREG_DST | sfpi::SFPIADD_MOD1_CC_NONE);
            TTI_SFPSHFT2(-16 & 0xfff, 0, a, 6);
            TTI_SFPLOAD(b, InstrModLoadStore::INT32, ADDR_MOD_7, 0);
            TTI_SFPOR(0, a, b, 0);
            TTI_SFPSTORE(b, InstrModLoadStore::LO16, ADDR_MOD_6, 0);
        }
        else
        {
            TTI_SFPIADD(0, p_sfpu::LCONST_0, a, sfpi::SFPIADD_MOD1_ARG_2SCOMP_LREG_DST | sfpi::SFPIADD_MOD1_CC_NONE);
            TTI_SFPSHFT2(-16 & 0xfff, 0, a, 6);
            TTI_SFPLOAD(b, InstrModLoadStore::INT32, ADDR_MOD_6, 0);
            TTI_SFPOR(0, a, b, 0);
            TTI_SFPSTORE(b, InstrModLoadStore::LO16, ADDR_MOD_6, 0);
        }
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_typecast_int32_to_uint16_()
{
    constexpr int a = p_sfpu::LREG0;
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(a, InstrModLoadStore::INT32, ADDR_MOD_7, 0);
        TTI_SFPCAST(a, a, 0);
        TTI_SFPSWAP(0, p_sfpu::LCONST_0, a, 0xf);
        TTI_SFPNOP;
        TTI_SFP_STOCH_RND(0, 0, 0, 0, a, 6);
        TTI_SFPSTORE(a, InstrModLoadStore::LO16, ADDR_MOD_6, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

// Init functions: set only scalar constants needed by respective calculate functions.

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
    sfpi::vConstIntPrgm0 = -31;
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
