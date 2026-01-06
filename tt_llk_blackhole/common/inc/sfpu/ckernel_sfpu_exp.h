// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <limits>

#include "../../../tests/helpers/include/llk_sfpu_types.h"
#include "ckernel_addrmod.h"
#include "ckernel_ops.h"
#include "ckernel_sfpu_recip.h"
#include "ckernel_sfpu_rounding_ops.h"
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
            out = _sfpu_reciprocal_<2>(out);
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
            result = _sfpu_reciprocal_<2>(result);
        }
        v_endif;
    }

    return result;
}

constexpr auto bits = [](float x) constexpr { return __builtin_bit_cast(std::uint32_t, x); };
constexpr auto lo16 = [](float x) constexpr { return static_cast<std::uint16_t>(bits(x) & 0xFFFFu); };
constexpr auto hi16 = [](float x) constexpr { return static_cast<std::uint16_t>(bits(x) >> 16); };

template <uint32_t scale>
inline void init_clamp_loadmacro()
{
    constexpr float scale_fp32       = __builtin_bit_cast(float, scale);
    constexpr float THRESHOLD        = -88.5f;
    constexpr float THRESHOLD_scaled = THRESHOLD / scale_fp32;

    TTI_SFPLOADI(0, 0xA, lo16(THRESHOLD_scaled));
    TTI_SFPLOADI(0, 0x8, hi16(THRESHOLD_scaled));
    TTI_SFPCONFIG(0, 14, 0); // SFPCONFIG Dest 14 = LREG[14] =            -88.5               = 0xc2b10000

    // There are two ways to program the macro instruction registers, and this setup leverages both ways
    //  - we can either use the SFPCONFIG flow, by setting up the bits of the instruction into LREG[0] and then targeting the Macro instruction register
    //  - or we can use the shortcut / backdoor load method which relies on having some illegal destination register values as part of the instruction

    // Use SFPCONFIG method for the SWAP instruction, since we want the SWAP itself to use a destination register which is not normally a legal value
    //      (we are cheating a bit here, since we only care about one half of the swap and we want to use a constant for the other half)
    //
    //              imm12 = 0,       lreg_src_c = 0 (will be fed by value loaded from Dest into Loadmacro lreg_dest),  lreg_dest = LREG[14] = - 88.5,
    //              instr_mod1 = 1 swap the values with the larger of the two ending up in lreg_dest -> but we will use the Loadmacro lreg_dest register as
    //              output
    // TTI_SFP_SWAP(0,               0,                                                                                14,                            1);
    TTI_SFPLOADI(0, 0xA, 0x00E1);
    TTI_SFPLOADI(0, 0x8, 0x9200);
    TTI_SFPCONFIG(0, 0, 0); // SFPCONFIG Dest 0 = Programmable Macro instruction 0: TTI_SFPSWAP(0, 0, 14, 1); // compare against LREG[14] (-88.5), and put
                            // the larger value into LREG[loadmacro_lreg_dest]
    TTI_SFPNOP;

    // So at this point, we have the following instructions loaded into our macro registers:
    //
    // 00: (no macro instruction, just execute whatever is issued from Tensix) <-- these are fixed / not programmable
    // 01: ( Rsvd                                                            ) <-- these are fixed / not programmable
    // 02: ( NOP                                                             ) <-- these are fixed / not programmable
    // 03: ( SFPSTORE                                                        ) <-- these are fixed / not programmable
    // 04: TTI_SFPSWAP       (0, 0, 11, 1)

    // Now we want to set up our two sequences

    // Sequence 1 setup: we want to Load, SWAP, <delay>, Store
    //       Delay slot:                  0     1        2
    //                                                                                                                                                                                                 Use
    //                                                                                                                                                                                                 Loaded  Result          Macro
    //                                                                                                                                                                                                 Value   Value   Delay   Instruction
    //                                                                                                                                                                                                 SRCB    Stage   Slot    Select
    TTI_SFPLOADI(0, 0xA, 0x0004); // slot1 : SIMPLE UNIT, want SWAP  instruction which is in macro instruction mux[4], delayed by 0 ; not using staging flop
                                  // as dest; not using load reg as srcb : 8'b0_______0_______000_____100          = 0x04 slot2 : MAD    UNIT, unused :
                                  // 8'b0_______0_______000_____000          = 0x00
    TTI_SFPLOADI(0, 0x8, 0x1300); // slot3 : ROUND  UNIT, unused : 8'b0_______0_______000_____000          = 0x00 slot4 : STORE  UNIT, want STORE
                                  // instruction which is in macro instruction mux[3], delayed by 2 ; not using staging flop as src ; :
                                  // 8'b0_______0_______010_____011          = 0x13
    TTI_SFPCONFIG(0, 5, 0);       // SFPCONFIG Dest 5 = Macro Sequence Register 1
}

inline void run_clamp_loadmacro()
{
    // Sanitize the input values by loading from DEST, comparing against the value -88.5, and if the input value is more negative than that, swap the input
    // value with -88.5 and store back to DEST
    //  - in other words, after the sanitize step, the values in DEST will be in the range {-88.5 , +inf}

    // Macro Sequence Register 1 configured to read back in the original values from dest, sanitize them to a range we can handle, and then store them back
    // to dest
    //  LD     : bring in the original value from DEST (y)
    //  MAD    : unused
    //  ROUND  : unused
    //  SIMPLE : SWAP the larger value of y and -88.5 into the LREG
    //  STORE  : store the sanitized value back to dest
    TTI_SFPLOADMACRO(
        4,
        0,
        ADDR_MOD_7,
        0);     // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[0] for loaded value - Dest offset  0 is targeting the even columns for rows   3: 0
    TTI_SFPNOP; // NOP is necessary because the SWAP operation takes 2 cycles and unfortunately is not pipelined
    TTI_SFPLOADMACRO(
        5,
        0,
        ADDR_MOD_7,
        2); // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[1] for loaded value - Dest offset  2 is targeting the odd  columns for rows   3: 0
    TTI_SFPNOP;
    TTI_SFPLOADMACRO(
        6,
        0,
        ADDR_MOD_7,
        4); // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[2] for loaded value - Dest offset  4 is targeting the even columns for rows   7: 4
    TTI_SFPNOP;
    TTI_SFPLOADMACRO(
        7,
        0,
        ADDR_MOD_7,
        6); // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[3] for loaded value - Dest offset  6 is targeting the odd  columns for rows   7: 4
    TTI_SFPNOP;
    TTI_SFPLOADMACRO(
        4,
        0,
        ADDR_MOD_7,
        8); // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[0] for loaded value - Dest offset  8 is targeting the even columns for rows  11: 8
    TTI_SFPNOP;
    TTI_SFPLOADMACRO(
        5,
        0,
        ADDR_MOD_7,
        10); // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[1] for loaded value - Dest offset 10 is targeting the even columns for rows  11: 8
    TTI_SFPNOP;
    TTI_SFPLOADMACRO(
        6,
        0,
        ADDR_MOD_7,
        12); // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[2] for loaded value - Dest offset 12 is targeting the odd  columns for rows  15:12
    TTI_SFPNOP;
    TTI_SFPLOADMACRO(
        7,
        0,
        ADDR_MOD_7,
        14); // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[3] for loaded value - Dest offset 14 is targeting the even columns for rows  15:12
    // NOP not needed in this spot because the next LoadMacro is a computational macro which doesn't immediately use the SIMPLE unit
}

template <uint32_t scale>
void calculate_exponential_approx_no_swap_relu_init()
{
    constexpr float scale_fp32_ = __builtin_bit_cast(float, scale);
    constexpr float A_          = 184.6649652337873f;
    constexpr float A_scaled_   = A_ * scale_fp32_;
    constexpr float B_minus_C_  = 16250.4091796875f;

    // A.
    TTI_SFPLOADI(0, 0xA, lo16(A_scaled_));
    TTI_SFPLOADI(0, 0x8, hi16(A_scaled_));
    TTI_SFPCONFIG(0, 11, 0);

    // (B-C).
    TTI_SFPLOADI(0, 0xA, lo16(B_minus_C_));
    TTI_SFPLOADI(0, 0x8, hi16(B_minus_C_));
    TTI_SFPCONFIG(0, 12, 0);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_7);

    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);
}

template <int ITERATIONS>
void calculate_exponential_approx_no_swap_relu()
{
    for (int d = 0; d < ITERATIONS; d++)
    {
        // Load input.
        TTI_SFPLOAD(p_sfpu::LREG0, 0, ADDR_MOD_7, 0);

        // Compute i = A * x + (B-C).
        TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG11, p_sfpu::LREG12, p_sfpu::LREG0, 0);

        // Abs.
        TTI_SFPABS(0, p_sfpu::LREG0, p_sfpu::LREG2, 0);

        // Add.
        TTI_SFPADD(p_sfpu::LCONST_1, p_sfpu::LREG0, p_sfpu::LREG2, p_sfpu::LREG0, 0);

        // Convert to int and left shift by 15 bits.
        TTI_SFP_STOCH_RND(0, 0, 0, p_sfpu::LREG0, p_sfpu::LREG0, sfpi::SFPSTOCHRND_MOD1_FP32_TO_UINT16);
        TTI_SFPSHFT(15, p_sfpu::LREG0, p_sfpu::LREG0, 0x1);

        // Store the result.
        TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0);
        TTI_INCRWC(0, 2, 0, 0);
    }
}

template <uint32_t scale>
void calculate_exponential_no_swap_init()
{
    constexpr float M_LN2      = -0.69314718055994530942f; // -ln(2)
    constexpr float LN2_RECIP  = 1.44269504088896340736f;  // 1/ln(2)
    constexpr float scale_fp32 = __builtin_bit_cast(float, scale);
    constexpr float A          = 369.32992553710937500f; // 256 / ln(2)
    constexpr float A_scaled   = A * scale_fp32;
    constexpr float B_MINUS_C  = 32500.81835937500000f; // Polynomial offset

    // 1/ln(2) (for computing k = y/ln2).
    TTI_SFPLOADI(0, 0xA, lo16(LN2_RECIP));
    TTI_SFPLOADI(0, 0x8, hi16(LN2_RECIP));
    TTI_SFPCONFIG(0, 11, 0);

    // -ln(2) (for computing r = y - k*ln2).
    TTI_SFPLOADI(0, 0xA, lo16(M_LN2));
    TTI_SFPLOADI(0, 0x8, hi16(M_LN2));
    TTI_SFPCONFIG(0, 12, 0);

    // A = 256/ln(2) (for computing i = A*r + (B-C)).
    TTI_SFPLOADI(0, 0xA, lo16(A_scaled));
    TTI_SFPLOADI(0, 0x8, hi16(A_scaled));
    TTI_SFPCONFIG(0, 13, 0);

    // (B-C).
    TTI_SFPLOADI(0, 0xA, lo16(B_MINUS_C));
    TTI_SFPLOADI(0, 0x8, hi16(B_MINUS_C));
    TTI_SFPCONFIG(0, 14, 0);

    // 0.5.
    TTI_SFPLOADI(p_sfpu::LREG7, 0xA, lo16(0.5f));
    TTI_SFPLOADI(p_sfpu::LREG7, 0x8, hi16(0.5f));

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_7);

    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);

    // Enable conditional execution.
    // TTI_SFPENCC(1, 0, 0, 2);  // Mod1=2 (EI), Imm2=1: UseLaneFlagsForLaneEnable=true, sets all LaneFlags=true
}

template <int ITERATIONS>
void calculate_exponential_no_swap()
{
    for (int d = 0; d < ITERATIONS; d++)
    {
        // Load y.
        TTI_SFPLOAD(p_sfpu::LREG0, 0, ADDR_MOD_7, 0);

        // Multiply y by 1/ln(2), round to nearest and convert to int16.
        TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG11, p_sfpu::LCONST_0, p_sfpu::LREG4, 0);
        TTI_SFP_STOCH_RND(0, 0, 0, p_sfpu::LREG4, p_sfpu::LREG2, sfpi::SFPSTOCHRND_MOD1_FP32_TO_INT16);

        // Convert back to FP32 and compute r = y - k*ln2.
        TTI_SFPCAST(p_sfpu::LREG2, p_sfpu::LREG2, 0x0);
        TTI_SFPMAD(p_sfpu::LREG2, p_sfpu::LREG12, p_sfpu::LREG0, p_sfpu::LREG0, 0);

        // Compute i = A * r + (B-C).
        TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG13, p_sfpu::LREG14, p_sfpu::LREG0, 0);

        // Convert to int and left shift by 15 bits.
        TTI_SFP_STOCH_RND(0, 0, 0, p_sfpu::LREG0, p_sfpu::LREG0, sfpi::SFPSTOCHRND_MOD1_FP32_TO_INT16);
        TTI_SFPSHFT(15, p_sfpu::LREG0, p_sfpu::LREG0, 0x1);

        // Extract exponent and add k.
        TTI_SFPEXEXP(0, p_sfpu::LREG0, p_sfpu::LREG4, 1); // 0=debias, values ~0 1=no debias, values ~127

        // Cast to float to add signed values.
        TTI_SFPCAST(p_sfpu::LREG4, p_sfpu::LREG4, 0x0);
        TTI_SFPMAD(p_sfpu::LCONST_1, p_sfpu::LREG2, p_sfpu::LREG4, p_sfpu::LREG4, 0);

        // Clamp negative values to 0 using ReLU(x) = 0.5*(x + abs(x)).
        TT_SFPSETSGN(0, p_sfpu::LREG4, p_sfpu::LREG2, 0x1);
        TTI_SFPMAD(p_sfpu::LCONST_1, p_sfpu::LREG2, p_sfpu::LREG4, p_sfpu::LREG4, 0);
        TTI_SFPMAD(p_sfpu::LREG7, p_sfpu::LREG4, p_sfpu::LCONST_0, p_sfpu::LREG4, 0);

        // Cast to int16.
        TTI_SFP_STOCH_RND(0, 0, 0, p_sfpu::LREG4, p_sfpu::LREG4, sfpi::SFPSTOCHRND_MOD1_FP32_TO_INT16);

        // Clamp negative values to 0.
        // TTI_SFPSETCC(0, p_sfpu::LREG4, 0, 0);
        // TTI_SFPLOADI(p_sfpu::LREG4, 2, 0);
        // TTI_SFPENCC(2, 0, 0, 8);  // Re-enable all lanes. Mod1=8 (RI), Imm2=2 (bit1=1): sets all LaneFlags=true

        // Set the new exponent.
        TTI_SFPSETEXP(0, p_sfpu::LREG0, p_sfpu::LREG4, 0x0);

        // Store the result.
        TTI_SFPSTORE(p_sfpu::LREG4, 0, ADDR_MOD_7, 0);
        TTI_INCRWC(0, 2, 0, 0);
    }
}

#define USE_ARECIP_INSTR_ 0

template <bool USE_ARECIP_INSTR, int NUM_TERMS, uint32_t SCALE>
void calculate_exponential_more_terms_init()
{
    constexpr float M_LN2     = -0.69314718055994530942f; // -ln(2)
    constexpr float LN2_RECIP = 1.44269504088896340736f;  // 1/ln(2)

    // 1/ln(2) (for computing k = y/ln2).
    TTI_SFPLOADI(0, 0xA, lo16(LN2_RECIP));
    TTI_SFPLOADI(0, 0x8, hi16(LN2_RECIP));
    TTI_SFPCONFIG(0, 11, 0);

    // -ln(2) (for computing r = y - k*ln2).
    TTI_SFPLOADI(0, 0xA, lo16(M_LN2));
    TTI_SFPLOADI(0, 0x8, hi16(M_LN2));
    TTI_SFPCONFIG(0, 12, 0);

    if constexpr (!USE_ARECIP_INSTR)
    {
        LLK_ASSERT(NUM_TERMS >= 1 && NUM_TERMS <= 3, "Only 1, 2, or 3 terms is supported");

        // First coefficient in expansion.
        float c0 = (NUM_TERMS == 1)   ? 0.95696433397637822605477003788850394713182741710354f
                   : (NUM_TERMS == 2) ? 1.00247605640014803650231996175375441870054216548083f
                                      : 0.99989296565052922983621778370827266474330264667853;
        TTI_SFPLOADI(0, 0xA, lo16(c0));
        TTI_SFPLOADI(0, 0x8, hi16(c0));
        TTI_SFPCONFIG(0, 13, 0);
    }

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_7);

    init_clamp_loadmacro<SCALE>();
}

template <bool USE_ARECIP_INSTR, bool SCALE_EN, int ITERATIONS, int NUM_TERMS>
void calculate_exponential_more_terms(const uint16_t exp_base_scale_factor)
{
    run_clamp_loadmacro();

    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);

    // scale factor.
    if constexpr (SCALE_EN)
    {
        TTI_SFPLOADI(p_sfpu::LREG3, 0, exp_base_scale_factor);
    }

    if constexpr (!USE_ARECIP_INSTR)
    {
        LLK_ASSERT(NUM_TERMS >= 1 && NUM_TERMS <= 3, "Only 1, 2, or 3 terms is supported");
        float c1 = (NUM_TERMS == 1)   ? 1.44269504088896339978827134447864798074740875888603f
                   : (NUM_TERMS == 2) ? 0.93926196133115784467495772308965170123990519277365f
                                      : 1.00477562994735353143572627317263842140761033115717f;
        float c2 = (NUM_TERMS == 1)   ? 0
                   : (NUM_TERMS == 2) ? 0.71599323332452799859983839812738341840434589928973f
                                      : 0.46693091371682332628846847179930982027545404395215;
        float c3 = (NUM_TERMS == 1) ? 0 : (NUM_TERMS == 2) ? 0 : 0.237832964457101087894820992366976881073392833669424f;
        switch (NUM_TERMS)
        {
            case 3:
                TTI_SFPLOADI(p_sfpu::LREG5, 0xA, lo16(c3));
                TTI_SFPLOADI(p_sfpu::LREG5, 0x8, hi16(c3));
            case 2:
                TTI_SFPLOADI(p_sfpu::LREG6, 0xA, lo16(c2));
                TTI_SFPLOADI(p_sfpu::LREG6, 0x8, hi16(c2));
            case 1:
                TTI_SFPLOADI(p_sfpu::LREG7, 0xA, lo16(c1));
                TTI_SFPLOADI(p_sfpu::LREG7, 0x8, hi16(c1));
            default:
                break;
        }
    }

    for (int d = 0; d < ITERATIONS; d++)
    {
        // Load.
        TTI_SFPLOAD(p_sfpu::LREG0, 0, ADDR_MOD_7, 0);

        if constexpr (SCALE_EN)
        {
            TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG3, p_sfpu::LCONST_0, p_sfpu::LREG0, 0);
        }

        // Multiply by 1/ln(2).
        TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG11, p_sfpu::LCONST_0, p_sfpu::LREG1, 0);

        // On BH this will round incorrectly for 3 values: 0.9999998807907, 0.9999999403954, 1.999999880791.
        // Alternatively _floor_body_() can be used.
        TTI_SFP_STOCH_RND(2, 0, 0, p_sfpu::LREG1, p_sfpu::LREG1, sfpi::SFPSTOCHRND_MOD1_FP32_TO_INT16);

        // Compute r = y - k*ln2.
        TTI_SFPCAST(p_sfpu::LREG1, p_sfpu::LREG1, 0x0);
        TTI_SFPMAD(p_sfpu::LREG1, p_sfpu::LREG12, p_sfpu::LREG0, p_sfpu::LREG0, 0);

        if constexpr (USE_ARECIP_INSTR)
        {
            TTI_SFPARECIP(0, p_sfpu::LREG0, p_sfpu::LREG0, 2);
        }
        else
        {
            // Compute polynomial.
            if constexpr (NUM_TERMS == 1)
            {
                TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG7, p_sfpu::LREG13, p_sfpu::LREG0, 0);
            }
            else if constexpr (NUM_TERMS == 2)
            {
                TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG6, p_sfpu::LREG7, p_sfpu::LREG2, 0);
                TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG2, p_sfpu::LREG13, p_sfpu::LREG0, 0);
            }
            else
            {
                TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG5, p_sfpu::LREG6, p_sfpu::LREG2, 0);
                TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG2, p_sfpu::LREG7, p_sfpu::LREG2, 0);
                TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG2, p_sfpu::LREG13, p_sfpu::LREG0, 0);
            }
        }

        // Set the new exponent.
        TTI_SFPEXEXP(0, p_sfpu::LREG0, p_sfpu::LREG2, 1); // 0=debias, 1=no debias.
        TTI_SFPCAST(p_sfpu::LREG2, p_sfpu::LREG2, 0x0);
        TTI_SFPMAD(p_sfpu::LCONST_1, p_sfpu::LREG1, p_sfpu::LREG2, p_sfpu::LREG2, 0);
        TTI_SFP_STOCH_RND(0, 0, 0, p_sfpu::LREG2, p_sfpu::LREG2, sfpi::SFPSTOCHRND_MOD1_FP32_TO_INT16);
        TTI_SFPSETEXP(0, p_sfpu::LREG0, p_sfpu::LREG2, 0x0);

        // Store the result.
        TTI_SFPSTORE(p_sfpu::LREG2, 0, ADDR_MOD_7, 0);
        TTI_INCRWC(0, 2, 0, 0);
    }
}

template <int NUM_TERMS, int NUM_SCALE_TERMS>
void calculate_exponential_no_swap_relu_init()
{
    constexpr float SCALE   = 1.0f / float(1 << NUM_SCALE_TERMS);
    constexpr float COEFF_1 = SCALE / 2.0f;

    if constexpr (NUM_TERMS == 2)
    {
        TTI_SFPLOADI(0, 0xA, lo16(0.5f));
        TTI_SFPLOADI(0, 0x8, hi16(0.5f));
        TTI_SFPCONFIG(0, 11, 0);

        TTI_SFPLOADI(0, 0xA, lo16(COEFF_1));
        TTI_SFPLOADI(0, 0x8, hi16(COEFF_1));
        TTI_SFPCONFIG(0, 12, 0);
    }
    else if constexpr (NUM_TERMS == 4)
    {
        constexpr float COEFF_2 = SCALE * SCALE / 4.0f;
        constexpr float COEFF_3 = SCALE * SCALE * SCALE / 12.0f;

        TTI_SFPLOADI(0, 0xA, lo16(COEFF_2));
        TTI_SFPLOADI(0, 0x8, hi16(COEFF_2));
        TTI_SFPCONFIG(0, 11, 0);

        TTI_SFPLOADI(0, 0xA, lo16(COEFF_3));
        TTI_SFPLOADI(0, 0x8, hi16(COEFF_3));
        TTI_SFPCONFIG(0, 12, 0);

        TTI_SFPLOADI(0, 0xA, lo16(0.5f));
        TTI_SFPLOADI(0, 0x8, hi16(0.5f));
        TTI_SFPCONFIG(0, 13, 0);

        TTI_SFPLOADI(0, 0xA, lo16(COEFF_1));
        TTI_SFPLOADI(0, 0x8, hi16(COEFF_1));
        TTI_SFPCONFIG(0, 14, 0);
    }
    else if constexpr (NUM_TERMS == 6)
    {
        constexpr float COEFF_2 = SCALE * SCALE / 4.0f;
        constexpr float COEFF_3 = SCALE * SCALE * SCALE / 12.0f;
        constexpr float COEFF_4 = SCALE * SCALE * SCALE * SCALE / 48.0f;
        constexpr float COEFF_5 = SCALE * SCALE * SCALE * SCALE * SCALE / 240.0f;

        TTI_SFPLOADI(0, 0xA, lo16(COEFF_4));
        TTI_SFPLOADI(0, 0x8, hi16(COEFF_4));
        TTI_SFPCONFIG(0, 11, 0);

        TTI_SFPLOADI(0, 0xA, lo16(COEFF_5));
        TTI_SFPLOADI(0, 0x8, hi16(COEFF_5));
        TTI_SFPCONFIG(0, 12, 0);

        TTI_SFPLOADI(0, 0xA, lo16(COEFF_2));
        TTI_SFPLOADI(0, 0x8, hi16(COEFF_2));
        TTI_SFPCONFIG(0, 13, 0);

        TTI_SFPLOADI(0, 0xA, lo16(COEFF_3));
        TTI_SFPLOADI(0, 0x8, hi16(COEFF_3));
        TTI_SFPCONFIG(0, 14, 0);

        TTI_SFPLOADI(p_sfpu::LREG6, 0xA, lo16(0.5f));
        TTI_SFPLOADI(p_sfpu::LREG6, 0x8, hi16(0.5f));

        TTI_SFPLOADI(p_sfpu::LREG7, 0xA, lo16(COEFF_1));
        TTI_SFPLOADI(p_sfpu::LREG7, 0x8, hi16(COEFF_1));
    }

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_7);

    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);
}

template <int ITERATIONS, unsigned int NUM_TERMS, unsigned int NUM_SCALE_TERMS>
void calculate_exponential_no_swap_relu()
{
    // TODO: scale factor is not used.

    LLK_ASSERT(NUM_TERMS == 2 || NUM_TERMS == 4 || NUM_TERMS == 6, "Only 2, 4, or 6 terms supported");

    for (int d = 0; d < ITERATIONS; d++)
    {
        // Load input.
        TTI_SFPLOAD(p_sfpu::LREG0, 0, ADDR_MOD_7, 0);
        TTI_SFPMAD(p_sfpu::LREG12, p_sfpu::LREG0, p_sfpu::LREG11, p_sfpu::LREG2, 0);

        if constexpr (NUM_TERMS >= 4)
        {
            TTI_SFPMAD(p_sfpu::LREG2, p_sfpu::LREG0, p_sfpu::LREG14, p_sfpu::LREG2, 0);
            TTI_SFPMAD(p_sfpu::LREG2, p_sfpu::LREG0, p_sfpu::LREG13, p_sfpu::LREG2, 0);
        }
        if constexpr (NUM_TERMS >= 6)
        {
            TTI_SFPMAD(p_sfpu::LREG2, p_sfpu::LREG0, p_sfpu::LREG7, p_sfpu::LREG2, 0);
            TTI_SFPMAD(p_sfpu::LREG2, p_sfpu::LREG0, p_sfpu::LREG6, p_sfpu::LREG2, 0);
        }

        TTI_SFPABS(0, p_sfpu::LREG2, p_sfpu::LREG0, 1);
        TTI_SFPMAD(p_sfpu::LCONST_1, p_sfpu::LREG0, p_sfpu::LREG2, p_sfpu::LREG0, 0);

        for (unsigned int i = 0; i < NUM_SCALE_TERMS; i++)
        {
            TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG0, p_sfpu::LCONST_0, p_sfpu::LREG0, 0);
        }

        // Store the result.
        TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, 0);
        TTI_INCRWC(0, 2, 0, 0);
    }
}

sfpi_inline sfpi::vInt _float_to_int32_exp21f_(sfpi::vFloat val)
{
    sfpi::vInt exp   = exexp(val);
    sfpi::vInt man   = exman8(val); // get mantissa with implicit bit (man in [1; 2])
    sfpi::vInt shift = exp - 23;
    man              = sfpi::reinterpret<sfpi::vInt>(shft(sfpi::reinterpret<sfpi::vUInt>(man), shift));
    return man;
}

template <bool is_fp32_dest_acc_en = false>
sfpi_inline sfpi::vFloat _sfpu_exp_21f_(sfpi::vFloat val)
{
    sfpi::vFloat y = sfpi::vConst0;
    // Intermediary values can overflow if abs(val) is above 88.5f, which leads to output increasing again instead
    // of staying at 0 (or becoming finite on large inputs). This overflow happens when `| log2(e) * val | > 127.0f`,
    // which correspond to `|val| > 88.5f`
    // Intermediary values can overflow if values exceeds 88.72283935546875 or -88.72283172607421875
    // To prevent this, we clamp -88.5 < x < 89
    // (thresholds values are rounded to bf16, as it does not change result but only requires one SFPLOADI vs. two)
    sfpi::vFloat threshold_high = sfpi::vFloat(89);
    sfpi::vFloat threshold_low  = sfpi::vFloat(-88.5);
    vec_min_max(threshold_low, val);
    vec_min_max(val, threshold_high);

    // The paper relies on the following formula (c.f. Section 2 and 3 of paper):
    // z = (bias + x * factor * N_m); where:
    // factor = 0x00b8aa3b (computed through log(e))
    // bias = 0x3f800000
    //
    // Fundamentally, this computes exp(x) = 2**(x / ln2) = 2**(x_i) * 2**(x_f) where
    // - z_i = trunc(x / ln2) (integer part)
    // - z_f = x/ln2 - trunc(x/ln2) (fractional part)
    sfpi::vInt z                = _float_to_int32_exp21f_(val * sfpi::vFloat(0x00b8aa3b) + sfpi::vFloat(0x3f800000));
    sfpi::vInt exponential_part = exexp_nodebias(sfpi::reinterpret<sfpi::vFloat>(z)); // Extract exponent ( = 2**(integer part of val/ln2))
    sfpi::vInt fractional_part  = sfpi::exman9(sfpi::reinterpret<sfpi::vFloat>(z));   // Extract mantissa ( = leftover part, in [0; 1])

    // To refine approximation of 2**(x_f), we use an approximation of 2**x on [0; 1]
    // This uses a 2nd degree polynomial adjustment of the fractional part
    constexpr float POLY_D1 = 0.40196114e-7f;
    constexpr int POLY_D2   = 0xf94ee7;
    constexpr int POLY_D3   = 0x560e;

    // Compute polynomial through Horner's method
    sfpi::vFloat d1 = sfpi::vFloat(POLY_D1);
    sfpi::vFloat d2 = sfpi::int32_to_float(sfpi::vInt(POLY_D2) + fractional_part, 0);
    sfpi::vFloat d3 = sfpi::int32_to_float(sfpi::vInt(POLY_D3) + fractional_part, 0);
    d2              = d1 * d2;

    // Compute 2**(adjusted fractional part) through float -> int conversion
    fractional_part = _float_to_int32_exp21f_(d2 * d3);

    // Recombined exponent and mantissa: this is equivalent to 2**(x_i) * 2**(x_f)
    exponential_part = sfpi::reinterpret<sfpi::vInt>(sfpi::setexp(sfpi::reinterpret<sfpi::vFloat>(fractional_part), exponential_part)); // restore exponent

    y = sfpi::reinterpret<sfpi::vFloat>(exponential_part);

    if constexpr (!is_fp32_dest_acc_en)
    {
        // LRegs work on float32 data. If DST is bfloat16 then SFPSTORE will truncate it.
        // This can reduce accuracy: for instance, 9**2 = 80.8 gets round to 80.5
        // rather than 81 (which would have been correct).
        // To avoid this issue, we explicitly convert to bfloat16 using round-to-nearest-even.
        y = sfpi::reinterpret<sfpi::vFloat>(sfpi::float_to_fp16b(y, 0));
    }

    return y;
}

template <bool is_fp32_dest_acc_en>
sfpi_inline sfpi::vFloat _sfpu_exp_improved_(sfpi::vFloat val);

// is_fp32_dest_acc_en == false
template <>
sfpi_inline sfpi::vFloat _sfpu_exp_improved_<false>(sfpi::vFloat val)
{
    return _sfpu_exp_21f_<false>(val);
}

// is_fp32_dest_acc_en == true
// template <>
// sfpi_inline sfpi::vFloat _sfpu_exp_improved_<true>(sfpi::vFloat val) {
//    return _sfpu_exp_f32_accurate_(val);
//}

/*constexpr uint32_t EXPONENTIAL_INSTRUCTIONS = 30;

sfpi_inline void _program_exponential_replay_buffer_()
{
    lltt::record(0, EXPONENTIAL_INSTRUCTIONS);
    calculate_exponential_approx_no_swap_relu<2>();
}

sfpi_inline void _execute_exponential_replay_buffer_()
{
    lltt::replay(0, EXPONENTIAL_INSTRUCTIONS);
}*/

#define EXPONENTIAL_TESTING_MODE 3
#define EXP_CONFIG_NUM_TERMS     3
#define EXP_CONFIG_NUM_SCALE     3

template <bool APPROXIMATION_MODE, bool SCALE_EN, int ITERATIONS, bool FAST_APPROX, bool SKIP_POSITIVE_CHECK>
void _calculate_exponential_(const uint16_t exp_base_scale_factor /* 1.0f in BF16 */)
{
    if constexpr (FAST_APPROX && APPROXIMATION_MODE)
    {
#if EXPONENTIAL_TESTING_MODE == 1
        calculate_exponential_approx_no_swap_relu<ITERATIONS>();
        //_execute_exponential_replay_buffer_();
        return;
#elif EXPONENTIAL_TESTING_MODE == 2
        calculate_exponential_no_swap<ITERATIONS>();
        return;
#elif EXPONENTIAL_TESTING_MODE == 3
        calculate_exponential_more_terms<USE_ARECIP_INSTR_, SCALE_EN, ITERATIONS, EXP_CONFIG_NUM_TERMS>(exp_base_scale_factor);
        return;
#elif EXPONENTIAL_TESTING_MODE == 4
        calculate_exponential_no_swap_relu<ITERATIONS, EXP_CONFIG_NUM_TERMS, EXP_CONFIG_NUM_SCALE>();
        return;
#elif EXPONENTIAL_TESTING_MODE == 5
        /* template args:
         *   bool APPROXIMATION_MODE,
         *   bool FAST_APPROX,
         *   bool is_fp32_dest_acc_en,
         *   bool SCALE_EN = false,
         *   int ITERATIONS = 8,
         *   bool SKIP_POSITIVE_CHECK = false
         */
        // calculate_exponential<false,FAST_APPROX,false,false,ITERATIONS,false>(1);
        for (int d = 0; d < ITERATIONS; d++)
        {
            sfpi::vFloat val = sfpi::dst_reg[0];
            if constexpr (SCALE_EN)
            {
                val = val * sfpi::s2vFloat16b(exp_base_scale_factor);
            }
            sfpi::vFloat result = _sfpu_exp_improved_<false>(val);
            sfpi::dst_reg[0]    = result;
            sfpi::dst_reg++;
        }
        return;
#endif

        run_clamp_loadmacro();

        // Macro Sequence Register 0 configured to read back in the sanitized values and calculate the approximate exponential value
        //  LD     : the sanitized value from DEST (y)
        //  MAD    : compute (A * y) + (B-C)  , where A = (2^8)/ln(2) , B = 127 * (2^8) , C = Adjustment parameter of roughly 11.2 to minimize error
        //  ROUND  : convert the MAD result from FP32 to a 16-bit unsigned integer using stochastic rounding
        //  SIMPLE : shift the 16-bit integer to the left by 15 bits to place the MSB of the computed value into the MSB of the exponent bits of the fp32 format
        //  STORE  : store the shifted value back to dest
        TTI_SFPLOADMACRO(0, 0, ADDR_MOD_7, 0); // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[0] for loading and intermediate results
                                               // - Dest offset  0 is targeting the even columns for rows   3: 0
        TTI_SFPLOADMACRO(1, 0, ADDR_MOD_7, 2); // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[1] for loading and intermediate results
                                               // - Dest offset  2 is targeting the odd  columns for rows   3: 0
        TTI_SFPLOADMACRO(2, 0, ADDR_MOD_7, 4); // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[2] for loading and intermediate results
                                               // - Dest offset  4 is targeting the even columns for rows   7: 4
        TTI_SFPLOADMACRO(3, 0, ADDR_MOD_7, 6); // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[3] for loading and intermediate results
                                               // - Dest offset  6 is targeting the odd  columns for rows   7: 4
        TTI_SFPLOADMACRO(0, 0, ADDR_MOD_7, 8); // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[0] for loading and intermediate results
                                               // - Dest offset  8 is targeting the even columns for rows  11: 8
        TTI_SFPLOADMACRO(1, 0, ADDR_MOD_7, 10); // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[1] for loading and intermediate
                                                // results - Dest offset 10 is targeting the even columns for rows  11: 8
        TTI_SFPLOADMACRO(2, 0, ADDR_MOD_7, 12); // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[2] for loading and intermediate
                                                // results - Dest offset 12 is targeting the odd  columns for rows  15:12
        TTI_SFPLOADMACRO(3, 0, ADDR_MOD_7, 14); // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[3] for loading and intermediate
                                                // results - Dest offset 14 is targeting the even columns for rows  15:12
        // NOP needed to allow time for the final Computation Loadmacro to complete before returning to the Sanitation Loadmacro at the top for the next
        // iteration
        //  - to be completely safe, use 3 NOP; in practice 1 seems to be enough, probably because the overhead of the DEST INCRW stuff introduces 2 cycles of
        //  delay
        TTI_SFPNOP;
        // TTI_SFPNOP;
        // TTI_SFPNOP;
    }
    else
    {
        // Unroll 8 best for approx, unroll 0 for precise, compiler figures this out
        for (int d = 0; d < ITERATIONS; d++)
        {
            sfpi::vFloat in     = sfpi::dst_reg[0];
            sfpi::vFloat result = _calculate_exponential_piecewise_<APPROXIMATION_MODE, SCALE_EN, SKIP_POSITIVE_CHECK>(in, exp_base_scale_factor);
            sfpi::dst_reg[0]    = result;
            sfpi::dst_reg++;
        }
    }
}

template <bool APPROXIMATION_MODE, bool FAST_APPROX, uint32_t scale /* 1.0f in FP32 */>
inline void _init_exponential_()
{
    if constexpr (FAST_APPROX && APPROXIMATION_MODE)
    {
#if EXPONENTIAL_TESTING_MODE == 1
        calculate_exponential_approx_no_swap_relu_init<scale>();
        //_program_exponential_replay_buffer_();
        return;
#elif EXPONENTIAL_TESTING_MODE == 2
        calculate_exponential_no_swap_init<scale>();
        return;
#elif EXPONENTIAL_TESTING_MODE == 3
        calculate_exponential_more_terms_init<USE_ARECIP_INSTR_, EXP_CONFIG_NUM_TERMS, scale>();
        return;
#elif EXPONENTIAL_TESTING_MODE == 4
        calculate_exponential_no_swap_relu_init<EXP_CONFIG_NUM_TERMS, EXP_CONFIG_NUM_SCALE>();
        return;
#endif

        // Algorithm is adapted from:
        //      A Fast, Compact Approximation of the Exponential Function
        //      Nicol N. Schraudolph
        //      IDSIA, Lugano, Switzerland

        // First, set up constant values which are needed for the computation
        //      We will first sanitize the input values (y) to be in the range that won't cause underflow, which for our hardware means we need to limit
        //      negative values to be greater than or equal to -88.5 The computation that is needed is (A * y) + (B - C) , where A = (2^8)/ln(2) , B = 127 *
        //      (2^8) , C = Adjustment parameter of roughly 11.2 to minimize error
        //          - NOTE: we would like to be able to use 2^23 instead of 2^8 and compute a 32-bit quantity, but our hardware only supports rounding FP32 into
        //          a 16-bit integer, so we use 2^8 and then shift left by 15 bits after rounding
        //      So we will set up the following constants:
        //          LREG[14] =       =    -88.5               = 0xc2b10000
        //          LREG[12] = A     =    369.329925537109375 = 0x43b8aa3b
        //          LREG[13] = (B-C) =  32500.818359375       = 0x46fde9a3

        constexpr float LN2_RECIP = 1.4426950408889634f;
        constexpr float A         = 256.0f * LN2_RECIP;
        constexpr float B_minus_C = 32500.818359375f;
        constexpr float THRESHOLD = -88.5f;

        constexpr float scale_fp32 = __builtin_bit_cast(float, scale);

        constexpr float A_scaled         = A * scale_fp32;
        constexpr float THRESHOLD_scaled = THRESHOLD / scale_fp32;

        TTI_SFPLOADI(0, 0xA, lo16(THRESHOLD_scaled));
        TTI_SFPLOADI(0, 0x8, hi16(THRESHOLD_scaled));
        TTI_SFPCONFIG(0, 14, 0); // SFPCONFIG Dest 14 = LREG[14] =            -88.5               = 0xc2b10000

        TTI_SFPLOADI(0, 0xA, lo16(A_scaled));
        TTI_SFPLOADI(0, 0x8, hi16(A_scaled));
        TTI_SFPCONFIG(0, 12, 0); // SFPCONFIG Dest 12 = LREG[12] = A     =    369.329925537109375 = 0x43b8aa3b

        TTI_SFPLOADI(0, 0xA, lo16(B_minus_C));
        TTI_SFPLOADI(0, 0x8, hi16(B_minus_C));
        TTI_SFPCONFIG(0, 13, 0); // SFPCONFIG Dest 13 = LREG[13] = (B-C) =  32500.818359375       = 0x46fde9a3

        // Next, set up the macro instructions which will be necessary
        //  - for the sanitize function: we will need a SWAP instruction
        //  - for the main computation function: we will need MAD, ROUND, and SHIFT instructions

        // There are two ways to program the macro instruction registers, and this setup leverages both ways
        //  - we can either use the SFPCONFIG flow, by setting up the bits of the instruction into LREG[0] and then targeting the Macro instruction register
        //  - or we can use the shortcut / backdoor load method which relies on having some illegal destination register values as part of the instruction

        // Use SFPCONFIG method for the SWAP instruction, since we want the SWAP itself to use a destination register which is not normally a legal value
        //      (we are cheating a bit here, since we only care about one half of the swap and we want to use a constant for the other half)
        //
        //              imm12 = 0,       lreg_src_c = 0 (will be fed by value loaded from Dest into Loadmacro lreg_dest),  lreg_dest = LREG[14] = - 88.5,
        //              instr_mod1 = 1 swap the values with the larger of the two ending up in lreg_dest -> but we will use the Loadmacro lreg_dest register as
        //              output
        // TTI_SFP_SWAP(0,               0,                                                                                14,                            1);
        TTI_SFPLOADI(0, 0xA, 0x00E1);
        TTI_SFPLOADI(0, 0x8, 0x9200);
        TTI_SFPCONFIG(0, 0, 0); // SFPCONFIG Dest 0 = Programmable Macro instruction 0: TTI_SFPSWAP(0, 0, 14, 1); // compare against LREG[14] (-88.5), and put
                                // the larger value into LREG[loadmacro_lreg_dest]
        TTI_SFPNOP;

        // Backdoor load of Macro Instruction 1
        // Dummy version of MAD instruction with lreg_dest = 4'b11_01 = 13 to install into Programmable Macro instruction register 1, which is Macro Instruction
        // Register 5
        TTI_SFPMAD(12, 0, 13, 13, 0); // MACRO Instruction 1 <--- lreg X = lreg[12] (A) * lreg[0] (y) + lreg[13] (B-C)

        // Backdoor load of Macro Instruction 2
        // ROUND instruction to convert FP32 result into an integer value (int16)
        //                Stochastic = 0,  Imm(Descale),  SrcB(unused),   SrcC(input value),  Lreg_dest = 14 to install in Programmable Macro Instruction reg
        //                2'b10,  instr_mod1 = 14 to treat input as fp32, output as unsigned int16, use imm as descale
        TTI_SFP_STOCH_RND(0, 0, 0, 0, 14, 14); // Round to unsigned Int16

        // Backdoor load of Macro Instruction 3
        // If using the unsigned int rounding mode, then shift by 15; SHL to move integer bits to exponent;
        TTI_SFPSHFT(15, 0, 15, 1); // imm = 15 to shift left by 15 bits; lreg_c = 0 (will use macro reg); lreg_dest = 15 to install in Programmable Macro
                                   // Instruction reg 2'b11, which is Macro Instruction Register 7

        // So at this point, we have the following instructions loaded into our macro registers:
        //
        // 00: (no macro instruction, just execute whatever is issued from Tensix) <-- these are fixed / not programmable
        // 01: ( Rsvd                                                            ) <-- these are fixed / not programmable
        // 02: ( NOP                                                             ) <-- these are fixed / not programmable
        // 03: ( SFPSTORE                                                        ) <-- these are fixed / not programmable
        // 04: TTI_SFPSWAP       (0, 0, 11, 1)
        // 05: TTI_SFPMAD        (12, 0, 13, 13, 0)
        // 06: TTI_SFP_STOCH_RND (1, 0, 0, 0, 14, 14)
        // 07: TTI_SFPSHFT       (15,0,15,1)

        // Now we want to set up our two sequences

        // Sequence 1 setup: we want to Load, SWAP, <delay>, Store
        //       Delay slot:                  0     1        2
        //                                                                                                                                                                                                 Use
        //                                                                                                                                                                                                 Loaded  Result          Macro
        //                                                                                                                                                                                                 Value   Value   Delay   Instruction
        //                                                                                                                                                                                                 SRCB    Stage   Slot    Select
        TTI_SFPLOADI(0, 0xA, 0x0004); // slot1 : SIMPLE UNIT, want SWAP  instruction which is in macro instruction mux[4], delayed by 0 ; not using staging flop
                                      // as dest; not using load reg as srcb : 8'b0_______0_______000_____100          = 0x04 slot2 : MAD    UNIT, unused :
                                      // 8'b0_______0_______000_____000          = 0x00
        TTI_SFPLOADI(0, 0x8, 0x1300); // slot3 : ROUND  UNIT, unused : 8'b0_______0_______000_____000          = 0x00 slot4 : STORE  UNIT, want STORE
                                      // instruction which is in macro instruction mux[3], delayed by 2 ; not using staging flop as src ; :
                                      // 8'b0_______0_______010_____011          = 0x13
        TTI_SFPCONFIG(0, 5, 0);       // SFPCONFIG Dest 5 = Macro Sequence Register 1

        // Sequence 0 setup: we want to Load, MAD, <delay>, ROUND, SHIFT, Store
        //       Delay slot:                  0    1        2      3      4
        //                                                                                                                                                                                                 Use
        //                                                                                                                                                                                                 Loaded  Result          Macro
        //                                                                                                                                                                                                 Value   Value   Delay   Instruction
        //                                                                                                                                                                                                 SRCB    Stage   Slot    Select
        TTI_SFPLOADI(
            0,
            0xA,
            0x85DF); // slot1 : SIMPLE UNIT, want SHIFT instruction which is in macro instruction mux[7], delayed by 3 ;     using staging flop as dest; using
                     // load reg as srcb : 8'b1_______1_______011_____111          = 0xDF slot2 : MAD    UNIT, want MAD   instruction which is in macro
                     // instruction mux[5], delayed by 0 ; not using staging flop as dest;     using load reg as srcb : 8'b1_______0_______000_____101 = 0x85
        TTI_SFPLOADI(
            0,
            0x8,
            0x6316); // slot3 : ROUND  UNIT, want ROUND instruction which is in macro instruction mux[6], delayed by 2 ; not using staging flop as dest; using
                     // : 8'b0_______0_______010_____110          = 0x16 slot4 : STORE  UNIT, want STORE instruction which is in macro instruction mux[3],
                     // delayed by 4 ;     using staging flop as src ;     using                  : 8'b0_______1_______100_____011          = 0x63
        TTI_SFPCONFIG(0, 4, 0); // Load it into macro sequence register 0 (destination = 4)

        // Reset LoadMacroConfig[Lane].Misc for all lanes, in case it has been previously set by another use of macros.
        TTI_SFPCONFIG(0, 8, 1);
    }
    else if constexpr (APPROXIMATION_MODE)
    {
        sfpi::vConstFloatPrgm0 = 1.442695f; // ln2_recip
        sfpi::vConstFloatPrgm1 = sfpi::s2vFloat16b(p_exp::C23_73);
        sfpi::vConstFloatPrgm2 = sfpi::s2vFloat16b(p_exp::ADJ_EXP);
    }
    else
    {
        // Initialisation for use of _sfpu_reciprocal_<2> in _calculate_exponential_<APPROXIMATION_MODE=false>.
        _init_sfpu_reciprocal_<false>();
    }
}

} // namespace ckernel::sfpu
