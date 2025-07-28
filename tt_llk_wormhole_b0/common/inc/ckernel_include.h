// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_include.h
 * @brief Safe header inclusion list for Wormhole B0 firmware and compute kernel compilation
 *
 * @details This header provides a curated list of header files that are safe to include
 * in both firmware and compute kernel compilation contexts on the Wormhole B0 architecture.
 * The separation ensures compatibility across different compilation environments while
 * maintaining access to essential Tensix functionality.
 *
 * **Compilation Context Considerations:**
 * - **Firmware Context**: RISC-V processor code (TRISC, BRISC, NRISC)
 * - **Compute Kernel Context**: Tensix engine instruction sequences and data structures
 * - **Shared Definitions**: Common constants, structures, and interfaces
 * - **Architecture Specificity**: Wormhole B0 Tensix engine and memory layout specifics
 *
 * **Header Categories:**
 * - **Address Management**: `ckernel_addrmod.h` - Wormhole B0 address generation and counter control
 * - **Core Definitions**: `ckernel_defs.h` - Fundamental types and constants for Tensix programming
 * - **GPR Management**: `ckernel_gpr_map.h` - General Purpose Register allocation for 3-thread model
 * - **Instruction Interface**: `ckernel_instr_params.h` - Tensix instruction parameter encoding
 * - **Data Structures**: `ckernel_structs.h` - Synchronization primitives and communication structures
 * - **Hardware Interface**: `tensix.h` - Low-level Tensix hardware definitions and macros
 *
 * **Wormhole B0 Integration:**
 * These headers collectively provide comprehensive access to:
 * - 3-thread Tensix engine programming (Unpack, Math, Pack)
 * - 2048 hardware multiplier control and configuration
 * - Dual CFG state management and dynamic reconfiguration
 * - L1 SRAM multi-bank memory system interaction
 * - SFPU 8×4=32-lane SIMD special function programming
 */

#pragma once

//
// This file lists the includes that are safe to be included for both firmware and ckernels
//

/**
 * @brief Address generation and counter management for Wormhole B0 Tensix engine
 * @details Provides sophisticated address generation with X,Y,Z counters, stride control,
 * and carriage return functionality for efficient tensor data access patterns.
 */
#include "ckernel_addrmod.h"

/**
 * @brief Core definitions and constants for Wormhole B0 Tensix programming
 * @details Fundamental type definitions, enumerations, and utility functions
 * tailored for the Wormhole B0 architecture specifications and data format support.
 */
#include "ckernel_defs.h"

/**
 * @brief General Purpose Register allocation and management for 3-thread Tensix engine
 * @details Defines GPR usage patterns across Unpack, Math, and Pack threads,
 * ensuring proper resource allocation and thread-specific register access.
 */
#include "ckernel_gpr_map.h"

/**
 * @brief Tensix instruction parameter encoding and structure definitions
 * @details Provides parameter encoding utilities for Tensix instructions,
 * enabling efficient instruction construction and hardware interface programming.
 */
#include "ckernel_instr_params.h"

/**
 * @brief Core data structures for synchronization and inter-thread communication
 * @details Defines semaphore, mutex, and communication structures essential
 * for coordinating the 3-thread Tensix engine and RISC-V processor interactions.
 */
#include "ckernel_structs.h"

/**
 * @brief Low-level Tensix hardware interface definitions and register access
 * @details Provides direct hardware access macros, register definitions,
 * and low-level interface functions for Wormhole B0 Tensix engine programming.
 */
#include "tensix.h"
