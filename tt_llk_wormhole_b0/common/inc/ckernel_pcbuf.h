// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_pcbuf.h
 * @brief PC Buffer command encoding/decoding for Wormhole B0 Tensix kernel launch
 *
 * @details This header provides functions for encoding and decoding commands written
 * to the Program Counter (PC) buffer, which is a critical component of the Wormhole B0
 * kernel launch mechanism. The PC buffer enables efficient kernel execution through
 * fast launch and parameterized command modes.
 *
 * **Wormhole B0 PC Buffer Architecture:**
 * The PC buffer is part of the Tensix core's kernel launch infrastructure:
 * - **Fast Launch Mode**: Direct kernel execution with minimal overhead
 * - **Parameterized Mode**: Flexible kernel execution with configurable parameters
 * - **Hardware Integration**: Interfaces with TRISC processors for kernel control
 * - **Multi-Core Coordination**: Supports coordinated kernel launch across cores
 *
 * **Command Encoding Structure:**
 * PC buffer commands use a structured bit layout for efficient hardware decoding:
 * ```
 * Fast Launch Mode (MSB = 1):
 * [31] Fast Flag | [30:12] Command Address | [11:0] Kernel ID
 * 
 * Parameterized Mode (MSB = 0):
 * [31] Fast Flag | [30:16] Iterations | [15:12] Extra Params | [11:0] Kernel ID
 * ```
 *
 * **Kernel Launch Optimization:**
 * The PC buffer design supports the Wormhole B0 performance optimization strategy:
 * - **Reduced Launch Overhead**: Minimal cycles for kernel initiation
 * - **Batch Execution**: Support for iteration-based kernel execution
 * - **Parameter Passing**: Efficient parameter delivery to compute kernels
 * - **Synchronization Support**: Integration with semaphore/mutex systems
 *
 * **Integration with Tensix Engine:**
 * - **3-Thread Coordination**: Commands coordinate Unpack, Math, and Pack threads
 * - **Resource Management**: Kernel launch coordinates with L1 memory and register files
 * - **Performance Monitoring**: Support for performance event tracking and analysis
 */

#pragma once

// Functions for encoding and decoding PC buffer writes
namespace ckernel
{

/**
 * @brief Encode fast launch PC buffer command for immediate kernel execution
 * @param ckernel_id Compute kernel identifier (12-bit max)
 * @param command_addr Target address for kernel execution (19-bit)
 * @return Encoded PC buffer command with fast launch flag set
 * 
 * @details Creates a fast launch command for immediate kernel execution with
 * minimal overhead. The MSB is set to indicate fast launch mode, enabling
 * hardware to bypass parameter processing for maximum performance.
 */
inline uint get_pc_buf_cmd(uint ckernel_id, uint command_addr)
{
    // Ckernel fast launch cmd has MSB set
    return ckernel_id | ((command_addr & 0x7FFFF) << 12) | (1 << 31);
}

/**
 * @brief Encode parameterized PC buffer command for configurable kernel execution
 * @param ckernel_id Compute kernel identifier (12-bit max)
 * @param iterations Number of iterations for batch execution (15-bit)
 * @param number_of_extra_params Count of additional parameters (4-bit)
 * @return Encoded PC buffer command for parameterized execution
 * 
 * @details Creates a parameterized command enabling flexible kernel execution
 * with configurable iteration counts and parameter passing. Supports batch
 * processing and complex kernel configurations for optimal resource utilization.
 */
inline uint get_pc_buf_cmd(uint ckernel_id, uint iterations, uint number_of_extra_params)
{
    // Ckernel ID can be a max of 12 bits.
    return ckernel_id | ((iterations & 0x7FFF) << 16) | ((number_of_extra_params & 0xF) << 12);
}

/**
 * @brief Check if PC buffer command is in fast launch mode
 * @param cmd PC buffer command to analyze
 * @return true if command uses fast launch mode, false for parameterized mode
 * 
 * @details Examines the MSB to determine command type, enabling appropriate
 * decoding and execution path selection in the kernel launch pipeline.
 */
inline bool is_pc_buf_fast_cmd(uint cmd)
{
    // MSB stores fast kernel mode
    return (cmd >> 31) & 0x1;
}

/**
 * @brief Decode fast launch PC buffer command
 * @param cmd Encoded PC buffer command with fast launch flag set
 * @param ckernel_id Output parameter for decoded kernel identifier (12-bit)
 * @param command_addr Output parameter for decoded command address (19-bit)
 * 
 * @details Decodes a fast launch PC buffer command into its constituent parts.
 * This function is used when the MSB indicates fast launch mode, extracting
 * the kernel ID and target address for immediate execution with minimal overhead.
 * 
 * **Wormhole B0 Fast Launch Architecture:**
 * - **Zero Parameter Overhead**: No iteration counts or parameter processing
 * - **Direct Execution**: Immediate kernel launch at specified address
 * - **Hardware Optimization**: Minimal cycles from command to execution
 * - **TRISC Integration**: Efficient interface with TRISC kernel management
 */
inline void decode_pc_buf_cmd(uint cmd, uint &ckernel_id, uint &command_addr)
{
    // Ckernel ID can be a max of 12 bits.
    ckernel_id   = cmd & 0xFFF;
    command_addr = (cmd >> 12) & 0x7FFFF;
}

/**
 * @brief Decode parameterized PC buffer command
 * @param cmd Encoded PC buffer command with parameterized execution
 * @param ckernel_id Output parameter for decoded kernel identifier (12-bit)
 * @param iterations Output parameter for decoded iteration count (15-bit)
 * @param number_of_extra_params Output parameter for decoded parameter count (4-bit)
 * 
 * @details Decodes a parameterized PC buffer command for flexible kernel execution.
 * This mode supports batch processing with configurable iteration counts and
 * additional parameter passing for complex kernel configurations.
 * 
 * **Wormhole B0 Parameterized Execution:**
 * - **Batch Processing**: Support for up to 32,767 iterations (15-bit)
 * - **Parameter Passing**: Up to 15 additional parameters (4-bit count)
 * - **Resource Optimization**: Amortizes kernel launch overhead across iterations
 * - **Pipeline Efficiency**: Enables sustained high-throughput computation
 * - **3-Thread Coordination**: Parameters configure Unpack, Math, and Pack operations
 */
inline void decode_pc_buf_cmd(uint cmd, uint &ckernel_id, uint &iterations, uint &number_of_extra_params)
{
    // Ckernel ID can be a max of 12 bits.
    ckernel_id             = cmd & 0xFFF;
    iterations             = (cmd >> 16) & 0x7FFF;
    number_of_extra_params = (cmd >> 12) & 0xF;
}

} // namespace ckernel
