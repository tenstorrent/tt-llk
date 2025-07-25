// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace ckernel::sfpu
{

/**
 * @brief Simple type conversion utility for reinterpreting uint32_t as float
 *
 * @details Provides a utility function to reinterpret the bit pattern of a 32-bit
 * unsigned integer as an IEEE-754 single-precision floating-point number without
 * changing the underlying bits. This is commonly known as "type punning."
 *
 * **What This Does:**
 * Takes a `uint32_t` value and returns a `float` that has the exact same bit pattern.
 * No arithmetic conversion is performed - only reinterpretation of the bits.
 *
 * **Example:**
 * ```cpp
 * uint32_t bits = 0x3F800000;  // IEEE-754 representation of 1.0f
 * float value = Converter::as_float(bits);  // value = 1.0f
 * ```
 *
 * **Observed Usage:**
 * This converter is used in SFPU functions, such as in `ckernel_reverseops.h`:
 * ```cpp
 * template<bool APPROXIMATION_MODE, int ITERATIONS = 8>
 * inline void calculate_rsub(uint value) {
 *     vFloat arg2 = Converter::as_float(value);  // Convert parameter
 *     // ... use arg2 in arithmetic operations
 * }
 * ```
 *
 * **Implementation Method:**
 * Uses a C++ union to safely perform the type punning. Unions provide well-defined
 * behavior for this kind of bit reinterpretation according to the C++ standard.
 */
class Converter
{
public:
    /**
     * @brief Reinterpret uint32_t bit pattern as float
     *
     * @details Uses a union to safely reinterpret the bit pattern of a 32-bit
     * unsigned integer as an IEEE-754 single-precision floating-point number.
     * The same 32 bits are preserved exactly, but interpreted as a different type.
     *
     * **How It Works:**
     * 1. Create a union containing both uint32_t and float members
     * 2. Initialize the uint32_t member with the input value
     * 3. Return the float member (same bits, different type interpretation)
     *
     * **Basic IEEE-754 Example:**
     * ```
     * 0x3F800000 → 1.0f
     * 0x40000000 → 2.0f  
     * 0x00000000 → 0.0f
     * ```
     *
     * @param value 32-bit unsigned integer containing the bit pattern to reinterpret
     * @return float with the same bit pattern as the input
     *
     * @note No arithmetic conversion is performed - only bit reinterpretation
     * @note The input should represent a valid IEEE-754 bit pattern for meaningful results
     * @note Uses union-based approach which is safer than pointer casting
     */
    static float as_float(std::uint32_t value)
    {
        // Union provides safe type punning
        union
        {
            std::uint32_t u;  // Integer view of the bits
            float f;          // Float view of the same bits
        } converter {value};  // Initialize with input bit pattern

        return converter.f;   // Return float interpretation
    }
};

} // namespace ckernel::sfpu
