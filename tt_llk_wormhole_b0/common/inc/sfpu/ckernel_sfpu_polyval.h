// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <utility>

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
 * This implementation uses fold expressions to compute the polynomial value.
 *
 * The polynomial is represented by coefficients in ascending order of powers:
 * coef[0] + coef[1]*x + coef[2]*x^2 + ... + coef[N-1]*x^(N-1)
 *
 * @note Uses Horner's method for numerical stability and O(N) complexity.
 * @note For N == 0, returns T{0}. For N == 1, returns the constant term.
 *
 * @see https://en.wikipedia.org/wiki/Horner%27s_method
 */
struct PolynomialEvaluator
{
private:
    // Helper to apply Horner's method with fold expressions
    // Process coefficients in reverse order: c_n, c_(n-1), ..., c_1, c_0
    template <typename U, typename... Coefficients, std::size_t... Is>
    sfpi_inline static constexpr auto eval_impl(U x, std::index_sequence<Is...>, Coefficients... coeffs)
    {
        // Store coefficients in an array for reverse indexing
        U coeff_array[]         = {static_cast<U>(coeffs)...};
        constexpr std::size_t N = sizeof...(Coefficients);

        // Start with highest degree coefficient
        U acc = coeff_array[N - 1];

        // Fold expression: process remaining coefficients in reverse order
        // For each coefficient from N-2 down to 0: acc = acc * x + coeff[i]
        ((acc = acc * x + coeff_array[N - 2 - Is]), ...);

        return acc;
    }

public:
    /**
     * @brief Evaluates the polynomial at the given point.
     *
     * @param x The point at which to evaluate the polynomial
     * @param coeffs Polynomial coefficients in ascending order of powers (c0, c1, c2, ..., cn)
     * @return The value of the polynomial at x using Horner's method
     *
     * @note Coefficients can be either float, sfpi::vFloat, ... (scalar and sfpi typed arguments can be mixed)
     * @note For polynomial c0 + c1*x + c2*x^2 + c3*x^3, computes: c0 + x*(c1 + x*(c2 + x*c3))
     */

    // Base case: f(x) = 0 (empty polynomial)
    template <typename U>
    sfpi_inline static constexpr auto eval(U x)
    {
        return U {0};
    }

    // One or more coefficients: use Horner's method with fold expressions
    template <typename U, typename Coefficient0, typename... Rest>
    sfpi_inline static constexpr auto eval(U x, Coefficient0 coeff0, Rest... rest)
    {
        constexpr std::size_t N = sizeof...(Rest) + 1;
        return eval_impl(x, std::make_index_sequence<N - 1> {}, coeff0, rest...);
    }
};

} // namespace ckernel::sfpu
