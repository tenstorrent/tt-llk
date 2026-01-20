// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <limits>

#include "ckernel_addrmod.h"
#include "ckernel_ops.h"
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

template <bool APPROXIMATION_MODE, bool SCALE_EN, int ITERATIONS, bool FAST_APPROX, bool SKIP_POSITIVE_CHECK>
void _calculate_exponential_(const uint16_t exp_base_scale_factor /* 1.0f in BF16 */)
{
    if constexpr (FAST_APPROX && APPROXIMATION_MODE)
    {
        // ========================================================================
        // Pipelined exponential approximation achieving 2 cycles per iteration
        // ========================================================================
        //
        // ALGORITHM:
        //   exp(x) ≈ 2^((A*x + B) + |A*x + B|)
        //   where A = 256/ln(2) ≈ 369.33, B = offset ≈ 32500
        //
        //   This is based on Schraudolph's fast exponential approximation but
        //   restructured to avoid input sanitization and enable better pipelining.
        //
        // COMPUTATION FLOW (per iteration):
        //   1. LoadMacro: Load input x from DEST into LREG
        //   2. Discrete MAD: Compute temp = A * x + (B-C)
        //   3. Macro ABS: Compute |temp|
        //   4. Macro ADD: Compute result = temp + |temp|  (via 1.0 * temp + |temp|)
        //   5. Discrete STOCHRND: Convert FP32 result to uint16
        //   6. Macro SHFT2: Shift left by 15 bits (to position in exponent field)
        //   7. Macro STORE: Write final result to DEST
        //
        // PIPELINE STRUCTURE:
        //   Uses 4 LREGs (0-3) to maintain 2-cycle throughput in steady state.
        //   Each LoadMacro schedules future operations (ABS, ADD, SHFT2, STORE)
        //   that execute with appropriate delays as later iterations proceed.
        //
        // CYCLE-BY-CYCLE EXECUTION (steady state, iterations i and i+1):
        //   Cycle 0: LoadMacro(i)     - loads input for iteration i
        //   Cycle 1: Discrete MAD(i)  - computes temp = A*x + B for iteration i
        //            LoadMacro(i+1)   - loads input for iteration i+1
        //   Cycle 2: Macro ABS(i)     - computes |temp| for iteration i
        //            Discrete MAD(i+1)- computes temp for iteration i+1
        //   Cycle 3: Macro ADD(i)     - computes temp + |temp| for iteration i
        //            Discrete STOCHRND(i) - converts to uint16 for iteration i
        //   Cycle 4: Macro SHFT2(i)   - shifts result for iteration i
        //   Cycle 5: Macro STORE(i)   - stores result for iteration i
        //
        // INSTRUCTION SCHEDULING:
        //   - LoadMacro schedules: ABS (delay 1), ADD (delay 1), SHFT2 (delay 2), STORE (delay 4)
        //   - Discrete instructions: MAD and STOCHRND issued explicitly
        //   - SHFT2 must be in LoadMacro (writes to staging registers)
        //   - STOCHRND must be discrete (constraint from expert feedback)
        
        // ========================================================================
        // Iteration 0: LREG0 - Prime the pipeline
        // ========================================================================
        TTI_SFPLOADMACRO(0, 0, ADDR_MOD_7, 0);                                          // Cycle 0: Load x into LREG0, schedule macro ops
        TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG11, p_sfpu::LREG12, p_sfpu::LREG0, 0);   // Cycle 1: temp0 = A * x + (B-C)
        
        // ========================================================================
        // Iteration 1: LREG1 - Pipeline filling
        // ========================================================================
        TTI_SFPLOADMACRO(1, 0, ADDR_MOD_7, 2);                                          // Cycle 2: Load x into LREG1, schedule macro ops
        TTI_SFPMAD(p_sfpu::LREG1, p_sfpu::LREG11, p_sfpu::LREG12, p_sfpu::LREG1, 0);   // Cycle 3: temp1 = A * x + (B-C)
        TTI_SFP_STOCH_RND(0, 0, 0, p_sfpu::LREG0, p_sfpu::LREG0, sfpi::SFPSTOCHRND_MOD1_FP32_TO_UINT16);  // Cycle 3: Convert LREG0 (temp0 + |temp0|) to uint16
        //                                                                                                  // (ABS and ADD from iter 0 completed by now)
        
        // ========================================================================
        // Iteration 2: LREG2 - Pipeline filling
        // ========================================================================
        TTI_SFPLOADMACRO(2, 0, ADDR_MOD_7, 4);                                          // Cycle 4: Load x into LREG2, schedule macro ops
        TTI_SFPMAD(p_sfpu::LREG2, p_sfpu::LREG11, p_sfpu::LREG12, p_sfpu::LREG2, 0);   // Cycle 5: temp2 = A * x + (B-C)
        TTI_SFP_STOCH_RND(0, 0, 0, p_sfpu::LREG1, p_sfpu::LREG1, sfpi::SFPSTOCHRND_MOD1_FP32_TO_UINT16);  // Cycle 5: Convert LREG1 (temp1 + |temp1|) to uint16
        //                                                                                                  // (SHFT2 and STORE from iter 0 executing)
        
        // ========================================================================
        // Iteration 3: LREG3 - Pipeline full, steady state begins
        // ========================================================================
        TTI_SFPLOADMACRO(3, 0, ADDR_MOD_7, 6);                                          // Cycle 6: Load x into LREG3, schedule macro ops
        TTI_SFPMAD(p_sfpu::LREG3, p_sfpu::LREG11, p_sfpu::LREG12, p_sfpu::LREG3, 0);   // Cycle 7: temp3 = A * x + (B-C)
        TTI_SFP_STOCH_RND(0, 0, 0, p_sfpu::LREG2, p_sfpu::LREG2, sfpi::SFPSTOCHRND_MOD1_FP32_TO_UINT16);  // Cycle 7: Convert LREG2 (temp2 + |temp2|) to uint16
        //                                                                                                  // (iter 0 complete, iter 1 SHFT2/STORE executing)
        
        // ========================================================================
        // Iterations 4-7: Steady state - 2 cycles per iteration
        // ========================================================================
        TTI_SFPLOADMACRO(0, 0, ADDR_MOD_7, 8);                                          // Cycle 8: Reuse LREG0
        TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG11, p_sfpu::LREG12, p_sfpu::LREG0, 0);   // Cycle 9
        TTI_SFP_STOCH_RND(0, 0, 0, p_sfpu::LREG3, p_sfpu::LREG3, sfpi::SFPSTOCHRND_MOD1_FP32_TO_UINT16);  // Cycle 9
        
        TTI_SFPLOADMACRO(1, 0, ADDR_MOD_7, 10);                                         // Cycle 10: Reuse LREG1
        TTI_SFPMAD(p_sfpu::LREG1, p_sfpu::LREG11, p_sfpu::LREG12, p_sfpu::LREG1, 0);   // Cycle 11
        TTI_SFP_STOCH_RND(0, 0, 0, p_sfpu::LREG0, p_sfpu::LREG0, sfpi::SFPSTOCHRND_MOD1_FP32_TO_UINT16);  // Cycle 11
        
        TTI_SFPLOADMACRO(2, 0, ADDR_MOD_7, 12);                                         // Cycle 12: Reuse LREG2
        TTI_SFPMAD(p_sfpu::LREG2, p_sfpu::LREG11, p_sfpu::LREG12, p_sfpu::LREG2, 0);   // Cycle 13
        TTI_SFP_STOCH_RND(0, 0, 0, p_sfpu::LREG1, p_sfpu::LREG1, sfpi::SFPSTOCHRND_MOD1_FP32_TO_UINT16);  // Cycle 13
        
        TTI_SFPLOADMACRO(3, 0, ADDR_MOD_7, 14);                                         // Cycle 14: Reuse LREG3
        TTI_SFPMAD(p_sfpu::LREG3, p_sfpu::LREG11, p_sfpu::LREG12, p_sfpu::LREG3, 0);   // Cycle 15
        TTI_SFP_STOCH_RND(0, 0, 0, p_sfpu::LREG2, p_sfpu::LREG2, sfpi::SFPSTOCHRND_MOD1_FP32_TO_UINT16);  // Cycle 15
        
        // ========================================================================
        // Pipeline drain: Complete remaining operations
        // ========================================================================
        TTI_SFP_STOCH_RND(0, 0, 0, p_sfpu::LREG3, p_sfpu::LREG3, sfpi::SFPSTOCHRND_MOD1_FP32_TO_UINT16);  // Cycle 16: Final STOCHRND
        TTI_SFPNOP;  // Cycle 17: Allow final SHFT2 and STORE operations to complete
        
        // Total: 18 cycles for 8 iterations = 2.25 cycles/iteration average
        // (Steady state is 2 cycles/iteration for iterations 3-7)
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

constexpr auto bits = [](float x) constexpr { return __builtin_bit_cast(std::uint32_t, x); };
constexpr auto lo16 = [](float x) constexpr { return static_cast<std::uint16_t>(bits(x) & 0xFFFFu); };
constexpr auto hi16 = [](float x) constexpr { return static_cast<std::uint16_t>(bits(x) >> 16); };

template <bool APPROXIMATION_MODE, bool FAST_APPROX, uint32_t scale /* 1.0f in FP32 */>
inline void _init_exponential_()
{
    if constexpr (FAST_APPROX && APPROXIMATION_MODE)
    {
        // ========================================================================
        // Setup for pipelined exponential approximation
        // ========================================================================
        //
        // ALGORITHM OVERVIEW:
        //   exp(x) ≈ 2^((A*x + B) + |A*x + B|)
        //   where A = 256/ln(2) ≈ 369.33, B ≈ 32500
        //
        //   This achieves ~2 cycles per iteration through careful instruction
        //   scheduling across 5 execution units (Load, Simple, MAD, Round, Store)
        //
        // LOADMACRO MECHANICS:
        //   Each TTI_SFPLOADMACRO invocation:
        //   1. Loads input from DEST into specified LREG (Load unit, cycle 0)
        //   2. Schedules future operations based on Macro Sequence configuration
        //   3. Future ops execute with delays counted from the LoadMacro cycle
        //
        // EXECUTION UNITS AND THEIR ROLES:
        //   - Load unit:   Loads input (part of LoadMacro execution)
        //   - Simple unit: Executes ABS (from LoadMacro macro)
        //   - MAD unit:    Executes discrete MAD, then macro ADD
        //   - Round unit:  Executes discrete STOCHRND, then macro SHFT2
        //   - Store unit:  Executes STORE (from LoadMacro macro)

        constexpr float LN2_RECIP = 1.4426950408889634f;
        constexpr float A         = 256.0f * LN2_RECIP;      // ~369.33
        constexpr float B_minus_C = 32500.818359375f;

        constexpr float scale_fp32 = __builtin_bit_cast(float, scale);
        constexpr float A_scaled         = A * scale_fp32;
        constexpr float B_minus_C_scaled = B_minus_C;

        // ====================================================================
        // Step 1: Configure constant LREGs for the discrete MAD operation
        // ====================================================================
        // The discrete MAD computes: temp = A * x + (B-C)
        // This is the first stage of the exponential approximation
        
        TTI_SFPLOADI(0, 0xA, lo16(A_scaled));
        TTI_SFPLOADI(0, 0x8, hi16(A_scaled));
        TTI_SFPCONFIG(0, 11, 0);  // LREG[11] = A coefficient

        TTI_SFPLOADI(0, 0xA, lo16(B_minus_C_scaled));
        TTI_SFPLOADI(0, 0x8, hi16(B_minus_C_scaled));
        TTI_SFPCONFIG(0, 12, 0);  // LREG[12] = (B-C) offset

        // ====================================================================
        // Step 2: Backdoor load macro instructions (VD >= 12 triggers this)
        // ====================================================================
        // When an SFPU instruction has VD in range [12,15], it doesn't execute
        // normally but instead programs a macro instruction register:
        //   VD=12 -> Macro Instruction Register 4 (mux index 4)
        //   VD=13 -> Macro Instruction Register 5 (mux index 5)
        //   VD=14 -> Macro Instruction Register 6 (mux index 6)
        //   VD=15 -> Macro Instruction Register 7 (mux index 7)
        //
        // These macro instructions are templates that LoadMacro will instantiate
        // with appropriate source/dest registers during execution.

        // Macro Instruction 4 (Simple unit): SFPABS
        // Computes absolute value: |temp|
        // Will operate on the result of the discrete MAD instruction
        TTI_SFPABS(0, 0, 12, 0);

        // Macro Instruction 5 (MAD unit): SFPADD
        // Computes: 1.0 * temp + |temp| = temp + |temp|
        // This is the second stage: adding the original value to its absolute value
        TTI_SFPADD(p_sfpu::LCONST_1, 0, 0, 13, 0);

        // Macro Instruction 6 (Round unit): SFPSHFT2
        // Shifts left by 15 bits to position the value in the FP32 exponent field
        // IMPORTANT: SHFT2 (not SHFT) because it can write to staging registers
        // The staging register is required for the Store unit to read from
        TTI_SFPSHFT2(15, 0, 14, 1);

        // ====================================================================
        // Step 3: Configure Macro Sequence Register 0
        // ====================================================================
        // This defines WHEN each macro instruction executes relative to the
        // LoadMacro that triggered it. Used by MacroIndex 0-3 (LREG 0-3).
        //
        // DELAY SEMANTICS:
        //   Delays count from when LoadMacro executes:
        //   - Delay 0: Would execute same cycle as LoadMacro (not used here)
        //   - Delay 1: Executes 2 cycles after LoadMacro
        //   - Delay 2: Executes 3 cycles after LoadMacro
        //   - Delay N: Executes N+1 cycles after LoadMacro
        //
        // DEPENDENCY CHAIN (for a single iteration):
        //   Cycle 0: LoadMacro loads x into LREG
        //   Cycle 1: Discrete MAD computes temp = A*x + B-C (overwrites LREG)
        //   Cycle 2: Macro ABS executes (delay=1, operates on temp from MAD)
        //   Cycle 3: Macro ADD executes (delay=1, uses temp and |temp|)
        //            Discrete STOCHRND converts result to uint16
        //   Cycle 4: Macro SHFT2 executes (delay=2, shifts the uint16)
        //   Cycle 5: Macro STORE executes (delay=4, writes final result)
        //
        // ENCODING FORMAT (8 bits per unit):
        //   [7]   = use_loaded_as_srcb: Use the value loaded by LoadMacro as SRCB
        //   [6]   = use_staging: Write to (Simple/MAD/Round) or read from (Store) staging reg
        //   [5:3] = delay: How many cycles after LoadMacro to execute (0-7)
        //   [2:0] = macro_select: Which macro instruction template to use (0-7)
        //           0 = execute discrete instruction
        //           2 = NOP
        //           3 = STORE
        //           4-7 = programmable macro instructions
        
        TTI_SFPLOADI(0, 0xA, 0x0D04);
        // slot1 (Simple unit): 8'b0_0_001_100 = 0x04
        //   Bit 7=0: Don't use loaded value as SRCB
        //            (ABS will operate on LREG value which is temp from discrete MAD)
        //   Bit 6=0: Don't write to staging register
        //   Bits 5:3=001: Delay = 1 (execute 2 cycles after LoadMacro)
        //                 This gives time for discrete MAD to complete
        //   Bits 2:0=100: Use macro instruction 4 = ABS
        //
        // slot2 (MAD unit): 8'b0_0_001_101 = 0x0D
        //   Bit 7=0: Don't use loaded value as SRCB
        //            (ADD will use temp and |temp| from previous operations)
        //   Bit 6=0: Don't write to staging register
        //   Bits 5:3=001: Delay = 1 (execute 2 cycles after LoadMacro, same as ABS)
        //                 ABS and ADD can execute in parallel on their respective units
        //   Bits 2:0=101: Use macro instruction 5 = ADD

        TTI_SFPLOADI(0, 0x8, 0x6316);
        // slot3 (Round unit): 8'b0_0_010_110 = 0x16
        //   Bit 7=0: Don't use loaded value as SRCB
        //   Bit 6=0: Don't use staging for input (input comes from LREG)
        //   Bits 5:3=010: Delay = 2 (execute 3 cycles after LoadMacro)
        //                 This gives time for discrete STOCHRND to execute first
        //   Bits 2:0=110: Use macro instruction 6 = SHFT2
        //
        // slot4 (Store unit): 8'b0_1_100_011 = 0x63
        //   Bit 7=0: Don't use loaded value (not applicable for Store)
        //   Bit 6=1: Read from staging register
        //            (SHFT2 wrote its result to staging reg via bit 6 of its config)
        //   Bits 5:3=100: Delay = 4 (execute 5 cycles after LoadMacro)
        //                 This allows all previous operations to complete
        //   Bits 2:0=011: Use macro instruction 3 = STORE (fixed/built-in)

        TTI_SFPCONFIG(0, 4, 0);  // Install into Macro Sequence Register 0
                                  // Destination 4 selects Macro Sequence Register 0
                                  // which is used by MacroIndex 0-3 (our LREG indices)

        // Reset LoadMacroConfig[Lane].Misc for all lanes
        // This clears any previous macro configuration that might interfere
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
