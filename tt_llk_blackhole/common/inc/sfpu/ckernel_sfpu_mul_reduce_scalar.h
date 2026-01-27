// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_ops.h"
#include "ckernel_sfpu_converter.h"
#include "ckernel_sfpu_load_config.h"

namespace ckernel::sfpu
{

/**
 * @brief Populate a single row of dest[0] with a specific bfloat16 value
 *
 * This function is used in mul_reduce_scalar to populate srcB with ones (row 0 of dest[0])
 * or to clear the accumulator (row 0 of dest[0] with zeros).
 *
 * @param row Row index (0-31) within the tile
 * @param bfloat16_value Value in bfloat16 format (e.g., 0x3F80 for 1.0, 0x0000 for 0.0)
 */
inline void populate_dest0_row_with_value_(uint row, uint16_t bfloat16_value)
{
    // Reset destination counter to ensure we're pointing to tile 0
    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);

    // Load constant value into LREG0 (bfloat16 format)
    TTI_SFPLOADI(p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_FLOATB, bfloat16_value);

    // Fill the specified row of Dest[0] using ADDR_MOD_7 addressing to prevent tile auto-advance
    // Row addressing pattern (same as reshuffle_rows):
    //   base        -> faces 0/2, even cols (columns 0,2,4,6,8,10,12,14)
    //   base+2      -> faces 0/2, odd  cols (columns 1,3,5,7,9,11,13,15)
    //   base+16     -> faces 1/3, even cols (columns 0,2,4,6,8,10,12,14)
    //   base+18     -> faces 1/3, odd  cols (columns 1,3,5,7,9,11,13,15)
    TT_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, row);      // faces 0/2, even cols
    TT_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, row + 2);  // faces 0/2, odd cols
    TT_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, row + 16); // faces 1/3, even cols
    TT_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, row + 18); // faces 1/3, odd cols

    // Clean-up
    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);
}

} // namespace ckernel::sfpu
