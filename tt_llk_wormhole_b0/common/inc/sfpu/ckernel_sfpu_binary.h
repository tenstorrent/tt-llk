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
#include "ckernel_sfpu_rounding_ops.h"

namespace ckernel
{
namespace sfpu
{

sfpi_inline sfpi::vFloat _calculate_sfpu_binary_power_null_(sfpi::vFloat base, sfpi::vFloat pow) {

    return sfpi::vFloat(80.819f);

}

sfpi_inline sfpi::vFloat _calculate_sfpu_binary_power_21f_(sfpi::vFloat base, sfpi::vFloat pow) {

    // Normalize base to calculation range
    sfpi::vFloat x = sfpi::setexp(base, 127); // set exp to exp bias (put base in range of 1-2)

    // 3rd order polynomial approx - determined using rminimax over [1,2]
    sfpi::vFloat series_result = x * (x * (x * 0x2.44734p-4f - 0xd.e712ap-4f) + 0x2.4f5388p+0f) - 0x1.952992p+0f;

    // Convert exponent to float
    sfpi::vInt exp = sfpi::exexp(base);
    v_if (exp < 0)
    {
        exp = sfpi::setsgn(~exp + 1, 1);
    }
    v_endif;
    sfpi::vFloat expf = sfpi::int32_to_float(exp, 0);


    // De-normalize to original range
    // sfpi::vFloat vConstLn2  = 0.692871f;
    sfpi::vFloat vConst1Ln2 = 1.4426950408889634f;
    sfpi::vFloat log2_result = expf + series_result * vConst1Ln2; // exp correction: ln(1+x) + exp*ln(2)

    sfpi::vFloat zff = pow * log2_result;
    v_if (zff < sfpi::vFloat(-127.f)) { // -126.99999237060546875
        zff = sfpi::vFloat(-127.f);
    }
    v_endif;

    zff = zff * sfpi::vFloat(8388608); // could be done by addexp or equivalent ?

    // log2_result = sfpi::vFloat(26591258.22649899f);

    

    sfpi::vInt z = _float_to_int32_(zff + sfpi::vFloat(0x3f800000)); // (bias + x * log2(a)) * N_m


    sfpi::vInt zii = z & 0x7f800000; 
    sfpi::vInt zif = z & sfpi::vInt(0x007fffff); // extra mantissa

    sfpi::vFloat d1 = sfpi::vFloat(0.40196114e-7); 
    sfpi::vFloat d2 = sfpi::int32_to_float(sfpi::vInt(0xf94ee7) + zif);
    sfpi::vFloat d3 = sfpi::int32_to_float(sfpi::vInt(0x560) + zif);
    d2 = d1 * d2;
    zif = _float_to_int32_(d2 * d3);
    
    zii |= zif; // restore exponent
    
    sfpi::vFloat y = sfpi::reinterpret<sfpi::vFloat>(zii);
    
    
    return y;
}

inline sfpi::vInt _float_to_int32_fast_(sfpi::vFloat in)
{
    sfpi::vInt result;
    sfpi::vInt exp = exexp(in); // extract exponent
    v_if (exp < 0)
    {
        result = 0;
    }
    v_elseif (exp > 30) // overflow occurs above this range
    {
        // set to int32 max value in case of overflow
        result = std::numeric_limits<int32_t>::max();
    }
    v_else
    {
        // extract mantissa
        sfpi::vInt man = exman8(in);
        // shift the mantissa by (23-exponent) to the right
        sfpi::vInt shift = exp - 23; // 23 is number of mantissa in float32
        man              = shft(sfpi::reinterpret<sfpi::vUInt>(man), shift);


        result = man;
    }
    v_endif;
    return result;
}

sfpi_inline sfpi::vFloat _calculate_sfpu_binary_power_21f_alt0_(sfpi::vFloat base, sfpi::vFloat pow) {

    // Normalize base to calculation range
    sfpi::vFloat x = sfpi::setexp(base, 127); // set exp to exp bias (put base in range of 1-2)


    // Check valid base range
    sfpi::vInt pow_int       = sfpi::float_to_int16(pow, 0); // int16 should be plenty, since large powers will approach 0/Inf
    sfpi::vFloat pow_rounded = sfpi::int32_to_float(pow_int, 0);

    sfpi::vFloat sign = sfpi::vConst1;
    v_if (base < 0.0f)
    { // negative base
        // Check for integer power
        v_if (pow_rounded == pow)
        {
            // if pow is odd integer, set result to negative
            v_if (pow_int & 0x1)
            {
                sign = sfpi::vConstNeg1;
            }
            v_endif;
        }
        v_else
        {
            sign = std::numeric_limits<float>::quiet_NaN();
        }
        v_endif;
    }
    v_endif;
    x *= sign;

    // 3rd order polynomial approx - determined using rminimax over [1,2]
    sfpi::vFloat series_result = x * (x * (x * 0x2.44734p-4f - 0xd.e712ap-4f) + 0x2.4f5388p+0f) - 0x1.952992p+0f;

    // Convert exponent to float
    sfpi::vInt exp = sfpi::exexp(base);
    v_if (exp < 0)
    {
        exp = sfpi::setsgn(~exp + 1, 1);
    }
    v_endif;
    sfpi::vFloat expf = sfpi::int32_to_float(exp, 0);


    // De-normalize to original range
    // sfpi::vFloat vConstLn2  = 0.692871f;
    const sfpi::vFloat vConst1Ln2 = sfpi::vConstFloatPrgm0; // 1.4426950408889634f;
    sfpi::vFloat log2_result = expf + series_result * vConst1Ln2; // exp correction: ln(1+x) + exp*ln(2)


    sfpi::vFloat zff = pow * log2_result;
    const sfpi::vFloat low_threshold = sfpi::vConstFloatPrgm2;
    v_if (zff < low_threshold) { // -126.99999237060546875
        zff = low_threshold;
    }
    v_endif;

    zff = addexp(zff, 23); // * 2**23 (Mn)

    // log2_result = sfpi::vFloat(26591258.22649899f);

    // sfpi::vFloat vOneF32 = sfpi::vConst1;
    // sfpi::vInt z = _float_to_int32_(zff) + sfpi::reinterpret<sfpi::vInt>(vOneF32); // (bias + x * log2(a)) * N_m // TODO: Problem with negative values 
    sfpi::vInt z = _float_to_int32_fast_(zff + sfpi::vFloat(0x3f800000)); // (bias + x * log2(a)) * N_m


    sfpi::vInt zii = exexp(sfpi::reinterpret<sfpi::vFloat>(z)); // & 0x7f800000; 
    sfpi::vInt zif = sfpi::exman9(sfpi::reinterpret<sfpi::vFloat>(z)); //z & sfpi::vInt(0x007fffff); // extract mantissa // 

    sfpi::vFloat d1 = sfpi::vFloat(0.40196114e-7); 
    sfpi::vFloat d2 = sfpi::int32_to_float(sfpi::vInt(0xf94ee7) + zif);
    sfpi::vFloat d3 = sfpi::int32_to_float(sfpi::vInt(0x560) + zif);
    d2 = d1 * d2;
    zif = _float_to_int32_fast_(d2 * d3);
    




    // zii |= zif; // restore exponent
    zii = sfpi::reinterpret<sfpi::vInt>(sfpi::setexp(sfpi::reinterpret<sfpi::vFloat>(zif), 127U + zii));

    sfpi::vFloat y = sfpi::reinterpret<sfpi::vFloat>(zii);
    

    y *= sign;


    // y = sfpi::reinterpret<sfpi::vFloat>(float_to_fp16b(y, 0));

    return y;
}

sfpi_inline sfpi::vFloat _calculate_sfpu_binary_power_(sfpi::vFloat base, sfpi::vFloat pow)
{
    sfpi::vFloat original_base = base;

    // Check for integer power
    sfpi::vInt pow_int       = float_to_int16(pow, 0); // int16 should be plenty, since large powers will approach 0/Inf
    sfpi::vFloat pow_rounded = int32_to_float(pow_int, 0);
    v_if (pow_rounded == pow)
    {
        // if pow is integer, set base to positive
        base = sfpi::setsgn(base, 0);
    }
    v_endif;

    // Normalize base to calculation range
    sfpi::vFloat x = setexp(base, 127); // set exp to exp bias (put base in range of 1-2)

    // 3rd order polynomial approx - determined using rminimax over [1,2]
    sfpi::vFloat series_result = x * (x * (x * 0x2.44734p-4f - 0xd.e712ap-4f) + 0x2.4f5388p+0f) - 0x1.952992p+0f;

    // Convert exponent to float
    sfpi::vInt exp = exexp(base);
    v_if (exp < 0)
    {
        exp = sfpi::setsgn(~exp + 1, 1);
    }
    v_endif;
    sfpi::vFloat expf = int32_to_float(exp, 0);

    // De-normalize to original range
    sfpi::vFloat vConstLn2  = 0.692871f;

    sfpi::vFloat log_result = expf * vConstLn2 + series_result; // exp correction: ln(1+x) + exp*ln(2)

    // Base case when input is 0. ln(0) = -inf
    v_if (base == 0.0f)
    { // Reload for register pressure
        log_result = -std::numeric_limits<float>::infinity();
    }
    v_endif;

    // Take exp(pow * log(base)) to produce base^pow
    sfpi::vFloat val = pow * log_result;

    // Force sign to 0 (make number positive)
    sfpi::vFloat result = _sfpu_exp_(sfpi::setsgn(val, 0));

    v_if (val < 0)
    {
        result = _sfpu_reciprocal_(result);
    }
    v_endif;

    // Check valid base range
    v_if (original_base < 0.0f)
    { // negative base
        // Check for integer power
        v_if (pow_rounded == pow)
        {
            // if pow is odd integer, set result to negative
            v_if (pow_int & 0x1)
            {
                result = sfpi::setsgn(result, 1);
            }
            v_endif;
        }
        v_else
        {
            result = std::numeric_limits<float>::quiet_NaN();
        }
        v_endif;
    }
    v_endif;

    return result;
}

template <bool APPROXIMATION_MODE, BinaryOp BINOP, int ITERATIONS = 8>
inline void _calculate_sfpu_binary_(const uint dst_offset)
{
    static constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        constexpr uint dst_tile_size = 32;
        sfpi::vFloat in0             = sfpi::dst_reg[0];
        sfpi::vFloat in1             = sfpi::dst_reg[dst_offset * dst_tile_size];
        sfpi::vFloat result          = 0.0f;

        if constexpr (BINOP == BinaryOp::ADD)
        {
            result = in0 + in1;
        }
        else if constexpr (BINOP == BinaryOp::SUB)
        {
            result = in0 - in1;
        }
        else if constexpr (BINOP == BinaryOp::MUL)
        {
            result = in0 * in1;
        }
        else if constexpr (BINOP == BinaryOp::DIV)
        {
            v_if (in1 == 0)
            {
                v_if (in0 == 0)
                {
                    result = std::numeric_limits<float>::quiet_NaN();
                }
                v_else
                {
                    result = std::numeric_limits<float>::infinity();
                    result = sfpi::setsgn(result, in0);
                }
                v_endif;
            }
            v_elseif (in0 == in1)
            {
                result = sfpi::vConst1;
            }
            v_else
            {
                result = in0 * sfpi::setsgn(_sfpu_reciprocal_<4>(in1), in1);
            }
            v_endif;
        }
        else if constexpr (BINOP == BinaryOp::RSUB)
        {
            result = in1 - in0;
        }
        else if constexpr (BINOP == BinaryOp::POW)
        {
            // for (uint32_t i = 0; i < 100'000; i++) {
                result = _calculate_sfpu_binary_power_21f_alt0_(in0, in1);
                // result = _calculate_sfpu_binary_power_(in0, in1);
            // }
        }
        else if constexpr (BINOP == BinaryOp::XLOGY)
        {
            v_if ((in1 < 0.0f) || (in1 == nan))
            {
                result = nan;
            }
            v_else
            {
                sfpi::dst_reg[0] = in1;
                _calculate_log_body_<false>(0);
                result = sfpi::dst_reg[0] * in0;
            }
            v_endif;
        }

        sfpi::dst_reg[0] = result;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE /*unused*/, BinaryOp BINOP>
inline void _sfpu_binary_init_()
{
    if constexpr (BINOP == BinaryOp::DIV)
    {
        // Note: pow_21f version does not call reciprocal, but uses similar fixed-register (1/log(2))
        _init_reciprocal_<APPROXIMATION_MODE>();
    }
    else if constexpr (BINOP == BinaryOp::POW)
    {
        _init_reciprocal_<APPROXIMATION_MODE>();
        sfpi::vConstFloatPrgm2 = -127.0f;
    }
    else if constexpr (BINOP == BinaryOp::XLOGY)
    {
        _init_log_<APPROXIMATION_MODE>();
    }
}

} // namespace sfpu
} // namespace ckernel
