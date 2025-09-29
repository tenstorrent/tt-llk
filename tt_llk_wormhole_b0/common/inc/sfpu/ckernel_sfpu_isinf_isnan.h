// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel.h"
#include "ckernel_defs.h"
#include "sfpi.h"

namespace ckernel::sfpu
{

template <bool APPROXIMATION_MODE>
inline sfpi::vFloat _calculate_isinf_(const sfpi::vFloat& in)
{
    // SFPU microcode
    sfpi::vInt raw_bits = sfpi::reinterpret<sfpi::vInt>(in);
    sfpi::vFloat out    = sfpi::vConst0;

    // Mask out sign bit to check for both +inf and -inf
    // Positive infinity: 0x7F800000, Negative infinity: 0xFF800000
    // After masking sign bit: both become 0x7F800000
    sfpi::vInt abs_bits = raw_bits & 0x7FFFFFFF;

    v_if (abs_bits == 0x7F800000)
    {
        out = sfpi::vConst1;
    }
    v_endif;
    return out;
}

template <bool APPROXIMATION_MODE>
inline sfpi::vFloat _calculate_isposinf_(const sfpi::vFloat& in)
{
    // SFPU microcode
    sfpi::vInt raw_bits = sfpi::reinterpret<sfpi::vInt>(in);
    sfpi::vFloat out    = sfpi::vConst0;

    // Exact check for positive infinity
    v_if (raw_bits == 0x7F800000)
    {
        out = sfpi::vConst1;
    }
    v_endif;
    return out;
}

template <bool APPROXIMATION_MODE>
inline sfpi::vFloat _calculate_isneginf_(const sfpi::vFloat& in)
{
    // SFPU microcode
    // Direct bit pattern comparison for negative infinity (0xFF800000)
    sfpi::vInt raw_bits = sfpi::reinterpret<sfpi::vInt>(in);
    sfpi::vFloat out    = sfpi::vConst0;

    v_if (raw_bits == 0xFF800000)
    {
        out = sfpi::vConst1;
    }
    v_endif;
    return out;
}

template <bool APPROXIMATION_MODE>
inline sfpi::vFloat _calculate_isnan_(const sfpi::vFloat& in)
{
    // SFPU microcode
    sfpi::vInt raw_bits = sfpi::reinterpret<sfpi::vInt>(in);
    sfpi::vFloat out    = sfpi::vConst0;

    // NaN check: abs(raw_bits) > 0x7F800000 (greater than infinity)
    // NaN has max exponent + non-zero mantissa
    sfpi::vInt abs_bits = raw_bits & 0x7FFFFFFF; // Remove sign bit
    v_if (abs_bits > 0x7F800000)
    {
        out = sfpi::vConst1;
    }
    v_endif;
    return out;
}

template <bool APPROXIMATION_MODE>
inline sfpi::vFloat _calculate_isfinite_(const sfpi::vFloat& v)
{
    // SFPU microcode
    sfpi::vInt raw_bits = sfpi::reinterpret<sfpi::vInt>(v);
    sfpi::vFloat result = sfpi::vConst1;

    // A number is finite if exponent != 255 (0xFF)
    sfpi::vInt exp_mask = raw_bits & 0x7F800000;

    v_if (exp_mask == 0x7F800000) // Infinity or NaN
    {
        result = sfpi::vConst0;
    }
    v_endif;
    return result;
}

template <SfpuType operation, bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_sfpu_isinf_isnan_()
{
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat in = sfpi::dst_reg[0];

        if constexpr (operation == SfpuType::isinf)
        {
            sfpi::dst_reg[0] = _calculate_isinf_<APPROXIMATION_MODE>(in);
        }
        else if constexpr (operation == SfpuType::isposinf)
        {
            sfpi::dst_reg[0] = _calculate_isposinf_<APPROXIMATION_MODE>(in);
        }
        else if constexpr (operation == SfpuType::isneginf)
        {
            sfpi::dst_reg[0] = _calculate_isneginf_<APPROXIMATION_MODE>(in);
        }
        else if constexpr (operation == SfpuType::isnan)
        {
            sfpi::dst_reg[0] = _calculate_isnan_<APPROXIMATION_MODE>(in);
        }
        else if constexpr (operation == SfpuType::isfinite)
        {
            sfpi::dst_reg[0] = _calculate_isfinite_<APPROXIMATION_MODE>(in);
        }

        sfpi::dst_reg++;
    }
}

} // namespace ckernel::sfpu
