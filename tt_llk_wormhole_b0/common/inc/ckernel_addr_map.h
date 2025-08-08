// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_addr_map.h
 * @brief Memory Address Mapping for Wormhole B0 Tensix TRISC Parameters
 *
 * This header defines the memory address mapping for TRISC (Tensix RISC)
 * parameter storage in the local memory space. Each TRISC has its own
 * dedicated parameter area for kernel configuration and data.
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # Memory Layout
 *
 * The Tensix local memory is partitioned to provide dedicated parameter
 * storage areas for each of the three TRISC processors:
 *
 * - **TRISC 0 (Unpack)**: 32KB base address for unpack parameters
 * - **TRISC 1 (Math)**: 48KB base address for math parameters
 * - **TRISC 2 (Pack)**: 64KB base address for pack parameters
 *
 * Each parameter area provides 16KB of storage space (32KB, 48KB, 64KB boundaries).
 *
 * # Usage Pattern
 *
 * ```cpp
 * #include "ckernel_addr_map.h"
 *
 * // Access parameter base for specific TRISC
 * uint32_t unpack_params = params_base[0];  // TRISC 0
 * uint32_t math_params = params_base[1];    // TRISC 1
 * uint32_t pack_params = params_base[2];    // TRISC 2
 *
 * // Or use macros directly
 * uint32_t addr = TRISC1_PARAMS_BASE_ADDRESS + offset;
 * ```
 *
 * @note This is a temporary address mapping that may be subject to change
 * @note Each TRISC parameter area is 16KB in size
 *
 * @see ckernel_defs.h for TRISC thread definitions
 * @see ckernel_gpr_map.h for TRISC-specific register mappings
 */

#pragma once

/**
 * @defgroup AddressMapping TRISC Parameter Address Mapping
 * @brief Memory address definitions for TRISC parameter storage
 * @{
 */

/**
 * @brief Base address for TRISC 0 (Unpack) parameters
 *
 * Defines the base memory address for storing parameters used by the
 * unpack thread (TRISC 0). Located at 32KB boundary in local memory.
 */
#define TRISC0_PARAMS_BASE_ADDRESS 32 * 1024

/**
 * @brief Base address for TRISC 1 (Math) parameters
 *
 * Defines the base memory address for storing parameters used by the
 * math thread (TRISC 1). Located at 48KB boundary in local memory.
 */
#define TRISC1_PARAMS_BASE_ADDRESS 48 * 1024

/**
 * @brief Base address for TRISC 2 (Pack) parameters
 *
 * Defines the base memory address for storing parameters used by the
 * pack thread (TRISC 2). Located at 64KB boundary in local memory.
 */
#define TRISC2_PARAMS_BASE_ADDRESS 64 * 1024

/**
 * @brief Array of parameter base addresses indexed by TRISC ID
 *
 * Provides convenient array access to parameter base addresses:
 * - Index 0: TRISC 0 (Unpack) parameter base
 * - Index 1: TRISC 1 (Math) parameter base
 * - Index 2: TRISC 2 (Pack) parameter base
 *
 * @note Use ThreadId constants as indices for type safety
 */
uint params_base[3] = {TRISC0_PARAMS_BASE_ADDRESS, TRISC1_PARAMS_BASE_ADDRESS, TRISC2_PARAMS_BASE_ADDRESS};

/** @} */ // end of AddressMapping group
