// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

/**
 * @brief Main kernel execution entry point for Tensix compute kernels
 *
 * @details Declares the primary entry point function for Tensix compute kernel execution.
 * This function serves as the main execution loop for Low-Level Kernel (LLK) operations
 * on the Blackhole architecture's Tensix cores.
 *
 * **Function Purpose:**
 * - Primary execution entry point for compute kernels
 * - Coordinates between firmware and kernel operations
 * - Manages kernel lifecycle and execution flow
 * - Returns execution status to the firmware layer
 *
 * **Execution Context:**
 * - Called by firmware after kernel initialization
 * - Runs on Tensix core compute units
 * - Operates within the compute kernel runtime environment
 * - Has access to all kernel resources and hardware interfaces
 *
 * **Typical Implementation Pattern:**
 * ```cpp
 * uint run_kernel() {
 *     // Kernel initialization
 *     // Main computation loop
 *     // Resource cleanup
 *     return execution_status;
 * }
 * ```
 *
 * **Hardware Context:**
 * - Executes on Tensix compute cores (Blackhole architecture)
 * - Has access to SFPU, unpacker, math, and packer units
 * - Can utilize local memory and register files
 * - Operates within kernel memory space and timing constraints
 *
 * **Integration with LLK:**
 * - Entry point for LLK (Low-Level Kernel) operations
 * - Coordinates with ckernel framework components
 * - Manages compute pipeline stages (unpack, math, pack)
 * - Handles synchronization between kernel threads
 *
 * @return Execution status code indicating success or error condition
 *
 * @note Function must be implemented by specific kernel instances
 * @note Declaration provides interface contract for firmware integration
 * @note Implementation varies based on specific kernel functionality
 * @note Return value used by firmware for error handling and flow control
 * @note Function operates in kernel execution context with hardware access
 */
uint run_kernel();
