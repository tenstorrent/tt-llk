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

sfpi_inline sfpi::vFloat _calculate_sfpu_binary_power_(sfpi::vFloat base, sfpi::vFloat pow) {

    // Normalize base to calculation range
    sfpi::vFloat x = setsgn(base, 0); // set base as positive
    x = sfpi::setexp(x, 127); // set exp to exp bias (put base in range of 1-2)


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
    const sfpi::vFloat vConst1Ln2 = sfpi::vConstFloatPrgm0; // 1.4426950408889634f;
    sfpi::vFloat log2_result = expf + series_result * vConst1Ln2; // exp correction: ln(1+x) + exp*ln(2)


    sfpi::vFloat zff = pow * log2_result;
    const sfpi::vFloat low_threshold = sfpi::vConstFloatPrgm2;
    v_if (zff < low_threshold) { // -126.99999237060546875
        zff = low_threshold;
    }
    v_endif;

    zff = addexp(zff, 23); // * 2**23 (Mn)
    sfpi::vInt z = _float_to_int32_fast_(zff + sfpi::vFloat(0x3f800000)); // (bias + x * log2(a)) * N_m


    sfpi::vInt zii = exexp(sfpi::reinterpret<sfpi::vFloat>(z)); 
    sfpi::vInt zif = sfpi::exman9(sfpi::reinterpret<sfpi::vFloat>(z)); 

    sfpi::vFloat d1 = sfpi::vFloat(0.40196114e-7); 
    sfpi::vFloat d2 = sfpi::int32_to_float(sfpi::vInt(0xf94ee7) + zif);
    sfpi::vFloat d3 = sfpi::int32_to_float(sfpi::vInt(0x560) + zif);
    d2 = d1 * d2;
    zif = _float_to_int32_fast_(d2 * d3);
    
    // zii |= zif; // restore exponent
    zii = sfpi::reinterpret<sfpi::vInt>(sfpi::setexp(sfpi::reinterpret<sfpi::vFloat>(zif), 127U + zii));

    sfpi::vFloat y = sfpi::reinterpret<sfpi::vFloat>(zii);
    
    // Check valid base range
    sfpi::vInt pow_int       = sfpi::float_to_int16(pow, 0); // int16 should be plenty, since large powers will approach 0/Inf
    sfpi::vFloat pow_rounded = sfpi::int32_to_float(pow_int, 0);

    v_if (base == 0.f) {
        v_if (pow < 0.f) {
            y = std::numeric_limits<float>::quiet_NaN();
        }
        v_endif
    }
    v_endif

    v_if (base < 0.0f)
    { // negative base
        // Check for integer power
        
        v_if (pow_rounded == pow)
        {
            // if pow is odd integer, set result to negative
            v_if (pow_int & 0x1)
            {
                // if negative base and negative pow then x**y = -(abs(x))**(abs(y))
                // `sign` will be used at the end
                y = setsgn(y, 1);
            }
            v_endif;
        }
        v_else
        {
            // multiplication by NaN gives NaN. 
            // Since we are going to multiply the result by `sign` to handle negative bases, we also use
            // `sign` to handle NaN results
            y = std::numeric_limits<float>::quiet_NaN();
        }
        v_endif;
    }
    v_endif;

    return y;
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
            for (uint32_t i = 0; i < 100'000; i++) {
                result = _calculate_sfpu_binary_power_21f_alt0_(in0, in1);
                // result = _calculate_sfpu_binary_power_(in0, in1);
            }
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
