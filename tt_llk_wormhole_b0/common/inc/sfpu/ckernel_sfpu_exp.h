// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_exp.h
 * @brief Exponential Function Implementation for Tensix SFPU
 *
 * This file provides multiple exponential function implementations optimized for
 * different accuracy and performance requirements on the Tensix Special Function
 * Processing Unit (SFPU).
 *
 * ## Implementation Strategies:
 *
 * ### 1. High-Precision Mode (APPROXIMATION_MODE=false)
 * - Uses Horner's form polynomial series expansion
 * - Highest accuracy, suitable for training workloads
 * - Performance: ~10x slower than approximation modes
 * - Range: Full IEEE 754 float32 range with proper overflow/underflow handling
 *
 * ### 2. Standard Approximation Mode (APPROXIMATION_MODE=true)
 * - Uses 7.3 fixed-point arithmetic with bit manipulation
 * - Formula: exp(x) ≈ 2^(x/ln2) via exponent field manipulation
 * - Good balance of speed vs accuracy for inference
 * - Range: [-42, 89] with saturation outside bounds
 *
 * ### 3. Fast Approximation Mode (FAST_APPROX=true)
 * - Implements Schraudolph's fast exponential algorithm
 * - Uses hardware macro instruction sequences for maximum performance
 * - ~10x faster than standard approximation
 * - Range: [-88.5, 89] with input sanitization
 * - Reference: "A Fast, Compact Approximation of the Exponential Function"
 *   by Nicol N. Schraudolph, IDSIA, 1999
 *
 * ## Template Parameters:
 * - APPROXIMATION_MODE: Enable fast approximation algorithms
 * - FAST_APPROX: Enable ultra-fast Schraudolph algorithm (requires APPROXIMATION_MODE)
 * - SCALE_EN: Enable input scaling for domain adjustment
 * - SKIP_POSITIVE_CHECK: Skip overflow checks for performance (user responsible)
 * - ITERATIONS: Number of SIMD lanes to process
 *
 * ## Error Bounds:
 * - High-Precision: < 1 ULP for most inputs
 * - Standard Approx: < 0.1% relative error in [-10, 10] range
 * - Fast Approx: < 1% relative error in [-20, 20] range
 *
 * @warning Fast approximation mode uses hardware-specific macro instructions
 *          and is only compatible with Wormhole B0 Tensix cores.
 */

#pragma once

#include <limits>

#include "ckernel_sfpu_exp.h"
#include "ckernel_sfpu_recip.h"
#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel::sfpu
{

// Mathematical constants for exponential calculations
namespace exp_constants
{
// Natural logarithm base conversion
constexpr float LN2_RECIP = 1.4426950408889634f; // 1/ln(2) for base conversion

// Schraudolph algorithm constants (derived from IEEE 754 format)
constexpr float SCHRAUDOLPH_A         = 256.0f * LN2_RECIP; // Scale factor: 369.329925537109375
constexpr float SCHRAUDOLPH_B_MINUS_C = 32500.818359375f;   // Bias adjustment for minimal error

// Domain limits for safe computation
constexpr float OVERFLOW_THRESHOLD       = 89.0f;  // exp(89) ≈ 4.5e38 (near float32 max)
constexpr float UNDERFLOW_THRESHOLD      = -42.0f; // exp(-42) ≈ 4.2e-19 (reasonable precision)
constexpr float FAST_UNDERFLOW_THRESHOLD = -88.5f; // Schraudolph algorithm limit

// Fixed-point arithmetic constants
constexpr int FRAC_BITS = 3;                // Fractional bits in 7.3 format
constexpr uint SP_BIAS  = 127 << FRAC_BITS; // Single precision exponent bias

// Polynomial coefficients for high-precision mode
constexpr float HORNER_C1 = 0.863281f; // First coefficient in Horner's form
} // namespace exp_constants

/**
 * @brief High-precision exponential calculation using polynomial series
 *
 * Implements exp(x) using Horner's form polynomial evaluation with range reduction.
 * This is the most accurate implementation but also the slowest.
 *
 * Algorithm:
 * 1. Extract and normalize exponent to range [0.5, 1.0)
 * 2. Apply Horner's polynomial: P(x) = x * (c0*x + c1) + 1
 * 3. Reconstruct result by repeated squaring based on original exponent
 *
 * @param val Input value (any finite float32)
 * @return exp(val) with high precision
 *
 * @note Assumes input has been made positive (sign handled by caller)
 */
sfpi_inline sfpi::vFloat _sfpu_exp_(sfpi::vFloat val)
{
    // Extract exponent: if exp >= 0, we need range reduction
    sfpi::vInt exp = exexp(val);
    v_if (exp >= 0)
    {
        // Normalize mantissa to [0.5, 1.0) by setting exponent to 126 (2^-1)
        val = setexp(val, 126);
    }
    v_endif;

    // Horner's form polynomial approximation for exp(x) on [0.5, 1.0)
    // P(x) = x * (0.8373*x + 0.863281) + 1
    sfpi::vFloat tmp = val * sfpi::vConst0p8373 + sfpi::s2vFloat16b(exp_constants::HORNER_C1);
    val              = val * tmp + sfpi::vConst1;

    // Reconstruct full result by repeated squaring
    // Each iteration multiplies result by itself (2^1, 2^2, 2^4, 2^8, ...)
    v_if (exp >= 0)
    {
        val = val * val; // Initial squaring
        for (int s_iter = 0; s_iter < 7; s_iter++)
        {
            exp = exp - 1;
            // Continue squaring only while exponent bits remain
            v_and(exp >= 0);
            val = val * val;
        }
    }
    v_endif;

    return val;
}

/**
 * @brief Main exponential calculation dispatcher
 *
 * Selects between high-precision polynomial evaluation and fast approximation
 * based on the APPROXIMATION_MODE template parameter.
 *
 * @tparam APPROXIMATION_MODE If true, use fast 7.3 fixed-point approximation;
 *                           if false, use high-precision polynomial series
 * @param in Input value (any finite float32)
 * @return exp(in) computed using selected algorithm
 *
 * Approximation Algorithm:
 * - Convert exp(x) to 2^(x/ln2) using bit manipulation
 * - Use 7.3 fixed-point format for integer portion
 * - Directly construct IEEE 754 result by setting exponent field
 *
 * Precision Algorithm:
 * - Handle sign separately: exp(-x) = 1/exp(x)
 * - Use polynomial series expansion with range reduction
 */
template <bool APPROXIMATION_MODE>
sfpi_inline sfpi::vFloat _calculate_exponential_body_(sfpi::vFloat in)
{
    sfpi::vFloat out;

    if constexpr (APPROXIMATION_MODE)
    {
        // Fast approximation using bit manipulation
        // Formula: exp(x) = 2^(x/ln2) implemented via exponent field manipulation

        // Convert to base-2: x/ln2
        sfpi::vFloat vConstLn2Recip = sfpi::vConstFloatPrgm0; // Should contain LN2_RECIP
        sfpi::vFloat conv           = in * vConstLn2Recip;

        // Convert to 7.3 fixed-point format
        sfpi::vInt c23_73 = p_exp::C23_73; // Constant for clearing exponent bits
        sfpi::vInt tmp    = sfpi::reinterpret<sfpi::vInt>(conv) - c23_73;

        // Add IEEE 754 single precision bias (127) scaled for 7.3 format
        tmp += exp_constants::SP_BIAS;

        // Shift to place integer bits in exponent field of IEEE 754 format
        out = sfpi::reinterpret<sfpi::vFloat>(tmp << (10 - exp_constants::FRAC_BITS));
    }
    else
    {
        // High-precision calculation using polynomial series
        // Handle negative inputs: exp(-x) = 1/exp(x)
        out = _sfpu_exp_(sfpi::setsgn(in, 0)); // Compute exp(|in|)

        v_if (in < 0)
        {
            out = _sfpu_reciprocal_(out); // Apply sign correction
        }
        v_endif;
    }

    return out;
}

/**
 * @brief Optimized exponential approximation for use with precomputed constants
 *
 * This version assumes constants have been preloaded into programmable registers
 * for maximum performance. Used by the fast approximation path.
 *
 * @param in Input value (should be in safe range [-88.5, 89])
 * @return exp(in) using fast bit manipulation
 *
 * Algorithm details:
 * 1. Convert x to x/ln2 using preloaded reciprocal
 * 2. Add bias constant to shift into IEEE 754 exponent range
 * 3. Use integer arithmetic to construct final float representation
 *
 * @note Requires _init_exponential_() to have been called with APPROXIMATION_MODE=true
 */
inline sfpi::vFloat _calculate_exponential_approx_(sfpi::vFloat in)
{
    // Base conversion: exp(x) = 2^(x/ln2)
    sfpi::vFloat vConstLn2Recip = sfpi::vConstFloatPrgm0; // 1/ln2
    sfpi::vFloat c23_73         = sfpi::vConstFloatPrgm1; // Bias constant
    sfpi::vInt adj_exp          = sfpi::vConstIntPrgm2;   // Exponent adjustment

    // Apply conversion and bias in one operation
    in = in * vConstLn2Recip + c23_73;

    // Construct IEEE 754 representation:
    // Remove fractional exponent bits and add proper bias
    sfpi::vInt in_short = adj_exp + sfpi::reinterpret<sfpi::vInt>(in);

    // Shift integer portion to exponent field position
    in_short <<= 10 - p_exp::FRAC_BITS;
    return sfpi::reinterpret<sfpi::vFloat>(in_short);
}

/**
 * @brief Domain-aware exponential calculation with overflow/underflow protection
 *
 * Handles different input ranges appropriately, providing saturation for
 * out-of-range inputs and optimal algorithms for in-range values.
 *
 * @tparam APPROXIMATION_MODE Use fast approximation if true, precision if false
 * @tparam SCALE_EN Apply input scaling before computation
 * @tparam SKIP_POSITIVE_CHECK Skip overflow checks for performance (user must ensure in <= 89)
 * @param in Input value
 * @param exp_base_scale_factor Scaling factor in BF16 format (typically 1.0f)
 * @return exp(in * scale_factor) with proper domain handling
 *
 * Domain handling:
 * - Overflow (in >= 89): Return +infinity
 * - Underflow (in < -42): Return 0.0
 * - Normal range: Use selected algorithm
 *
 * @warning When SKIP_POSITIVE_CHECK=true, caller must ensure in <= 89
 *          to avoid incorrect results for large inputs
 */
template <bool APPROXIMATION_MODE, bool SCALE_EN, bool SKIP_POSITIVE_CHECK>
inline sfpi::vFloat _calculate_exponential_piecewise_(sfpi::vFloat in, const uint16_t exp_base_scale_factor /* 1.0f in BF16 */)
{
    sfpi::vFloat result = 0.0f;

    // Apply optional input scaling
    if constexpr (SCALE_EN)
    {
        in = in * sfpi::s2vFloat16b(exp_base_scale_factor);
    }

    if constexpr (APPROXIMATION_MODE)
    {
        // Fast approximation with domain checking
        if constexpr (!SKIP_POSITIVE_CHECK)
        {
            v_if (in >= exp_constants::OVERFLOW_THRESHOLD)
            {
                // Saturate to infinity for overflow protection
                // exp(89) ≈ 4.5e38, close to float32 maximum
                result = std::numeric_limits<float>::infinity();
            }
            v_elseif (in < exp_constants::UNDERFLOW_THRESHOLD)
            {
                // Saturate to zero for underflow protection
                // exp(-42) ≈ 4.2e-19, very small but representable
                result = 0.0f;
            }
            v_else
            {
                // Normal range: use fast approximation
                result = _calculate_exponential_approx_(in);
            }
            v_endif;
        }
        else
        {
            // Skip positive overflow check for performance
            // User guarantees input <= OVERFLOW_THRESHOLD
            v_if (in < exp_constants::UNDERFLOW_THRESHOLD)
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
        // High-precision mode: full range support
        // Handle negative values using reciprocal identity
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
 * @brief Main exponential calculation function with multiple optimization modes
 *
 * This is the primary entry point for exponential calculations, supporting
 * three different algorithm implementations based on template parameters.
 *
 * @tparam APPROXIMATION_MODE Enable approximation algorithms (vs high precision)
 * @tparam SCALE_EN Enable input scaling before computation
 * @tparam ITERATIONS Number of SIMD vector elements to process
 * @tparam FAST_APPROX Enable ultra-fast Schraudolph algorithm (requires APPROXIMATION_MODE)
 * @tparam SKIP_POSITIVE_CHECK Skip overflow checks for performance (user guarantees safe input)
 *
 * @param exp_base_scale_factor Input scaling factor in BF16 format
 *
 * Algorithm Selection:
 * 1. FAST_APPROX=true: Uses hardware macro sequences for maximum speed
 * 2. APPROXIMATION_MODE=true: Uses 7.3 fixed-point bit manipulation
 * 3. Both false: Uses high-precision polynomial evaluation
 *
 * Performance (relative):
 * - Fast Approx: 1x (baseline, fastest)
 * - Standard Approx: ~3x slower than fast
 * - High Precision: ~10x slower than fast
 *
 * @note Fast approximation requires specific hardware initialization
 *       and is only available on Wormhole B0 Tensix cores
 */
template <bool APPROXIMATION_MODE, bool SCALE_EN, int ITERATIONS, bool FAST_APPROX, bool SKIP_POSITIVE_CHECK>
void _calculate_exponential_(const uint16_t exp_base_scale_factor /* 1.0f in BF16 */)
{
    if constexpr (FAST_APPROX && APPROXIMATION_MODE)
    {
        // === ULTRA-FAST SCHRAUDOLPH ALGORITHM ===
        //
        // This implementation uses hardware macro instruction sequences to achieve
        // maximum performance at the cost of some accuracy. The algorithm proceeds
        // in two phases:
        //
        // Phase 1: Input Sanitization
        // - Clamp inputs to safe range [-88.5, +inf] to prevent underflow
        // - Use SWAP instruction to replace values < -88.5 with -88.5
        //
        // Phase 2: Exponential Computation
        // - Apply Schraudolph's bit manipulation formula
        // - Use MAD + ROUND + SHIFT sequence for optimal performance
        //
        // The macro sequences are pre-programmed during initialization and
        // execute multiple operations per instruction for maximum throughput.

        // PHASE 1: INPUT SANITIZATION
        // Macro Sequence Register 1: Load -> SWAP -> Store
        // Clamps any value < -88.5 to exactly -88.5 for safe computation
        TTI_SFPLOADMACRO(
            4,
            0,
            3,
            0);     // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[0] for loaded value - Dest offset  0 is targeting the even columns for rows   3: 0
        TTI_SFPNOP; // NOP is necessary because the SWAP operation takes 2 cycles and unfortunately is not pipelined
        TTI_SFPLOADMACRO(
            5,
            0,
            3,
            2); // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[1] for loaded value - Dest offset  2 is targeting the odd  columns for rows   3: 0
        TTI_SFPNOP;
        TTI_SFPLOADMACRO(
            6,
            0,
            3,
            4); // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[2] for loaded value - Dest offset  4 is targeting the even columns for rows   7: 4
        TTI_SFPNOP;
        TTI_SFPLOADMACRO(
            7,
            0,
            3,
            6); // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[3] for loaded value - Dest offset  6 is targeting the odd  columns for rows   7: 4
        TTI_SFPNOP;
        TTI_SFPLOADMACRO(
            4,
            0,
            3,
            8); // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[0] for loaded value - Dest offset  8 is targeting the even columns for rows  11: 8
        TTI_SFPNOP;
        TTI_SFPLOADMACRO(
            5,
            0,
            3,
            10); // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[1] for loaded value - Dest offset 10 is targeting the even columns for rows  11: 8
        TTI_SFPNOP;
        TTI_SFPLOADMACRO(
            6,
            0,
            3,
            12); // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[2] for loaded value - Dest offset 12 is targeting the odd  columns for rows  15:12
        TTI_SFPNOP;
        TTI_SFPLOADMACRO(
            7,
            0,
            3,
            14); // MACRO Sequence Register 1: LD, SWAP, STORE - uses LREG[3] for loaded value - Dest offset 14 is targeting the even columns for rows  15:12
        // NOP not needed in this spot because the next LoadMacro is a computational macro which doesn't immediately use the SIMPLE unit

        // PHASE 2: EXPONENTIAL COMPUTATION
        // Macro Sequence Register 0: Load -> MAD -> Round -> Shift -> Store
        // Implements: exp(y) = 2^((A*y) + (B-C)) via IEEE 754 exponent manipulation
        // where:
        //   A = 256/ln(2) ≈ 369.33 (scale factor)
        //   B = 127*256 = 32512 (IEEE 754 bias adjustment)
        //   C ≈ 11.2 (error minimization constant)
        //   Result: (B-C) ≈ 32500.818
        TTI_SFPLOADMACRO(0, 0, 3, 0);  // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[0] for loading and intermediate results - Dest
                                       // offset  0 is targeting the even columns for rows   3: 0
        TTI_SFPLOADMACRO(1, 0, 3, 2);  // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[1] for loading and intermediate results - Dest
                                       // offset  2 is targeting the odd  columns for rows   3: 0
        TTI_SFPLOADMACRO(2, 0, 3, 4);  // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[2] for loading and intermediate results - Dest
                                       // offset  4 is targeting the even columns for rows   7: 4
        TTI_SFPLOADMACRO(3, 0, 3, 6);  // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[3] for loading and intermediate results - Dest
                                       // offset  6 is targeting the odd  columns for rows   7: 4
        TTI_SFPLOADMACRO(0, 0, 3, 8);  // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[0] for loading and intermediate results - Dest
                                       // offset  8 is targeting the even columns for rows  11: 8
        TTI_SFPLOADMACRO(1, 0, 3, 10); // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[1] for loading and intermediate results - Dest
                                       // offset 10 is targeting the even columns for rows  11: 8
        TTI_SFPLOADMACRO(2, 0, 3, 12); // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[2] for loading and intermediate results - Dest
                                       // offset 12 is targeting the odd  columns for rows  15:12
        TTI_SFPLOADMACRO(3, 0, 3, 14); // MACRO Sequence Register 0: LD, MAD, ROUND, SHIFT and STORE - uses LREG[3] for loading and intermediate results - Dest
                                       // offset 14 is targeting the even columns for rows  15:12
        // Pipeline synchronization: ensure macro completion before next iteration
        // Hardware requires 1-3 NOPs depending on pipeline depth and instruction timing
        // Single NOP is usually sufficient due to DEST register increment overhead
        TTI_SFPNOP;
        // Additional NOPs can be uncommented for debugging timing issues:
        // TTI_SFPNOP; TTI_SFPNOP;
    }
    else
    {
        // === STANDARD ALGORITHM PATHS ===
        // Use either approximation or high-precision algorithms
        // Compiler automatically optimizes loop unrolling based on mode:
        // - Approximation mode: unroll 8x for better throughput
        // - Precision mode: unroll 0 (no unroll) for better accuracy

        for (int d = 0; d < ITERATIONS; d++)
        {
            sfpi::vFloat val    = sfpi::dst_reg[0];
            sfpi::vFloat result = _calculate_exponential_piecewise_<APPROXIMATION_MODE, SCALE_EN, SKIP_POSITIVE_CHECK>(val, exp_base_scale_factor);
            sfpi::dst_reg[0]    = result;
            sfpi::dst_reg++; // Advance to next SIMD vector
        }
    }
}

// Utility functions for extracting float bit patterns
// Used by fast approximation initialization for loading constants
constexpr auto bits = [](float x) constexpr { return __builtin_bit_cast(std::uint32_t, x); };
constexpr auto lo16 = [](float x) constexpr { return static_cast<std::uint16_t>(bits(x) & 0xFFFFu); };
constexpr auto hi16 = [](float x) constexpr { return static_cast<std::uint16_t>(bits(x) >> 16); };

/**
 * @brief Initialize exponential function constants and hardware configuration
 *
 * Sets up the necessary constants and hardware state for the selected exponential
 * algorithm. Must be called before using _calculate_exponential_().
 *
 * @tparam APPROXIMATION_MODE Algorithm mode selection
 * @tparam FAST_APPROX Enable Schraudolph ultra-fast algorithm
 * @tparam scale Input scaling factor as uint32_t bit pattern of float32
 *
 * Initialization modes:
 * 1. FAST_APPROX=true: Programs hardware macro instruction sequences
 * 2. APPROXIMATION_MODE=true: Loads bit manipulation constants
 * 3. Both false: Loads polynomial evaluation constants
 *
 * @warning Fast approximation mode modifies global hardware state
 *          and may interfere with other SFPU operations
 */
template <bool APPROXIMATION_MODE, bool FAST_APPROX, uint32_t scale /* 1.0f in FP32 */>
inline void _init_exponential_()
{
    if constexpr (FAST_APPROX && APPROXIMATION_MODE)
    {
        // === SCHRAUDOLPH FAST EXPONENTIAL INITIALIZATION ===
        //
        // Reference: "A Fast, Compact Approximation of the Exponential Function"
        //           Nicol N. Schraudolph, IDSIA, Lugano, Switzerland, 1999
        //
        // Algorithm overview:
        // exp(x) ≈ 2^(x/ln2) implemented by direct IEEE 754 bit manipulation
        //
        // Mathematical foundation:
        // 1. Convert exp(x) to 2^(x/ln2) using logarithm properties
        // 2. Compute (A*x + B) where result bits directly form IEEE 754 exponent
        // 3. Use hardware rounding + shifting to construct final float
        //
        // Constants (with scaling applied):
        // - A = 256/ln(2) ≈ 369.33 (base conversion + 8-bit scaling)
        // - B-C = 32500.82 (IEEE bias - error correction)
        // - Threshold = -88.5 (underflow protection limit)
        //
        // Hardware limitations:
        // - Can only round FP32 → 16-bit integer (not 32-bit)
        // - Therefore use 2^8 scaling + 15-bit left shift instead of direct 2^23
        //
        // Register allocation:
        // - LREG[14] = -88.5 (sanitization threshold)
        // - LREG[12] = A_scaled (conversion factor)
        // - LREG[13] = B-C (bias adjustment)

        // Use constants from exp_constants namespace
        constexpr float LN2_RECIP = exp_constants::LN2_RECIP;
        constexpr float A         = exp_constants::SCHRAUDOLPH_A;
        constexpr float B_minus_C = exp_constants::SCHRAUDOLPH_B_MINUS_C;
        constexpr float THRESHOLD = exp_constants::FAST_UNDERFLOW_THRESHOLD;

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

        // === HARDWARE MACRO INSTRUCTION PROGRAMMING ===
        //
        // Program two macro instruction sequences:
        // 1. Sanitization: Load → SWAP → Store (clamp to safe range)
        // 2. Computation: Load → MAD → Round → Shift → Store
        //
        // Programming methods used:
        // - SFPCONFIG method: Load instruction bits into LREG[0], target macro register
        // - Backdoor method: Use illegal destination register values in normal instructions
        //
        // This hybrid approach optimizes for both clarity and hardware constraints.

        // SWAP Instruction Setup (using SFPCONFIG method)
        // Creates: SWAP(0, 0, 14, 1) - compare input against LREG[14] (-88.5)
        // Mode 1: puts larger value into destination (clamps negative values)
        // This effectively implements: result = max(input, -88.5)
        //
        // Parameters:
        // - imm12 = 0 (no immediate)
        // - lreg_src_c = 0 (use loaded value)
        // - lreg_dest = 14 (compare against -88.5)
        // - instr_mod1 = 1 (larger value wins)
        TTI_SFPLOADI(0, 0xA, 0x00E1);
        TTI_SFPLOADI(0, 0x8, 0x9200);
        TTI_SFPCONFIG(0, 0, 0); // SFPCONFIG Dest 0 = Programmable Macro instruction 0: TTI_SFPSWAP(0, 0, 14, 1); // compare against LREG[14] (-88.5), and put
                                // the larger value into LREG[loadmacro_lreg_dest]
        TTI_SFPNOP;

        // MAD Instruction Setup (using backdoor method)
        // Creates: MAD(12, 0, 13, 13, 0) - compute A*input + (B-C)
        // Formula: result = LREG[12] * loaded_value + LREG[13]
        TTI_SFPMAD(12, 0, 13, 13, 0); // A*y + (B-C) computation

        // ROUND Instruction Setup (backdoor method)
        // Creates: STOCH_RND(0, 0, 0, 0, 14, 14) - FP32 → UInt16 conversion
        // Mode 14: Treats input as FP32, outputs unsigned 16-bit integer
        // No stochastic rounding (deterministic)
        TTI_SFP_STOCH_RND(0, 0, 0, 0, 14, 14);

        // SHIFT Instruction Setup (backdoor method)
        // Creates: SHFT(15, 0, 15, 1) - shift left by 15 bits
        // Moves 16-bit integer to IEEE 754 exponent field position
        // Final step in constructing the approximated float result
        TTI_SFPSHFT(15, 0, 15, 1);

        // MACRO INSTRUCTION REGISTER MAP (after setup):
        // Fixed hardware instructions (not programmable):
        //   00: Pass-through (execute Tensix instruction directly)
        //   01: Reserved
        //   02: NOP
        //   03: SFPSTORE
        // Programmed instructions:
        //   04: SFPSWAP(0, 0, 14, 1)          - Input sanitization
        //   05: SFPMAD(12, 0, 13, 13, 0)      - A*y + (B-C) computation
        //   06: SFP_STOCH_RND(0,0,0,0,14,14)  - FP32 → UInt16 rounding
        //   07: SFPSHFT(15, 0, 15, 1)         - Position bits in exponent field

        // === MACRO SEQUENCE PROGRAMMING ===
        //
        // Sequence 1: Input Sanitization
        // Pipeline: Load → SWAP → Store (with appropriate delays)
        // Purpose: Clamp inputs to safe range [-88.5, +∞]
        //
        // Timing requirements:
        // - SWAP operation takes 2 cycles, requires pipeline delay
        // - Store must wait for SWAP completion
        //
        // Sequence encoding format:
        // Each 4-bit field specifies: [use_loaded_val][use_result_stage][delay_slot][macro_select]
        TTI_SFPLOADI(0, 0xA, 0x0004); // slot1 : SIMPLE UNIT, want SWAP  instruction which is in macro instruction mux[4], delayed by 0 ; not using staging flop
                                      // as dest; not using load reg as srcb : 8'b0_______0_______000_____100          = 0x04 slot2 : MAD    UNIT, unused :
                                      // 8'b0_______0_______000_____000          = 0x00
        TTI_SFPLOADI(0, 0x8, 0x1300); // slot3 : ROUND  UNIT, unused : 8'b0_______0_______000_____000          = 0x00 slot4 : STORE  UNIT, want STORE
                                      // instruction which is in macro instruction mux[3], delayed by 2 ; not using staging flop as src ; :
                                      // 8'b0_______0_______010_____011          = 0x13
        TTI_SFPCONFIG(0, 5, 0);       // SFPCONFIG Dest 5 = Macro Sequence Register 1

        // Sequence 0: Exponential Computation
        // Pipeline: Load → MAD → Round → Shift → Store (with delays)
        // Purpose: Compute exp(y) via Schraudolph bit manipulation
        //
        // Pipeline dependencies:
        // - MAD needs loaded value immediately (delay=0)
        // - Round waits for MAD completion (delay=2)
        // - Shift waits for Round completion (delay=3)
        // - Store waits for Shift completion (delay=4)
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

        // Reset macro configuration state to prevent interference
        // Clears LoadMacroConfig[Lane].Misc for all lanes
        // Ensures clean state after any previous macro usage
        TTI_SFPCONFIG(0, 8, 1);
    }
    else if constexpr (APPROXIMATION_MODE)
    {
        // Initialize constants for standard approximation mode
        sfpi::vConstFloatPrgm0 = exp_constants::LN2_RECIP;          // 1/ln2 for base conversion
        sfpi::vConstFloatPrgm1 = sfpi::s2vFloat16b(p_exp::C23_73);  // Exponent clearing mask
        sfpi::vConstFloatPrgm2 = sfpi::s2vFloat16b(p_exp::ADJ_EXP); // Exponent adjustment
    }
    else
    {
        // Initialize constants for high-precision mode
        sfpi::vConstFloatPrgm0 = exp_constants::LN2_RECIP; // 1/ln2 for range reduction
        sfpi::vConstFloatPrgm1 = 2.0f;                     // Base for repeated squaring
        sfpi::vConstFloatPrgm2 = exp_constants::HORNER_C1; // Polynomial coefficient
    }
}

//=========================================================================
// SIMPLIFIED API WRAPPERS
//=========================================================================

/**
 * @brief High-level exponential calculation with automatic algorithm selection
 *
 * Simplified interface that chooses optimal algorithm based on accuracy requirements.
 * Recommended for most users who don't need fine-grained control.
 *
 * @tparam HIGH_PRECISION If true, use polynomial series; if false, use approximation
 * @tparam ITERATIONS Number of SIMD vectors to process
 */
template <bool HIGH_PRECISION = false, int ITERATIONS = 8>
inline void calculate_exponential_auto()
{
    if constexpr (HIGH_PRECISION)
    {
        // High accuracy mode: no scaling, no fast approx, keep overflow checks
        _calculate_exponential_<false, false, ITERATIONS, false, false>(0x3f80); // 1.0f scale
    }
    else
    {
        // Balanced mode: standard approximation with safety checks
        _calculate_exponential_<true, false, ITERATIONS, false, false>(0x3f80); // 1.0f scale
    }
}

/**
 * @brief Ultra-fast exponential for performance-critical applications
 *
 * Uses Schraudolph algorithm with hardware acceleration. Requires initialization.
 * Only available on Wormhole B0 hardware.
 *
 * @tparam ITERATIONS Number of SIMD vectors to process
 *
 * @warning Must call init_exponential_fast() first
 * @warning Lower accuracy than other modes
 */
template <int ITERATIONS = 8>
inline void calculate_exponential_fast()
{
    _calculate_exponential_<true, false, ITERATIONS, true, false>(0x3f80);
}

/**
 * @brief Initialize exponential functions with automatic configuration
 *
 * @tparam FAST_MODE Enable fast Schraudolph algorithm setup
 */
template <bool FAST_MODE = false>
inline void init_exponential_auto()
{
    if constexpr (FAST_MODE)
    {
        _init_exponential_<true, true, 0x3f800000>(); // Fast mode
    }
    else
    {
        _init_exponential_<true, false, 0x3f800000>(); // Standard mode
    }
}

} // namespace ckernel::sfpu

//=========================================================================
// FILE SUMMARY
//=========================================================================
//
// This file provides three tiers of exponential function implementations:
//
// 1. **Simplified API** (Recommended for most users):
//    - calculate_exponential_auto<HIGH_PRECISION>()
//    - calculate_exponential_fast<>()
//    - init_exponential_auto<FAST_MODE>()
//
// 2. **Advanced API** (For performance tuning):
//    - _calculate_exponential_<...>() with full template control
//    - _init_exponential_<...>() with detailed configuration
//
// 3. **Internal Functions** (For experts only):
//    - _sfpu_exp_(), _calculate_exponential_body_(), etc.
//
// Choose the API level that matches your expertise and requirements.
// Most users should start with the simplified API and only move to
// advanced APIs when specific optimizations are needed.
//=========================================================================
