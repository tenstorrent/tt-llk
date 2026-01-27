// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_addrmod.h"
#include "ckernel_instr_params.h"
#include "ckernel_sfpu_load_config.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Load a constant value (3.0f) into the first row of the SFPU tile.
 *
 * This kernel loads a constant floating-point value (3.0f) into the first row
 * (row 0) of a 32x32 tile. The first row spans 32 columns across Face 0 and Face 1.
 *
 * Tile layout (32x32):
 *   Face 0 (cols 0-15)  | Face 1 (cols 16-31)
 *   Face 2 (cols 0-15)  | Face 3 (cols 16-31)
 *
 * Row 0 = Face 0 row 0 (16 cols) + Face 1 row 0 (16 cols) = 32 columns total
 *
 * SFPU addressing for first row:
 *   - offset 0:  Face 0, row 0, even columns (0,2,4,6,8,10,12,14)
 *   - offset 2:  Face 0, row 0, odd columns (1,3,5,7,9,11,13,15)
 *   - offset 16: Face 1, row 0, even columns (16,18,20,22,24,26,28,30)
 *   - offset 18: Face 1, row 0, odd columns (17,19,21,23,25,27,29,31)
 *
 * Each SFPSTORE writes 4 datums (row 0 values replicated across 4 vector lanes).
 *
 * @tparam APPROXIMATION_MODE Unused, kept for API consistency with other SFPU ops.
 */
template <bool APPROXIMATION_MODE>
inline void _calculate_load_const_first_row_()
{
    // Size of each tile in Dest is 64 rows (4 faces × 16 rows per face)

    // The value 3.0f in IEEE 754 FP32 is 0x40400000
    // Binary: 0 10000000 10000000000000000000000
    //   Sign: 0 (positive)
    //   Exponent: 10000000 = 128 (bias 127, so actual exp = 1)
    //   Mantissa: 1.1 = 1.5, so 1.5 * 2^1 = 3.0

    // Load constant into LREG0 using the helper function
    TTI_SFPLOADI(p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_FLOATB, 0x4040);

    // Store constant to first row of Face 0 (columns 0-15)
    // and first row of Face 1 (columns 16-31)
    // We write to offsets: 0, 2, 16, 18
    // TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);
    TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP16B, ADDR_MOD_3, 0); // Face 0, row 0, even cols
    TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP16B, ADDR_MOD_3, 2); // Face 0, row 0, odd cols
    // TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP32, ADDR_MOD_3, tile_offset + 16);  // Face 1, row 0, even cols
    // TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP32, ADDR_MOD_3, tile_offset + 18);  // Face 1, row 0, odd cols
}

/**
 * @brief Initialize function for load_const_first_row operation.
 *
 * Sets up any required SFPU configuration. Currently a no-op as this
 * kernel doesn't require special initialization.
 */
inline void _init_load_const_first_row_()
{
    // No special initialization required for this simple operation
}

} // namespace sfpu
} // namespace ckernel
