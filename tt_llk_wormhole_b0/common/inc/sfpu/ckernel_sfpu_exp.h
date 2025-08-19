// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <limits>

#include "ckernel_sfpu_exp.h"
#include "ckernel_sfpu_recip.h"
#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel::sfpu
{

sfpi_inline sfpi::vFloat _sfpu_exp_(sfpi::vFloat val)
{
    // If exponent is > -1 extract it and replace with -1
    sfpi::vInt exp = exexp(val);
    v_if (exp >= 0)
    {
        val = setexp(val, 126);
    }
    v_endif;

    // Run series in Horner form
    sfpi::vFloat tmp = val * sfpi::vConst0p8373 + sfpi::s2vFloat16b(0.863281);
    val              = val * tmp + sfpi::vConst1;

    v_if (exp >= 0)
    {
        val = val * val;
        for (int s_iter = 0; s_iter < 7; s_iter++)
        {
            exp = exp - 1;
            // Narrow predication on each loop
            v_and(exp >= 0);
            val = val * val;
        }
    }
    v_endif;

    return val;
}

template <bool APPROXIMATION_MODE>
sfpi_inline sfpi::vFloat _calculate_exponential_body_(sfpi::vFloat in)
{
    sfpi::vFloat out;

    if constexpr (APPROXIMATION_MODE)
    {
        constexpr int FRAC_BITS = 3;
        constexpr uint SP_BIAS  = 127 << FRAC_BITS;

        // * by 1/ln2 and add convert to 7.3 FxP format
        sfpi::vFloat vConstLn2Recip = sfpi::vConstFloatPrgm0;
        sfpi::vFloat conv           = in * vConstLn2Recip;

        // Clear exp bits
        sfpi::vInt c23_73 = p_exp::C23_73;
        sfpi::vInt tmp    = sfpi::reinterpret<sfpi::vInt>(conv) - c23_73;

        // Add bias
        tmp += SP_BIAS;

        // SHL to move integer bits to exponent
        out = sfpi::reinterpret<sfpi::vFloat>(tmp << (10 - FRAC_BITS));
    }
    else
    {
        // Force sign to 0 (make number positive)
        out = _sfpu_exp_(sfpi::setsgn(in, 0));

        v_if (in < 0)
        {
            out = _sfpu_reciprocal_(out);
        }
        v_endif;
    }

    return out;
}

inline sfpi::vFloat _calculate_exponential_approx_(sfpi::vFloat in)
{
    // * by 1/ln2 and add convert to 7.3 FxP format
    sfpi::vFloat vConstLn2Recip = sfpi::vConstFloatPrgm0;
    sfpi::vFloat c23_73         = sfpi::vConstFloatPrgm1;
    sfpi::vInt adj_exp          = sfpi::vConstIntPrgm2;
    in                          = in * vConstLn2Recip + c23_73;

    // Remove Exponent of 7 and bias the Mantissa to 127.
    sfpi::vInt in_short = adj_exp + sfpi::reinterpret<sfpi::vInt>(in);

    // SHL to move integer bits to exponent
    in_short <<= 10 - p_exp::FRAC_BITS;
    return sfpi::reinterpret<sfpi::vFloat>(in_short);
}

template <bool APPROXIMATION_MODE, bool SCALE_EN, bool SKIP_POSITIVE_CHECK>
inline sfpi::vFloat _calculate_exponential_piecewise_(sfpi::vFloat in, const uint16_t exp_base_scale_factor /* 1.0f in BF16 */)
{
    // This function is used to calculate the exponential of a value in a more accurate manner.
    sfpi::vFloat result = 0.0f;
    if constexpr (SCALE_EN)
    {
        in = in * sfpi::s2vFloat16b(exp_base_scale_factor);
    }
    if constexpr (APPROXIMATION_MODE)
    {
        if constexpr (!SKIP_POSITIVE_CHECK)
        {
            v_if (in >= 89)
            {
                // Algorithm is incorrect for inputs >= 89, so saturate output to infinity.
                sfpi::vFloat in_inf = std::numeric_limits<float>::infinity();
                result              = in_inf;
            }
            v_elseif (in < -42)
            {
                // Algorithm is incorrect for inputs < -42, so saturate output to 0.
                result = 0.0f;
            }
            v_else
            {
                result = _calculate_exponential_approx_(in);
            }
            v_endif;
        }
        else
        {
            // SKIP_POSITIVE_CHECK is true, so user is responsible for ensuring inputs are <= 89.
            v_if (in < -42)
            {
                result = 0.0f;
            }
            v_else
            {
                result = _calculate_exponential_approx_(in);
            }
            v_endif;
        }
    }
    else
    {
        result = _sfpu_exp_(sfpi::setsgn(in, 0));

        v_if (in < 0)
        {
            result = _sfpu_reciprocal_(result);
        }
        v_endif;
    }

    return result;
}

template <bool APPROXIMATION_MODE, bool SCALE_EN, int ITERATIONS, bool FAST_APPROX, bool SKIP_POSITIVE_CHECK>
void _calculate_exponential_(const uint16_t exp_base_scale_factor /* 1.0f in BF16 */)
{
    constexpr uint dst_tile_size_rows = 32;
    constexpr float LN2_RECIP         = 1.4426950408889634f;
    constexpr float A                 = 256.0f * LN2_RECIP;
    constexpr float B_minus_C         = 32500.818359375f;
    constexpr float THRESHOLD         = -88.5f;

    if constexpr (FAST_APPROX && APPROXIMATION_MODE)
    {
        TTI_SFPLOADMACRO(0x0, 0x2, 0x0, 0x400);
        TTI_SFPNOP;
        TTI_SFPNOP;

        TTI_SFPLOADMACRO(0x1, 0x2, 0x0, 0x402);
        TTI_SFPNOP;
        TTI_SFPNOP;

        TTI_SFPLOADMACRO(0x2, 0x2, 0x0, 0x404);
        TTI_SFPNOP;
        TTI_SFPNOP;

        TTI_SFPLOADMACRO(0x3, 0x2, 0x0, 0x406);
        TTI_SFPNOP;
        TTI_SFPNOP;

        TTI_SFPLOADMACRO(0x0, 0x2, 0x0, 0x408);
        TTI_SFPNOP;
        TTI_SFPNOP;

        TTI_SFPLOADMACRO(0x1, 0x2, 0x0, 0x40A);
        TTI_SFPNOP;
        TTI_SFPNOP;

        TTI_SFPLOADMACRO(0x2, 0x2, 0x0, 0x40C);
        TTI_SFPNOP;
        TTI_SFPNOP;

        TTI_SFPLOADMACRO(0x3, 0x2, 0x0, 0x40E);
        TTI_SFPNOP;
        TTI_SFPNOP;
    }
    else
    {
        // Unroll 8 best for approx, unroll 0 for precise, compiler figures this out
        for (int d = 0; d < ITERATIONS; d++)
        {
            sfpi::vFloat val    = sfpi::dst_reg[0];
            sfpi::vFloat result = _calculate_exponential_piecewise_<APPROXIMATION_MODE, SCALE_EN, SKIP_POSITIVE_CHECK>(val, exp_base_scale_factor);
            sfpi::dst_reg[0]    = result;
            sfpi::dst_reg++;
        }
    }
}

constexpr auto bits = [](float x) constexpr { return __builtin_bit_cast(std::uint32_t, x); };
constexpr auto lo16 = [](float x) constexpr { return static_cast<std::uint16_t>(bits(x) & 0xFFFFu); };
constexpr auto hi16 = [](float x) constexpr { return static_cast<std::uint16_t>(bits(x) >> 16); };

template <bool APPROXIMATION_MODE, bool FAST_APPROX, uint32_t scale /* 1.0f in FP32 */>
inline void _init_exponential_()
{
    if constexpr (FAST_APPROX && APPROXIMATION_MODE)
    {
        TTI_SFPLOADI(0, 0xA, 0x3333);
        TTI_SFPLOADI(0, 0x8, 0xC2AD);
        TTI_SFPCONFIG(0, 14, 0); // SFPCONFIG Dest 14 = LREG[14]         =    -86.6               = 0xC2AD_3333

        TTI_SFPLOADI(0, 0xA, 0xAA3B);
        TTI_SFPLOADI(0, 0x8, 0x43B8);
        TTI_SFPCONFIG(0, 12, 0); // SFPCONFIG Dest 12 = LREG[12] = A     =    369.329925537109375 = 0x43B8_AA3B

        TTI_SFPLOADI(0, 0xA, 0x3F7A);
        TTI_SFPLOADI(0, 0x8, 0x4BC0);
        TTI_SFPCONFIG(0, 13, 0); // SFPCONFIG Dest 13 = LREG[13] = (B-C) =  32500.818359375 + 12582912*2    = 0x4BC0_3F7A

        // Backdoor load of Macro Instruction 4
        // lreg[X] = lreg[12] (A) * lreg[X] (y) + lreg[13] (B-C)
        TTI_SFPSWAP(0x001, 0xE, 0xC, 0x1);

        // Backdoor load of Macro Instruction 5
        // lreg[X] = lreg[12] (A) * lreg[X] (y) + lreg[13] (B-C)
        TTI_SFPMAD(0xC, 0x0, 0xD, 0xD, 0x0);

        // Backdoor load of Macro Instruction 6
        // lreg[X] = lreg[X] << 16
        TTI_SFPSHFT2(16, 0x0, 0xE, 0x6);

        // Backdoor load of Macro Instruction 7
        // Mem[X+10] = Staging Flop
        TTI_SFPSTORE(0xF, 0x0, 0x0, 0x010);

        // Sequence 0 setup: we want to Load, SWAP, <delay>, MAD, <delay>, SHFT2, Store
        //       Delay slot:                  0     1        2    3        4,     5
        // Slot 0: Simple Unit  : 8'b1__________________0_____________000_____100 = 0x84
        //                           | Load Reg as SrcB | Use Staging | Delay | Instrn Sel
        // Slot 1: MAD Unit     : 8'b1__________________0_____________010_____101 = 0x95
        //                           | Load Reg as SrcB | Use Staging | Delay | Instrn Sel
        TTI_SFPLOADI(0x0, 0xA, 0x9584);
        // Slot 2: Round Unit   : 8'b1__________________1_____________100_____110 = 0xE6
        //                           | Load Reg as SrcB | Use Staging | Delay | Instrn Sel
        // Slot 3: Store Unit   : 8'b1__________________1_____________101_____111 = 0xEF
        //                           | Use Addr Offset  | Use Staging | Delay | Instrn Sel
        TTI_SFPLOADI(0x0, 0x8, 0xEFE6);
        TTI_SFPCONFIG(0x0000, 0x4, 0x0); // Load it into macro sequence register 0 (destination = 4)

        // Configure the stores in LOADMACRO sequence 0 to use the same insmod as the LOADMACRO
        TTI_SFPCONFIG(0x0010, 0x8, 0x1);

        // Invert the direction of SFPSWAP instructions so we put the larger operand in
        // the register the MAD will pick up as SrcB
        TTI_SFPCONFIG(0x0100, 0xF, 0x1);
    }
    else if constexpr (APPROXIMATION_MODE)
    {
        sfpi::vConstFloatPrgm0 = 1.442695f; // ln2_recip
        sfpi::vConstFloatPrgm1 = sfpi::s2vFloat16b(p_exp::C23_73);
        sfpi::vConstFloatPrgm2 = sfpi::s2vFloat16b(p_exp::ADJ_EXP);
    }
    else
    {
        sfpi::vConstFloatPrgm0 = 1.442695f; // ln2_recip
        sfpi::vConstFloatPrgm1 = 2.0f;
        sfpi::vConstFloatPrgm2 = 0.863281f;
    }
}

} // namespace ckernel::sfpu
