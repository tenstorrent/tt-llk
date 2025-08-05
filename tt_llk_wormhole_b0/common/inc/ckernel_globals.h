// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_globals.h
 * @brief Global Variable Declarations for Wormhole B0 Tensix Kernel
 *
 * This header declares global variables used throughout the kernel system for
 * state management, configuration tracking, synchronization, and memory layout.
 * These variables provide shared state across different kernel components and
 * threads.
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # Global Variable Categories
 *
 * - **Configuration State**: Variables tracking current hardware configuration
 * - **Buffer Management**: L1 buffer allocation and management
 * - **Synchronization**: Inter-thread synchronization variables
 * - **Memory Layout**: Linker-defined memory region boundaries
 *
 * # Usage Notes
 *
 * These variables are defined elsewhere in the kernel implementation and are
 * declared here for access by kernel components. They represent shared state
 * that must be coordinated across different execution contexts.
 *
 * @warning Global variables require careful synchronization in multi-threaded contexts
 * @warning Some variables are volatile and require special handling
 *
 * @see ckernel_structs.h for related data structure definitions
 * @see risc_attribs.h for RISC-specific attributes
 */

#pragma once

#include <cstdint>

#include "ckernel_structs.h"
#include "risc_attribs.h"

/**
 * @defgroup GlobalVariables Kernel Global Variables
 * @brief Shared global state variables for kernel operation
 * @{
 */

/**
 * @defgroup ConfigurationState Configuration State Variables
 * @brief Variables tracking current hardware configuration state
 * @{
 */

/**
 * @brief Current configuration state identifier
 *
 * Tracks the current configuration state for hardware components.
 * Used to manage configuration context switching and state coordination
 * across different kernel operations.
 */
extern uint32_t cfg_state_id;

/**
 * @brief Unpacker configuration context identifier
 *
 * Maintains the current configuration context for unpacker operations.
 * Used to track which unpacker configuration is currently active for
 * proper context management and state restoration.
 */
extern uint32_t unp_cfg_context;

/** @} */ // end of ConfigurationState group

/**
 * @defgroup BufferManagement Buffer Management Variables
 * @brief Variables for L1 buffer allocation and management
 * @{
 */

/**
 * @brief L1 buffer array for kernel operations
 *
 * Array of L1 memory pointers used for various kernel buffer management
 * operations. The volatile qualifier ensures proper memory ordering and
 * prevents compiler optimizations that could interfere with hardware access.
 *
 * @note Array size of 16 elements provides multiple buffer slots
 * @note Volatile access ensures hardware coherency
 * @note tt_l1_ptr attribute indicates L1 memory addressing
 */
extern uint32_t volatile tt_l1_ptr l1_buffer[16];

/** @} */ // end of BufferManagement group

/**
 * @defgroup SynchronizationVariables Synchronization Variables
 * @brief Variables for coordinating between kernel threads and operations
 * @{
 */

/**
 * @brief Pack synchronization destination pointer
 *
 * Destination pointer used for synchronizing pack operations with other
 * pipeline stages. Ensures proper coordination between math results and
 * pack operations.
 */
extern uint32_t pack_sync_tile_dst_ptr;

/**
 * @brief Math synchronization destination tile index
 *
 * Tile index used for synchronizing math operations with pack pipeline.
 * Tracks which destination tile is currently being processed for proper
 * pipeline coordination.
 */
extern uint32_t math_sync_tile_dst_index;

/** @} */ // end of SynchronizationVariables group

/**
 * @defgroup MemoryLayout Memory Layout Variables
 * @brief Linker-defined memory region boundaries and layout information
 * @{
 */

/**
 * @brief Start address of local read-only data section
 *
 * Linker-defined symbol marking the beginning of the local memory
 * read-only data section. Used for memory layout management and
 * address calculations.
 */
extern uint32_t __local_mem_rodata_start_addr[];

/**
 * @brief End address of local read-only data section
 *
 * Linker-defined symbol marking the end of the local memory
 * read-only data section. Used to calculate section size and
 * validate memory access boundaries.
 */
extern uint32_t __local_mem_rodata_end_addr[];

/**
 * @brief Firmware start address
 *
 * Linker-defined symbol marking the start of firmware code section.
 * Used for memory layout management and firmware loading operations.
 */
extern uint32_t __firmware_start[];

/** @} */ // end of MemoryLayout group
/** @} */ // end of GlobalVariables group
