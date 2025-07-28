// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_structs.h"
#include "risc_attribs.h"

/**
 * @brief Global configuration state identifier for kernel context switching
 *
 * @details Tracks the current configuration state for kernel operations, enabling
 * context switching between different kernel configurations without full reinitialization.
 * Used by the kernel framework to maintain state across different execution phases.
 *
 * **State Management:**
 * - Alternates between different configuration states (typically 0 and 1)
 * - Enables double-buffering of kernel configurations
 * - Supports seamless switching between different kernel setups
 * - Critical for performance optimization in multi-stage operations
 *
 * **Usage Context:**
 * - Modified by firmware via FLIP_STATE_ID messages
 * - Read by kernel functions to determine current configuration
 * - Synchronized across all kernel components
 * - Essential for stateful kernel operations
 */
extern uint32_t cfg_state_id;

/**
 * @brief Unpacker configuration context for operand handling
 *
 * @details Maintains the current context for unpacker configuration, tracking
 * which unpacker settings are active for operand processing. Essential for
 * coordinating data flow between memory and compute units.
 *
 * **Context Management:**
 * - Tracks active unpacker configuration parameters
 * - Coordinates between different unpacker instances
 * - Maintains consistency across unpacker operations
 * - Critical for proper data formatting and routing
 */
extern uint32_t unp_cfg_context;

/**
 * @brief L1 memory buffer array for kernel data storage
 *
 * @details Array of L1 memory pointers providing fast local storage for kernel
 * operations. These buffers serve as primary data staging areas for high-performance
 * compute operations on Tensix cores.
 *
 * **Buffer Characteristics:**
 * - 16 separate buffer slots for flexible data management
 * - Located in fast L1 memory for optimal access performance
 * - Volatile qualified for hardware coherency
 * - Used for input, output, and intermediate data storage
 *
 * **Memory Attributes:**
 * - `volatile`: Ensures memory operations are not optimized away by compiler
 * - `tt_l1_ptr`: Tensix-specific attribute for L1 memory allocation
 * - Direct hardware access for maximum performance
 * - Critical for data pipeline efficiency
 */
extern uint32_t volatile tt_l1_ptr l1_buffer[16];

/**
 * @brief Destination pointer for pack synchronization operations
 *
 * @details Tracks the destination tile pointer used for synchronizing pack
 * operations with other kernel components. Essential for coordinating data
 * output timing and memory management.
 *
 * **Synchronization Role:**
 * - Coordinates pack unit with destination memory
 * - Ensures proper timing for data output operations
 * - Critical for maintaining data consistency
 * - Used in multi-threaded kernel coordination
 */
extern uint32_t pack_sync_tile_dst_ptr;

/**
 * @brief Destination index for math synchronization operations
 *
 * @details Index tracking for synchronizing math unit operations with
 * destination register management. Ensures proper coordination between
 * mathematical computations and result storage.
 *
 * **Index Management:**
 * - Tracks current destination register index
 * - Coordinates math unit output timing
 * - Essential for register file management
 * - Critical for multi-operation synchronization
 */
extern uint32_t math_sync_tile_dst_index;

/**
 * @brief Start address marker for local memory read-only data section
 *
 * @details Linker-provided symbol marking the beginning of the read-only data
 * section in local memory. Used for memory layout management and validation
 * of local memory usage patterns.
 *
 * **Memory Layout:**
 * - Marks beginning of constant data section
 * - Used for memory boundary checking
 * - Essential for local memory management
 * - Critical for memory protection and validation
 */
extern uint32_t __local_mem_rodata_start_addr[];

/**
 * @brief End address marker for local memory read-only data section
 *
 * @details Linker-provided symbol marking the end of the read-only data
 * section in local memory. Used in conjunction with start address for
 * complete memory section management.
 *
 * **Boundary Management:**
 * - Defines end of constant data section
 * - Enables calculation of data section size
 * - Used for memory validation and protection
 * - Critical for proper memory layout enforcement
 */
extern uint32_t __local_mem_rodata_end_addr[];

/**
 * @brief Start address marker for firmware code section
 *
 * @details Linker-provided symbol marking the beginning of the firmware
 * code section. Used for memory layout management and coordination
 * between firmware and kernel components.
 *
 * **Code Section Management:**
 * - Marks beginning of firmware executable code
 * - Used for memory layout validation
 * - Essential for firmware-kernel coordination
 * - Critical for proper code execution boundaries
 */
extern uint32_t __firmware_start[];
