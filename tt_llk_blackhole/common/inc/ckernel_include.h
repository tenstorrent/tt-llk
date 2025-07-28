// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

/**
 * @file ckernel_include.h
 * @brief Safe include organization for firmware and kernel compatibility
 *
 * @details This file provides a curated list of header files that are safe to include
 * in both firmware and compute kernel (ckernel) compilation contexts. It serves as a
 * compatibility layer ensuring that shared components can be used across different
 * execution environments within the Tensix architecture.
 *
 * **Compatibility Requirements:**
 * - Headers must be safe for both firmware and kernel contexts
 * - No dependencies on context-specific resources or libraries
 * - Consistent behavior across different compilation targets
 * - Minimal overhead for performance-critical kernel operations
 *
 * **Include Categories:**
 * - **Address Management**: Memory addressing and modulation utilities
 * - **Core Definitions**: Fundamental types, enums, and constants
 * - **Register Mapping**: GPR (General Purpose Register) management
 * - **Instruction Parameters**: Kernel instruction parameter handling
 * - **Data Structures**: Shared structures and data types
 * - **Hardware Interface**: Low-level Tensix hardware abstractions
 *
 * **Safety Guarantees:**
 * - All included headers are tested for cross-context compatibility
 * - No runtime dependencies that differ between firmware and kernels
 * - Consistent memory layout and data representation
 * - Stable ABI (Application Binary Interface) across contexts
 *
 * **Usage Context:**
 * - Included by both firmware compilation units and kernel implementations
 * - Provides foundation for shared code development
 * - Enables consistent interface definitions across system components
 * - Critical for maintaining compatibility in mixed compilation environments
 *
 * @note This file should not include context-specific headers
 * @note All included files must maintain compatibility across compilation contexts
 * @note Changes to this file require validation in both firmware and kernel builds
 * @note Performance impact must be considered for kernel-critical code paths
 */

//
// This file lists the includes that are safe to be included for both firmware and ckernels
//

// MT: This should be dissolved and moved to the appropriate place
/**
 * @brief Address modulation utilities for memory management
 * @details Provides address calculation and modulation functions for memory operations
 */
#include "ckernel_addrmod.h"

/**
 * @brief Core kernel definitions including enums and constants
 * @details Fundamental definitions for sources, unpackers, threading, and kernel states
 */
#include "ckernel_defs.h"

/**
 * @brief General Purpose Register (GPR) mapping and management
 * @details Defines register allocation and mapping for kernel operations
 */
#include "ckernel_gpr_map.h"

/**
 * @brief Instruction parameter handling for kernel commands
 * @details Parameter encoding/decoding for kernel instruction interfaces
 */
#include "ckernel_instr_params.h"

/**
 * @brief Shared data structures for kernel framework
 * @details Common structures including semaphores, mutexes, and firmware messages
 */
#include "ckernel_structs.h"

/**
 * @brief Low-level Tensix hardware interface definitions
 * @details Hardware-specific definitions and interfaces for Tensix architecture
 */
#include "tensix.h"
