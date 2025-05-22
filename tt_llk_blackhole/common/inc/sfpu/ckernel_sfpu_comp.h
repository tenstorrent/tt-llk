// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu_is_fp16_zero.h"
#include "sfpi.h"

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

    for (int d = 0; d < iterations; d++)
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

template <bool APPROXIMATION_MODE, SfpuOpType COMP_MODE, int ITERATIONS = 8>
inline void _calculate_zero_comp_unary_(uint exponent_size_8)
{
    const sfpi::vFloat zero = 0.0f;
    const sfpi::vFloat one  = 1.0f;
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat v = sfpi::dst_reg[0];

        // a[i] == 0
        if constexpr (COMP_MODE == SfpuOpType::equal_zero)
        {
            v_if (_sfpu_is_fp16_zero_(v, exponent_size_8))
            {
                v = one;
            }
            v_else
            {
                v = zero;
            }
            v_endif;
        }

        // a[i] != 0
        if constexpr (COMP_MODE == SfpuOpType::not_equal_zero)
        {
            v_if (_sfpu_is_fp16_zero_(v, exponent_size_8))
            {
                v = zero;
            }
            v_else
            {
                v = one;
            }
            v_endif;
        }

        // a[i] < 0
        if constexpr (COMP_MODE == SfpuOpType::less_than_zero)
        {
            v_if (v >= 0.0f)
            {
                v = zero;
            }
            v_else
            {
                v = one;
            }
            v_endif;
        }

        // a[i] >= 0
        if constexpr (COMP_MODE == SfpuOpType::greater_than_equal_zero)
        {
            v_if (v >= 0.0f)
            {
                v = one;
            }
            v_else
            {
                v = zero;
            }
            v_endif;
        }

        // a[i] > 0
        if constexpr (COMP_MODE == SfpuOpType::greater_than_zero)
        {
            v_if (v > 0.0f)
            {
                v = one;
            }
            v_else
            {
                v = zero;
            }
            v_endif;
        }

        // a[i] <= 0
        if constexpr (COMP_MODE == SfpuOpType::less_than_equal_zero)
        {
            v_if (v > 0.0f)
            {
                v = zero;
            }
            v_else
            {
                v = one;
            }
            v_endif;
        }

        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, SfpuOpType COMP_MODE, int ITERATIONS = 8>
inline void _calculate_comp_int_()
{
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vInt v    = sfpi::dst_reg[0];
        sfpi::vInt zero = 0;

        // a[i] == 0
        if constexpr (COMP_MODE == SfpuOpType::equal_zero)
        {
            v_if (v == zero)
            {
                v = 1;
            }
            v_else
            {
                v = zero;
            }
            v_endif;
        }

        // a[i] != 0
        if constexpr (COMP_MODE == SfpuOpType::not_equal_zero)
        {
            v_if (v == zero)
            {
                v = zero;
            }
            v_else
            {
                v = 1;
            }
            v_endif;
        }

        // a[i] < 0
        if constexpr (COMP_MODE == SfpuOpType::less_than_zero)
        {
            v_if (v < zero)
            {
                v = 1;
            }
            v_else
            {
                v = zero;
            }
            v_endif;
        }

        // a[i] > 0
        if constexpr (COMP_MODE == SfpuOpType::greater_than_zero)
        {
            v_if (v > zero)
            {
                v = 1;
            }
            v_else
            {
                v = zero;
            }
            v_endif;
        }

        // a[i] <= 0
        if constexpr (COMP_MODE == SfpuOpType::less_than_equal_zero)
        {
            v_if (v <= zero)
            {
                v = 1;
            }
            v_else
            {
                v = zero;
            }
            v_endif;
        }

        // a[i] >= 0
        if constexpr (COMP_MODE == SfpuOpType::greater_than_equal_zero)
        {
            v_if (v >= zero)
            {
                v = 1;
            }
            v_else
            {
                v = zero;
            }
            v_endif;
        }

        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, SfpuOpType COMP_MODE, int ITERATIONS = 8>
inline void _calculate_comp_unary_int_(int scalar)
{
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vInt v    = sfpi::dst_reg[0];
        sfpi::vInt val  = 0;
        sfpi::vInt s    = scalar;
        sfpi::vInt zero = 0;

        // a[i] != scalar
        if constexpr (COMP_MODE == SfpuOpType::unary_ne)
        {
            v_if (v != scalar)
            {
                val = 1;
            }
            v_endif;
        }
        // a[i] == scalar
        else if constexpr (COMP_MODE == SfpuOpType::unary_eq)
        {
            v_if (v == scalar)
            {
                val = 1;
            }
            v_endif;
        }
        // a[i] > scalar
        else if constexpr (COMP_MODE == SfpuOpType::unary_gt)
        {
            v_if (v >= zero && s < zero)
            {
                val = 1;
            }
            v_elseif (v < zero && s >= zero)
            {
                val = 0;
            }
            v_elseif (v > s)
            {
                val = 1;
            }
            v_endif;
        }
        // a[i] < scalar
        else if constexpr (COMP_MODE == SfpuOpType::unary_lt)
        {
            v_if (v >= zero && s < zero)
            {
                val = 0;
            }
            v_elseif (v < zero && s >= zero)
            {
                val = 1;
            }
            v_elseif (v < s)
            {
                val = 1;
            }
            v_endif;
        }
        // a[i] >= scalar
        else if constexpr (COMP_MODE == SfpuOpType::unary_ge)
        {
            v_if (v >= zero && s <= zero)
            {
                val = 1;
            }
            v_elseif (v < zero && s >= zero)
            {
                val = 0;
            }
            v_elseif (v >= s)
            {
                val = 1;
            }
            v_endif;
        }
        // a[i] <= scalar
        else if constexpr (COMP_MODE == SfpuOpType::unary_le)
        {
            v_if (v < zero && s >= zero)
            {
                val = 1;
            }
            v_elseif (v > zero && s < zero)
            {
                val = 0;
            }
            v_elseif (v <= s)
            {
                val = 1;
            }
            v_else
            {
                val = 0;
            }
            v_endif;
        }
        sfpi::dst_reg[0] = val;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, SfpuOpType COMP_MODE, int ITERATIONS = 8>
inline void _calculate_comp_unary_(uint value)
{
    sfpi::vFloat s = value;

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat v = sfpi::dst_reg[0];

        if constexpr (COMP_MODE == SfpuOpType::unary_eq)
        {
            v_if (v == s)
            {
                v = 1.0f;
            }
            v_else
            {
                v = 0.0f;
            }
            v_endif;
        }
        else if constexpr (COMP_MODE == SfpuOpType::unary_ne)
        {
            v_if (v == s)
            {
                v = 0.0f;
            }
            v_else
            {
                v = 1.0f;
            }
            v_endif;
        }
        else if constexpr (COMP_MODE == SfpuOpType::unary_gt)
        {
            v_if (v > s)
            {
                v = 1.0f;
            }
            v_else
            {
                v = 0.0f;
            }
            v_endif;
        }
        else if constexpr (COMP_MODE == SfpuOpType::unary_lt)
        {
            v_if (v < s)
            {
                v = 1.0f;
            }
            v_else
            {
                v = 0.0f;
            }
            v_endif;
        }
        else if constexpr (COMP_MODE == SfpuOpType::unary_ge)
        {
            v_if (v >= s)
            {
                v = 1.0f;
            }
            v_else
            {
                v = 0.0f;
            }
            v_endif;
        }
        else if constexpr (COMP_MODE == SfpuOpType::unary_le)
        {
            v_if (v <= s)
            {
                v = 1.0f;
            }
            v_else
            {
                v = 0.0f;
            }
            v_endif;
        }

        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
