// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel.h"
#include "ckernel_defs.h"
#include "sfpi.h"

using namespace sfpi;

namespace ckernel::sfpu
{

/* Checks if the exponent is 128 and mantissa is 0.
If both conditions are met, the number is marked as
infinity, so '1' is written in the location of the DEST
where the number was stored. Otherwise, `0` is written instead
of the number.
*/
template <bool APPROXIMATION_MODE>
inline vFloat _calculate_isinf_(const vFloat& in)
{
    // SFPU microcode
    sfpi::vInt exp = sfpi::exexp(in);
    sfpi::vInt man = sfpi::exman9(in);
    vFloat out     = sfpi::vConst0;
    v_if (exp == 128 && man == 0)
    {
        out = sfpi::vConst1;
    }
    v_endif;
    return out;
}

/* Checks if the sign bit of the floating point number in DEST
is positive. Checks if the exponent is 128 and mantissa is 0.
If all of the three conditions are met, the number is marked as
positive infinity, so '1' is written in the location of the DEST
where the number was stored. Otherwise, `0` is written instead
of the number.
*/
template <bool APPROXIMATION_MODE>
inline vFloat _calculate_isposinf_(const vFloat& in)
{
    // SFPU microcode
    sfpi::vInt exp = sfpi::exexp(in);
    sfpi::vInt man = sfpi::exman9(in);
    vFloat out     = sfpi::vConst0;
    vInt signbit   = sfpi::reinterpret<sfpi::vInt>(in) & 0x80000000; // returns 0 for +ve value
    v_if (signbit == 0 && exp == 128 && man == 0)
    {
        out = sfpi::vConst1;
    }
    v_endif;
    return out;
}

/* Checks if the sign bit of the floating point number in DEST
is negative. Checks if the exponent is 128 and mantissa is 0.
If all of the three conditions are met, the number is marked as
negative infinity, so '1' is written in the location of the DEST
where the number was stored. Otherwise, `0` is written instead
of the number.
*/
template <bool APPROXIMATION_MODE>
inline vFloat _calculate_isneginf_(const vFloat& in)
{
    // SFPU microcode
    sfpi::vInt exp = sfpi::exexp(in);
    sfpi::vInt man = sfpi::exman9(in);
    vFloat out     = sfpi::vConst0;
    vInt signbit   = sfpi::reinterpret<sfpi::vInt>(in) & 0x80000000; // returns 0x80000000 for -ve value
    v_if (signbit == 0x80000000 && exp == 128 && man == 0)
    {
        out = sfpi::vConst1;
    }
    v_endif;
    return out;
}

/* Checks if the exponent is 128 and mantissa is not 0.
If both conditions are met, the number is marked as
nan, so '1' is written in the location of the DEST
where the number was stored. Otherwise, `0` is written instead
of the number.
*/
template <bool APPROXIMATION_MODE>
inline vFloat _calculate_isnan_(const vFloat& in)
{
    // SFPU microcode
    sfpi::vInt exp = sfpi::exexp(in);
    sfpi::vInt man = sfpi::exman9(in);
    vFloat out     = sfpi::vConst0;
    v_if (exp == 128 && man != 0)
    {
        out = sfpi::vConst1;
    }
    v_endif;
    return out;
}

template <bool APPROXIMATION_MODE>
inline vFloat _calculate_isfinite_(const vFloat& v)
{
    // SFPU microcode
    // A number is finite if it's neither infinity nor NaN
    vFloat is_inf = _calculate_isinf_<APPROXIMATION_MODE>(v);
    vFloat is_nan = _calculate_isnan_<APPROXIMATION_MODE>(v);
    vFloat result = sfpi::vConst1; // Assume finite (1.0f) by default

    // If it's either infinity or NaN, it's not finite
    v_if (is_inf == sfpi::vConst1 || is_nan == sfpi::vConst1)
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
        vFloat in = dst_reg[0];
        vFloat out;

        if constexpr (operation == SfpuType::isinf)
        {
            out = _calculate_isinf_<APPROXIMATION_MODE>(in);
        }
        else if constexpr (operation == SfpuType::isposinf)
        {
            out = _calculate_isposinf_<APPROXIMATION_MODE>(in);
        }
        else if constexpr (operation == SfpuType::isneginf)
        {
            out = _calculate_isneginf_<APPROXIMATION_MODE>(in);
        }
        else if constexpr (operation == SfpuType::isnan)
        {
            out = _calculate_isnan_<APPROXIMATION_MODE>(in);
        }
        else if constexpr (operation == SfpuType::isfinite)
        {
            out = _calculate_isfinite_<APPROXIMATION_MODE>(in);
        }

        dst_reg[0] = out;
        dst_reg++;
    }
}

} // namespace ckernel::sfpu
