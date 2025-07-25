// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_exp.h
 * @brief Multi-mode exponential function implementation for SFPU hardware
 *
 * @details This file implements exponential functions (e^x) using multiple algorithms
 * optimized for different accuracy and performance requirements. The implementation
 * supports both precise computation and fast approximation modes, with specialized
 * hardware acceleration using SFPU macro instructions for maximum throughput.
 *
 * **Algorithm Modes:**
 * - **PRECISE**: Uses range reduction + polynomial approximation with reciprocal
 * - **APPROXIMATION**: Fast bit-manipulation approach with range checking  
 * - **FAST_APPROX**: Ultra-fast macro instruction sequence for maximum throughput
 *
 * **Mathematical Foundation:**
 * All modes implement e^x using variations of:
 * 1. **Range Reduction**: e^x = e^(integer_part) * e^(fractional_part)
 * 2. **Polynomial Approximation**: e^(frac) ≈ polynomial series
 * 3. **Bit Manipulation**: Direct IEEE-754 exponent field manipulation
 *
 * **Hardware Acceleration:**
 * - Uses SFPU macro instruction sequences for pipeline optimization
 * - Leverages dedicated SFPU units (MAD, ROUND, SIMPLE, STORE)
 * - Implements range sanitization and conditional computation
 * - Optimized for 32-lane SIMD parallel processing
 */

#pragma once
#include <limits>

#include "ckernel_addrmod.h"
#include "ckernel_ops.h"
#include "ckernel_sfpu_recip.h"
#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel::sfpu
{

/**
 * @brief Core exponential computation using range reduction and polynomial approximation
 *
 * @details Implements the fundamental exponential algorithm using range reduction
 * to handle large inputs and polynomial approximation for the core computation.
 * This function handles positive inputs; negative inputs are processed by the caller
 * using reciprocal properties: e^(-x) = 1/e^x.
 *
 * **Algorithm Steps:**
 * 1. **Range Reduction**: Extract exponent if > -1, replace with -1
 * 2. **Polynomial Series**: Apply Horner's method polynomial evaluation
 * 3. **Range Restoration**: Apply repeated squaring based on extracted exponent
 *
 * **Mathematical Basis:**
 * ```
 * e^x = e^(integer_part + fractional_part)
 *     = e^(integer_part) * e^(fractional_part)
 *     = 2^(integer_part / ln(2)) * polynomial(fractional_part)
 * ```
 *
 * **Hardware Implementation:**
 * - `exexp(val)`: Extract exponent from IEEE-754 representation
 * - `setexp(val, 126)`: Set exponent to 126 (bias adjustment)
 * - `v_if/v_endif`: Hardware conditional execution per lane
 * - Polynomial coefficients: 0.8373, 0.863281 (from approximation theory)
 *
 * **Repeated Squaring Loop:**
 * The loop performs up to 7 iterations of squaring to reconstruct the full
 * exponential value from the fractional part approximation.
 *
 * @param val Input value (should be positive; sign handled by caller)
 * @return e^val computed using polynomial approximation
 *
 * @note Input value should have positive sign; negative handling done externally
 * @note Uses hardware conditional execution for efficient branch handling
 * @note Polynomial coefficients are tuned for accuracy over specific input range
 */
sfpi_inline sfpi::vFloat _sfpu_exp_(sfpi::vFloat val)
{
    // Range reduction: if exponent > -1, extract it and replace with -1
    sfpi::vInt exp = exexp(val);  // Extract IEEE-754 exponent
    v_if (exp >= 0)
    {
        val = setexp(val, 126);   // Set exponent to 126 (127-1 bias adjustment)
    }
    v_endif;

    // Polynomial approximation using Horner's method
    // Coefficients derived from exponential series approximation
    sfpi::vFloat tmp = val * sfpi::vConst0p8373 + sfpi::s2vFloat16b(0.863281);
    val              = val * tmp + sfpi::vConst1;

    // Range restoration: repeated squaring based on extracted exponent
    v_if (exp >= 0)
    {
        val = val * val;  // Initial squaring
        for (int s_iter = 0; s_iter < 7; s_iter++)
        {
            exp = exp - 1;
            v_and(exp >= 0);  // Narrow predication on each loop iteration
            val = val * val;  // Repeated squaring: val = val^(2^iteration)
        }
    }
    v_endif;

    return val;
}

/**
 * @brief Exponential computation with approximation mode selection
 *
 * @details Provides a unified interface for exponential computation with two
 * distinct algorithm implementations selected at compile time via template parameter.
 *
 * **APPROXIMATION_MODE=true: Fast Bit Manipulation**
 * Uses direct IEEE-754 bit manipulation for ultra-fast computation:
 * - Multiplies input by 1/ln(2) to convert to base-2
 * - Converts to fixed-point format for bit manipulation
 * - Directly constructs IEEE-754 result by shifting bits to exponent field
 *
 * **APPROXIMATION_MODE=false: Precise Polynomial**
 * Uses the precise `_sfpu_exp_()` algorithm with reciprocal for negative inputs:
 * - Handles sign by computing e^|x| then taking reciprocal if negative
 * - Uses polynomial approximation for higher accuracy
 * - Slower but more mathematically accurate
 *
 * @tparam APPROXIMATION_MODE If true, uses fast bit manipulation; if false, uses precise polynomial
 * @param in Input value for exponential computation
 * @return e^in computed using selected algorithm
 */
template <bool APPROXIMATION_MODE>
sfpi_inline sfpi::vFloat _calculate_exponential_body_(sfpi::vFloat in)
{
    sfpi::vFloat out;

    if constexpr (APPROXIMATION_MODE)
    {
        // Fast approximation using bit manipulation
        constexpr int FRAC_BITS = 3;
        constexpr uint SP_BIAS  = 127 << FRAC_BITS;  // IEEE-754 bias adjusted for fixed-point

        // Convert to base-2: multiply by 1/ln(2)
        sfpi::vFloat vConstLn2Recip = sfpi::vConstFloatPrgm0;  // 1.442695f
        sfpi::vFloat conv           = in * vConstLn2Recip;

        // Convert to fixed-point format and adjust bias
        sfpi::vInt c23_73 = p_exp::C23_73;
        sfpi::vInt tmp    = sfpi::reinterpret<sfpi::vInt>(conv) - c23_73;
        tmp += SP_BIAS;

        // Shift to place bits in IEEE-754 exponent field
        out = sfpi::reinterpret<sfpi::vFloat>(tmp << (10 - FRAC_BITS));
    }
    else
    {
        // Precise computation using polynomial approximation
        out = _sfpu_exp_(sfpi::setsgn(in, 0));  // Compute e^|x|

        // Handle negative inputs: e^(-x) = 1/e^x
        v_if (in < 0)
        {
            out = _sfpu_reciprocal_(out);
        }
        v_endif;
    }

    return out;
}

/**
 * @brief Fast exponential approximation using optimized bit manipulation
 *
 * @details Implements the ultra-fast exponential approximation using direct
 * IEEE-754 bit field manipulation. Based on Schraudolph's fast exponential
 * algorithm adapted for SFPU hardware constraints.
 *
 * **Algorithm Reference:**
 * "A Fast, Compact Approximation of the Exponential Function"
 * by Nicol N. Schraudolph, IDSIA, Lugano, Switzerland
 *
 * **Mathematical Approach:**
 * 1. Convert input to base-2: x' = x / ln(2)
 * 2. Add bias constants for IEEE-754 format alignment
 * 3. Shift result to exponent field position
 * 4. Reinterpret as floating-point number
 *
 * **Hardware Optimization:**
 * Uses pre-loaded constants in SFPU program registers for efficiency:
 * - vConstLn2Recip: 1/ln(2) for base conversion
 * - c23_73: Format conversion constant
 * - adj_exp: Bias adjustment for exponent field
 *
 * @param in Input value for exponential computation
 * @return e^in computed using fast bit manipulation
 *
 * @note Less accurate than polynomial methods but significantly faster
 * @note Optimized for cases where speed is more important than precision
 */
inline sfpi::vFloat _calculate_exponential_approx_(sfpi::vFloat in)
{
    // Convert input by multiplying with 1/ln(2) and adding format conversion constant
    sfpi::vFloat vConstLn2Recip = sfpi::vConstFloatPrgm0;  // 1.442695f
    sfpi::vFloat c23_73         = sfpi::vConstFloatPrgm1;
    sfpi::vInt adj_exp          = sfpi::vConstIntPrgm2;
    in                          = in * vConstLn2Recip + c23_73;

    // Adjust for IEEE-754 exponent bias and convert to integer
    sfpi::vInt in_short = adj_exp + sfpi::reinterpret<sfpi::vInt>(in);

    // Shift to place computed value in exponent field of IEEE-754 format
    in_short <<= 10 - p_exp::FRAC_BITS;
    return sfpi::reinterpret<sfpi::vFloat>(in_short);
}

/**
 * @brief Exponential computation with range checking and scaling support
 *
 * @details Implements exponential computation with optional input scaling and
 * comprehensive range checking to handle overflow and underflow conditions.
 * Provides both approximation and precise computation modes with saturation
 * for inputs outside the safe computation range.
 *
 * **Range Handling:**
 * - **Overflow Protection**: Inputs ≥ 89 saturate to +infinity
 * - **Underflow Protection**: Inputs < -42 saturate to 0.0
 * - **Safe Range**: [-42, 89] uses normal computation algorithms
 *
 * **Template Parameters:**
 * - `SCALE_EN`: Enables input scaling before computation
 * - `SKIP_POSITIVE_CHECK`: Skips overflow check (user responsibility)
 * - `APPROXIMATION_MODE`: Selects fast vs precise algorithm
 *
 * **Input Scaling:**
 * When SCALE_EN=true, input is pre-multiplied by scale factor:
 * `result = e^(input * scale_factor)`
 *
 * @tparam APPROXIMATION_MODE Algorithm selection (fast vs precise)
 * @tparam SCALE_EN Enable input scaling multiplication
 * @tparam SKIP_POSITIVE_CHECK Skip overflow checking for performance
 * @param in Input value for exponential computation
 * @param exp_base_scale_factor Scaling factor in BF16 format (if SCALE_EN=true)
 * @return e^(in * scale) with range saturation applied
 */
template <bool APPROXIMATION_MODE, bool SCALE_EN, bool SKIP_POSITIVE_CHECK>
inline sfpi::vFloat _calculate_exponential_piecewise_(sfpi::vFloat in, const uint16_t exp_base_scale_factor /* 1.0f in BF16 */)
{
    sfpi::vFloat result = 0.0f;
    
    // Apply input scaling if enabled
    if constexpr (SCALE_EN)
    {
        in = in * sfpi::s2vFloat16b(exp_base_scale_factor);
    }
    
    if constexpr (APPROXIMATION_MODE)
    {
        // Range-checked approximation mode
        if constexpr (!SKIP_POSITIVE_CHECK)
        {
            v_if (in >= 89)
            {
                // Overflow protection: saturate to infinity
                sfpi::vFloat in_inf = std::numeric_limits<float>::infinity();
                result              = in_inf;
            }
            v_elseif (in < -42)
            {
                // Underflow protection: saturate to zero
                result = 0.0f;
            }
            v_else
            {
                // Safe range: use fast approximation
                result = _calculate_exponential_approx_(in);
            }
            v_endif;
        }
        else
        {
            // Skip positive check: user ensures input ≤ 89
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
        // Precise mode: use polynomial approximation with reciprocal
        result = _sfpu_exp_(sfpi::setsgn(in, 0));

        v_if (in < 0)
        {
            result = _sfpu_reciprocal_(result);
        }
        v_endif;
    }

    return result;
}

/**
 * @brief Main exponential computation function with multiple optimization modes
 *
 * @details Provides the primary interface for exponential computation with extensive
 * optimization options. Supports ultra-fast macro instruction mode, standard
 * approximation mode, and precise computation mode selected via template parameters.
 *
 * **FAST_APPROX Mode (Ultra-High Performance):**
 * Uses SFPU macro instruction sequences for maximum throughput:
 * - **Input Sanitization**: Clips inputs to safe range [-88.5, +∞)
 * - **Macro Sequences**: Pre-programmed instruction pipelines
 * - **Pipeline Optimization**: Overlapped load/compute/store operations
 * - **Throughput**: Processes full tiles with minimal instruction overhead
 *
 * **Standard Modes:**
 * Uses traditional loop-based processing with selectable algorithms:
 * - **Approximation**: Fast bit manipulation with range checking
 * - **Precise**: Polynomial approximation with higher accuracy
 *
 * **Template Parameters:**
 * - `FAST_APPROX`: Enable ultra-fast macro instruction mode
 * - `APPROXIMATION_MODE`: Select fast vs precise algorithm (when not FAST_APPROX)
 * - `SCALE_EN`: Enable input scaling support
 * - `SKIP_POSITIVE_CHECK`: Skip overflow checking for performance
 * - `ITERATIONS`: Number of processing iterations for standard modes
 *
 * **Macro Instruction Pipeline (FAST_APPROX):**
 * 1. **Sanitization Macros**: Clip inputs to safe range
 * 2. **Computation Macros**: Apply fast exponential algorithm
 * 3. **Pipeline NOPs**: Ensure proper timing between macro sequences
 *
 * **Why NOPs After LOADMACRO:**
 * SFPU macro instructions have multi-cycle latency and are not fully pipelined:
 * - **SWAP Operation**: Takes 2 cycles, not pipelined → requires NOP
 * - **Macro Completion**: Final computation needs time before next sanitization
 * - **Pipeline Hazards**: Prevents data races between macro sequences
 *
 * As noted in the code:
 * ```
 * TTI_SFPNOP; // NOP is necessary because the SWAP operation takes 2 cycles 
 *            // and unfortunately is not pipelined
 * ```
 *
 * @tparam APPROXIMATION_MODE Algorithm selection (ignored if FAST_APPROX=true)
 * @tparam SCALE_EN Enable input scaling multiplication
 * @tparam ITERATIONS Number of processing iterations (ignored if FAST_APPROX=true)
 * @tparam FAST_APPROX Enable ultra-fast macro instruction mode
 * @tparam SKIP_POSITIVE_CHECK Skip overflow checking for performance
 * @param exp_base_scale_factor Scaling factor in BF16 format (if SCALE_EN=true)
 *
 * @note FAST_APPROX mode requires proper macro instruction initialization
 * @note NOPs are critical for correct pipeline timing in FAST_APPROX mode
 * @note Standard modes provide more flexibility at cost of lower throughput
 * @note Input range safety depends on mode and template parameter selection
 */
template <bool APPROXIMATION_MODE, bool SCALE_EN, int ITERATIONS, bool FAST_APPROX, bool SKIP_POSITIVE_CHECK>
void _calculate_exponential_(const uint16_t exp_base_scale_factor /* 1.0f in BF16 */)
{
    if constexpr (FAST_APPROX && APPROXIMATION_MODE)
    {
        /**
         * Ultra-fast macro instruction sequence for maximum throughput.
         * 
         * **Two-Stage Pipeline:**
         * 1. **Sanitization Stage**: Clips inputs to safe range [-88.5, +∞)
         * 2. **Computation Stage**: Applies fast exponential algorithm
         *
         * **Macro Sequence Registers:**
         * - Register 0: Computation sequence (LD→MAD→ROUND→SHIFT→STORE)
         * - Register 1: Sanitization sequence (LD→SWAP→STORE)
         *
         * **Pipeline Timing with NOPs:**
         * NOPs are essential because:
         * - SWAP operation takes 2 cycles and is not pipelined
         * - Macro sequences need completion time before next operation
         * - Prevents data hazards between sanitization and computation
         */
        
        // Sanitization macros: clip inputs to safe range [-88.5, +∞)
        // Uses SWAP operation to replace values < -88.5 with -88.5
        TTI_SFPLOADMACRO(4, 0, ADDR_MOD_7, 0);     // Even columns, rows 3:0
        TTI_SFPNOP; // ⚠️ CRITICAL: SWAP operation takes 2 cycles, not pipelined
        TTI_SFPLOADMACRO(5, 0, ADDR_MOD_7, 2);     // Odd columns, rows 3:0  
        TTI_SFPNOP; // ⚠️ CRITICAL: SWAP operation takes 2 cycles, not pipelined
        TTI_SFPLOADMACRO(6, 0, ADDR_MOD_7, 4);     // Even columns, rows 7:4
        TTI_SFPNOP; // ⚠️ CRITICAL: SWAP operation takes 2 cycles, not pipelined
        TTI_SFPLOADMACRO(7, 0, ADDR_MOD_7, 6);     // Odd columns, rows 7:4
        TTI_SFPNOP; // ⚠️ CRITICAL: SWAP operation takes 2 cycles, not pipelined
        TTI_SFPLOADMACRO(4, 0, ADDR_MOD_7, 8);     // Even columns, rows 11:8
        TTI_SFPNOP; // ⚠️ CRITICAL: SWAP operation takes 2 cycles, not pipelined
        TTI_SFPLOADMACRO(5, 0, ADDR_MOD_7, 10);    // Odd columns, rows 11:8
        TTI_SFPNOP; // ⚠️ CRITICAL: SWAP operation takes 2 cycles, not pipelined
        TTI_SFPLOADMACRO(6, 0, ADDR_MOD_7, 12);    // Even columns, rows 15:12
        TTI_SFPNOP; // ⚠️ CRITICAL: SWAP operation takes 2 cycles, not pipelined
        TTI_SFPLOADMACRO(7, 0, ADDR_MOD_7, 14);    // Odd columns, rows 15:12
        
        // Computation macros: apply fast exponential algorithm
        // Sequence: LD→MAD→ROUND→SHIFT→STORE (5-stage pipeline)
        TTI_SFPLOADMACRO(0, 0, ADDR_MOD_7, 0);     // Process sanitized values
        TTI_SFPLOADMACRO(1, 0, ADDR_MOD_7, 2);     // Parallel processing
        TTI_SFPLOADMACRO(2, 0, ADDR_MOD_7, 4);     // Multiple LREG targets
        TTI_SFPLOADMACRO(3, 0, ADDR_MOD_7, 6);     // for throughput
        TTI_SFPLOADMACRO(0, 0, ADDR_MOD_7, 8);     // Reuse LREG registers
        TTI_SFPLOADMACRO(1, 0, ADDR_MOD_7, 10);    // for next tile section
        TTI_SFPLOADMACRO(2, 0, ADDR_MOD_7, 12);    // Continue processing
        TTI_SFPLOADMACRO(3, 0, ADDR_MOD_7, 14);    // Complete tile
        
        // Final NOP ensures macro completion before potential next iteration
        TTI_SFPNOP;
        /**
         * As documented in code:
         * "NOP needed to allow time for the final Computation Loadmacro to complete 
         *  before returning to the Sanitation Loadmacro at the top for the next iteration"
         * 
         * Could use 3 NOPs for complete safety, but 1 NOP + destination register
         * increment overhead provides sufficient delay in practice.
         */
    }
    else
    {
        // Standard processing modes: loop-based with algorithm selection
        for (int d = 0; d < ITERATIONS; d++)
        {
            sfpi::vFloat in     = sfpi::dst_reg[0];
            sfpi::vFloat result = _calculate_exponential_piecewise_<APPROXIMATION_MODE, SCALE_EN, SKIP_POSITIVE_CHECK>(in, exp_base_scale_factor);
            sfpi::dst_reg[0]    = result;
            sfpi::dst_reg++;
        }
    }
}

// Compile-time constexpr helpers for constant generation
constexpr auto bits = [](float x) constexpr { return __builtin_bit_cast(std::uint32_t, x); };
constexpr auto lo16 = [](float x) constexpr { return static_cast<std::uint16_t>(bits(x) & 0xFFFFu); };
constexpr auto hi16 = [](float x) constexpr { return static_cast<std::uint16_t>(bits(x) >> 16); };

/**
 * @brief Initialize exponential computation constants and macro instructions
 *
 * @details Sets up the necessary constants and, for FAST_APPROX mode, programs
 * the SFPU macro instruction registers for ultra-high performance operation.
 * This initialization is critical for proper function operation and must be
 * called before any exponential computations.
 *
 * **FAST_APPROX Initialization (Most Complex):**
 * 
 * **1. Constant Setup:**
 * Based on Schraudolph's algorithm with hardware adaptations:
 * - A = 256 * (1/ln(2)) ≈ 369.33 (base conversion factor)
 * - B-C = 32500.82 (bias adjustment for IEEE-754 format)
 * - THRESHOLD = -88.5 (minimum safe input value)
 * - All constants scaled by input scale factor if provided
 *
 * **2. Macro Instruction Programming:**
 * Programs two macro sequence registers:
 * 
 * **Sequence 0 (Computation): LD→MAD→ROUND→SHIFT→STORE**
 * - MAD: Compute (A * input + B-C) for exponential approximation
 * - ROUND: Convert FP32 to 16-bit integer using stochastic rounding
 * - SHIFT: Move 16-bit result to IEEE-754 exponent field (shift left 15)
 * - STORE: Write final result back to destination
 *
 * **Sequence 1 (Sanitization): LD→SWAP→STORE**
 * - SWAP: Compare input with -88.5 threshold, keep larger value
 * - Prevents underflow in subsequent exponential computation
 * - Uses illegal destination register value as constant source
 *
 * **3. Hardware Limitations Addressed:**
 * - Hardware only supports FP32→16-bit rounding (not 32-bit)
 * - Uses 2^8 scaling instead of 2^23, then shifts by 15 bits
 * - SWAP operation requires 2-cycle completion time
 *
 * **Standard Mode Initialization:**
 * Sets up constants in SFPU program registers:
 * - ln2_recip = 1.442695 (for base conversion)
 * - Mode-specific polynomial coefficients
 * - Bias and adjustment constants
 *
 * @tparam APPROXIMATION_MODE Algorithm mode selection
 * @tparam FAST_APPROX Enable ultra-fast macro instruction mode
 * @tparam scale Input scaling factor as compile-time constant (FP32 format)
 *
 * @note FAST_APPROX mode requires extensive macro instruction setup
 * @note Constants are computed at compile-time using constexpr helpers
 * @note Macro instruction registers persist until explicitly reprogrammed
 * @note Standard modes use simpler constant-only initialization
 */
template <bool APPROXIMATION_MODE, bool FAST_APPROX, uint32_t scale /* 1.0f in FP32 */>
inline void _init_exponential_()
{
    if constexpr (FAST_APPROX && APPROXIMATION_MODE)
    {
        /**
         * Ultra-fast mode initialization: Set up constants and macro instructions
         * 
         * Algorithm adapted from:
         * "A Fast, Compact Approximation of the Exponential Function"
         * by Nicol N. Schraudolph, IDSIA, Lugano, Switzerland
         */

        // Mathematical constants for Schraudolph's algorithm
        constexpr float LN2_RECIP = 1.4426950408889634f;  // 1/ln(2)
        constexpr float A         = 256.0f * LN2_RECIP;   // Base conversion factor
        constexpr float B_minus_C = 32500.818359375f;     // Bias - adjustment term
        constexpr float THRESHOLD = -88.5f;               // Minimum safe input

        // Apply compile-time scaling to constants
        constexpr float scale_fp32 = __builtin_bit_cast(float, scale);
        constexpr float A_scaled         = A * scale_fp32;
        constexpr float THRESHOLD_scaled = THRESHOLD / scale_fp32;

        // Load constants into SFPU registers (split into 16-bit chunks)
        TTI_SFPLOADI(0, 0xA, lo16(THRESHOLD_scaled));
        TTI_SFPLOADI(0, 0x8, hi16(THRESHOLD_scaled));
        TTI_SFPCONFIG(0, 14, 0); // LREG[14] = -88.5 threshold

        TTI_SFPLOADI(0, 0xA, lo16(A_scaled));
        TTI_SFPLOADI(0, 0x8, hi16(A_scaled));
        TTI_SFPCONFIG(0, 12, 0); // LREG[12] = A (base conversion factor)

        TTI_SFPLOADI(0, 0xA, lo16(B_minus_C));
        TTI_SFPLOADI(0, 0x8, hi16(B_minus_C));
        TTI_SFPCONFIG(0, 13, 0); // LREG[13] = B-C (bias adjustment)

        /**
         * Macro Instruction Programming:
         * 
         * **Method 1: SFPCONFIG** - For complex instructions like SWAP
         * **Method 2: Backdoor Load** - For standard computational instructions
         */

        // Program SWAP instruction for sanitization (uses SFPCONFIG method)
        TTI_SFPLOADI(0, 0xA, 0x00E1);
        TTI_SFPLOADI(0, 0x8, 0x9200);
        TTI_SFPCONFIG(0, 0, 0); // Macro Instruction 0: SWAP with LREG[14] constant
        TTI_SFPNOP;

        // Program computational instructions (uses backdoor method)
        TTI_SFPMAD(12, 0, 13, 13, 0);     // Macro Instruction 1: MAD operation
        TTI_SFP_STOCH_RND(0, 0, 0, 0, 14, 14); // Macro Instruction 2: Round to int16
        TTI_SFPSHFT(15, 0, 15, 1);        // Macro Instruction 3: Shift left 15 bits

        /**
         * Final Macro Sequence Register Programming:
         * 
         * **Sequence 1 (Sanitization)**: LD→SWAP→delay→STORE
         * **Sequence 0 (Computation)**: LD→MAD→delay→ROUND→SHIFT→STORE
         */

        // Program Sequence 1: Sanitization sequence
        TTI_SFPLOADI(0, 0xA, 0x0004); // SIMPLE unit uses SWAP (macro 4), no delay
        TTI_SFPLOADI(0, 0x8, 0x1300); // STORE unit uses STORE (macro 3), 2-cycle delay
        TTI_SFPCONFIG(0, 5, 0);       // Load into Macro Sequence Register 1

        // Program Sequence 0: Full computation sequence  
        TTI_SFPLOADI(0, 0xA, 0x85DF); // SIMPLE uses SHIFT (macro 7), 3-cycle delay
                                      // MAD uses MAD (macro 5), no delay
        TTI_SFPLOADI(0, 0x8, 0x6316); // ROUND uses ROUND (macro 6), 2-cycle delay
                                      // STORE uses STORE (macro 3), 4-cycle delay
        TTI_SFPCONFIG(0, 4, 0);       // Load into Macro Sequence Register 0

        // Reset macro configuration state for clean operation
        TTI_SFPCONFIG(0, 8, 1);
    }
    else if constexpr (APPROXIMATION_MODE)
    {
        // Standard approximation mode: basic constants only
        sfpi::vConstFloatPrgm0 = 1.442695f; // ln2_recip for base conversion
        sfpi::vConstFloatPrgm1 = sfpi::s2vFloat16b(p_exp::C23_73);    // Format conversion
        sfpi::vConstFloatPrgm2 = sfpi::s2vFloat16b(p_exp::ADJ_EXP);   // Bias adjustment
    }
    else
    {
        // Precise mode: polynomial coefficients
        sfpi::vConstFloatPrgm0 = 1.442695f; // ln2_recip for range reduction
        sfpi::vConstFloatPrgm1 = 2.0f;      // Polynomial coefficient
        sfpi::vConstFloatPrgm2 = 0.863281f; // Polynomial coefficient
    }
}

} // namespace ckernel::sfpu
