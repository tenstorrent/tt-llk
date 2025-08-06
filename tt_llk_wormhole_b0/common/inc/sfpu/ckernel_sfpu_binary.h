// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <limits>

#include "ckernel_sfpu_binary.h"
#include "ckernel_sfpu_exp.h"
#include "ckernel_sfpu_log.h"
#include "ckernel_sfpu_recip.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

// Helper function: Check if power is an integer
sfpi_inline bool _is_integer_power_(sfpi::vFloat pow, sfpi::vInt& pow_int, sfpi::vFloat& pow_rounded)
{
    pow_int     = float_to_int16(pow, 0); // int16 should be plenty, since large powers will approach 0/Inf
    pow_rounded = int32_to_float(pow_int, 0);
    return true; // Return value not used in vector context, but maintains function signature
}

// Helper function: Calculate logarithm of base using polynomial approximation
sfpi_inline sfpi::vFloat _calculate_log_for_power_(sfpi::vFloat base)
{
    // Normalize base to calculation range [1,2]
    sfpi::vFloat x = setexp(base, 127); // set exp to exp bias

    // 3rd order polynomial approximation - determined using rminimax over [1,2]
    sfpi::vFloat series_result = x * (x * (x * 0x2.44734p-4f - 0xd.e712ap-4f) + 0x2.4f5388p+0f) - 0x1.952992p+0f;

    // Convert exponent to float for correction
    sfpi::vInt exp = exexp(base);
    v_if (exp < 0)
    {
        exp = sfpi::setsgn(~exp + 1, 1);
    }
    v_endif;
    sfpi::vFloat expf = int32_to_float(exp, 0);

    // De-normalize to original range: ln(base) = ln(normalized) + exp*ln(2)
    sfpi::vFloat vConstLn2  = 0.692871f;
    sfpi::vFloat log_result = expf * vConstLn2 + series_result;

    // Handle special case: ln(0) = -infinity
    v_if (base == 0.0f)
    {
        log_result = -std::numeric_limits<float>::infinity();
    }
    v_endif;

    return log_result;
}

// Helper function: Handle negative base cases with proper sign logic
sfpi_inline sfpi::vFloat _handle_negative_base_power_(sfpi::vFloat result, sfpi::vFloat pow_rounded, sfpi::vFloat pow, sfpi::vInt pow_int)
{
    // For negative base, check if power is integer
    v_if (pow_rounded == pow)
    {
        // Integer power: check if odd to determine result sign
        v_if (pow_int & 0x1)
        {
            result = sfpi::setsgn(result, 1); // Odd power: negative result
        }
        v_endif;
        // Even power: result stays positive (no action needed)
    }
    v_else
    {
        // Non-integer power with negative base: undefined, return NaN
        result = std::numeric_limits<float>::quiet_NaN();
    }
    v_endif;

    return result;
}

// Main power function: base^pow using exp(pow * ln(base))
sfpi_inline sfpi::vFloat _calculate_sfpu_binary_power_(sfpi::vFloat base, sfpi::vFloat pow)
{
    sfpi::vFloat original_base = base;

    // Determine if power is integer (needed for negative base handling)
    sfpi::vInt pow_int;
    sfpi::vFloat pow_rounded;
    _is_integer_power_(pow, pow_int, pow_rounded);

    // For integer powers, work with positive base (sign handled later)
    v_if (pow_rounded == pow)
    {
        base = sfpi::setsgn(base, 0); // Remove sign for logarithm calculation
    }
    v_endif;

    // Calculate ln(base) using polynomial approximation
    sfpi::vFloat log_result = _calculate_log_for_power_(base);

    // Calculate base^pow = exp(pow * ln(base))
    sfpi::vFloat val    = pow * log_result;
    sfpi::vFloat result = _sfpu_exp_(sfpi::setsgn(val, 0)); // Force positive for exp

    // Handle negative exponent by taking reciprocal
    v_if (val < 0)
    {
        result = _sfpu_reciprocal_(result);
    }
    v_endif;

    // Handle negative base cases with proper sign logic
    v_if (original_base < 0.0f)
    {
        result = _handle_negative_base_power_(result, pow_rounded, pow, pow_int);
    }
    v_endif;

    return result;
}

// Helper function: Handle division with proper special case handling
sfpi_inline sfpi::vFloat _calculate_division_(sfpi::vFloat in0, sfpi::vFloat in1)
{
    sfpi::vFloat result;

    v_if (in1 == 0)
    {
        // Division by zero cases
        v_if (in0 == 0)
        {
            result = std::numeric_limits<float>::quiet_NaN(); // 0/0 = NaN
        }
        v_else
        {
            result = std::numeric_limits<float>::infinity(); // x/0 = ±Inf
            result = sfpi::setsgn(result, in0);              // Preserve sign of numerator
        }
        v_endif;
    }
    v_elseif (in0 == in1)
    {
        result = sfpi::vConst1; // x/x = 1 (optimization)
    }
    v_else
    {
        // Standard division: x/y = x * (1/y)
        result = in0 * sfpi::setsgn(_sfpu_reciprocal_<4>(in1), in1);
    }
    v_endif;

    return result;
}

// Helper function: Handle xlogy operation (x * log(y))
sfpi_inline sfpi::vFloat _calculate_xlogy_(sfpi::vFloat in0, sfpi::vFloat in1)
{
    static constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    sfpi::vFloat result;

    v_if ((in1 < 0.0f) || (in1 == nan))
    {
        result = nan; // log of negative or NaN is NaN
    }
    v_else
    {
        // Calculate log(in1) and multiply by in0
        sfpi::dst_reg[0] = in1;
        _calculate_log_body_<false>(0);
        result = sfpi::dst_reg[0] * in0;
    }
    v_endif;

    return result;
}

// Main binary operations dispatcher
template <bool APPROXIMATION_MODE, BinaryOp BINOP, int ITERATIONS = 8>
inline void _calculate_sfpu_binary_(const uint dst_offset)
{
    // Process vector elements in batches
    for (int d = 0; d < ITERATIONS; d++)
    {
        constexpr uint dst_tile_size = 32;
        sfpi::vFloat in0             = sfpi::dst_reg[0];                          // First operand
        sfpi::vFloat in1             = sfpi::dst_reg[dst_offset * dst_tile_size]; // Second operand
        sfpi::vFloat result;

        // Dispatch to appropriate operation (compile-time selection)
        if constexpr (BINOP == BinaryOp::ADD)
        {
            result = in0 + in1;
        }
        else if constexpr (BINOP == BinaryOp::SUB)
        {
            result = in0 - in1;
        }
        else if constexpr (BINOP == BinaryOp::RSUB)
        {
            result = in1 - in0; // Reverse subtraction
        }
        else if constexpr (BINOP == BinaryOp::MUL)
        {
            result = in0 * in1;
        }
        else if constexpr (BINOP == BinaryOp::DIV)
        {
            result = _calculate_division_(in0, in1);
        }
        else if constexpr (BINOP == BinaryOp::POW)
        {
            result = _calculate_sfpu_binary_power_(in0, in1); // in0^in1
        }
        else if constexpr (BINOP == BinaryOp::XLOGY)
        {
            result = _calculate_xlogy_(in0, in1); // in0 * log(in1)
        }

        sfpi::dst_reg[0] = result;
        sfpi::dst_reg++;
    }
}

// Initialize SFPU constants/tables for specific binary operations
template <bool APPROXIMATION_MODE /*unused*/, BinaryOp BINOP>
inline void _sfpu_binary_init_()
{
    // Operations requiring reciprocal initialization
    if constexpr (BINOP == BinaryOp::DIV || BINOP == BinaryOp::POW)
    {
        _init_reciprocal_<APPROXIMATION_MODE>();
    }
    // Operations requiring logarithm initialization
    else if constexpr (BINOP == BinaryOp::XLOGY)
    {
        _init_log_<APPROXIMATION_MODE>();
    }
    // Other operations (ADD, SUB, RSUB, MUL) require no special initialization
}

} // namespace sfpu
} // namespace ckernel
