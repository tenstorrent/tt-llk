// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_polyval.h
 * @brief Polynomial evaluation utilities for SFPU mathematical functions
 *
 * @details This file provides efficient polynomial evaluation functions optimized
 * for SFPU hardware. Polynomial approximation is a fundamental technique used
 * throughout SFPU mathematical functions to achieve high accuracy with minimal
 * computational overhead.
 *
 * **Mathematical Foundation:**
 * Implements Horner's method for polynomial evaluation:
 * - P(x) = ((((a₄x + a₃)x + a₂)x + a₁)x + a₀)
 * - Minimizes multiplication operations for optimal SFPU performance
 * - Maintains numerical stability through careful coefficient ordering
 *
 * **POLYVAL5 Function:**
 * - **5th-degree polynomial**: Supports polynomials up to degree 4
 * - **Template-based**: Type-generic for float/double/vector types
 * - **Horner's Method**: Optimal evaluation order for performance
 * - **SFPU Optimized**: Designed for SFPU instruction scheduling
 *
 * **Performance Benefits:**
 * - **Minimal Operations**: Only 4 multiplications + 4 additions for 5th-degree
 * - **Numerical Stability**: Horner's method reduces roundoff error accumulation
 * - **SFPU Efficiency**: Maps well to SFPU MAD (multiply-add) instructions
 * - **Template Optimization**: Compile-time optimization for specific types
 *
 * **Usage Pattern:**
 * ```cpp
 * // Evaluate polynomial: 2x⁴ + 3x³ + 4x² + 5x + 6
 * float result = POLYVAL5(2.0f, 3.0f, 4.0f, 5.0f, 6.0f, x);
 * 
 * // Vector evaluation with SFPI types
 * sfpi::vFloat result = POLYVAL5(c4, c3, c2, c1, c0, input_vector);
 * ```
 *
 * **Integration with SFPU Functions:**
 * This utility is used throughout SFPU mathematical functions:
 * - Exponential approximations
 * - Trigonometric functions  
 * - Logarithmic computations
 * - Activation function implementations
 */

#pragma once

namespace ckernel::sfpu
{

template <typename T>
constexpr T POLYVAL5(T coef4, T coef3, T coef2, T coef1, T coef0, T val)
{
    return (((((coef4 * val) + coef3) * val + coef2) * val + coef1) * val + coef0);
}
} // namespace ckernel::sfpu
