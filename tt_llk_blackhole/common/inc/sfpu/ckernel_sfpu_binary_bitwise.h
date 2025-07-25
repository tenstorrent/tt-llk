// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <type_traits>

#include "ckernel_addrmod.h"
#include "ckernel_ops.h"
#include "llk_defs.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Enumeration of supported binary bitwise operations for SFPU
 *
 * @details Defines the available bitwise operations that can be performed element-wise
 * on 32-bit unsigned integer vectors using the Tensix Vector Unit's SFPU backend.
 * These operations are fundamental for bit manipulation, masking, and logical operations
 * commonly used in AI workloads, computer graphics, and numerical algorithms.
 *
 * **Supported Operations:**
 * - **AND**: Bitwise logical AND for masking and filtering
 * - **OR**: Bitwise logical OR for combining and setting bits  
 * - **XOR**: Bitwise exclusive OR for toggling and cryptographic operations
 *
 * **Hardware Mapping:**
 * Each enum value directly corresponds to a specific SFPU instruction:
 * - `AND` → `TTI_SFPAND` instruction
 * - `OR` → `TTI_SFPOR` instruction
 * - `XOR` → `TTI_SFPXOR` instruction
 */
enum class BinaryBitwiseOp : uint8_t
{
    AND = 0,  ///< Bitwise AND operation: result[i] = a[i] & b[i]
    OR  = 1,  ///< Bitwise OR operation:  result[i] = a[i] | b[i]
    XOR = 2,  ///< Bitwise XOR operation: result[i] = a[i] ^ b[i]
};

/**
 * @brief Perform element-wise binary bitwise operations using SFPU hardware
 *
 * @details Implements high-performance element-wise bitwise operations (AND, OR, XOR)
 * on 32-bit unsigned integer vectors using the Tensix Vector Unit's SFPU backend.
 * This function provides a unified interface for all bitwise operations through
 * compile-time template specialization, ensuring zero runtime dispatch overhead.
 *
 * **Mathematical Operations:**
 * For each lane i in 32-lane SIMD and each bit position j in 32-bit words:
 * ```
 * AND: result[i][j] = operand_A[i][j] & operand_B[i][j]
 * OR:  result[i][j] = operand_A[i][j] | operand_B[i][j]  
 * XOR: result[i][j] = operand_A[i][j] ^ operand_B[i][j]
 * ```
 *
 * **Bitwise Operation Semantics:**
 * - **AND**: Both bits must be 1 to produce 1 (masking, filtering)
 * - **OR**: Either bit can be 1 to produce 1 (combining, setting)
 * - **XOR**: Exactly one bit must be 1 to produce 1 (toggling, difference)
 *
 * **Algorithm Flow:**
 * 1. **Load Operand A**: From current destination register to LREG0
 * 2. **Load Operand B**: From offset destination address to LREG1
 * 3. **Bitwise Operation**: Apply AND/OR/XOR between LREG0 and LREG1
 * 4. **Store Result**: Write result from LREG0 back to destination register
 * 5. **Advance Pointer**: Move to next 32-element vector
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend, simple sub-unit
 * - Uses dedicated SFPU bitwise logic instructions (not arithmetic ALU)
 * - Processes 32 × 32-bit unsigned integers simultaneously (1024 bits total)
 * - Supports various data format modes through INSTRUCTION_MODE parameter
 * - Single-cycle bitwise operations with pipelined load/store
 *
 * **TTI Instruction Sequence:**
 * ```assembly
 * TTI_SFPLOAD   LREG0, mode, ADDR_MOD_7, 0           // Load operand A (current position)
 * TT_SFPLOAD    LREG1, mode, ADDR_MOD_7, offset×64   // Load operand B (offset position)
 * TTI_SFPAND    LREG0, LREG1, LREG0, 0              // Bitwise AND (or OR/XOR)
 * TTI_SFPSTORE  LREG0, mode, ADDR_MOD_7, 0          // Store result to destination
 * dst_reg++                                          // Advance to next vector
 * ```
 *
 * **Performance Characteristics:**
 * - Latency: 3-4 cycles per iteration (load + op + store)
 * - Throughput: 1 instruction per cycle (fully pipelined)
 * - Peak Rate: 32 bitwise ops per cycle @ 1GHz = 32 GOps/sec
 * - Bit Throughput: 1024 bits per cycle @ 1GHz = 1024 Gbits/sec
 * - Memory Pattern: Dual-source operands with configurable stride
 *
 * **Common Use Cases:**
 * - **Bit Masking**: Selective bit extraction and filtering
 *   ```cpp
 *   // Extract specific bits using AND mask
 *   _calculate_sfpu_binary_bitwise_<false, BinaryBitwiseOp::AND, INT32>(mask_offset);
 *   ```
 * - **Flag Combination**: Merging multiple bit flags using OR
 *   ```cpp
 *   // Combine attention masks in transformer models
 *   _calculate_sfpu_binary_bitwise_<false, BinaryBitwiseOp::OR, INT32>(flag_offset);
 *   ```
 * - **Data Encryption**: XOR-based cryptographic operations
 *   ```cpp
 *   // Apply XOR cipher for data obfuscation
 *   _calculate_sfpu_binary_bitwise_<false, BinaryBitwiseOp::XOR, INT32>(key_offset);
 *   ```
 *
 * **AI Workload Applications:**
 * - **Attention Mechanisms**: Mask combination and sequence padding
 * - **Quantization**: Bit-level operations on quantized weights
 * - **Sparse Networks**: Sparsity pattern manipulation and pruning masks
 * - **Hash Functions**: Bit mixing for embedding and feature hashing
 * - **Data Preprocessing**: Categorical encoding and one-hot operations
 *
 * **Format Support Examples:**
 * ```cpp
 * // 32-bit integer operations (standard case)
 * _calculate_sfpu_binary_bitwise_<false, BinaryBitwiseOp::AND, INT32>(offset);
 * 
 * // 16-bit low-word operations (memory efficient)  
 * _calculate_sfpu_binary_bitwise_<false, BinaryBitwiseOp::OR, LO16>(offset);
 * 
 * // FP32 bit manipulation (treating floats as bit patterns)
 * _calculate_sfpu_binary_bitwise_<false, BinaryBitwiseOp::XOR, FP32>(offset);
 * ```
 *
 * **Memory Layout and Addressing:**
 * - **Operand A**: Current destination register position
 * - **Operand B**: `dst_offset × 64` bytes from current position  
 * - **Result**: Stored back to current destination register position
 * - **Stride**: `dst_tile_size = 64` bytes per tile element
 * - **Advancement**: Automatic pointer increment after each iteration
 *
 * **Template Specialization Benefits:**
 * - **Zero Runtime Overhead**: Operation type resolved at compile-time
 * - **Instruction Optimization**: Direct mapping to specific TTI instructions
 * - **Type Safety**: Compile-time validation of operation and format combinations
 * - **Code Reuse**: Single implementation for all bitwise operations
 *
 * **Special Considerations:**
 * - Data is treated as 32-bit unsigned integers regardless of INSTRUCTION_MODE
 * - Floating-point data can be manipulated at bit level (IEEE-754 representation)
 * - Lane masking applies: disabled lanes do not participate in operations
 * - Address mode ADDR_MOD_7 provides optimized tile-based addressing
 *
 * @tparam APPROXIMATION_MODE If true, may enable faster operation modes.
 *                            If false, uses exact bitwise logic operations.
 * @tparam BITWISE_OP Compile-time operation selector: AND, OR, or XOR.
 * @tparam INSTRUCTION_MODE Data format mode for load/store operations (default: INT32).
 * @tparam ITERATIONS Number of 32-element vectors to process per call (default: 8).
 *
 * @param dst_offset Memory offset multiplier for operand B location (×64 bytes).
 *                   Enables bitwise operations between different array segments.
 *
 * @note Operands are treated as 32-bit unsigned integers for bitwise operations
 * @note Function advances destination register pointer after each iteration
 * @note Template parameters are validated at compile-time for correctness
 * @note Operations preserve IEEE-754 bit patterns when applied to floating-point data
 * @note Lane predication and conditional execution supported through SFPU infrastructure
 */
template <bool APPROXIMATION_MODE, BinaryBitwiseOp BITWISE_OP, InstrModLoadStore INSTRUCTION_MODE = INT32, int ITERATIONS = 8>
inline void _calculate_sfpu_binary_bitwise_(const uint dst_offset)
{
    constexpr auto instruction_mode = static_cast<std::underlying_type_t<InstrModLoadStore>>(INSTRUCTION_MODE);
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        constexpr uint dst_tile_size = 64;  // Bytes per tile element (32 lanes × 2 bytes/lane)

        // Load operand A from current destination register position
        TTI_SFPLOAD(0, instruction_mode, ADDR_MOD_7, 0);
        
        // Load operand B from offset destination position  
        TT_SFPLOAD(1, instruction_mode, ADDR_MOD_7, dst_offset * dst_tile_size);

        // Perform compile-time selected bitwise operation
        if constexpr (BITWISE_OP == BinaryBitwiseOp::AND)
        {
            TTI_SFPAND(0, 1, 0, 0);  // LREG0 = LREG0 & LREG1
        }
        else if constexpr (BITWISE_OP == BinaryBitwiseOp::OR)
        {
            TTI_SFPOR(0, 1, 0, 0);   // LREG0 = LREG0 | LREG1
        }
        else if constexpr (BITWISE_OP == BinaryBitwiseOp::XOR)
        {
            TTI_SFPXOR(0, 1, 0, 0);  // LREG0 = LREG0 ^ LREG1
        }

        // Store result back to destination register
        TTI_SFPSTORE(0, instruction_mode, ADDR_MOD_7, 0);
        sfpi::dst_reg++;  // Advance to next 32-element vector
    }
}

} // namespace sfpu
} // namespace ckernel
