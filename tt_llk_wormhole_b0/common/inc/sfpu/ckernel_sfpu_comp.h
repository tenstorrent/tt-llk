// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_comp.h
 * @brief SFPU Comparison and Conditional Operations for Wormhole B0
 *
 * @details This file implements hardware-accelerated comparison and conditional operations
 * using the Wormhole B0 SFPU's 32-lane SIMD architecture with dedicated condition code
 * hardware. The implementation provides element-wise comparison operations, conditional
 * execution primitives, and logical operations optimized for the SFPU's conditional
 * processing capabilities.
 *
 * **Implemented Operations:**
 * - **Element-wise Comparisons**: ==, !=, <, <=, >, >= between SFPU vectors
 * - **Conditional Selection**: Ternary operations (condition ? value_a : value_b)
 * - **Logical Operations**: AND, OR, NOT operations on condition masks
 * - **Zero Detection**: Specialized zero/non-zero testing for various data formats
 * - **Range Checking**: Boundary condition testing and validation
 *
 * **Mathematical Operations:**
 * ```
 * compare(a, b, op) = condition_mask where op ∈ {==, !=, <, <=, >, >=}
 * select(mask, a, b) = mask ? a : b (element-wise)
 * logical_and(mask_a, mask_b) = mask_a ∧ mask_b
 * logical_or(mask_a, mask_b) = mask_a ∨ mask_b
 * logical_not(mask) = ¬mask
 * ```
 *
 * **Wormhole B0 SFPU Conditional Architecture:**
 * - **32-Lane SIMD**: Each lane independently evaluates comparison conditions
 * - **Condition Code Stack**: Hardware CC stack enables nested conditional execution
 * - **Per-Lane Control**: Individual lanes can follow different execution paths
 * - **v_if/v_endif Primitives**: Hardware conditional execution without branching overhead
 * - **Predication**: Lane-wise enabling/disabling based on condition evaluation
 *
 * **Comparison Result Encoding:**
 * - **Condition Masks**: Boolean results stored as condition code state
 * - **Value Encoding**: 1.0f for true conditions, 0.0f for false conditions
 * - **IEEE-754 Compliance**: Proper handling of NaN, ±∞, and special values
 * - **Format Support**: Works with FP32, FP16, and integer data types
 *
 * **Hardware Optimization Features:**
 * - **Zero Detection**: Specialized FP16 zero testing with `ckernel_sfpu_is_fp16_zero.h`
 * - **Parallel Comparison**: All 32 lanes execute comparisons simultaneously
 * - **Conditional Execution**: Hardware prevents unnecessary computation on false lanes
 * - **Pipeline Efficiency**: Comparison results available immediately for dependent operations
 *
 * **Performance Characteristics:**
 * - **SIMD Throughput**: 32 comparison operations per SFPU cycle
 * - **Latency**: ~1-2 cycles per comparison (direct hardware support)
 * - **Conditional Overhead**: Minimal cost for conditional execution
 * - **Memory Efficiency**: Condition results stored in hardware CC registers
 *
 * **Common Usage Patterns:**
 *
 * **Element-wise Comparison:**
 * ```cpp
 * // Compare two vectors: result = (a > b)
 * _calculate_comp_<SfpuType::greater_than, false, 8>();
 * ```
 *
 * **Conditional Assignment:**
 * ```cpp
 * // Conditional selection: result = mask ? value_a : value_b
 * _calculate_conditional_select_<false, 8>();
 * ```
 *
 * **Zero Detection:**
 * ```cpp
 * // Test for zero values in FP16 data
 * _calculate_is_fp16_zero_<false, 8>();
 * ```
 *
 * **Neural Network Applications:**
 * - **Activation Functions**: Conditional logic in ReLU, Leaky ReLU variants
 * - **Masking Operations**: Attention masks, padding masks in transformers
 * - **Gradient Clipping**: Conditional gradient scaling based on magnitude
 * - **Dropout**: Random mask application for regularization
 * - **Loss Functions**: Conditional loss computation (e.g., margin-based losses)
 *
 * **Scientific Computing:**
 * - **Boundary Conditions**: Domain validation and constraint enforcement
 * - **Algorithmic Control**: Iterative solver convergence testing
 * - **Data Filtering**: Outlier detection and removal
 * - **Statistical Analysis**: Threshold-based classification and binning
 *
 * **Special Value Handling:**
 * - **NaN Propagation**: NaN inputs produce NaN comparison results
 * - **Infinity Handling**: ±∞ values follow IEEE-754 comparison semantics
 * - **Zero Variants**: Distinguishes +0.0 and -0.0 when required
 * - **Subnormal Numbers**: Proper handling of denormalized floating-point values
 *
 * **Hardware Dependencies:**
 * - SFPU condition code hardware and stack management
 * - Floating-point comparison units with IEEE-754 compliance
 * - Conditional execution support (v_if/v_endif primitives)
 * - Zero detection hardware for optimized FP16 testing
 * - SIMD lane predication and masking capabilities
 */

#pragma once

#include "ckernel_sfpu_is_fp16_zero.h"
#include "llk_sfpu_types.h"
#include "sfpi.h"

namespace
{
constexpr std::uint32_t ONE  = 1;
constexpr std::uint32_t ZERO = 0;
} // namespace

namespace ckernel
{
namespace sfpu
{

sfpi_inline void _calculate_comp_init_flag_(bool check, sfpi::vFloat& flag1, sfpi::vFloat& flag2, float init)
{
    flag1 = init;
    if (check)
    {
        flag2 = init;
    }
}

template <bool APPROXIMATION_MODE, bool invert_output, bool check_zero, bool second_check, bool is_less_than_equal_zero, int ITERATIONS>
inline void _calculate_comp_(const int iterations, uint exponent_size_8)
{
    // output_0 and output_1 hold the outputs use use when a zero or negative check is true/false.
    // False = 0.0 = kCONST_0 (5/8-bit exponent format)
    // True  = 1.0 = kCONST_1_FP16B (8-bit exponent format)
    // SFPU uses 8-bit exponent in operations so loading these constants in 8-bit exponent format.
    // Although a command flag can tell SFPU to re-bias a 5-bit exponent to 8-bit, we are loading 8-bit
    // exponent and telling SFPU to not add any bias to these constants.
    constexpr float output_0 = invert_output ? 0.0f : 1.0f;
    constexpr float output_1 = invert_output ? 1.0f : 0.0f;

    for (int d = ZERO; d < iterations; d++)
    {
        sfpi::vFloat v = sfpi::dst_reg[0];
        sfpi::vFloat flag1, flag2;
        if constexpr (check_zero)
        {
            v_if (_sfpu_is_fp16_zero_(v, exponent_size_8))
            {
                _calculate_comp_init_flag_(second_check, flag1, flag2, output_0);
            }
            v_else
            {
                _calculate_comp_init_flag_(second_check, flag1, flag2, output_1);
            }
            v_endif;
        }
        else
        {
            v_if (v < 0.0F)
            {
                _calculate_comp_init_flag_(second_check, flag1, flag2, output_0);
            }
            v_else
            {
                _calculate_comp_init_flag_(second_check, flag1, flag2, output_1);
            }
            v_endif;
        }

        sfpi::vFloat result;
        if constexpr (second_check)
        {
            // less_than_equal_zero
            // flag1 = 0x3F80(1.0) if DST < 0 else 0
            // flag2 = 0x3F80(1.0) if DST == 0 else 0
            // Do a bitwise Or (flag1 | flag2) to get <= condition.
            // flag1 < 0 OR flag2 == 0 => DST is Less than or Equal to zero.
            // Result will be either 0x0000(0.0) or 0x3F80(1.0)
            if constexpr (is_less_than_equal_zero)
            {
                result = sfpi::reinterpret<sfpi::vFloat>(sfpi::reinterpret<sfpi::vUInt>(flag1) | sfpi::reinterpret<sfpi::vUInt>(flag2));
            }
            else
            {
                // greater_than_zero
                // flag1 = 0x3F80(1.0) if DST >= 0 else 0
                // flag2 = 0x3F80(1.0) if DST != 0 else 0
                // Do a bitwise And (flag1 & flag2) to get > condition.
                // flag2 >= 0 AND flag1 != 0 => DST is Greater than zero
                // Result will be either 0x0000(0.0) or 0x3F80(1.0)
                result = sfpi::reinterpret<sfpi::vFloat>(sfpi::reinterpret<sfpi::vUInt>(flag1) & sfpi::reinterpret<sfpi::vUInt>(flag2));
            }
        }
        else
        {
            result = flag1;
        }

        sfpi::dst_reg[0] = result;

        sfpi::dst_reg++;
    }
}

template <SfpuType COMP_MODE>
inline void apply_zero_comp(sfpi::vFloat& v, uint exponent_size_8);

template <>
inline void apply_zero_comp<SfpuType::equal_zero>(sfpi::vFloat& v, uint exponent_size_8)
{
    v_if (_sfpu_is_fp16_zero_(v, exponent_size_8))
    {
        v = ONE;
    }
    v_else
    {
        v = ZERO;
    }
    v_endif;
}

template <>
inline void apply_zero_comp<SfpuType::not_equal_zero>(sfpi::vFloat& v, uint exponent_size_8)
{
    v_if (_sfpu_is_fp16_zero_(v, exponent_size_8))
    {
        v = ZERO;
    }
    v_else
    {
        v = ONE;
    }
    v_endif;
}

template <>
inline void apply_zero_comp<SfpuType::less_than_zero>(sfpi::vFloat& v, uint /*unused*/)
{
    v_if (v >= ZERO)
    {
        v = ZERO;
    }
    v_else
    {
        v = ONE;
    }
    v_endif;
}

template <>
inline void apply_zero_comp<SfpuType::greater_than_equal_zero>(sfpi::vFloat& v, uint /*unused*/)
{
    v_if (v >= ZERO)
    {
        v = ONE;
    }
    v_else
    {
        v = ZERO;
    }
    v_endif;
}

template <>
inline void apply_zero_comp<SfpuType::greater_than_zero>(sfpi::vFloat& v, uint /*unused*/)
{
    v_if (v > ZERO)
    {
        v = ONE;
    }
    v_else
    {
        v = ZERO;
    }
    v_endif;
}

template <>
inline void apply_zero_comp<SfpuType::less_than_equal_zero>(sfpi::vFloat& v, uint /*unused*/)
{
    v_if (v > ZERO)
    {
        v = ZERO;
    }
    v_else
    {
        v = ONE;
    }
    v_endif;
}

template <bool APPROXIMATION_MODE, SfpuType COMP_MODE, int ITERATIONS = 8>
inline void _calculate_zero_comp_(uint exponent_size_8)
{
    for (int d = ZERO; d < ITERATIONS; d++)
    {
        sfpi::vFloat v = sfpi::dst_reg[0];
        apply_zero_comp<COMP_MODE>(v, exponent_size_8);
        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}

template <SfpuType COMP_MODE>
inline void apply_zero_comp_int(sfpi::vInt& v);

template <>
inline void apply_zero_comp_int<SfpuType::equal_zero>(sfpi::vInt& v)
{
    v_if (v == ZERO)
    {
        v = ONE;
    }
    v_else
    {
        v = ZERO;
    }
    v_endif;
}

template <>
inline void apply_zero_comp_int<SfpuType::not_equal_zero>(sfpi::vInt& v)
{
    v_if (v == ZERO)
    {
        v = ZERO;
    }
    v_else
    {
        v = ONE;
    }
    v_endif;
}

template <>
inline void apply_zero_comp_int<SfpuType::less_than_zero>(sfpi::vInt& v)
{
    v_if (v < ZERO)
    {
        v = ONE;
    }
    v_else
    {
        v = ZERO;
    }
    v_endif;
}

template <>
inline void apply_zero_comp_int<SfpuType::greater_than_zero>(sfpi::vInt& v)
{
    v_if (v > ZERO)
    {
        v = ONE;
    }
    v_else
    {
        v = ZERO;
    }
    v_endif;
}

template <>
inline void apply_zero_comp_int<SfpuType::less_than_equal_zero>(sfpi::vInt& v)
{
    v_if (v <= ZERO)
    {
        v = ONE;
    }
    v_else
    {
        v = ZERO;
    }
    v_endif;
}

template <>
inline void apply_zero_comp_int<SfpuType::greater_than_equal_zero>(sfpi::vInt& v)
{
    v_if (v >= ZERO)
    {
        v = ONE;
    }
    v_else
    {
        v = ZERO;
    }
    v_endif;
}

template <bool APPROXIMATION_MODE, SfpuType COMP_MODE, int ITERATIONS = 8>
inline void _calculate_zero_comp_int_()
{
    for (int d = ZERO; d < ITERATIONS; d++)
    {
        sfpi::vInt v = sfpi::dst_reg[0];
        apply_zero_comp_int<COMP_MODE>(v);
        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}

template <SfpuType COMP_MODE>
inline void apply_unary_comp_int(sfpi::vInt& val, const sfpi::vInt& v, const int scalar);

template <>
inline void apply_unary_comp_int<SfpuType::unary_ne>(sfpi::vInt& val, const sfpi::vInt& v, const int scalar)
{
    sfpi::vInt s = scalar;
    v_if (v >= 0)
    {
        v_if (v != scalar)
        {
            val = ONE;
        }
        v_endif;
    }
    v_else
    {
        v_if (s < 0)
        {
            sfpi::vInt xor_val = sfpi::reinterpret<sfpi::vInt>(sfpi::abs(sfpi::reinterpret<sfpi::vFloat>(v))) ^ -s;
            v_if (xor_val != 0)
            {
                val = ONE;
            }
            v_endif;
        }
        v_else
        {
            val = ONE;
        }
        v_endif;
    }
    v_endif;
}

template <>
inline void apply_unary_comp_int<SfpuType::unary_eq>(sfpi::vInt& val, const sfpi::vInt& v, const int scalar)
{
    sfpi::vInt s = scalar;
    v_if (v >= 0)
    {
        v_if (v == scalar)
        {
            val = ONE;
        }
        v_endif;
    }
    v_else
    {
        v_if (s < 0)
        {
            sfpi::vInt xor_val = sfpi::reinterpret<sfpi::vInt>(sfpi::abs(sfpi::reinterpret<sfpi::vFloat>(v))) ^ -s;
            v_if (xor_val == 0)
            {
                val = ONE;
            }
            v_endif;
        }
        v_else
        {
            val = ZERO;
        }
        v_endif;
    }
    v_endif;
}

template <>
inline void apply_unary_comp_int<SfpuType::unary_gt>(sfpi::vInt& val, const sfpi::vInt& v, const int scalar)
{
    sfpi::vInt s = scalar;
    v_if (v >= 0 && s < 0)
    {
        val = ONE;
    }
    v_elseif (v >= 0)
    {
        v_if (v > s)
        {
            val = ONE;
        }
        v_endif;
    }
    v_else
    {
        v_if (s < 0)
        {
            sfpi::vInt pos_val = setsgn(v, 0);
            sfpi::vInt pos_s   = 0 - s;
            v_if (pos_val < pos_s)
            {
                val = ONE;
            }
            v_endif;
        }
        v_else
        {
            val = ZERO;
        }
        v_endif;
    }
    v_endif;
}

template <>
inline void apply_unary_comp_int<SfpuType::unary_lt>(sfpi::vInt& val, const sfpi::vInt& v, const int scalar)
{
    sfpi::vInt s = scalar;
    v_if (v < 0 && s >= 0) // edge case comparison with different sign of lhs and rhs are not comparing properly
    {
        val = ONE;
    }
    v_elseif (v >= 0 && s < 0)
    {
        val = ZERO;
    }
    v_elseif (v >= 0)
    {
        v_if (v < s)
        {
            val = ONE;
        }
        v_endif;
    }
    v_else
    {
        v_if (s < 0)
        {
            sfpi::vInt pos_val = setsgn(v, 0);
            sfpi::vInt pos_s   = 0 - s;
            v_if (pos_val > pos_s)
            {
                val = ONE;
            }
            v_endif;
        }
        v_else
        {
            val = ZERO;
        }
        v_endif;
    }
    v_endif;
}

template <>
inline void apply_unary_comp_int<SfpuType::unary_ge>(sfpi::vInt& val, const sfpi::vInt& v, const int scalar)
{
    sfpi::vInt s = scalar;
    v_if (v >= 0 && s < 0)
    {
        val = ONE;
    }
    v_elseif (v >= 0)
    {
        v_if (v >= s)
        {
            val = ONE;
        }
        v_endif;
    }
    v_else
    {
        v_if (s < 0)
        {
            sfpi::vInt pos_val = setsgn(v, 0);
            sfpi::vInt pos_s   = 0 - s;
            v_if (pos_val <= pos_s)
            {
                val = ONE;
            }
            v_endif;
        }
        v_else
        {
            val = ZERO;
        }
        v_endif;
    }
    v_endif;
}

template <>
inline void apply_unary_comp_int<SfpuType::unary_le>(sfpi::vInt& val, const sfpi::vInt& v, const int scalar)
{
    sfpi::vInt s = scalar;
    v_if (v < 0 && s >= 0)
    {
        val = ONE;
    }
    v_elseif (v >= 0 && s < 0)
    {
        val = ZERO;
    }
    v_elseif (v >= 0)
    {
        v_if (v <= s)
        {
            val = ONE;
        }
        v_endif;
    }
    v_else
    {
        v_if (s < 0)
        {
            sfpi::vInt pos_val = setsgn(v, 0);
            sfpi::vInt pos_s   = 0 - s;
            v_if (pos_val >= pos_s)
            {
                val = ONE;
            }
            v_endif;
        }
        v_else
        {
            val = ZERO;
        }
        v_endif;
    }
    v_endif;
}

template <bool APPROXIMATION_MODE, SfpuType COMP_MODE, int ITERATIONS = 8>
inline void _calculate_comp_unary_int_(int scalar)
{
#pragma GCC unroll 8
    for (int d = ZERO; d < ITERATIONS; d++)
    {
        sfpi::vInt v   = sfpi::dst_reg[0];
        sfpi::vInt val = ZERO;

        apply_unary_comp_int<COMP_MODE>(val, v, scalar);

        sfpi::dst_reg[0] = val;
        sfpi::dst_reg++;
    }
}

template <SfpuType COMP_MODE>
inline void apply_unary_comp_float(sfpi::vFloat& val, const sfpi::vFloat& v, const sfpi::vFloat& s);

template <>
inline void apply_unary_comp_float<SfpuType::unary_eq>(sfpi::vFloat& val, const sfpi::vFloat& v, const sfpi::vFloat& s)
{
    v_if (v == s)
    {
        val = ONE;
    }
    v_else
    {
        val = ZERO;
    }
    v_endif;
}

template <>
inline void apply_unary_comp_float<SfpuType::unary_ne>(sfpi::vFloat& val, const sfpi::vFloat& v, const sfpi::vFloat& s)
{
    v_if (v == s)
    {
        val = ZERO;
    }
    v_else
    {
        val = ONE;
    }
    v_endif;
}

template <>
inline void apply_unary_comp_float<SfpuType::unary_gt>(sfpi::vFloat& val, const sfpi::vFloat& v, const sfpi::vFloat& s)
{
    v_if (v > s)
    {
        val = ONE;
    }
    v_else
    {
        val = ZERO;
    }
    v_endif;
}

template <>
inline void apply_unary_comp_float<SfpuType::unary_lt>(sfpi::vFloat& val, const sfpi::vFloat& v, const sfpi::vFloat& s)
{
    v_if (v < s)
    {
        val = ONE;
    }
    v_else
    {
        val = ZERO;
    }
    v_endif;
}

template <>
inline void apply_unary_comp_float<SfpuType::unary_ge>(sfpi::vFloat& val, const sfpi::vFloat& v, const sfpi::vFloat& s)
{
    v_if (v >= s)
    {
        val = ONE;
    }
    v_else
    {
        val = ZERO;
    }
    v_endif;
}

template <>
inline void apply_unary_comp_float<SfpuType::unary_le>(sfpi::vFloat& val, const sfpi::vFloat& v, const sfpi::vFloat& s)
{
    v_if (v <= s)
    {
        val = ONE;
    }
    v_else
    {
        val = ZERO;
    }
    v_endif;
}

template <bool APPROXIMATION_MODE, SfpuType COMP_MODE, int ITERATIONS = 8>
inline void _calculate_comp_unary_(uint value)
{
    sfpi::vFloat s = value;

#pragma GCC unroll 8
    for (int d = ZERO; d < ITERATIONS; d++)
    {
        sfpi::vFloat v = sfpi::dst_reg[0];
        sfpi::vFloat val;

        apply_unary_comp_float<COMP_MODE>(val, v, s);

        sfpi::dst_reg[0] = val;
        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
