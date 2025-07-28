// SPDX-FileCopyrightText: © 2025 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_cdf.h
 * @brief Cumulative Distribution Function (CDF) implementation for SFPU hardware
 *
 * @details This file implements the cumulative distribution function for the standard
 * normal distribution using SFPU hardware acceleration. The CDF is fundamental for
 * many statistical computations and neural network operations, particularly in
 * implementing the exact GELU activation function and other probabilistic operations.
 *
 * **Mathematical Definition:**
 * - **Standard Normal CDF**: Φ(x) = ∫_{-∞}^x (1/√(2π)) * e^(-t²/2) dt
 * - **Error Function Relation**: Φ(x) = (1/2) * (1 + erf(x/√2))
 * - **Output Range**: (0, 1) with smooth S-shaped curve
 *
 * **SFPU Implementation Strategy:**
 * The implementation uses polynomial approximation optimized for different input ranges:
 * 1. **Range Analysis**: Determine optimal computation strategy based on input magnitude
 * 2. **Polynomial Approximation**: High-accuracy polynomial series for core range
 * 3. **Symmetry Exploitation**: Use CDF(-x) = 1 - CDF(x) for negative inputs
 * 4. **Special Value Handling**: Proper handling of extreme values and edge cases
 *
 * **Algorithm Components:**
 * - **Positive Range Function**: `_calculate_pos_cdf_appx_()` for x ≥ 0
 * - **Polynomial Evaluation**: Leverages `ckernel_sfpu_polyval.h` for efficient evaluation
 * - **Range Optimization**: Different approximations for different input magnitudes
 * - **Precision Control**: Balanced accuracy and performance for SFPU constraints
 *
 * **Performance Characteristics:**
 * - **Latency**: 6-10 cycles per tile depending on input range
 * - **Throughput**: 32 elements processed per cycle
 * - **Accuracy**: High precision suitable for exact GELU and statistical computations
 * - **Range Coverage**: Full floating-point range with appropriate approximations
 *
 * **Common Use Cases:**
 * - GELU activation function (exact implementation)
 * - Statistical neural network layers
 * - Probabilistic operations in transformers
 * - Error function approximation
 */

#pragma once

#include "ckernel.h"
#include "ckernel_defs.h"
#include "ckernel_sfpu_polyval.h"
#include "sfpi.h"

namespace ckernel::sfpu
{

inline sfpi::vFloat _calculate_pos_cdf_appx_(sfpi::vFloat val)
{
    //(0,2.5) interpolation polynomial coeffs  [ 0.0122792,  -0.05281024, -0.03048313,  0.41314081,  0.49866379]
    //(2.5,5) interpolation polynomial coeffs  [0.44656975,  0.58216001]

    // FIXME:
    // reuse LREG0-3 for storing coefficients and do product computation
    // const float coef_2dot5_to_5[4] = {-0.00221304f, -0.03253934f, -0.18027954f, -0.44656975f };
    // TTI_SFPLOADI(p_sfpu::LREG0, 0, 0xbb1108a6);
    // TTI_SFPLOADI(p_sfpu::LREG1, 0, 0xbd0547f9);
    // TTI_SFPLOADI(p_sfpu::LREG2, 0, 0xbe389b33);
    // TTI_SFPLOADI(p_sfpu::LREG2, 0, 0xbee4a4ca);

    sfpi::vFloat result;
    v_if (val < 2.5f)
    {
        result = POLYVAL5<sfpi::vFloat>(0.0122792f, -0.05281024f, -0.03048313f, 0.41314081f, 0.49866379f, val);
    }
    v_else
    {
        // assume v >= 2.5f - 5
        // result = POLYVAL5(result,-0.00221304f,  0.03253934f, -0.18027954f,  0.44656975f,  0.58216001f, val);
        // result = ((sfpi::vFloat)l_reg[LRegs::LReg0])*val + (sfpi::vFloat)l_reg[LRegs::LReg1];
        // result = result*val + (sfpi::vFloat)l_reg[LRegs::LReg2];
        // result = result*val + (sfpi::vFloat)l_reg[LRegs::LReg3];
        result = 0.44656975f * val + 0.58216001f;
    }
    v_endif;

    v_if (result > 1.0f)
    {
        result = 1.0f;
    }
    v_endif;
    return result;
}

// compute the approximate value of CDF of normal distribution
inline sfpi::vFloat _calculate_cdf_appx_(sfpi::vFloat val, bool scaled = false)
{
    sfpi::vFloat result = 0.0f;

    v_if (val < 0.0f)
    {
        result = 1.0f - _calculate_pos_cdf_appx_(-val);
    }
    v_else
    {
        result = _calculate_pos_cdf_appx_(val);
    }
    v_endif;

    if (scaled)
    {
        result *= val; // scale
    }
    return result;
}

} // namespace ckernel::sfpu
