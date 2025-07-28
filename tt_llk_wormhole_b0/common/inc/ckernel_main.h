// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_main.h
 * @brief Primary entry point for Tensix compute kernel execution on Wormhole B0
 *
 * @details This header declares the main kernel entry point function for execution
 * on the Tensix compute core. The Tensix core in Wormhole B0 features a sophisticated
 * multi-threaded architecture with 5 RISC-V processors and a custom Tensix engine
 * optimized for high-throughput tensor operations.
 *
 * **Wormhole B0 Tensix Architecture:**
 * - **5 RISC-V Processors**: 3 TRISC + 1 BRISC + 1 NRISC
 * - **3-Thread Tensix Engine**: Thread 0 (Unpack), Thread 1 (Math), Thread 2 (Pack)
 * - **2048 Hardware Multipliers**: 5×7 bit width for efficient AI computations
 * - **L1 SRAM**: Multi-bank, multi-port shared memory (16B word size)
 * - **Compute Pipeline**: L1 → Unpack → Math → Pack → L1 data flow
 *
 * **Kernel Execution Context:**
 * The `run_kernel()` function executes within the TRISC processor context and
 * coordinates the multi-threaded Tensix engine for high-performance tensor
 * operations. It typically implements the following execution pattern:
 *
 * 1. **Initialization**: Configure Tensix engine threads and memory subsystems
 * 2. **Data Processing**: Coordinate Unpack, Math, and Pack operations
 * 3. **Synchronization**: Manage inter-thread communication via semaphores/mutexes
 * 4. **Result Generation**: Produce computed outputs and update memory state
 *
 * **Integration with LLK (Low-Level Kernel):**
 * This entry point integrates with the LLK framework to provide hardware-optimized
 * implementations for tensor operations including matrix multiplication, convolution,
 * elementwise operations, and specialized mathematical functions via the SFPU.
 */

#pragma once

uint run_kernel();
