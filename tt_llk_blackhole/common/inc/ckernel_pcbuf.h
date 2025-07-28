// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

/**
 * @file ckernel_pcbuf.h
 * @brief PC buffer command encoding and decoding utilities for kernel management
 *
 * @details Provides functions for encoding and decoding commands written to the Program Counter (PC)
 * buffer, which serves as the primary interface for launching and controlling compute kernels on
 * Tensix cores. These utilities enable efficient kernel dispatch and parameter passing.
 *
 * **PC Buffer Overview:**
 * - Primary interface for kernel command dispatch
 * - Supports both fast launch and parameterized execution modes
 * - Enables efficient kernel switching and parameter passing
 * - Critical component of the kernel execution pipeline
 *
 * **Command Types:**
 * - **Fast Commands**: Direct address-based kernel launch (MSB = 1)
 * - **Parameter Commands**: Iteration-based execution with parameters (MSB = 0)
 * - Different encoding schemes optimize for different use cases
 *
 * **Bit Field Organization:**
 * ```
 * Fast Command (MSB = 1):
 * [31] = 1 (fast mode flag)
 * [30:12] = command_addr (19 bits)
 * [11:0] = ckernel_id (12 bits)
 * 
 * Parameter Command (MSB = 0):
 * [31] = 0 (parameter mode flag)
 * [30:16] = iterations (15 bits)
 * [15:12] = number_of_extra_params (4 bits)
 * [11:0] = ckernel_id (12 bits)
 * ```
 *
 * **Performance Characteristics:**
 * - Optimized bit packing for minimal memory usage
 * - Fast encoding/decoding for real-time kernel dispatch
 * - Supports up to 4096 different kernel IDs (12 bits)
 * - Efficient parameter passing mechanism
 */

// Functions for encoding and decoding PC buffer writes
namespace ckernel
{

/**
 * @brief Encode a fast launch command for direct kernel execution
 *
 * @details Creates a PC buffer command for fast kernel launch mode, where the kernel
 * is launched directly at a specific command address. This mode is optimized for
 * kernels that don't require runtime parameters and can execute immediately.
 *
 * **Fast Launch Characteristics:**
 * - Direct address-based execution (no parameter processing)
 * - Minimal dispatch overhead for performance-critical operations
 * - MSB set to 1 to indicate fast launch mode
 * - Command address embedded directly in the command word
 *
 * **Bit Field Layout:**
 * ```
 * [31]    = 1 (fast launch flag)
 * [30:12] = command_addr & 0x7FFFF (19-bit address)
 * [11:0]  = ckernel_id (12-bit kernel identifier)
 * ```
 *
 * **Use Cases:**
 * - High-frequency kernel launches without parameters
 * - Performance-critical kernel dispatch
 * - Simple kernels with fixed behavior
 * - Real-time execution scenarios
 *
 * @param ckernel_id Unique identifier for the target kernel (12 bits max)
 * @param command_addr Direct execution address for the kernel (19 bits max)
 * @return Encoded command word ready for PC buffer write
 *
 * @note Command address is masked to 19 bits for safety
 * @note Kernel ID is limited to 12 bits (4096 unique kernels)
 * @note MSB automatically set to indicate fast launch mode
 * @note Optimized for minimal dispatch latency
 */
inline uint get_pc_buf_cmd(uint ckernel_id, uint command_addr)
{
    // Ckernel fast launch cmd has MSB set
    return ckernel_id | ((command_addr & 0x7FFFF) << 12) | (1 << 31);
}

/**
 * @brief Encode a parameterized command for iterative kernel execution
 *
 * @details Creates a PC buffer command for parameterized kernel execution, where the
 * kernel runs for a specified number of iterations with optional extra parameters.
 * This mode provides flexible runtime configuration for complex kernel operations.
 *
 * **Parameterized Execution Features:**
 * - Configurable iteration count for repeated operations
 * - Support for additional runtime parameters
 * - MSB cleared to indicate parameter mode
 * - Flexible execution control for complex kernels
 *
 * **Bit Field Layout:**
 * ```
 * [31]    = 0 (parameter mode flag)
 * [30:16] = iterations & 0x7FFF (15-bit iteration count)
 * [15:12] = number_of_extra_params & 0xF (4-bit parameter count)
 * [11:0]  = ckernel_id (12-bit kernel identifier)
 * ```
 *
 * **Parameter Limits:**
 * - Iterations: Up to 32767 (15 bits)
 * - Extra parameters: Up to 15 additional parameters (4 bits)
 * - Kernel ID: Up to 4095 unique kernels (12 bits)
 *
 * **Use Cases:**
 * - Kernels requiring runtime configuration
 * - Iterative operations (loops, reductions, etc.)
 * - Complex kernels with multiple execution phases
 * - Kernels needing additional parameter data
 *
 * @param ckernel_id Unique identifier for the target kernel (12 bits max)
 * @param iterations Number of iterations for kernel execution (15 bits max)
 * @param number_of_extra_params Count of additional parameters (4 bits max)
 * @return Encoded command word ready for PC buffer write
 *
 * @note All parameters are masked for safety and proper bit packing
 * @note MSB automatically cleared to indicate parameter mode
 * @note Extra parameters passed separately after command word
 * @note Iteration count of 0 may have special meaning depending on kernel
 */
inline uint get_pc_buf_cmd(uint ckernel_id, uint iterations, uint number_of_extra_params)
{
    // Ckernel ID can be a max of 12 bits.
    return ckernel_id | ((iterations & 0x7FFF) << 16) | ((number_of_extra_params & 0xF) << 12);
}

/**
 * @brief Check if a PC buffer command is in fast launch mode
 *
 * @details Examines the MSB of a PC buffer command to determine if it represents
 * a fast launch command or a parameterized command. This is essential for proper
 * command interpretation and dispatch routing.
 *
 * **Mode Detection:**
 * - Fast launch: MSB = 1 (direct address execution)
 * - Parameter mode: MSB = 0 (iterative execution with parameters)
 * - Simple bit test for efficient runtime decision making
 *
 * **Usage Context:**
 * - Command dispatcher decision making
 * - Kernel launch path selection
 * - Performance optimization routing
 * - Debug and monitoring systems
 *
 * @param cmd PC buffer command word to examine
 * @return true if command is fast launch mode, false if parameter mode
 *
 * @note Simple bit test operation for maximum performance
 * @note Used by firmware and kernel dispatchers
 * @note Critical for proper command interpretation
 */
inline bool is_pc_buf_fast_cmd(uint cmd)
{
    // MSB stores fast kernel mode
    return (cmd >> 31) & 0x1;
}

/**
 * @brief Decode a fast launch command to extract kernel ID and command address
 *
 * @details Extracts the kernel identifier and direct command address from a fast
 * launch PC buffer command. Used by the command dispatcher to route fast launch
 * commands to the appropriate execution path.
 *
 * **Extraction Process:**
 * - Kernel ID: Lower 12 bits of command word
 * - Command address: Bits 12-30 (19 bits total)
 * - MSB ignored (assumed to be 1 for fast commands)
 *
 * **Field Recovery:**
 * ```
 * ckernel_id = cmd & 0xFFF
 * command_addr = (cmd >> 12) & 0x7FFFF
 * ```
 *
 * @param cmd Fast launch command word to decode
 * @param[out] ckernel_id Reference to store extracted kernel ID
 * @param[out] command_addr Reference to store extracted command address
 *
 * @note Caller responsible for verifying command is fast launch mode
 * @note Output parameters modified by reference
 * @note Masks ensure proper bit field extraction
 * @note Used by fast launch execution path
 */
inline void decode_pc_buf_cmd(uint cmd, uint &ckernel_id, uint &command_addr)
{
    // Ckernel ID can be a max of 12 bits.
    ckernel_id   = cmd & 0xFFF;                // Extract lower 12 bits
    command_addr = (cmd >> 12) & 0x7FFFF;      // Extract bits 12-30 (19 bits)
}

/**
 * @brief Decode a parameterized command to extract kernel ID, iterations, and parameter count
 *
 * @details Extracts the kernel identifier, iteration count, and extra parameter count
 * from a parameterized PC buffer command. Used by the command dispatcher to set up
 * iterative kernel execution with proper parameter handling.
 *
 * **Extraction Process:**
 * - Kernel ID: Lower 12 bits of command word
 * - Parameter count: Bits 12-15 (4 bits total)
 * - Iterations: Bits 16-30 (15 bits total)
 * - MSB ignored (assumed to be 0 for parameter commands)
 *
 * **Field Recovery:**
 * ```
 * ckernel_id = cmd & 0xFFF
 * number_of_extra_params = (cmd >> 12) & 0xF
 * iterations = (cmd >> 16) & 0x7FFF
 * ```
 *
 * **Parameter Handling:**
 * - Extra parameters follow command word in PC buffer
 * - Parameter count indicates how many additional words to read
 * - Iterations control kernel execution loop count
 *
 * @param cmd Parameterized command word to decode
 * @param[out] ckernel_id Reference to store extracted kernel ID
 * @param[out] iterations Reference to store extracted iteration count
 * @param[out] number_of_extra_params Reference to store extracted parameter count
 *
 * @note Caller responsible for verifying command is parameter mode
 * @note Output parameters modified by reference
 * @note Masks ensure proper bit field extraction
 * @note Used by parameterized execution path
 * @note Parameter words must be read separately from PC buffer
 */
inline void decode_pc_buf_cmd(uint cmd, uint &ckernel_id, uint &iterations, uint &number_of_extra_params)
{
    // Ckernel ID can be a max of 12 bits.
    ckernel_id             = cmd & 0xFFF;          // Extract lower 12 bits
    iterations             = (cmd >> 16) & 0x7FFF; // Extract bits 16-30 (15 bits)
    number_of_extra_params = (cmd >> 12) & 0xF;    // Extract bits 12-15 (4 bits)
}

} // namespace ckernel
