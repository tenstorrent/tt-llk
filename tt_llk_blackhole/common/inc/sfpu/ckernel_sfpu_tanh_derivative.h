// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_tanh_derivative.h
 * @brief Hyperbolic tangent derivative function for SFPU hardware
 *
 * @details This file implements the derivative of the hyperbolic tangent function
 * using SFPU hardware acceleration. The tanh derivative is commonly used in
 * neural network backpropagation and gradient computation, particularly for
 * networks using tanh activation functions.
 *
 * **Mathematical Definition:**
 * - **Tanh Derivative**: f'(x) = 1 - tanh²(x)
 * - **Alternative Form**: f'(x) = sech²(x) = 1/cosh²(x)  
 * - **Range**: (0, 1] with maximum value of 1 at x = 0
 * - **Properties**: Always positive, symmetric around x = 0
 *
 * **SFPU Implementation:**
 * The implementation leverages the mathematical identity tanh'(x) = 1 - tanh²(x):
 * 1. **Input Processing**: Handle pre-computed tanh or compute tanh via LUT
 * 2. **Square Computation**: Calculate tanh²(x) using SFPU multiply
 * 3. **Subtraction**: Compute 1 - tanh²(x) using SFPU arithmetic
 * 4. **Result Storage**: Store derivative value to destination register
 *
 * **Template Parameters:**
 * - **APPROXIMATION_MODE**: Precision control for tanh computation
 * - **WITH_PRECOMPUTED_TANH**: Optimization flag for pre-computed tanh values
 *   - `true`: Input is already tanh(x), skip tanh computation
 *   - `false`: Compute tanh(x) first using lookup table
 * - **ITERATIONS**: Number of tiles to process in batched operation
 *
 * **Lookup Table Integration:**
 * - Uses SFPU lookup table registers (LReg0, LReg1, LReg2)
 * - Efficient tanh computation when input is not pre-computed
 * - Preserves and restores lookup table state for pipeline efficiency
 *
 * **Performance Characteristics:**
 * - **Latency**: 3-5 cycles per tile depending on precomputed flag
 * - **Throughput**: 32 elements processed per cycle
 * - **Accuracy**: High precision suitable for gradient computation
 * - **Memory**: Efficient use of SFPU registers and lookup tables
 *
 * **Common Use Cases:**
 * - Neural network backpropagation
 * - Gradient computation for tanh activation layers
 * - Mathematical optimization algorithms
 * - Automatic differentiation systems
 */

#pragma once

#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int WITH_PRECOMPUTED_TANH, int ITERATIONS>
inline void _calculate_tanh_derivative_(const int iterations)
{
    sfpi::vUInt l0 = sfpi::l_reg[sfpi::LRegs::LReg0];
    sfpi::vUInt l1 = sfpi::l_reg[sfpi::LRegs::LReg1];
    sfpi::vUInt l2 = sfpi::l_reg[sfpi::LRegs::LReg2];

    // tanh'(x) = 1 - (tanh(x))^2
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat val = sfpi::dst_reg[0];

        if constexpr (!WITH_PRECOMPUTED_TANH)
        {
            val = lut(val, l0, l1, l2);
        }

        val              = val * (-val) + sfpi::vConst1;
        sfpi::dst_reg[0] = val;

        sfpi::dst_reg++;
    }

    sfpi::l_reg[sfpi::LRegs::LReg0] = l0;
    sfpi::l_reg[sfpi::LRegs::LReg1] = l1;
    sfpi::l_reg[sfpi::LRegs::LReg2] = l2;
}

} // namespace sfpu
} // namespace ckernel
