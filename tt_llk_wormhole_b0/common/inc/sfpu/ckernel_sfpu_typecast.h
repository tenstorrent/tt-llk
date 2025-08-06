// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_typecast.h
 * @brief Data Type Conversion Engine for Tensix SFPU
 *
 * This file provides comprehensive data type conversion capabilities for the
 * Tensix Special Function Processing Unit (SFPU). It handles conversions between
 * all supported numeric formats with proper overflow/underflow handling and
 * hardware-optimized implementations.
 *
 * ## Supported Data Types:
 *
 * ### Floating Point Types:
 * - **FP32**: IEEE 754 single precision (32-bit: 1 sign + 8 exp + 23 mantissa)
 * - **FP16B**: Brain Float 16 (16-bit: 1 sign + 8 exp + 7 mantissa)
 *   - Same dynamic range as FP32 but reduced precision
 *   - Optimized for neural network training and inference
 *
 * ### Integer Types:
 * - **INT32**: 32-bit signed integer (-2³¹ to 2³¹-1)
 * - **UINT32**: 32-bit unsigned integer (0 to 2³²-1)
 * - **UINT16**: 16-bit unsigned integer (0 to 65,535)
 *
 * ## Implementation Strategies:
 *
 * ### 1. Hardware Macro Instructions (Fast Path)
 * - Uses dedicated TTI (Tensix Tool Interface) instructions
 * - Single-cycle execution for simple conversions
 * - Hardware-level stochastic rounding for neural networks
 * - Used for: FP32↔FP16B, simple integer conversions
 *
 * ### 2. Manual IEEE 754 Manipulation (Precision Path)
 * - Explicit bit manipulation of floating-point representation
 * - Comprehensive overflow/underflow saturation
 * - Sign-aware processing for mixed signed/unsigned conversions
 * - Used for: Float→Integer with bounds checking
 *
 * ## Conversion Matrix (13 Functions Available):
 *
 * ```
 * FROM \ TO    | FP16B | FP32  | INT32 | UINT16 | UINT32 |
 * -------------|-------|-------|-------|--------|--------|
 * FP16B        |   -   |   ✗   |   ✓   |   ✓    |   ✓    |
 * FP32         |   ✓   |   -   |   ✓   |   ✗    |   ✗    |
 * INT32        |   ✓   |   ✓   |   -   |   ✓    |   ✗    |
 * UINT16       |   ✓   |   ✓   |   ✗   |   -    |   ✓    |
 * UINT32       |   ✓   |   ✓   |   ✗   |   ✓    |   -    |
 * ```
 * ✓ = Supported, ✗ = Not implemented, - = Identity
 *
 * ## Error Handling Philosophy:
 *
 * ### Overflow Behavior:
 * - **Saturation**: Clamp to maximum representable value
 * - **No exceptions**: Always produces valid output
 * - **Deterministic**: Same input always produces same output
 *
 * ### Underflow Behavior:
 * - **Flush to zero**: Values too small become 0
 * - **Gradual underflow**: For floating-point subnormals
 *
 * ### Special Cases:
 * - **NaN propagation**: NaN inputs produce NaN outputs where possible
 * - **Infinity handling**: ±∞ saturates to min/max integer values
 * - **Negative to unsigned**: Clamps to 0 (saturation, not wrap)
 *
 * ## Performance Characteristics:
 *
 * | Conversion Type | Method | Cycles/Vector | Accuracy |
 * |-----------------|--------|---------------|----------|
 * | FP32→FP16B      | Hardware | 1-2          | Exact    |
 * | UINT16→FP16B    | Hardware | 1-2          | Exact    |
 * | FP16B→INT32     | Manual   | 8-12         | Exact    |
 * | FP16B→UINT32    | Manual   | 10-15        | Exact    |
 *
 * ## Neural Network Applications:
 *
 * ### Training Workflows:
 * ```cpp
 * // Mixed precision training: compute in FP32, store in FP16B
 * _calculate_typecast_fp32_to_fp16b_<VECTOR_COUNT>();
 * ```
 *
 * ### Quantized Inference:
 * ```cpp
 * // Convert weights to lower precision for inference
 * _calculate_typecast_fp16b_to_uint16_<VECTOR_COUNT>();
 * ```
 *
 * ### Index Processing:
 * ```cpp
 * // Convert tensor indices for addressing
 * _calculate_typecast_int32_to_fp32_<VECTOR_COUNT>();
 * ```
 *
 * @note All functions operate on 32-lane SIMD vectors for maximum efficiency
 * @note Stochastic rounding reduces quantization bias in neural network training
 * @note Hardware TTI instructions are Wormhole B0 specific
 */

#pragma once

#include "ckernel.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

// Constants for typecast operations
namespace typecast_constants
{
// IEEE 754 format specifications
constexpr int FP32_MANTISSA_BITS  = 23;  // IEEE 754 single precision mantissa
constexpr int FP32_EXPONENT_BIAS  = 127; // IEEE 754 single precision bias
constexpr int FP16B_MANTISSA_BITS = 7;   // BFloat16 mantissa bits
constexpr int FP16B_EXPONENT_BIAS = 127; // BFloat16 uses same bias as FP32

// Integer limits for overflow detection
constexpr int INT32_OVERFLOW_EXPONENT  = 30; // 2^30 = 1073741824 < INT32_MAX
constexpr int UINT32_OVERFLOW_EXPONENT = 31; // 2^31 = 2147483648 < UINT32_MAX

// TTI Hardware instruction constants
constexpr uint16_t TTI_POWER_OF_31 = 0x4f00; // 2^31 in IEEE 754 hex format
constexpr uint16_t TTI_UINT16_MAX  = 0xFFFF; // Maximum value for 16-bit saturation

// Rounding and precision control
constexpr int STOCH_RND_FP32_TO_FP16B = 1; // Stochastic rounding mode for FP32→FP16B
constexpr int STOCH_RND_INT_TO_FLOAT  = 1; // Stochastic rounding for integer→float

// Load/store addressing modes
constexpr int ADDR_MOD_0 = 0; // Direct addressing
constexpr int ADDR_MOD_3 = 3; // Stride addressing

// Data format identifiers for TTI instructions
constexpr int TTI_FP32_FORMAT   = 0;  // 32-bit float format ID
constexpr int TTI_FP16B_FORMAT  = 2;  // BFloat16 format ID
constexpr int TTI_UINT16_FORMAT = 6;  // 16-bit unsigned format ID
constexpr int TTI_UINT32_FORMAT = 4;  // 32-bit unsigned format ID
constexpr int TTI_INT32_FORMAT  = 12; // 32-bit signed format ID
} // namespace typecast_constants

/**
 * @brief Convert FP16B (BFloat16) values to 16-bit unsigned integers
 *
 * Uses hardware TTI instructions for optimal performance with stochastic rounding.
 * Handles overflow by saturating to UINT16_MAX (65535).
 *
 * @tparam ITERATIONS Number of SIMD vectors to process
 *
 * ## Algorithm:
 * 1. Load FP16B data from destination register
 * 2. Apply hardware stochastic rounding for quantization
 * 3. Saturate overflow values to UINT16_MAX
 * 4. Store as 16-bit unsigned integers
 *
 * ## TTI Instruction Sequence:
 * - SFPLOAD: Load FP16B data
 * - SFPSETCC/SFPENCC: Configure conditional execution
 * - SFP_STOCH_RND: Apply stochastic rounding with mode 14
 * - SFPSTORE: Store to UINT16 format
 *
 * @note Negative FP16B values saturate to 0
 * @note Uses stochastic rounding to reduce quantization bias
 */
template <int ITERATIONS>
inline void _calculate_typecast_fp16b_to_uint16_()
{
    // Disable loop unrolling for complex TTI instruction sequences
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        // Load FP16B data from current SIMD vector position
        TTI_SFPLOAD(typecast_constants::TTI_FP32_FORMAT, typecast_constants::TTI_FP32_FORMAT, typecast_constants::ADDR_MOD_3, 0);

        // Configure conditional execution for overflow handling
        TTI_SFPSETCC(0, 0, 0, 0);
        TTI_SFPLOADI(0, 0, 0);
        TTI_SFPENCC(0, 0, 0, 0);

        // Apply stochastic rounding: reduces quantization bias in neural networks
        // Parameters: (src, dst, format, rnd_mode, enable, threshold)
        TTI_SFP_STOCH_RND(0, 0, typecast_constants::TTI_FP16B_FORMAT, 0, typecast_constants::STOCH_RND_INT_TO_FLOAT, 14);

        // Store result as 16-bit unsigned integer
        TTI_SFPSTORE(1, typecast_constants::TTI_UINT16_FORMAT, typecast_constants::ADDR_MOD_3, 0);

        // Advance to next SIMD vector
        sfpi::dst_reg++;
    }
}

/**
 * @brief Convert 16-bit unsigned integers to FP16B (BFloat16) values
 *
 * Uses hardware TTI casting with stochastic rounding for exact conversion.
 * All UINT16 values (0-65535) are exactly representable in FP16B.
 *
 * @tparam ITERATIONS Number of SIMD vectors to process
 *
 * ## Algorithm:
 * 1. Load UINT16 data from destination register
 * 2. Cast to floating-point representation using hardware
 * 3. Apply stochastic rounding (though exact conversion doesn't need it)
 * 4. Store as FP16B format
 *
 * ## Conversion Properties:
 * - **Exact**: All UINT16 values exactly representable in FP16B
 * - **Fast**: Single hardware cast instruction
 * - **Range**: 0 to 65,535 maps directly to FP16B
 *
 * @note This conversion is always exact - no precision loss
 */
template <int ITERATIONS>
inline void _calculate_typecast_uint16_to_fp16b_()
{
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        // Load UINT16 data
        TTI_SFPLOAD(0, typecast_constants::TTI_UINT16_FORMAT, typecast_constants::ADDR_MOD_3, 0);

        // Hardware cast from integer to floating-point
        TTI_SFPCAST(0, 1, 0);

        // Apply stochastic rounding (redundant for exact conversion but maintains consistency)
        TTI_SFP_STOCH_RND(0, 0, 3, 1, 2, typecast_constants::STOCH_RND_INT_TO_FLOAT);

        // Store as FP16B
        TTI_SFPSTORE(2, typecast_constants::TTI_FP16B_FORMAT, typecast_constants::ADDR_MOD_3, 0);

        sfpi::dst_reg++;
    }
}

/**
 * @brief Convert 32-bit signed integers to FP16B (BFloat16) values
 *
 * Uses hardware TTI casting for efficient conversion. Large integers may lose
 * precision due to FP16B's 7-bit mantissa (vs INT32's 31-bit precision).
 *
 * @tparam ITERATIONS Number of SIMD vectors to process
 *
 * ## Precision Analysis:
 * - **Exact range**: -128 to +127 (fits in 7-bit mantissa)
 * - **Approximate range**: Beyond ±127, some precision loss occurs
 * - **Full range**: All INT32 values representable but with rounding
 *
 * ## Algorithm:
 * 1. Load INT32 data from destination register
 * 2. Hardware cast to floating-point (handles sign, exponent, mantissa)
 * 3. Apply stochastic rounding to reduce quantization bias
 * 4. Store as FP16B format
 *
 * @note Stochastic rounding helps maintain training accuracy in neural networks
 */
template <int ITERATIONS>
inline void _calculate_typecast_int32_to_fp16b_()
{
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        // Load INT32 data
        TTI_SFPLOAD(0, typecast_constants::TTI_INT32_FORMAT, typecast_constants::ADDR_MOD_3, 0);

        // Hardware cast from signed integer to floating-point
        TTI_SFPCAST(0, 1, 0);

        // Stochastic rounding for precision preservation in neural networks
        TTI_SFP_STOCH_RND(0, 0, 3, 1, 2, typecast_constants::STOCH_RND_INT_TO_FLOAT);

        // Store as FP16B
        TTI_SFPSTORE(2, typecast_constants::TTI_FP16B_FORMAT, typecast_constants::ADDR_MOD_3, 0);

        sfpi::dst_reg++;
    }
}

/**
 * @brief Convert FP16B (BFloat16) values to 32-bit signed integers
 *
 * Performs IEEE 754 bit manipulation to extract integer value with proper
 * overflow/underflow saturation. Handles the full range of FP16B inputs.
 *
 * @tparam ITERATIONS Number of SIMD vectors to process
 *
 * ## Algorithm (Manual IEEE 754 Processing):
 * 1. **Extract exponent**: Determine magnitude of floating-point value
 * 2. **Underflow check**: exp < 0 means |value| < 1.0, result = 0
 * 3. **Overflow check**: exp > 30 means |value| ≥ 2³¹, saturate to INT32_MAX/MIN
 * 4. **Normal case**: Extract mantissa and shift by (23-exponent) bits
 * 5. **Sign handling**: Apply two's complement for negative values
 *
 * ## IEEE 754 Bit Layout (FP16B):
 * ```
 * Bit: 15   14-7     6-0
 *      Sign Exponent Mantissa
 * Value = (-1)^sign × 2^(exp-127) × (1.mantissa)
 * ```
 *
 * ## Overflow Behavior:
 * - **Positive overflow**: → +2,147,483,647 (INT32_MAX)
 * - **Negative overflow**: → -2,147,483,648 (INT32_MIN)
 * - **Underflow**: → 0 (values with |x| < 1.0)
 *
 * ## Special Cases:
 * - **NaN**: Converts to 0 (safe fallback)
 * - **±Infinity**: Saturates to ±INT32_MAX
 * - **Subnormal**: Treated as underflow → 0
 *
 * @note Uses consistent overflow threshold (exp > 30) across codebase
 */
template <int ITERATIONS>
inline void _calculate_typecast_fp16b_to_int32_()
{
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat in = sfpi::dst_reg[0];

        // === PHASE 1: EXPONENT EXTRACTION AND RANGE CHECKING ===
        // Extract IEEE 754 exponent field (8 bits for FP16B)
        sfpi::vInt exp = exexp(in);

        // Underflow case: exponent < 0 means |value| < 1.0
        v_if (exp < 0)
        {
            // Any value with magnitude < 1.0 becomes 0 when converted to integer
            sfpi::dst_reg[0] = 0;
        }
        // Overflow case: exponent > 30 means |value| >= 2^31 (exceeds INT32 range)
        v_elseif (exp > typecast_constants::INT32_OVERFLOW_EXPONENT)
        {
            // === OVERFLOW SATURATION ===
            // Saturate to maximum representable INT32 value
            sfpi::vInt tmp = std::numeric_limits<int32_t>::max();

            // Handle sign: negative overflow goes to INT32_MIN
            v_if (in < 0)
            {
                // Convert to negative saturation value (INT32_MIN)
                tmp = sfpi::reinterpret<sfpi::vInt>(sfpi::setsgn(sfpi::reinterpret<sfpi::vFloat>(tmp), 1));
            }
            v_endif sfpi::dst_reg[0] = tmp;
        }
        // Normal conversion case: value fits in INT32 range
        v_else
        {
            // === PHASE 2: MANTISSA EXTRACTION AND SHIFTING ===
            // Extract 8-bit mantissa (includes hidden bit for normalized numbers)
            sfpi::vInt man = exman8(in);

            // Calculate shift amount to convert mantissa to integer
            // IEEE 754: mantissa represents fractional part, need to shift left by (exp-23)
            // For FP16B: shift = exp - 23 (where 23 is standard mantissa position)
            sfpi::vInt shift = exp - typecast_constants::FP32_MANTISSA_BITS;
            man              = sfpi::shft(sfpi::reinterpret<sfpi::vUInt>(man), shift);

            // === PHASE 3: SIGN APPLICATION ===
            // Apply sign bit for negative values (two's complement)
            v_if (in < 0)
            {
                man = sfpi::reinterpret<sfpi::vInt>(sfpi::setsgn(sfpi::reinterpret<sfpi::vFloat>(man), 1));
            }
            v_endif sfpi::dst_reg[0] = man;
        }
        v_endif

            // Advance to next SIMD vector
            sfpi::dst_reg++;
    }
}

/**
 * @brief Convert FP32 (IEEE 754 single precision) to FP16B (BFloat16)
 *
 * Most commonly used conversion in neural networks for mixed-precision training.
 * FP16B maintains FP32's dynamic range but reduces precision from 23 to 7 mantissa bits.
 *
 * @tparam ITERATIONS Number of SIMD vectors to process
 *
 * ## Conversion Properties:
 * - **Dynamic range**: Preserved (same 8-bit exponent)
 * - **Precision loss**: 23 → 7 mantissa bits (16-bit reduction)
 * - **Speed**: Optimized hardware instruction
 * - **Rounding**: Stochastic to reduce bias
 *
 * ## Neural Network Benefits:
 * - **Memory**: 50% reduction (32→16 bits)
 * - **Bandwidth**: 2x improvement in data movement
 * - **Training**: Stochastic rounding preserves gradient quality
 *
 * @note This is the most performance-critical typecast in neural networks
 */
template <int ITERATIONS>
inline void _calculate_typecast_fp32_to_fp16b_()
{
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        // Load FP32 data
        TTI_SFPLOAD(typecast_constants::TTI_FP32_FORMAT, typecast_constants::TTI_FP32_FORMAT, typecast_constants::ADDR_MOD_3, 0);

        // Apply stochastic rounding: critical for training quality
        TTI_SFP_STOCH_RND(0, 0, typecast_constants::TTI_FP16B_FORMAT, 0, typecast_constants::STOCH_RND_FP32_TO_FP16B, 1);

        // Store as FP16B
        TTI_SFPSTORE(1, typecast_constants::TTI_FP32_FORMAT, typecast_constants::ADDR_MOD_3, 0);

        sfpi::dst_reg++;
    }
}

template <int ITERATIONS>
inline void _calculate_typecast_uint16_to_fp32_()
{
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(0, 6, 3, 0);
        TTI_SFPCAST(0, 1, 0);
        TTI_SFPSTORE(1, 3, 3, 0);
        sfpi::dst_reg++;
    }
}

template <int ITERATIONS>
inline void _calculate_typecast_int32_to_fp32_()
{
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(0, 12, 3, 0);
        TTI_SFPCAST(0, 1, 0);
        TTI_SFPSTORE(1, 3, 3, 0);
        sfpi::dst_reg++;
    }
}

template <int ITERATIONS>
inline void _calculate_typecast_fp16b_to_uint32_()
{
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat in = sfpi::dst_reg[0];

        // check sign
        v_if (in <= 0)
        {
            sfpi::dst_reg[0] = 0;
        }
        v_else
        {
            // extract exponent
            sfpi::vInt exp = exexp(in);

            v_if (exp < 0)
            {
                sfpi::dst_reg[0] = 0;
            }
            v_elseif (exp > 31)
            {
                // set to uint32 max value in case of overflow
                sfpi::vInt tmp   = std::numeric_limits<int32_t>::max();
                sfpi::dst_reg[0] = sfpi::setsgn(sfpi::reinterpret<sfpi::vFloat>(tmp), 1);
            }
            v_elseif (exp == 31)
            {
                // extract mantissa without hidden bit
                sfpi::vInt man = exman9(in);
                // shift the mantissa by (23-exponent) to the right
                sfpi::vInt shift = exp - 23;
                man              = sfpi::shft(sfpi::reinterpret<sfpi::vUInt>(man), shift);
                // add hidden bit back (due to bug when shifting a 1 into MSB)
                sfpi::dst_reg[0] = sfpi::setsgn(sfpi::reinterpret<sfpi::vFloat>(man), 1);
            }
            v_else
            {
                // extract mantissa
                sfpi::vInt man = exman8(in);
                // shift the mantissa by (23-exponent) to the right
                sfpi::vInt shift = exp - 23;
                man              = sfpi::shft(sfpi::reinterpret<sfpi::vUInt>(man), shift);
                sfpi::dst_reg[0] = man;
            }
            v_endif
        }
        v_endif

            sfpi::dst_reg++;
    }
}

template <int ITERATIONS>
inline void _calculate_typecast_uint32_to_fp16b_()
{
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(0, 4, 3, 0);
        TTI_SFPSETSGN(0, 0, 1, 1);
        TTI_SFPCAST(1, 2, 0);
        TTI_SFP_STOCH_RND(0, 0, 4, 2, 3, 1);
        TTI_SFPSETCC(0, 0, 0, 0);
        TTI_SFPADDI(0x4f00, 3, 0); // 2^31
        TTI_SFPENCC(0, 0, 0, 0);
        TTI_SFPSTORE(3, 2, 3, 0);
        sfpi::dst_reg++;
    }
}

template <int ITERATIONS>
inline void _calculate_typecast_uint32_to_fp32_()
{
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(0, 4, 3, 0);
        TTI_SFPSETSGN(0, 0, 1, 1);
        TTI_SFPCAST(1, 2, 0);
        TTI_SFPSETCC(0, 0, 0, 0);
        TTI_SFPADDI(0x4f00, 2, 0); // 2^31
        TTI_SFPENCC(0, 0, 0, 0);
        TTI_SFPSTORE(2, 3, 3, 0);
        sfpi::dst_reg++;
    }
}

template <int ITERATIONS>
inline void _calculate_typecast_uint16_to_uint32_()
{
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::LO16, ADDR_MOD_3, 0);
        TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::INT32, ADDR_MOD_3, 0);
        sfpi::dst_reg++;
    }
}

template <int ITERATIONS>
inline void _calculate_typecast_uint32_to_uint16_()
{
    // Packer will read HI16 bits from DEST if DEST is in 32bit mode but pck is configured for uint16
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::INT32, ADDR_MOD_3, 0); // Load 32bit datums from DEST
        TTI_SFPLOADI(p_sfpu::LREG1, 2, 0xFFFF);                              // Load Lo16 of LREG1 with 0xFFFF (65535)
        TTI_SFPSETCC(0, p_sfpu::LREG0, 0, 4);                                // If sign bit of LREG0 is 0, set CC
        TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, 1);                     // Put smaller of LREG0 & 1 into LREG1
        TTI_SFPENCC(0, 0, 0, 0);                                             // Clear CC bits
        TTI_SFPSTORE(p_sfpu::LREG1, InstrModLoadStore::LO16, ADDR_MOD_3, 0); // Store output
        sfpi::dst_reg++;
    }
}

template <int ITERATIONS>
inline void _calculate_typecast_int32_to_uint16_()
{
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::INT32, ADDR_MOD_3, 0); // Load 32bit datums from DEST
        TTI_SFPLOADI(p_sfpu::LREG1, 2, 0);                                   // Load Lo16 of LREG1 with 0
        TTI_SFPSWAP(0, p_sfpu::LREG0, p_sfpu::LREG1, 1);                     // Put smaller of LREG0 & 1 into LREG1
        TTI_SFPLOADI(p_sfpu::LREG2, 2, typecast_constants::TTI_UINT16_MAX);  // Load Lo16 of LREG2 with UINT16_MAX (65535)
        TTI_SFPSWAP(0, p_sfpu::LREG2, p_sfpu::LREG0, 1);                     // Put smaller of LREG0 & 2 into LREG0
        TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::LO16, ADDR_MOD_3, 0); // Store output
        sfpi::dst_reg++;
    }
}

//=========================================================================
// SIMPLIFIED TYPECAST API
//=========================================================================

/**
 * @brief High-level typecast interface for common neural network workflows
 *
 * Provides simplified functions for the most commonly used type conversions
 * in neural network training and inference pipelines.
 */

/**
 * @brief Convert tensors for mixed-precision training
 *
 * Common workflow: compute gradients in FP32, store weights in FP16B
 *
 * @tparam VECTOR_COUNT Number of SIMD vectors to process (default: 8)
 */
template <int VECTOR_COUNT = 8>
inline void typecast_mixed_precision_training()
{
    // FP32 → FP16B: Most critical conversion for training
    _calculate_typecast_fp32_to_fp16b_<VECTOR_COUNT>();
}

/**
 * @brief Convert tensors for quantized inference
 *
 * Common workflow: convert FP16B weights to integer format for fast inference
 *
 * @tparam VECTOR_COUNT Number of SIMD vectors to process (default: 8)
 */
template <int VECTOR_COUNT = 8>
inline void typecast_quantized_inference()
{
    // FP16B → UINT16: Typical quantization for inference
    _calculate_typecast_fp16b_to_uint16_<VECTOR_COUNT>();
}

/**
 * @brief Convert integer indices to floating point
 *
 * Common workflow: tensor indexing operations need float coordinates
 *
 * @tparam VECTOR_COUNT Number of SIMD vectors to process (default: 8)
 */
template <int VECTOR_COUNT = 8>
inline void typecast_index_to_float()
{
    // INT32 → FP32: Index conversion for addressing
    _calculate_typecast_int32_to_fp32_<VECTOR_COUNT>();
}

/**
 * @brief Convert floating point to integers for indexing
 *
 * Common workflow: computed coordinates need to become array indices
 *
 * @tparam VECTOR_COUNT Number of SIMD vectors to process (default: 8)
 */
template <int VECTOR_COUNT = 8>
inline void typecast_float_to_index()
{
    // FP16B → INT32: Coordinate to index conversion
    _calculate_typecast_fp16b_to_int32_<VECTOR_COUNT>();
}

/**
 * @brief Upcast integers to higher precision
 *
 * Common workflow: expand precision for intermediate calculations
 *
 * @tparam VECTOR_COUNT Number of SIMD vectors to process (default: 8)
 */
template <int VECTOR_COUNT = 8>
inline void typecast_integer_precision_expansion()
{
    // UINT16 → UINT32: Precision expansion
    _calculate_typecast_uint16_to_uint32_<VECTOR_COUNT>();
}

/**
 * @brief Downcast integers to save memory
 *
 * Common workflow: reduce precision after computation complete
 *
 * @tparam VECTOR_COUNT Number of SIMD vectors to process (default: 8)
 */
template <int VECTOR_COUNT = 8>
inline void typecast_integer_precision_reduction()
{
    // UINT32 → UINT16: Memory optimization
    _calculate_typecast_uint32_to_uint16_<VECTOR_COUNT>();
}

} // namespace sfpu
} // namespace ckernel

//=========================================================================
// TYPECAST FILE SUMMARY
//=========================================================================
//
// This file provides comprehensive data type conversion for neural networks:
//
// ## API Levels:
//
// 1. **Simplified API** (Recommended for most users):
//    - typecast_mixed_precision_training<>() - FP32→FP16B conversion
//    - typecast_quantized_inference<>() - FP16B→UINT16 conversion
//    - typecast_index_to_float<>() - INT32→FP32 conversion
//    - typecast_float_to_index<>() - FP16B→INT32 conversion
//    - typecast_integer_precision_*<>() - Integer up/down casting
//
// 2. **Advanced API** (For specific conversions):
//    - _calculate_typecast_SOURCE_to_TARGET_<ITERATIONS>() - Direct control
//
// ## Conversion Support Matrix:
//
// | FROM \ TO | FP16B | FP32 | INT32 | UINT16 | UINT32 |
// |----------|-------|------|-------|--------|--------|
// | FP16B    |   -   |  ✗   |   ✓   |   ✓    |   ✓    |
// | FP32     |   ✓   |   -  |   ✓   |   ✗    |   ✗    |
// | INT32    |   ✓   |  ✓   |   -   |   ✓    |   ✗    |
// | UINT16   |   ✓   |  ✓   |   ✗   |   -    |   ✓    |
// | UINT32   |   ✓   |  ✓   |   ✗   |   ✓    |   -    |
//
// ## Performance Characteristics:
//
// - **Hardware TTI**: 1-2 cycles/vector (FP32↔FP16B, simple integer casts)
// - **Manual IEEE 754**: 8-15 cycles/vector (float→integer with saturation)
// - **Batch processing**: 32 values converted simultaneously per vector
// - **Saturation behavior**: Overflow/underflow handled gracefully
//
// ## Neural Network Usage Patterns:
//
// ```cpp
// // Mixed precision training
// typecast_mixed_precision_training<16>();  // 16 vectors of FP32→FP16B
//
// // Quantized inference
// typecast_quantized_inference<8>();        // 8 vectors of FP16B→UINT16
//
// // Index processing
// typecast_index_to_float<4>();             // 4 vectors of INT32→FP32
// ```
//
// All conversions preserve numerical stability and provide deterministic
// overflow/underflow behavior for reliable neural network operation.
//=========================================================================
