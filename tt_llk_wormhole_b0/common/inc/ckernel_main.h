// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_main.h
 * @brief Main Kernel Entry Point for Wormhole B0 Tensix
 *
 * This header declares the main kernel entry point function for Wormhole B0
 * Tensix cores. It provides the primary interface for executing kernel
 * operations on the Tensix processing units.
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # Kernel Execution Model
 *
 * The kernel execution follows a simple model:
 * 1. Kernel is loaded and initialized by the runtime
 * 2. `run_kernel()` is called to execute the main kernel logic
 * 3. Kernel returns execution status to the runtime
 *
 * # Return Value Convention
 *
 * The return value from `run_kernel()` indicates execution status:
 * - **0**: Successful completion
 * - **Non-zero**: Error or specific status code
 *
 * # Usage Pattern
 *
 * This header is typically included by kernel implementation files that
 * need to provide the main kernel entry point:
 *
 * ```cpp
 * #include "ckernel_main.h"
 *
 * uint run_kernel() {
 *     // Kernel implementation
 *     initialize_hardware();
 *     process_data();
 *     cleanup();
 *     return 0; // Success
 * }
 * ```
 *
 * @note This function is called directly by the Tensix runtime system
 * @note The implementation must be provided by the specific kernel
 *
 * @see ckernel.h for core kernel infrastructure
 * @see ckernel_template.h for kernel template programming
 */

#pragma once

/**
 * @brief Main kernel execution entry point
 *
 * This function serves as the primary entry point for kernel execution on
 * Wormhole B0 Tensix cores. It is called by the runtime system to initiate
 * kernel processing and must be implemented by each specific kernel.
 *
 * @return Execution status code
 * @retval 0 Successful kernel execution
 * @retval non-zero Error code or specific status information
 *
 * @note This function must be implemented by the kernel developer
 * @note Called directly by the Tensix runtime - do not call manually
 * @note Should handle all kernel initialization, execution, and cleanup
 */
uint run_kernel();
