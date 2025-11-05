// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace ckernel::sfpu
{

template <typename T>
constexpr T POLYVAL5(T coef4, T coef3, T coef2, T coef1, T coef0, T val)
{
    return (((((coef4 * val) + coef3) * val + coef2) * val + coef1) * val + coef0);
}

template <typename T>
constexpr T POLYVAL7(T coef6, T coef5, T coef4, T coef3, T coef2, T coef1, T coef0, T val)
{
    return (((((((coef6 * val) + coef5) * val + coef4) * val + coef3) * val + coef2) * val + coef1) * val + coef0);
}

/**
 * @brief Compile-time polynomial evaluator using Horner's method.
 *
 * This template struct provides efficient polynomial evaluation at both compile-time
 * and runtime using Horner's method (synthetic division).
 * This implementation uses a recursive varadic template function to compute the polynomial value.
 *
 * The polynomial is represented by coefficients in ascending order of powers:
 * coef[0] + coef[1]*x + coef[2]*x^2 + ... + coef[N-1]*x^(N-1)
 *
 * @note Uses Horner's method for numerical stability and O(N) complexity.
 * @note For N <= 0, returns T{0}. For N == 1, returns the constant term.
 *
 * @see https://en.wikipedia.org/wiki/Horner%27s_method
 */
struct PolynomialEvaluator
{
    /**
     * @brief Evaluates the polynomial at the given point.
     *
     * @param x The point at which to evaluate the polynomial
     * @param first First coefficient in the polynomial.
     * @param rest Rest of the polynomial coefficients in ascending order of powers.
     * @return The value of the polynomial at the given point
     *
     * @note Coefficients can be either float, sfpi::vFloat, ... (scalar and sfpi typed arguments can be mixed)
     */
    template <typename U, typename First, typename... Rest>
    static constexpr auto eval(U x, First first, Rest... rest)
    {
        if constexpr (sizeof...(Rest) == 0)
        {
            // Last coefficient: We don't want to call function
            // recursively (unsure if SFPU addition by 0 would be optimized away)
            // which is why we handled sizeof..(Rest) == 0 separately
            return first;
        }
        else
        {
            // Recursive case: Horner's method
            return first + x * eval(x, rest...);
        }
    }
};

} // namespace ckernel::sfpu
