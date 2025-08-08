// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_pcbuf.h
 * @brief PC Buffer Command Encoding/Decoding for Wormhole B0 Tensix
 *
 * This header provides utilities for encoding and decoding commands written to
 * the PC (Program Counter) buffer. The PC buffer is used for kernel launch
 * commands and parameter passing between firmware and kernel components.
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # PC Buffer Command Format
 *
 * PC buffer commands use a 32-bit encoding format with two distinct modes:
 *
 * ## Fast Launch Mode (MSB = 1):
 * - **Bit 31**: Fast launch flag (1)
 * - **Bits 30-12**: Command address (19 bits, max 512KB)
 * - **Bits 11-0**: Kernel ID (12 bits, max 4096 kernels)
 *
 * ## Standard Mode (MSB = 0):
 * - **Bit 31**: Fast launch flag (0)
 * - **Bits 30-16**: Iterations (15 bits, max 32768)
 * - **Bits 15-12**: Extra parameters count (4 bits, max 16)
 * - **Bits 11-0**: Kernel ID (12 bits, max 4096 kernels)
 *
 * # Usage Patterns
 *
 * ## Fast Launch Command
 * ```cpp
 * uint32_t cmd = get_pc_buf_cmd(kernel_id, command_addr);
 * write_to_pc_buffer(cmd);
 * ```
 *
 * ## Standard Launch Command
 * ```cpp
 * uint32_t cmd = get_pc_buf_cmd(kernel_id, iterations, extra_params);
 * write_to_pc_buffer(cmd);
 * ```
 *
 * ## Command Decoding
 * ```cpp
 * if (is_pc_buf_fast_cmd(cmd)) {
 *     uint kernel_id, addr;
 *     decode_pc_buf_cmd(cmd, kernel_id, addr);
 *     // Handle fast launch
 * } else {
 *     uint kernel_id, iterations, extra_params;
 *     decode_pc_buf_cmd(cmd, kernel_id, iterations, extra_params);
 *     // Handle standard launch
 * }
 * ```
 *
 * @note All functions are inlined for performance in critical launch paths
 * @note Command encoding must match hardware expectations exactly
 *
 * @see ckernel_structs.h for related communication structures
 * @see ckernel_globals.h for PC buffer related globals
 */

#pragma once

/**
 * @namespace ckernel
 * @brief Core kernel functionality and hardware abstraction layer
 */
namespace ckernel
{

/**
 * @defgroup PCBufferCommands PC Buffer Command Utilities
 * @brief Functions for encoding and decoding PC buffer commands
 * @{
 */

/**
 * @brief Encode fast launch PC buffer command
 *
 * Creates a fast launch command for the PC buffer with the specified kernel ID
 * and command address. Fast launch commands have the MSB set to indicate
 * immediate execution mode.
 *
 * @param ckernel_id Kernel identifier (0-4095, 12 bits)
 * @param command_addr Command address for execution (0-524287, 19 bits)
 * @return 32-bit encoded fast launch command
 *
 * # Command Format:
 * - Bit 31: Fast launch flag (1)
 * - Bits 30-12: Command address (19 bits)
 * - Bits 11-0: Kernel ID (12 bits)
 */
inline uint get_pc_buf_cmd(uint ckernel_id, uint command_addr)
{
    // Ckernel fast launch cmd has MSB set
    return ckernel_id | ((command_addr & 0x7FFFF) << 12) | (1 << 31);
}

/**
 * @brief Encode standard PC buffer command
 *
 * Creates a standard launch command for the PC buffer with the specified kernel ID,
 * iteration count, and extra parameters. Standard commands have the MSB clear.
 *
 * @param ckernel_id Kernel identifier (0-4095, 12 bits)
 * @param iterations Number of iterations to execute (0-32767, 15 bits)
 * @param number_of_extra_params Count of extra parameters (0-15, 4 bits)
 * @return 32-bit encoded standard launch command
 *
 * # Command Format:
 * - Bit 31: Fast launch flag (0)
 * - Bits 30-16: Iterations (15 bits)
 * - Bits 15-12: Extra parameters count (4 bits)
 * - Bits 11-0: Kernel ID (12 bits)
 */
inline uint get_pc_buf_cmd(uint ckernel_id, uint iterations, uint number_of_extra_params)
{
    // Ckernel ID can be a max of 12 bits.
    return ckernel_id | ((iterations & 0x7FFF) << 16) | ((number_of_extra_params & 0xF) << 12);
}

/**
 * @brief Check if PC buffer command is fast launch mode
 *
 * Determines whether a PC buffer command is a fast launch command by
 * checking the MSB (bit 31).
 *
 * @param cmd 32-bit PC buffer command
 * @return true if fast launch command, false if standard command
 */
inline bool is_pc_buf_fast_cmd(uint cmd)
{
    // MSB stores fast kernel mode
    return (cmd >> 31) & 0x1;
}

/**
 * @brief Decode fast launch PC buffer command
 *
 * Extracts kernel ID and command address from a fast launch PC buffer command.
 * Should only be called after verifying the command is a fast launch command.
 *
 * @param cmd 32-bit fast launch PC buffer command
 * @param ckernel_id [out] Extracted kernel identifier (12 bits)
 * @param command_addr [out] Extracted command address (19 bits)
 */
inline void decode_pc_buf_cmd(uint cmd, uint &ckernel_id, uint &command_addr)
{
    // Ckernel ID can be a max of 12 bits.
    ckernel_id   = cmd & 0xFFF;
    command_addr = (cmd >> 12) & 0x7FFFF;
}

/**
 * @brief Decode standard PC buffer command
 *
 * Extracts kernel ID, iterations, and extra parameters from a standard PC buffer
 * command. Should only be called after verifying the command is a standard command.
 *
 * @param cmd 32-bit standard PC buffer command
 * @param ckernel_id [out] Extracted kernel identifier (12 bits)
 * @param iterations [out] Extracted iteration count (15 bits)
 * @param number_of_extra_params [out] Extracted extra parameters count (4 bits)
 */
inline void decode_pc_buf_cmd(uint cmd, uint &ckernel_id, uint &iterations, uint &number_of_extra_params)
{
    // Ckernel ID can be a max of 12 bits.
    ckernel_id             = cmd & 0xFFF;
    iterations             = (cmd >> 16) & 0x7FFF;
    number_of_extra_params = (cmd >> 12) & 0xF;
}

/** @} */ // end of PCBufferCommands group

} // namespace ckernel
