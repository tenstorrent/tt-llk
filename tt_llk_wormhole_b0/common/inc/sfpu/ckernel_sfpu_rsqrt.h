// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
sfpi_inline sfpi::vFloat _calculate_ln_rsqrt_body_(sfpi::vFloat val)
{
    sfpi::vFloat result;

    if constexpr (APPROXIMATION_MODE)
    {
        // Fast approximation mode: sqrt approximation then reciprocal
        sfpi::vUInt magic = sfpi::vConstIntPrgm2;

        // sqrt initial approximation
        sfpi::vUInt val_s = magic + sfpi::reinterpret<sfpi::vUInt>(val);
        val_s >>= 1;
        sfpi::vFloat sqrt_approx = sfpi::reinterpret<sfpi::vFloat>(val_s);

        // Apply reciprocal to sqrt approximation
        // Force sign to 1 (make number negative)
        sfpi::vFloat sqrt_val = sfpi::setsgn(sqrt_approx, 1);
        sqrt_val              = setexp(sqrt_val, 126); // Set exponent to 126 to make the number in 0.5-1

        // Use constants for reciprocal calculation
        sfpi::vFloat vConstLn2Recip = sfpi::vConstFloatPrgm0;
        sfpi::vFloat two            = sfpi::vConstFloatPrgm1;
        result                      = vConstLn2Recip * (sqrt_val * vConstLn2Recip + two);

        // Multiple iterations for approximation mode based on APPROXIMATION_MODE
        constexpr int max_iter = APPROXIMATION_MODE ? 2 : 3;
        for (int s_iter = 0; s_iter < (max_iter - 1); s_iter++)
        {
            result = result * (sqrt_val * result + two);
        }

        // Handle exponent adjustment for reciprocal
        sfpi::vInt orig_exp = exexp(sqrt_approx);
        sfpi::vInt new_exp  = exexp(result);
        new_exp -= orig_exp;
        new_exp += 126;

        v_if (new_exp < 0)
        {
            result  = 0.0F;
            new_exp = 0;
        }
        v_endif;

        result = setexp(result, new_exp);

        v_if (sqrt_approx < 0.0F)
        {
            result = -result;
        }
        v_endif;
    }
    else
    {
        // High precision mode: direct reciprocal square root calculation
        v_if (val != 0.0f)
        {
            // Direct rsqrt using fast inverse square root algorithm
            sfpi::vUInt magic = sfpi::vConstIntPrgm2;

            // Fast inverse square root initial approximation
            // This directly computes 1/sqrt(x) approximation
            sfpi::vFloat approx = sfpi::reinterpret<sfpi::vFloat>(magic - (sfpi::reinterpret<sfpi::vUInt>(val) >> 1));

            // Newton-Raphson iterations for 1/sqrt(x)
            // Formula: x_new = x * (1.5 - 0.5 * val * x * x)
            for (int r = 0; r < ITERATIONS; r++)
            {
                approx = approx * (1.5f - 0.5f * val * approx * approx);
            }

            result = approx;
        }
        v_else
        {
            // Handle zero case
            result = 0.0f;
        }
        v_endif;
    }

    return result;
}

template <bool APPROXIMATION_MODE, int ITERATIONS, bool is_fp32_dest_acc_en>
inline void _calculate_ln_rsqrt_()
{
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat val = sfpi::dst_reg[0];
        sfpi::vFloat out = _calculate_ln_rsqrt_body_<APPROXIMATION_MODE, ITERATIONS>(val);

        if constexpr (is_fp32_dest_acc_en || APPROXIMATION_MODE)
        {
            sfpi::dst_reg[0] = out;
        }
        else
        {
            sfpi::dst_reg[0] = sfpi::reinterpret<sfpi::vFloat>(float_to_fp16b(out, 0));
        }

        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
inline void _init_ln_rsqrt_()
{
    // Initialize constants for reciprocal calculation (used in approximation mode)
    sfpi::vConstFloatPrgm0 = 1.442695f; // ln2_recip
    sfpi::vConstFloatPrgm1 = 2.0f;

    // Initialize magic constant for rsqrt calculation
    if (APPROXIMATION_MODE)
    {
        // Magic constant for sqrt approximation
        sfpi::vConstFloatPrgm2 = sfpi::s2vFloat16b(127 << 7);
    }
    else
    {
        // Magic constant for fast inverse square root (0x5f3759df equivalent for 16-bit)
        sfpi::vConstFloatPrgm2 = sfpi::s2vFloat16b(0x5f37);
    }
}

} // namespace sfpu
} // namespace ckernel
