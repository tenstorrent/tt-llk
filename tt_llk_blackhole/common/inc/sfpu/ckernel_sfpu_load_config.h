// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_load_config.h
 * @brief SFPU configuration loading utilities for hardware setup
 *
 * @details This file provides utility functions for loading configuration values
 * and constants into SFPU registers. These utilities are essential for setting up
 * SFPU operations that require specific constants, coefficients, or configuration
 * parameters that cannot be efficiently encoded as immediate values.
 *
 * **Primary Function: _sfpu_load_imm32_**
 * Loads a full 32-bit immediate value into an SFPU register using a two-instruction
 * sequence to handle the 16-bit immediate limitation of individual SFPU instructions.
 *
 * **Implementation Strategy:**
 * The function splits 32-bit values into high and low 16-bit components:
 * 1. **Lower 16 bits**: Loaded using instruction modifier 10 (preserves upper bits)
 * 2. **Upper 16 bits**: Loaded using instruction modifier 8 (preserves lower bits)
 * 3. **Atomic Operation**: Ensures complete 32-bit value is loaded correctly
 *
 * **SFPU Instruction Details:**
 * - **TT_SFPLOADI with mod 10**: Writes lower 16 bits, preserves upper 16 bits
 * - **TT_SFPLOADI with mod 8**: Writes upper 16 bits, preserves lower 16 bits
 * - **Sequential Execution**: Two instructions combine to load full 32-bit value
 *
 * **Use Cases:**
 * - Loading polynomial coefficients for approximation functions
 * - Setting up lookup table parameters and scaling factors
 * - Configuring mathematical constants for complex operations
 * - Initializing SFPU registers with runtime-computed values
 *
 * **Performance Characteristics:**
 * - **Latency**: 2 SFPU instruction cycles for complete 32-bit load
 * - **Throughput**: Can load multiple 32-bit values in parallel to different registers
 * - **Precision**: Full 32-bit precision maintained throughout loading process
 * - **Efficiency**: Minimal instruction overhead for complex value loading
 *
 * **Integration with SFPU Functions:**
 * This utility is used throughout SFPU mathematical functions for:
 * - Mathematical constant initialization
 * - Runtime coefficient loading
 * - Configuration parameter setup
 * - Lookup table preparation
 */

#pragma once

#include "ckernel_ops.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

inline void _sfpu_load_imm32_(const uint dest, const uint val)
{
    TT_SFPLOADI(dest, 10, (val & 0xFFFF));      // insmod == 10 will write the lower bits, and not affect the upper bits;
    TT_SFPLOADI(dest, 8, (val >> 16) & 0xFFFF); // insmod == 8 will write the upper bits, and not affect the lower bits;
}

inline void _sfpu_load_imm16_(const uint dest, const uint val)
{
    TT_SFPLOADI(dest, 2, val); // insmod == 2 will write imm16 value treated as unsigned integer, right justified and padded with zeroes on the MSBs
}

inline void _sfpu_load_config32_(const uint dest, const uint upper16, const uint lower16)
{
    // registers 11 through 14 are programmable "constants" which are shared across all 4 rows
    // They are updated only through the CONFIG path, which uses LREG[0] first and then copies it to the desired register location
    TTI_SFPLOADI(0, 10, lower16); // insmod == A will write the lower bits, and not affect the upper bits;
    TTI_SFPLOADI(0, 8, upper16);  // insmod == 8 will write the upper bits, and not affect the lower bits;
    TTI_SFPCONFIG(0, dest, 0);
}

inline void _init_sfpu_config_reg()
{
    _sfpu_load_config32_(0xF, 0x0, 0x0);
}

} // namespace sfpu
} // namespace ckernel
