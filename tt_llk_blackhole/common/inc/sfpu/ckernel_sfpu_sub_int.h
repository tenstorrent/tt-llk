// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_sub_int.h
 * @brief Integer subtraction operation for SFPU hardware
 *
 * @details This file implements integer subtraction using SFPU hardware acceleration
 * with support for multiple integer formats and representation modes. The implementation
 * provides efficient element-wise integer subtraction across 32-lane SIMD data with
 * configurable precision and sign representation.
 *
 * **Mathematical Operation:**
 * - **Integer Subtraction**: result = a - b (integer semantics)
 * - **SIMD Processing**: 32 integer pairs subtracted simultaneously
 * - **Multiple Formats**: Support for different integer bit widths
 * - **Sign Handling**: Configurable sign-magnitude vs. two's complement
 *
 * **Format Support:**
 * The implementation supports multiple integer representations:
 * - **Two's Complement**: Standard signed integer representation
 * - **Sign-Magnitude**: Separate sign and magnitude representation
 * - **Configurable Width**: Support for different integer bit widths
 * - **Load/Store Modes**: Flexible data movement with format conversion
 *
 * **Template Parameters:**
 * - **APPROXIMATION_MODE**: Precision control (not applicable for exact integer ops)
 * - **ITERATIONS**: Number of tiles to process in batched operation
 * - **INSTRUCTION_MODE**: Load/store instruction format selection
 * - **SIGN_MAGNITUDE_FORMAT**: Choose between sign-magnitude and two's complement
 *
 * **SFPU Implementation:**
 * 1. **Input Loading**: Load integer operands with format-specific interpretation
 * 2. **Subtraction**: Use SFPU arithmetic with proper sign handling
 * 3. **Format Conversion**: Convert between representation formats as needed
 * 4. **Result Storage**: Store results with appropriate format encoding
 *
 * **Performance Characteristics:**
 * - **Latency**: 2-4 cycles per tile depending on format conversions
 * - **Throughput**: 32 integer subtractions per cycle
 * - **Precision**: Exact integer arithmetic with proper overflow handling
 * - **Flexibility**: Runtime format selection via template parameters
 *
 * **Use Cases:**
 * - Neural network quantized operations
 * - Index calculations and offset arithmetic
 * - Integer-based difference computations
 * - Mixed-precision numerical algorithms
 */

#pragma once

#include <type_traits>

#include "ckernel.h"
#include "ckernel_addrmod.h"
#include "ckernel_ops.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS, InstrModLoadStore INSTRUCTION_MODE, bool SIGN_MAGNITUDE_FORMAT>
inline void _sub_int_(const uint dst_offset)
{
    // Operand A is input1 (int32/uint16)
    // Operand B is input2 (int32/uint16)
    // Output is int32/uint16
    static_assert(is_valid_instruction_mode(INSTRUCTION_MODE), "INSTRUCTION_MODE must be one of: INT32_2S_COMP, INT32, LO16.");

    // INSTRUCTION_MODE = InstrModLoadStore::INT32_2S_COMP enables LOAD/STORE operations to convert INT32 sign-magnitude to 2's complement.
    // However, in Blackhole, this mode has no effect and the data format remains unchanged.

    // If LOAD/STORE have the value in INT sign-magnitude format and SFPU needs it as 2's complement.
    constexpr auto INSTR_MOD_CAST = InstrModCast::INT_SIGN_MAGN_TO_INT32_2S_COMP;

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        // operand B
        TT_SFPLOAD(p_sfpu::LREG0 /*lreg*/, INSTRUCTION_MODE, ADDR_MOD_7, dst_offset * 64 /*dest_reg_addr */);
        if constexpr (SIGN_MAGNITUDE_FORMAT)
        {
            apply_sign_magnitude_conversion(p_sfpu::LREG0, p_sfpu::LREG2, INSTR_MOD_CAST);
        }

        // operand A
        TTI_SFPLOAD(p_sfpu::LREG1 /*lreg*/, INSTRUCTION_MODE, ADDR_MOD_7, 0);
        if constexpr (SIGN_MAGNITUDE_FORMAT)
        {
            apply_sign_magnitude_conversion(p_sfpu::LREG1, p_sfpu::LREG2, INSTR_MOD_CAST);
        }

        // Set instruction modifier to 6 to get B's 2's complement
        TTI_SFPIADD(0 /*imm*/, p_sfpu::LREG1 /*lreg_c*/, p_sfpu::LREG0 /*lreg_dest*/, 6 /*imod*/);

        // LREG_0 -> dest
        if constexpr (SIGN_MAGNITUDE_FORMAT)
        {
            apply_sign_magnitude_conversion(p_sfpu::LREG0, p_sfpu::LREG1, INSTR_MOD_CAST);
        }
        TTI_SFPSTORE(p_sfpu::LREG0, INSTRUCTION_MODE, ADDR_MOD_7, 0);
        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
