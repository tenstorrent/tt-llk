// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_include.h
 * @brief Common Include Collection for Wormhole B0 Tensix
 *
 * This header provides a convenient collection of core kernel headers that are
 * safe to include in both firmware and kernel contexts. It serves as a central
 * include point for common kernel infrastructure without pulling in dependencies
 * that might conflict between different execution environments.
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # Purpose
 *
 * This file addresses the need for a common set of kernel definitions that can
 * be safely used across different compilation contexts:
 * - **Firmware Context**: Host-side firmware code that configures and controls kernels
 * - **Kernel Context**: Tensix-side kernel code that executes on the processing units
 *
 * # Included Headers
 *
 * This file includes only headers that provide:
 * - Type definitions and constants
 * - Structure definitions without implementation
 * - Hardware register definitions
 * - Instruction parameter definitions
 *
 * # Safe Inclusion Policy
 *
 * The headers included here are carefully selected to avoid:
 * - Hardware-specific implementations that only work on Tensix
 * - Inline assembly or low-level hardware operations
 * - Thread-specific functionality that assumes kernel execution context
 *
 * # Usage Pattern
 *
 * ```cpp
 * // In firmware code
 * #include "ckernel_include.h"
 * // Use definitions for kernel configuration
 * addr_mod_t config = {.srca = {.incr = 1}};
 *
 * // In kernel code
 * #include "ckernel_include.h"
 * // Use same definitions for kernel execution
 * configure_addressing(config);
 * ```
 *
 * @note This header is intended for shared definitions only
 * @note For kernel-specific functionality, include individual headers directly
 *
 * @see ckernel.h for full kernel infrastructure (kernel context only)
 * @see Individual headers for specific functionality
 */

#pragma once

// Core kernel definitions and types - safe for both firmware and kernel contexts

#include "ckernel_addrmod.h"      ///< Address modification structures and constants
#include "ckernel_defs.h"         ///< Core kernel definitions, enums, and constants
#include "ckernel_gpr_map.h"      ///< General Purpose Register mapping definitions
#include "ckernel_instr_params.h" ///< Instruction parameter structures and constants
#include "ckernel_structs.h"      ///< Common kernel data structures
#include "tensix.h"               ///< Tensix hardware definitions and constants
