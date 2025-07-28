// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_fill.h
 * @brief Fill/constant assignment SFPU kernels for Wormhole B0 Tensix Special Function Processing Unit
 * 
 * @details This header provides hardware-accelerated constant value assignment using the Wormhole B0
 * SFPU's 32-lane SIMD architecture. The SFPU enables efficient broadcasting of constant values
 * across DEST registers with support for multiple data types (float, integer, bitcast) and
 * optimized constant loading mechanisms for high-throughput initialization operations.
 * 
 * **Mathematical Operations:**
 * - **Float Fill**: f(x) = constant (floating-point constant assignment)
 * - **Integer Fill**: f(x) = constant (integer constant assignment)
 * - **Bitcast Fill**: f(x) = constant (bit-pattern reinterpretation assignment)
 * 
 * **Wormhole B0 SFPU Constant Broadcasting:**
 * - **32-Lane SIMD**: Parallel constant assignment across all SFPU lanes within current register
 * - **Constant Loading**: Optimized mechanisms for loading immediate values into SFPU
 * - **Type Support**: Float, integer, and bitcast constant assignments
 * - **LREG Utilization**: Uses SFPU local registers (LREG) for efficient constant storage
 * - **Store Instructions**: Direct SFPU store operations for integer constant assignment
 * 
 * **Implementation Variants:**
 * 
 * **1. Float Fill**
 * Direct floating-point constant assignment:
 * ```cpp
 * sfpi::vFloat fill_val = value;    // Load constant into SFPU register
 * sfpi::dst_reg[0] = fill_val;      // Broadcast to all 32 lanes
 * ```
 * 
 * **2. Integer Fill**
 * Integer constant assignment via LREG and store operations:
 * ```cpp
 * _sfpu_load_imm32_(p_sfpu::LREG1, value);                    // Load into LREG1
 * TTI_SFPSTORE(p_sfpu::LREG1, INT32, ADDR_MOD_3, 0);         // Store to DEST
 * ```
 * 
 * **3. Bitcast Fill**
 * Bit-pattern reinterpretation for custom constant patterns:
 * ```cpp
 * sfpi::vFloat fill_val = Converter::as_float(value_bit_mask); // Bitcast to float
 * sfpi::dst_reg[0] = fill_val;                                 // Broadcast pattern
 * ```
 * 
 * **Hardware Context:**
 * - **SFPI Interface**: Direct constant loading and assignment operations
 * - **LREG System**: 8 local registers (LREGs) for efficient constant storage
 * - **Store Operations**: Dedicated SFPU store instructions for type-specific assignment
 * - **Converter Utility**: Safe type conversion and bitcast operations
 * - **Address Modification**: ADDR_MOD_3 for optimized register advancement
 * 
 * **Performance Characteristics:**
 * - **Throughput**: 32 constant assignments per clock cycle (within one register)
 * - **Latency**: Single-cycle latency per DEST register for float/bitcast fill
 * - **Integer Overhead**: Additional cycle for LREG load and store operations
 * - **Memory Efficiency**: Efficient constant broadcasting without memory bandwidth
 * 
 * **Constant Loading Mechanisms:**
 * - **Immediate Load**: Direct constant loading for floating-point values
 * - **LREG Loading**: _sfpu_load_imm32_ for 32-bit integer immediate values
 * - **Type Conversion**: Safe bitcast operations for arbitrary bit patterns
 * - **Hardware Optimization**: Leverages SFPU constant register capabilities
 * 
 * **Common Use Cases:**
 * - **Neural Networks**: Weight initialization and bias setting
 * - **Mathematical Operations**: Creating constant tensors and identity matrices
 * - **Signal Processing**: DC offset addition and signal baseline setting
 * - **Computer Graphics**: Background color fill and texture initialization
 * - **Testing**: Creating known test patterns and validation data
 * 
 * **Type Support and Conversion:**
 * - **Float Constants**: Direct IEEE-754 floating-point assignment
 * - **Integer Constants**: 32-bit signed/unsigned integer assignment
 * - **Bit Patterns**: Arbitrary 32-bit patterns via bitcast mechanism
 * - **Format Preservation**: Maintains target format characteristics during assignment
 * 
 * **Integration with Wormhole B0:**
 * - **Thread 1 Execution**: Optimized for Math thread execution context
 * - **DEST Register Management**: Integrates with register file double-buffering
 * - **Pipeline Integration**: Maintains efficient Unpack→Math→Pack data flow
 * - **Format Flexibility**: Supports all DEST register data format configurations
 */

#pragma once

#include "ckernel_ops.h"
#include "ckernel_sfpu_converter.h"
#include "ckernel_sfpu_load_config.h"

namespace ckernel::sfpu
{

/**
 * @brief Hardware-accelerated floating-point constant fill using Wormhole B0 SFPU
 * @tparam APPROXIMATION_MODE Unused for exact constant assignment
 * @tparam ITERATIONS Compile-time loop unrolling factor for performance optimization
 * @param value Floating-point constant value to broadcast to all DEST registers
 * 
 * @details Assigns a constant floating-point value to DEST registers using the Wormhole B0
 * SFPU's 32-lane SIMD architecture. The operation broadcasts the constant value across
 * all 32 lanes within each register, providing efficient initialization and constant
 * tensor creation capabilities.
 * 
 * **Wormhole B0 SFPU Float Constant Broadcasting:**
 * - **Immediate Loading**: Direct constant loading into SFPU register
 * - **32-Lane Broadcast**: Simultaneous assignment across all lanes within current register
 * - **Single-Cycle**: One clock cycle per register through SFPU pipeline
 * - **IEEE-754 Preserving**: Maintains floating-point format and precision
 * 
 * **SFPI Programming Model:**
 * ```cpp
 * sfpi::vFloat fill_val = value;        // Load constant into SFPU register
 * sfpi::dst_reg[0] = fill_val;          // Broadcast to all 32 lanes
 * sfpi::dst_reg++;                      // Advance to next register
 * ```
 * 
 * @note This function operates in-place on DEST registers
 * @note Broadcasts single constant value to all lanes within each register
 * @note Optimized for floating-point constant initialization
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_fill_(const float value)
{
    // SFPU microcode
    sfpi::vFloat fill_val = value;                     // Load constant into SFPU register

    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::dst_reg[0] = fill_val;                   // Broadcast to all 32 lanes
        sfpi::dst_reg++;                               // Advance to next register
    }
}

/**
 * @brief Hardware-accelerated integer constant fill using Wormhole B0 SFPU LREG system
 * @tparam APPROXIMATION_MODE Unused for exact constant assignment
 * @tparam ITERATIONS Compile-time loop unrolling factor for performance optimization
 * @param value Integer constant value to assign to all DEST registers
 * 
 * @details Assigns a constant integer value to DEST registers using the Wormhole B0 SFPU's
 * local register (LREG) system and dedicated store operations. The implementation uses
 * LREG1 for constant storage and SFPSTORE instructions for efficient integer assignment
 * across all 32 lanes.
 * 
 * **Wormhole B0 SFPU Integer Constant Processing:**
 * - **LREG Loading**: _sfpu_load_imm32_ loads 32-bit immediate into LREG1
 * - **Store Operations**: TTI_SFPSTORE transfers from LREG to DEST registers
 * - **Type Preservation**: Maintains integer format and bit patterns
 * - **Address Modification**: ADDR_MOD_3 optimizes register advancement
 * 
 * **SFPI Programming Model:**
 * ```cpp
 * _sfpu_load_imm32_(p_sfpu::LREG1, value);          // Load constant into LREG1
 * TTI_SFPSTORE(p_sfpu::LREG1, INT32, ADDR_MOD_3, 0); // Store to DEST register
 * sfpi::dst_reg++;                                   // Advance to next register
 * ```
 * 
 * @note Uses SFPU local register (LREG1) for constant storage
 * @note Requires additional cycle for LREG load and store operations
 * @note Optimized for 32-bit integer constant initialization
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_fill_int_(const uint value)
{
    // SFPU microcode
    _sfpu_load_imm32_(p_sfpu::LREG1, value);           // Load constant into LREG1

    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPSTORE(p_sfpu::LREG1, InstrModLoadStore::INT32, ADDR_MOD_3, 0); // Store to DEST
        sfpi::dst_reg++;                               // Advance to next register
    }
}

/**
 * @brief Hardware-accelerated bitcast constant fill using Wormhole B0 SFPU type conversion
 * @tparam APPROXIMATION_MODE Unused for exact constant assignment
 * @tparam ITERATIONS Compile-time loop unrolling factor for performance optimization
 * @param value_bit_mask 32-bit pattern to reinterpret as floating-point and broadcast
 * 
 * @details Assigns a constant bit pattern to DEST registers by reinterpreting the integer
 * bit pattern as a floating-point value and broadcasting it using the Wormhole B0 SFPU's
 * type conversion utilities. This enables arbitrary bit pattern initialization without
 * format constraints.
 * 
 * **Wormhole B0 SFPU Bitcast Processing:**
 * - **Type Conversion**: Converter::as_float safely reinterprets bit patterns
 * - **Pattern Preservation**: Maintains exact bit patterns during conversion
 * - **32-Lane Broadcast**: Simultaneous assignment across all lanes within current register
 * - **Format Agnostic**: Works with arbitrary 32-bit patterns regardless of validity
 * 
 * **SFPI Programming Model:**
 * ```cpp
 * sfpi::vFloat fill_val = Converter::as_float(value_bit_mask); // Bitcast to float
 * sfpi::dst_reg[0] = fill_val;                                 // Broadcast pattern
 * sfpi::dst_reg++;                                             // Advance to next register
 * ```
 * 
 * **Use Cases:**
 * - **Custom Patterns**: Arbitrary bit patterns for testing and validation
 * - **Special Values**: NaN, infinity, or denormal value initialization
 * - **Bit Manipulation**: Creating specific IEEE-754 representations
 * - **Format Testing**: Validating behavior with edge-case bit patterns
 * 
 * @note Uses safe type conversion to avoid undefined behavior
 * @note Preserves exact bit patterns during reinterpretation
 * @note Enables initialization with arbitrary 32-bit patterns
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_fill_bitcast_(const uint32_t value_bit_mask)
{
    // SFPU microcode
    sfpi::vFloat fill_val = Converter::as_float(value_bit_mask); // Bitcast to float

    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::dst_reg[0] = fill_val;                   // Broadcast bit pattern
        sfpi::dst_reg++;                               // Advance to next register
    }
}
} // namespace ckernel::sfpu
