// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu_load_config.h"
#include "ckernel_sfpu_sigmoid.h"
#include "sfpi.h"
#include "sfpi_fp16.h"
#include "sfpu/ckernel_sfpu_polyval.h"

namespace ckernel
{
namespace sfpu
{

template <bool is_fp32_dest_acc_en>
sfpi_inline sfpi::vFloat _sfpu_tanh_fp32_accurate_(sfpi::vFloat val)
{
    sfpi::vFloat result = sfpi::vConst0;

    constexpr float POLYNOMIAL_THRESHOLD = 0.6f;

    sfpi::vFloat abs_val = sfpi::abs(val);

    v_if (abs_val < POLYNOMIAL_THRESHOLD)
    {
        // Small |x|: Use minimax polynomial for better accuracy
        // Polynomial coefficients found with Sollya using the following command:
        // fpminimax(tanh(x)/x, [|0,2,4,6,8|], [|single...|], [-0.6; -2^(-40)] + [2^(-40); 0.6], relative);
        sfpi::vFloat x2 = val * val;

        sfpi::vFloat p = PolynomialEvaluator::eval(
            x2,
            0.999999940395355224609375f,
            -0.33332359790802001953125f,
            0.13310669362545013427734375f,
            -5.21197654306888580322265625e-2f,
            1.5497927553951740264892578125e-2f);

        result = val * p;
    }
    v_else
    {
        // Normal region: Use tanh(x) = 2*sigmoid(2x) - 1
        sfpi::vFloat two_x = 2.f * val;
        sfpi::vFloat sig   = _sfpu_sigmoid_<is_fp32_dest_acc_en>(two_x);

        // Compute 2*sigmoid(2x) - 1
        result = 2.f * sig - sfpi::vConst1;
    }
    v_endif;

    return result;
}

template <bool is_fp32_acc_to_dest_mode>
sfpi_inline sfpi::vFloat _sfpu_tanh_continued_fraction_(sfpi::vFloat val)
{
    // Formula found at
    // https://varietyofsound.wordpress.com/2011/02/14/efficient-tanh-computation-using-lamberts-continued-fraction/
    // This approximation is derived from a continued fraction formula of tanh(x)

    // For negative numbers, we compute tanh(x) = -tanh(x)
    sfpi::vFloat x = sfpi::abs(val); // set positive

    // Compute numerator and denominator of continued fraction using Horner's method
    sfpi::vFloat x2        = x * x;
    sfpi::vFloat numerator = x * (135135.f + x2 * (17326.f + x2 * (378.f + x2)));

    constexpr float denominator_coefs[] = {135135.f, 62370.f, 3150.f, 28.f};
    sfpi::vFloat denominator            = PolynomialEvaluator::eval(x2, 135135.f, 62370.f, 3150.f, 28.f);

    sfpi::vFloat result = numerator * ckernel::sfpu::_sfpu_reciprocal_<2>(denominator);

    // For larger x, the continued fraction may exceed 1.0.
    // Since tanh(x) is bounded by [-1, 1], we clamp output to 1.0.
    sfpi::vFloat threshold_value = sfpi::vConst1;
    sfpi::vec_min_max(result, threshold_value);

    result = sfpi::setsgn(result, val); // restore sign (i.e. tanh(-x) = -tanh(x))

    return result;
}

template <bool is_fp32_acc_to_dest_mode>
sfpi_inline sfpi::vFloat _sfpu_tanh_polynomial_(sfpi::vFloat x)
{
    // For negative numbers, we compute tanh(-x) = -tanh(x)
    sfpi::vFloat val = sfpi::abs(x); // set positive

    // Polynomial coefficients found using Sollya
    // val * (0.999004364013671875 + val * (3.0897438526153564453125e-2 + val * (-0.4890659749507904052734375 + val *
    // (0.281917631626129150390625 + val * (-6.6649019718170166015625e-2 + val *
    // (5.876733921468257904052734375e-3))))));
    sfpi::vFloat result = PolynomialEvaluator::eval(
        val,
        sfpi::vConst0,
        0.999004364013671875,
        3.0897438526153564453125e-2,
        -0.4890659749507904052734375,
        sfpi::vConstFloatPrgm2,
        sfpi::vConstFloatPrgm1,
        sfpi::vConstFloatPrgm0);

    // For larger x, the polynomial approximation may exceed 1.0.
    // Since tanh(x) is bounded by [-1, 1], we clamp output to 1.0.
    sfpi::vFloat threshold_value = sfpi::vConst1;
    sfpi::vec_min_max(result, threshold_value);

    result = sfpi::setsgn(result, x); // restore sign (i.e. tanh(-x) = -tanh(x))

    return result;
}

template <bool APPROXIMATION_MODE, bool is_fp32_dest_acc_en, int ITERATIONS>
inline void _calculate_tanh_()
{
    if constexpr (APPROXIMATION_MODE)
    {
        sfpi::vUInt l0 = sfpi::l_reg[sfpi::LRegs::LReg0];
        sfpi::vUInt l1 = sfpi::l_reg[sfpi::LRegs::LReg1];
        sfpi::vUInt l2 = sfpi::l_reg[sfpi::LRegs::LReg2];
        sfpi::vUInt l4 = sfpi::l_reg[sfpi::LRegs::LReg4];
        sfpi::vUInt l5 = sfpi::l_reg[sfpi::LRegs::LReg5];
        sfpi::vUInt l6 = sfpi::l_reg[sfpi::LRegs::LReg6];
#pragma GCC unroll 8
        for (int d = 0; d < ITERATIONS; d++)
        {
            sfpi::vFloat x = sfpi::dst_reg[0];

            sfpi::vFloat two_x = 2.0f * x;
            sfpi::vFloat sig2x = lut2(two_x, l0, l1, l2, l4, l5, l6, 0);

            sfpi::vFloat y = 2.0f * sig2x;

            // sfpi::vFloat one = sfpi::vConst1;
            // sfpi::vec_min_max(y, one);

            sfpi::dst_reg[0] = y;
            sfpi::dst_reg++;
        }
    }
    else
    { // APPROXIMATION_MODE is false

        for (int d = 0; d < ITERATIONS; d++)
        {
            sfpi::vFloat val = sfpi::dst_reg[0];

            sfpi::vFloat result;

            if constexpr (is_fp32_dest_acc_en)
            {
                // Use accurate sigmoid-based tanh for fp32
                result = _sfpu_tanh_fp32_accurate_<is_fp32_dest_acc_en>(val);
            }
            else
            {
                result = _sfpu_tanh_polynomial_<is_fp32_dest_acc_en>(val);
                result = sfpi::reinterpret<sfpi::vFloat>(sfpi::float_to_fp16b(result, 0));
            }

            sfpi::dst_reg[0] = result;
            sfpi::dst_reg++;
        }
    }
}

template <bool APPROXIMATION_MODE, bool is_fp32_dest_acc_en>
inline void _init_tanh_()
{
    if constexpr (APPROXIMATION_MODE)
    {
        // Using a 6 piece LUT to calculate and model tanh
        // x <= 0.5 --> 0.2452x + (-0.0004997)
        // x <= 1.0 --> 0.2173x + 0.0152
        // x <= 1.5 --> 0.1731x + 0.05988
        // x <= 2.0 --> 0.1262x + 0.1298
        // x <= 4.0 --> 0.0485x + 0.2998
        // x >  4.0 --> 0.4998

        // imm0[15:0] = A0=0.2452 = 0x33D9 -- imm0[31:16] = A1=0.2173 = 0x32F4
        _sfpu_load_imm32_(0, 0x32F433D9);
        // imm4[15:0] = B0= -0.0004997  = 0x9018 -- imm4[31:16] = B1= 0.0152 = 0x23c8
        _sfpu_load_imm32_(4, 0x23C89018);

        // imm1[15:0] = A2=0.1731 = 0x318a -- imm1[31:16] = A3=0.1262 = 0x300a
        _sfpu_load_imm32_(1, 0x300A318A);
        // imm5[15:0] = B2=0.05988 = 0x2BAA -- imm5[31:16] = B3=0.1298 = 0x3027
        _sfpu_load_imm32_(5, 0x30272BAA);

        // imm2[15:0] = A4=0.0485 = 0x2A35 -- imm2[31:16] = A5=0.0 = 0x7C00
        _sfpu_load_imm32_(2, 0x7C002A35);
        // imm6[15:0] = B4=0.2998 = 0x34CC -- imm6[31:16] = B5=0.4998 = 0x37ff
        _sfpu_load_imm32_(6, 0x37ff34CC);
    }
    else
    {
        if constexpr (is_fp32_dest_acc_en)
        {
            _init_sigmoid_<false>();
        }
        else
        {
            // Polynomial approximation
            // Store some polynomial coefficients in programmable registers
            sfpi::vConstFloatPrgm0 = 5.876733921468257904052734375e-3;
            sfpi::vConstFloatPrgm1 = -6.6649019718170166015625e-2;
            sfpi::vConstFloatPrgm2 = 0.281917631626129150390625;
        }
    }
}

} // namespace sfpu
} // namespace ckernel
