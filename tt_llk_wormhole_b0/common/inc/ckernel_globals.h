// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_globals.h
 * @brief Global variables for Wormhole B0 Tensix kernel configuration and state management
 *
 * @details This header declares essential global variables used throughout the Wormhole B0
 * Tensix compute kernel framework for managing configuration state, memory buffers, and
 * inter-thread synchronization. These globals are critical for coordinating the complex
 * 3-thread architecture and dual configuration state system.
 *
 * **Wormhole B0 Configuration Architecture:**
 * - **Dual CFG States**: Two complete sets of configuration registers (State 0 and State 1)
 * - **Thread-Specific Context**: Each of the 3 threads can independently select CFG state
 * - **Dynamic Reconfiguration**: Runtime switching between states for performance optimization
 * - **Unpacker Context**: Separate configuration contexts for efficient data streaming
 *
 * **Memory Management:**
 * The Wormhole B0 L1 SRAM system is a multi-bank, multi-port structure:
 * - **16 Access Ports**: Shared between multiple Tensix core clients
 * - **16B Word Size**: All L1 operations aligned to 16-byte boundaries
 * - **Bank Interleaving**: 16 consecutive addresses map to different banks (configurable)
 * - **Round-Robin Arbitration**: Fair access across all requesting clients
 *
 * **Synchronization Framework:**
 * These globals support the sophisticated Wormhole B0 synchronization model:
 * - **Hardware Semaphores**: Resource counters (not true atomics) for thread coordination
 * - **DEST Register Sync**: Coordination between Math and Pack threads on register access
 * - **Pipeline Synchronization**: Maintaining proper data flow through Unpack→Math→Pack
 */

#pragma once

#include <cstdint>

#include "ckernel_structs.h"
#include "risc_attribs.h"

/**
 * @brief Current configuration state ID (0 or 1) for Tensix engine
 * @details Controls which of the dual CFG register sets is currently active.
 * Wormhole B0 provides two complete configuration contexts that can be
 * switched dynamically for performance optimization and reduced reconfiguration overhead.
 */
extern uint32_t cfg_state_id;

/**
 * @brief Unpacker configuration context selector
 * @details Manages the current configuration context for unpacker operations.
 * Supports multiple unpacker contexts for efficient data streaming patterns
 * and pipeline optimization in the Unpack thread (Thread 0).
 */
extern uint32_t unp_cfg_context;

/**
 * @brief L1 memory buffer array for data movement operations
 * @details Provides 16 buffer pointers for managing data in the Wormhole B0
 * L1 SRAM system. The volatile qualifier ensures proper memory access semantics
 * for hardware-visible data structures in the multi-bank L1 memory.
 */
extern uint32_t volatile tt_l1_ptr l1_buffer[16];

/**
 * @brief Pack thread synchronization pointer for DEST register access
 * @details Synchronization primitive for coordinating Pack thread (Thread 2)
 * access to DEST registers. Ensures proper data flow from Math thread
 * computation results to Pack thread L1 storage operations.
 */
extern uint32_t pack_sync_tile_dst_ptr;

/**
 * @brief Math thread synchronization index for DEST register management
 * @details Index-based synchronization for Math thread (Thread 1) DEST
 * register operations. Coordinates with Pack thread to ensure proper
 * double-buffering and pipeline efficiency.
 */
extern uint32_t math_sync_tile_dst_index;

/**
 * @brief Linker-provided start address of read-only data in local memory
 * @details Marks the beginning of the read-only data section in TRISC local memory.
 * Local memory provides lowest-latency access (~1 cycle vs 5-7 cycles for L1)
 * and is used for frequently accessed constants and program data.
 */
extern uint32_t __local_mem_rodata_start_addr[];

/**
 * @brief Linker-provided end address of read-only data in local memory
 * @details Marks the end of the read-only data section, enabling bounds
 * checking and memory layout management for the TRISC local memory space.
 */
extern uint32_t __local_mem_rodata_end_addr[];

/**
 * @brief Linker-provided firmware start address
 * @details Starting address of firmware code section, used for memory
 * management and initialization of the Tensix compute kernel runtime.
 */
extern uint32_t __firmware_start[];
