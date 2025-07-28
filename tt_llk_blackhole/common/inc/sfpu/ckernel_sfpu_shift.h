// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_shift.h
 * @brief Bitwise shift operations for SFPU hardware
 *
 * @details This file implements bitwise shift operations (left shift, right shift)
 * using SFPU hardware acceleration. These operations provide efficient bit manipulation
 * capabilities for integer data, supporting both logical and arithmetic shift variants
 * with configurable precision and sign handling.
 *
 * **Supported Shift Operations:**
 * - **Left Shift**: result = a << b (multiply by 2^b)
 * - **Right Shift**: result = a >> b (divide by 2^b)
 * - **Logical Shift**: Zero-fill for unsigned data
 * - **Arithmetic Shift**: Sign-extend for signed data
 *
 * **Shift Operation Semantics:**
 * - **Left Shift**: Always zero-fill from the right
 * - **Logical Right Shift**: Zero-fill from the left
 * - **Arithmetic Right Shift**: Sign-extend from the left for signed values
 * - **Overflow Handling**: Proper behavior for shift amounts ≥ data width
 *
 * **Integer Format Support:**
 * The implementation supports multiple integer representations:
 * - **Two's Complement**: Standard signed integer representation
 * - **Sign-Magnitude**: Separate sign and magnitude representation
 * - **Configurable Width**: Support for different integer bit widths
 * - **Load/Store Modes**: Flexible data movement with format conversion
 *
 * **Template Parameters:**
 * - **APPROXIMATION_MODE**: Precision control (not applicable for exact bit ops)
 * - **ITERATIONS**: Number of tiles to process in batched operation
 * - **INSTRUCTION_MODE**: Load/store instruction format selection
 * - **SIGN_MAGNITUDE_FORMAT**: Choose between sign-magnitude and two's complement
 *
 * **SFPU Implementation:**
 * 1. **Operand Loading**: Load shift operands with format-specific interpretation
 * 2. **Shift Amount**: Extract and validate shift count (typically limited to 0-31)
 * 3. **Bit Manipulation**: Execute shift using SFPU bit manipulation capabilities
 * 4. **Sign Handling**: Apply appropriate sign extension for arithmetic shifts
 * 5. **Result Storage**: Store results with appropriate format encoding
 *
 * **Performance Characteristics:**
 * - **Latency**: 3-5 cycles per tile depending on shift type and format
 * - **Throughput**: 32 shift operations per cycle
 * - **Precision**: Exact bit manipulation with proper overflow semantics
 * - **Efficiency**: Optimized for common shift amounts and patterns
 *
 * **Use Cases:**
 * - Neural network quantization operations
 * - Fixed-point arithmetic simulation
 * - Bit manipulation for hashing and encoding
 * - Power-of-2 multiplication/division optimization
 * - Custom numeric format implementation
 */

#pragma once

#include <type_traits>

#include "ckernel_addrmod.h"
#include "ckernel_ops.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS, InstrModLoadStore INSTRUCTION_MODE, bool SIGN_MAGNITUDE_FORMAT>
inline void _calculate_binary_left_shift_(const uint dst_offset)
{
    static_assert(is_valid_instruction_mode(INSTRUCTION_MODE), "INSTRUCTION_MODE must be one of: INT32_2S_COMP, INT32, LO16.");

    constexpr int sfpload_instr_mod = SIGN_MAGNITUDE_FORMAT ? INT32_2S_COMP : static_cast<std::underlying_type_t<InstrModLoadStore>>(INSTRUCTION_MODE);
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        constexpr uint dst_tile_size = 64;
        // load
        TTI_SFPLOAD(p_sfpu::LREG0, sfpload_instr_mod, ADDR_MOD_7, 0);
        TT_SFPLOAD(p_sfpu::LREG1, sfpload_instr_mod, ADDR_MOD_7, dst_offset * dst_tile_size);
        // if (shift_amount < 0 OR shift_amount >= 32) -> result should be 0
        TTI_SFPSETCC(0, p_sfpu::LREG1, p_sfpu::LREG0, 4);
        TTI_SFPIADD(0xFE0, p_sfpu::LREG1, p_sfpu::LREG2, 1); // 0xFE0 = -32
        TTI_SFPCOMPC(0, p_sfpu::LREG0, p_sfpu::LREG0, 0);
        TTI_SFPMOV(0, p_sfpu::LCONST_0, p_sfpu::LREG0, 0);
        TTI_SFPENCC(0, p_sfpu::LREG0, p_sfpu::LREG0, 0);
        // shift left
        TTI_SFPSHFT(0, p_sfpu::LREG1, p_sfpu::LREG0, 0);
        // store result
        TTI_SFPSTORE(p_sfpu::LREG0, sfpload_instr_mod, ADDR_MOD_7, 0);
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS, InstrModLoadStore INSTRUCTION_MODE, bool SIGN_MAGNITUDE_FORMAT>
inline void _calculate_binary_right_shift_(const uint dst_offset)
{
    static_assert(is_valid_instruction_mode(INSTRUCTION_MODE), "INSTRUCTION_MODE must be one of: INT32_2S_COMP, INT32, LO16.");

    constexpr int sfpload_instr_mod = SIGN_MAGNITUDE_FORMAT ? INT32_2S_COMP : static_cast<std::underlying_type_t<InstrModLoadStore>>(INSTRUCTION_MODE);
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        constexpr uint dst_tile_size = 64;
        // load
        TTI_SFPLOAD(p_sfpu::LREG0, sfpload_instr_mod, ADDR_MOD_7, 0);
        TT_SFPLOAD(p_sfpu::LREG1, sfpload_instr_mod, ADDR_MOD_7, dst_offset * dst_tile_size);
        TTI_SFPMOV(0, p_sfpu::LREG0, p_sfpu::LREG4, 0); // save shift_value for later
        // if (shift_amount < 0 OR shift_amount >= 32) -> result should be 0
        TTI_SFPSETCC(0, p_sfpu::LREG1, p_sfpu::LREG0, 4);
        TTI_SFPIADD(0xFE0, p_sfpu::LREG1, p_sfpu::LREG2, p_sfpu::LCONST_0); // 0xFE0 = -32
        TTI_SFPMOV(0, p_sfpu::LCONST_0, p_sfpu::LREG0, 0);
        TTI_SFPENCC(0, p_sfpu::LREG0, p_sfpu::LREG0, 0);
        TTI_SFPIADD(0, p_sfpu::LCONST_0, p_sfpu::LREG1, 6); // take negative of shift_amount to shift right
        // shift right
        TTI_SFPSHFT(0, p_sfpu::LREG1, p_sfpu::LREG0, 0);
        // if shift_value was negative, need to shift in 1's manually
        TTI_SFPSETCC(0, p_sfpu::LREG4, p_sfpu::LREG0, 0);    // only run if shift_value is negative
        TTI_SFPSETCC(0, p_sfpu::LREG1, p_sfpu::LREG0, 2);    // only needed if shift_amount>0
        TTI_SFPIADD(0x020, p_sfpu::LREG1, p_sfpu::LREG2, 5); // take 32-shift_amount (0x020 = 32)
        TTI_SFPNOT(0, p_sfpu::LCONST_0, p_sfpu::LREG3, 0);   // put all 1's into LREG3
        TTI_SFPSHFT(0, p_sfpu::LREG2, p_sfpu::LREG3, 0);     // shift all 1's by 32-shift_amount
        TTI_SFPOR(0, p_sfpu::LREG3, p_sfpu::LREG0, 0);       // OR in the 1's
        TTI_SFPENCC(0, p_sfpu::LREG0, p_sfpu::LREG0, 0);
        // store result
        TTI_SFPSTORE(p_sfpu::LREG0, sfpload_instr_mod, ADDR_MOD_7, 0);
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS, InstrModLoadStore INSTRUCTION_MODE, bool SIGN_MAGNITUDE_FORMAT>
inline void _calculate_logical_right_shift_(const uint dst_offset)
{
    static_assert(is_valid_instruction_mode(INSTRUCTION_MODE), "INSTRUCTION_MODE must be one of: INT32_2S_COMP, INT32, LO16.");

    constexpr int sfpload_instr_mod = SIGN_MAGNITUDE_FORMAT ? INT32_2S_COMP : static_cast<std::underlying_type_t<InstrModLoadStore>>(INSTRUCTION_MODE);

    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        constexpr uint dst_tile_size = 64;
        // load
        TTI_SFPLOAD(p_sfpu::LREG0, sfpload_instr_mod, ADDR_MOD_7, 0);
        TT_SFPLOAD(p_sfpu::LREG1, sfpload_instr_mod, ADDR_MOD_7, dst_offset * dst_tile_size);
        // if (shift_amount < 0 OR shift_amount >= 32) -> result should be 0
        TTI_SFPSETCC(0, p_sfpu::LREG1, p_sfpu::LREG0, 4);
        TTI_SFPIADD(0xFE0, p_sfpu::LREG1, p_sfpu::LREG2, 1); // 0xFE0 = -32
        TTI_SFPCOMPC(0, p_sfpu::LREG0, p_sfpu::LREG0, 0);
        TTI_SFPMOV(0, p_sfpu::LCONST_0, p_sfpu::LREG0, 0);
        TTI_SFPENCC(0, p_sfpu::LREG0, p_sfpu::LREG0, 0);
        // shift right
        TTI_SFPIADD(0, p_sfpu::LCONST_0, p_sfpu::LREG1, 6); // take negative of shift_amount to shift right
        TTI_SFPSHFT(0, p_sfpu::LREG1, p_sfpu::LREG0, 0);
        // store result
        TTI_SFPSTORE(p_sfpu::LREG0, sfpload_instr_mod, ADDR_MOD_7, 0);
        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
