// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
// SPDX-FileCopyrightText: © 2025 Jason Davies <jason@jasondavies.com>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu_rsqrt_compat.h"
#include "lltt.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

// Computes the reciprocal of a floating point value x.
template <int max_iter = 2>
sfpi_inline sfpi::vFloat _sfpu_reciprocal_(const sfpi::vFloat x)
{
    // sfpi::approx_recip(x) will return ±0 for x = ±inf or x ≥ ±2**126, and ±inf for x = ±0.
    sfpi::vFloat y = sfpi::approx_recip(x);

    // Optionally improve the approximation using Newton-Raphson.
    if (max_iter > 0)
    {
        // Normally, t = 2.0 - x * y, but we negate this (and negate again using y = y * -t later).
        // On Blackhole, when x=0 and y=infinity (and vice versa), t=+NaN regardless of the operand signs.
        // Negating the meaning of t makes it easier to detect NaN using a trivial sign check t>=0.
        // Equivalently, we could use v_if (t >= 2.0) instead, but SFPI doesn't support SFPLE/SFPGT at the moment.
        sfpi::vFloat t = x * y - sfpi::vConstFloatPrgm0;

        if (max_iter > 1)
        {
            sfpi::vFloat y1 = y * -t - sfpi::vConst0;
            // If t=NaN, then t>=0.  This check consumes the SFPNOP slot of the preceding SFPMAD.
            v_if (t < 0)
            {
                t = x * y1 - sfpi::vConstFloatPrgm0;
                y = y1 * -t - sfpi::vConst0;
            }
            v_endif;
        }
        else
        {
            // If t=NaN, then t>=0.  This check cannot be hidden in a SFPNOP slot as it depends on the result of the preceding SFPMAD.
            v_if (t < 0)
            {
                y = y * -t - sfpi::vConst0;
            }
            v_endif;
        }
    }

    return y;
}

// Approximate reciprocal, with throughput of 1c/32.
inline void _calculate_reciprocal_fast_7b_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        TTI_SFPLOADMACRO((0 << 2) | 0, 0, ADDR_MOD_6, 0);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
}

// BF16 reciprocal, with throughput of 3c/32.
inline void _calculate_reciprocal_fast_8b_3c_(const int iterations)
{
    // We use SFPMAD_MOD1_INDIRECT_VD to schedule SFPMAD and write to L[L7],
    // with L7=x throughout.
    //
    // We also set L0=0x80000000 throughout, which allows us to store it as
    // 0x8000 (BF16), and then load using MOD0_LO16_ONLY to write this to the
    // low bits of y.
    //
    // For all macros, we disable UsesLoadMod0ForStore, and set
    // StoreMod0=MOD0_FMT_SRCB.
    //
    // In pseudocode, the following steps allow the LSB of the BF16 result to
    // be corrected:
    //
    // y = load()
    // x = 0*0 + y
    // y = arecip(y)
    // y = y | (1<<15) # via load of 0x8000 with MOD0_LO16_ONLY
    // y = x * y - 1
    // t = y >> 16     # via load of y with MOD0_LO16
    // y += t
    // store(y)
    //
    // Notation: [x] means scheduled by SFPLOADMACRO with VD=x.
    //
    // t | Load           | Simple                 | MAD                    | Store   |
    // - | -------------- | ---------------------- | ---------------------- |-------- |
    // 0 | [y] SRCB       |                        |                        |         |
    // 1 |                | [y] = arecip([y])      | [y] x = mad(0, 0, [y]) | [y] L0  |
    // 2 | [y1] LO16_ONLY |                        |                        |         |
    // 3 |                |                        | [y1] = mad(x, y1, -1)  |         |
    // 4 |                |                        |                        |         |
    // 5 |                |                        |                        | [y1]    |
    // 6 |                |                        |                        |         |
    // 7 | [t] LO16       |                        |                        |         |
    // 8 |                | [y1] L16 = iadd(t, y1) |                        |         |
    // 9 |                |                        |                        | [t] L16 |

    constexpr int x           = p_sfpu::LREG1;
    constexpr int t           = p_sfpu::LREG1;
    constexpr int offset      = 0;
    constexpr int prev_offset = -4 & 0x3ff;

    TTI_SFPLOADI(p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_FLOATB, 0x8000);
    TTI_SFPLOADI(p_sfpu::LREG7, SFPLOADI_MOD0_USHORT, x);

#pragma GCC unroll 10
    for (int d = 0; d < iterations + 2; d++)
    {
        int y = 3 + (d % 3);

        if (d < iterations)
        {
            // MOD0_FMT_SRCB
            TT_SFPLOADMACRO((0 << 2) | (y & 3), 0, ADDR_MOD_7, offset | (y >> 2));
        }
        else
        {
            TTI_SFPNOP;
        }
        if (d <= 1)
        {
            TTI_SFPNOP;
        }
        else if (d < iterations)
        {
            // MOD0_FMT_LO16
            TT_SFPLOADMACRO((2 << 2) | (t & 3), 9, ADDR_MOD_7, prev_offset | (t >> 2));
        }
        else
        {
            // MOD0_FMT_LO16
            TT_SFPLOADMACRO((2 << 2) | (t & 3), 9, ADDR_MOD_6, prev_offset | (t >> 2));
        }
        if (d < iterations)
        {
            // MOD0_FMT_LO16_ONLY
            TT_SFPLOADMACRO((1 << 2) | (y & 3), 14, ADDR_MOD_6, offset | (y >> 2));
        }
        else
        {
            TTI_SFPNOP;
        }
    }
    TTI_SFPNOP;
}

// FP32 reciprocal, with throughput of 5c/32.
inline void _calculate_reciprocal_fast_24b_5c_(const int iterations)
{
    // Pseudocode:
    //
    // y = arecip(x)
    // e = 1 - x*y
    // t = e * e + e
    // t2 = t * e + e    # e**3 + e**2 + e
    // t2 = min(t2, 1.0) # replace NaN with 1.0
    // y = t2 * y + y    # y = y * (e**3 + e**2 + e + 1)
    //                   # if y = ±0 or ±inf, then y = y+y
    //
    // Notation: [x] means scheduled by SFPLOADMACRO with VD=x.
    //
    // t  | Load | Simple                 | MAD                     | Store   |
    // -- | -----| ---------------------- | ----------------------- |-------- |
    //  0 | [y]  |                        |                         |         |
    //  1 |      | [y] = arecip(y)        |                         |         |
    //  2 | [e]  |                        |                         |         |
    //  3 |      | [e] L16 = arecip(e)    | e = mad(-e, y, 1.0)     |         |
    //  4 |      |                        |                         |         |
    //  5 |      |                        | [e] = mad(e, e, e)      | [e]     |
    //  6 | [t2] |                        |                         |         |
    //  7 |      |                        | [t2] = mad(t2, e, t2)   | [y] L16 |
    //  8 |      |                        |                         |         |
    //  9 | [z]  | [t2] = swap(t2, 1.0)   |                         |         |
    // 10 |      |                        |                         |         |
    // 11 |      |                        | [z] L16 = mad(t2, z, z) |         |
    // 12 |      |                        |                         |         |
    // 13 |      |                        |                         | [z] L16 |

    lltt::replay(0, 4);
    TTI_SFPLOAD(7, 0, ADDR_MOD_6, 0);

#pragma GCC unroll 7
    for (int d = 0; d < iterations - 1; d++)
    {
        lltt::replay(0, 5);
    }

    TTI_SFPNOP;
    lltt::replay(1, 1);
    TTI_SFPNOP;
    lltt::replay(3, 2);

    TTI_SFPNOP;
    TTI_SFPNOP;
    TTI_SFPNOP;
    TTI_SFPNOP;
}

template <bool APPROXIMATION_MODE, int ITERATIONS, bool is_fp32_dest_acc_en>
inline void _calculate_reciprocal_internal_(const int iterations)
{
    if constexpr (APPROXIMATION_MODE)
    {
        _calculate_reciprocal_fast_7b_(iterations);
    }
    else if constexpr (is_fp32_dest_acc_en)
    {
        _calculate_reciprocal_fast_24b_5c_(iterations);
    }
    else
    {
        _calculate_reciprocal_fast_8b_3c_(iterations);
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS, bool is_fp32_dest_acc_en, bool legacy_compat = false>
inline void _calculate_reciprocal_(const int iterations)
{
    if constexpr (legacy_compat)
    {
        _calculate_reciprocal_compat_<APPROXIMATION_MODE, ITERATIONS, is_fp32_dest_acc_en>(iterations);
    }
    else
    {
        _calculate_reciprocal_internal_<APPROXIMATION_MODE, ITERATIONS, is_fp32_dest_acc_en>(iterations);
    }
}

// ~7b precision; 1c/element
inline void _init_reciprocal_fast_7b_()
{
    // Notation: [x] means scheduled by SFPLOADMACRO with VD=x.
    //
    // t | Load | Simple                | Store   |
    // - | ---- | --------------------- | ------- |
    // 0 | [x]  |                       |         |
    // 1 |      | [x] L16 = arecip([x]) |         |
    // 2 |      |                       | [x] L16 |

    TTI_SFPARECIP(0, 0, 12, 0);

    constexpr uint simple_bits = 0x00 | 0x40 | (0 << 3) | 4;
    constexpr uint mad_bits    = 0;
    constexpr uint round_bits  = 0;
    constexpr uint store_bits  = 0x00 | 0x40 | (1 << 3) | 3;

    TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_LOWER, (mad_bits << 8) | simple_bits);
    TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_UPPER, (store_bits << 8) | round_bits);

    TTI_SFPCONFIG(0, 4, 0);

    // Misc: {UsesLoadMod0ForStore=1, WaitForElapsedInstructions=1} for macro 0.
    TTI_SFPCONFIG(0x110, 8, 1);
}

inline void _init_reciprocal_fast_8b_3c_()
{
    constexpr int x = p_sfpu::LREG1;
    constexpr int t = p_sfpu::LREG1;

    // InstructionTemplate[0]
    TTI_SFPARECIP(0, 0, 12, 0);

    // InstructionTemplate[1]
    TTI_SFPMAD(p_sfpu::LCONST_0, p_sfpu::LCONST_0, 0, 13, 8); // SFPMAD_MOD1_INDIRECT_VD

    // InstructionTemplate[2]
    TTI_SFPMAD(x, 0, p_sfpu::LCONST_neg1, 14, 0);

    // InstructionTemplate[3]
    TTI_SFPIADD(0, t, 15, sfpi::SFPIADD_MOD1_CC_NONE);

    {
        constexpr uint simple_bits = 0x00 | 0x00 | (0 << 3) | 4;
        constexpr uint mad_bits    = 0x00 | 0x00 | (0 << 3) | 5;
        constexpr uint round_bits  = 0;
        constexpr uint store_bits  = 0x80 | 0x00 | (0 << 3) | 3;

        TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_LOWER, (mad_bits << 8) | simple_bits);
        TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_UPPER, (store_bits << 8) | round_bits);
        TTI_SFPCONFIG(0, 4 + 0, 0);
    }
    {
        constexpr uint simple_bits = 0x80 | 0x40 | (5 << 3) | 7;
        constexpr uint mad_bits    = 0x80 | 0x40 | (0 << 3) | 6;
        constexpr uint round_bits  = 0;
        constexpr uint store_bits  = 0x00 | 0x40 | (2 << 3) | 3;

        TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_LOWER, (mad_bits << 8) | simple_bits);
        TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_UPPER, (store_bits << 8) | round_bits);
        TTI_SFPCONFIG(0, 4 + 1, 0);
    }
    {
        constexpr uint simple_bits = 0;
        constexpr uint mad_bits    = 0;
        constexpr uint round_bits  = 0;
        constexpr uint store_bits  = 0x00 | 0x40 | (1 << 3) | 3;

        TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_LOWER, (mad_bits << 8) | simple_bits);
        TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_UPPER, (store_bits << 8) | round_bits);
        TTI_SFPCONFIG(0, 4 + 2, 0);
    }

    // Misc: {
    //   StoreMod0: MOD0_FMT_SRCB,
    //   UsesLoadMod0ForStore: {0,0,0},
    //   UnitDelayKind: {1,1,1}, (WaitForElapsedInstructions=1)
    // }
    TTI_SFPCONFIG(0x700, 8, 1);
}

inline void _init_reciprocal_fast_24b_5c_()
{
    constexpr int e  = p_sfpu::LREG0;
    constexpr int t2 = p_sfpu::LREG1;
    constexpr int z  = p_sfpu::LREG2;
    constexpr int y  = p_sfpu::LREG3;

    // InstructionTemplate[0]
    TTI_SFPARECIP(0, 0, 12, 0);

    // InstructionTemplate[1]
    TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG0, 0, 13, 0);

    // InstructionTemplate[2]
    // SFPMAD(VA=t2, VB=0 or VD, VC=VD or z)
    TTI_SFPMAD(t2, p_sfpu::LREG0, z, 14, 0);

    // InstructionTemplate[3]
    TTI_SFPSWAP(0, p_sfpu::LCONST_1, 15, 1);

    // Macro 0: [y]
    {
        constexpr uint simple_bits = 0x00 | 0x00 | (0 << 3) | 4;
        constexpr uint mad_bits    = 0;
        constexpr uint round_bits  = 0;
        constexpr uint store_bits  = 0x00 | 0x40 | (6 << 3) | 3;

        TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_LOWER, (mad_bits << 8) | simple_bits);
        TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_UPPER, (store_bits << 8) | round_bits);
        TTI_SFPCONFIG(0, 4, 0);
    }

    // Macro 1: [e]
    {
        constexpr uint simple_bits = 0x00 | 0x40 | (0 << 3) | 4;
        constexpr uint mad_bits    = 0x00 | 0x00 | (2 << 3) | 5;
        constexpr uint round_bits  = 0;
        constexpr uint store_bits  = 0x00 | 0x00 | (2 << 3) | 3;

        TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_LOWER, (mad_bits << 8) | simple_bits);
        TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_UPPER, (store_bits << 8) | round_bits);
        TTI_SFPCONFIG(0, 4 + 1, 0);
    }

    // Macro 2: [t2]
    {
        constexpr uint simple_bits = 0x80 | 0x00 | (2 << 3) | 7;
        constexpr uint mad_bits    = 0x00 | 0x00 | (0 << 3) | 6;

        TTI_SFPCONFIG((mad_bits << 8) | simple_bits, 4 + 2, 1);
    }

    // Macro 3: [z]
    {
        constexpr uint simple_bits = 0;
        constexpr uint mad_bits    = 0x80 | 0x40 | (1 << 3) | 6;
        constexpr uint round_bits  = 0;
        constexpr uint store_bits  = 0x00 | 0x40 | (3 << 3) | 3;

        TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_LOWER, (mad_bits << 8) | simple_bits);
        TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_UPPER, (store_bits << 8) | round_bits);
        TTI_SFPCONFIG(0, 4 + 3, 0);
    }

    // Misc: {UsesLoadMod0ForStore=1, WaitForElapsedInstructions=1} for all macros.
    TTI_SFPCONFIG(0xff0, 8, 1);

    constexpr uint prev_offset = -2 & 0x3ff;
    constexpr uint offset      = 0;

    load_replay_buf(
        0,
        6,
        [e, t2, z, y, offset, prev_offset]
        {
            TTI_SFPLOADMACRO((0 << 2) | (y & 3), 0, ADDR_MOD_7, offset | (y >> 2));
            TTI_SFPLOADMACRO((2 << 2) | (t2 & 3), 0, ADDR_MOD_7, prev_offset | (t2 >> 2));
            TTI_SFPLOADMACRO((1 << 2) | (e & 3), 0, ADDR_MOD_7, offset | (e >> 2));
            TTI_SFPMAD(0, y, 10, 0, 1);
            TTI_SFPLOADMACRO((3 << 2) | (z & 3), 0, ADDR_MOD_6, prev_offset | (z >> 2));
            TTI_SFPLOADMACRO((3 << 2) | (z & 3), 0, ADDR_MOD_7, prev_offset | (z >> 2));
        });
}

template <bool APPROXIMATION_MODE>
inline void _init_sfpu_reciprocal_()
{
    if constexpr (!APPROXIMATION_MODE)
    {
        sfpi::vConstFloatPrgm0 = 2.0f;
    }
}

template <bool APPROXIMATION_MODE, bool is_fp32_dest_acc_en, bool legacy_compat = false>
inline void _init_reciprocal_()
{
    if constexpr (!legacy_compat)
    {
        if (APPROXIMATION_MODE)
        {
            _init_reciprocal_fast_7b_();
        }
        else if (is_fp32_dest_acc_en)
        {
            _init_reciprocal_fast_24b_5c_();
        }
        else
        {
            _init_reciprocal_fast_8b_3c_();
        }
    }
}

} // namespace sfpu
} // namespace ckernel
