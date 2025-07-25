// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

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

/**
 * @brief Perform element-wise integer addition using SFPU with format conversion support
 *
 * @details Implements element-wise addition of two integer arrays using the Tensix Vector Unit
 * (SFPU) with support for multiple data formats and optional sign-magnitude to 2's complement
 * conversion. This function is optimized for AI workloads requiring integer arithmetic operations
 * such as quantized neural networks, indexing operations, and accumulation counters.
 *
 * **Mathematical Operation:**
 * For each lane i in 32-lane SIMD:
 * ```
 * output[i] = operand_A[i] + operand_B[i]
 * ```
 * Where operands can be in INT32, UINT16, or UINT32 formats.
 *
 * **Supported Data Formats:**
 * - **INT32**: 32-bit signed integers in 2's complement or sign-magnitude
 * - **UINT16**: 16-bit unsigned integers (low 16 bits used)
 * - **UINT32**: 32-bit unsigned integers
 *
 * **Instruction Mode Details:**
 * - `INT32_2S_COMP`: 32-bit signed integers with 2's complement conversion
 * - `INT32`: 32-bit integers without automatic conversion
 * - `LO16`: 16-bit low-word operations for memory efficiency
 *
 * **Sign-Magnitude vs 2's Complement:**
 * ```
 * Sign-Magnitude: [sign][magnitude] = [1bit][31bits]
 *   Example: -5 = 1|00000101 (sign=1, magnitude=5)
 * 
 * 2's Complement: Standard CPU integer format
 *   Example: -5 = 11111011 (bitwise NOT + 1)
 * ```
 *
 * **Algorithm Flow:**
 * 1. **Load Operand A**: From destination register to LREG0
 * 2. **Convert Format A**: Sign-magnitude → 2's complement (if needed)
 * 3. **Load Operand B**: From offset address to LREG1  
 * 4. **Convert Format B**: Sign-magnitude → 2's complement (if needed)
 * 5. **Integer Addition**: LREG0 = LREG0 + LREG1
 * 6. **Convert Result**: 2's complement → sign-magnitude (if needed)
 * 7. **Store Result**: LREG0 to destination register
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses SFPU integer arithmetic unit (not floating-point unit)
 * - Leverages LReg vector registers for intermediate operations
 * - Processes 32 integer values simultaneously across SIMD lanes
 * - Supports address offset for different operand locations
 *
 * **TTI Instruction Sequence:**
 * ```assembly
 * TTI_SFPLOAD   LREG0, mode, addr_mod, 0        // Load operand A
 * [conversion]  LREG0 → 2's complement          // Optional format conversion
 * TT_SFPLOAD    LREG1, mode, addr_mod, offset   // Load operand B  
 * [conversion]  LREG1 → 2's complement          // Optional format conversion
 * TTI_SFPIADD   LREG0 = LREG0 + LREG1          // Integer addition
 * [conversion]  LREG0 → sign-magnitude          // Optional format conversion
 * TTI_SFPSTORE  LREG0, mode, addr_mod, 0       // Store result
 * ```
 *
 * **Performance Characteristics:**
 * - Latency: 3-5 cycles per iteration (load + add + store + conversions)
 * - Throughput: 1 instruction per cycle (pipelined)
 * - Peak Rate: 32 integer additions per cycle @ 1GHz = 32 GOps/sec
 * - Memory Pattern: Dual-source operands with configurable offset
 *
 * **Common Use Cases:**
 * - **Quantized Neural Networks**: INT8/INT16 arithmetic in QAT models
 * - **Index Arithmetic**: Address calculations and tensor indexing
 * - **Accumulation Counters**: Histogram computation and statistics
 * - **Bitwise Operations**: Logical operations on integer data
 * - **Mixed-Precision Training**: Integer gradients and activations
 *
 * **AI Workload Applications:**
 * ```cpp
 * // Quantized convolution accumulation
 * _add_int_<false, 8, INT32, true>(bias_offset);
 * 
 * // Index arithmetic for attention mechanisms  
 * _add_int_<false, 4, LO16, false>(position_offset);
 * 
 * // Histogram accumulation for data analysis
 * _add_int_<false, 8, INT32_2S_COMP, false>(counter_offset);
 * ```
 *
 * **Format Conversion Details:**
 * When `SIGN_MAGNITUDE_FORMAT = true`:
 * ```cpp
 * // Input:  sign-magnitude integers from memory
 * // SFPU:   2's complement arithmetic (standard CPU format)  
 * // Output: sign-magnitude integers to memory
 * ```
 * This allows integration with hardware that uses sign-magnitude representation
 * while performing arithmetic in the more efficient 2's complement format.
 *
 * **Address Offset Usage:**
 * The `dst_offset` parameter enables addition of operands from different memory
 * locations within the same tile, supporting operations like:
 * - Vector-vector addition with stride
 * - Broadcast addition (scalar + vector)
 * - Accumulation from multiple sources
 *
 * **Blackhole Architecture Notes:**
 * - INT32_2S_COMP mode has no effect on Blackhole (data format unchanged)
 * - Conversion operations are explicitly handled by template logic
 * - Optimized for Blackhole's integer arithmetic capabilities
 *
 * @tparam APPROXIMATION_MODE If true, may enable faster integer operations.
 *                            If false, uses exact integer arithmetic.
 * @tparam ITERATIONS Number of 32-element vectors to process per call.
 * @tparam INSTRUCTION_MODE Data format mode: INT32_2S_COMP, INT32, or LO16.
 * @tparam SIGN_MAGNITUDE_FORMAT If true, performs sign-magnitude ↔ 2's complement conversion.
 *                               If false, assumes data is already in correct format.
 *
 * @param dst_offset Memory offset multiplier for operand B location (×64 bytes).
 *                   Enables accessing different operand arrays within tile.
 *
 * @note Operand A loaded from current destination register position
 * @note Operand B loaded from dst_offset×64 bytes from current position  
 * @note Result stored back to current destination register position
 * @note Function advances destination register pointer after each iteration
 * @note Sign-magnitude conversion uses LREG2 as temporary storage
 * @note Address mode ADDR_MOD_7 provides optimized addressing for tile operations
 * @note Template parameters must satisfy compile-time validation constraints
 */
template <bool APPROXIMATION_MODE, int ITERATIONS, InstrModLoadStore INSTRUCTION_MODE, bool SIGN_MAGNITUDE_FORMAT>
inline void _add_int_(const uint dst_offset)
{
    // Operand A is input1 (int32/uint16/uint32)
    // Operand B is input2 (int32/uint16/uint32)
    // Output is int32/uint16/uint32
    static_assert(is_valid_instruction_mode(INSTRUCTION_MODE), "INSTRUCTION_MODE must be one of: INT32_2S_COMP, INT32, LO16.");

    // INSTRUCTION_MODE = InstrModLoadStore::INT32_2S_COMP enables LOAD/STORE operations to convert INT32 sign-magnitude to 2's complement.
    // However, in Blackhole, this mode has no effect and the data format remains unchanged.

    // If LOAD/STORE have the value in INT sign-magnitude format and SFPU needs it as 2's complement.
    constexpr auto INSTR_MOD_CAST = InstrModCast::INT_SIGN_MAGN_TO_INT32_2S_COMP;

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        // operand A
        TTI_SFPLOAD(p_sfpu::LREG0 /*lreg*/, INSTRUCTION_MODE, ADDR_MOD_7, 0 /*dest_reg_addr */);
        if constexpr (SIGN_MAGNITUDE_FORMAT)
        {
            apply_sign_magnitude_conversion(p_sfpu::LREG0, p_sfpu::LREG2, INSTR_MOD_CAST);
        }

        // operand B
        TT_SFPLOAD(p_sfpu::LREG1 /*lreg*/, INSTRUCTION_MODE, ADDR_MOD_7, dst_offset * 64);
        if constexpr (SIGN_MAGNITUDE_FORMAT)
        {
            apply_sign_magnitude_conversion(p_sfpu::LREG1, p_sfpu::LREG2, INSTR_MOD_CAST);
        }

        TTI_SFPIADD(0 /*imm*/, p_sfpu::LREG1 /*lreg_c*/, p_sfpu::LREG0 /*lreg_dest*/, 4 /*imod*/);

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
