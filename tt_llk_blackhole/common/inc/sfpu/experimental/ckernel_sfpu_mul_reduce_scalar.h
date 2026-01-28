// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../../ckernel_ops.h"
#include "../ckernel_sfpu_load_config.h"

namespace ckernel::sfpu
{

/**
 * @brief Populate a single row of dest[0] with a specific bfloat16 value
 *
 * @param row Row index (0-31) within the tile
 * @param bfloat16_value Value in bfloat16 format (e.g., 0x3F80 for 1.0, 0x0000 for 0.0)
 */
inline void populate_dest0_row_with_value_(uint row, uint16_t bfloat16_value)
{
    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);
    TTI_SFPLOADI(p_sfpu::LREG0, sfpi::SFPLOADI_MOD0_FLOATB, bfloat16_value);

    TT_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, row);
    TT_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_7, row + 2);

    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);
}

} // namespace ckernel::sfpu
