// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_ops.h"
#include "ckernel_sfpu_converter.h"
#include "ckernel_sfpu_load_config.h"

namespace ckernel::sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_fill_(const float value)
{
    // SFPU microcode
    sfpi::vFloat fill_val = value;

    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::dst_reg[0] = fill_val;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_fill_int_(const uint value)
{
    // SFPU microcode
    _sfpu_load_imm32_(p_sfpu::LREG1, value);

    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_SFPSTORE(p_sfpu::LREG1, InstrModLoadStore::INT32, ADDR_MOD_3, 0);
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_fill_bitcast_(const uint32_t value_bit_mask)
{
    // SFPU microcode
    sfpi::vFloat fill_val = Converter::as_float(value_bit_mask);

    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::dst_reg[0] = fill_val;
        sfpi::dst_reg++;
    }
}

inline void _populate_first_tile_with_ones_()
{
    // Second tile starts at offset 64 (32x32 tile = 64 rows in dest layout)  
    constexpr uint first_tile_offset = 0;  
      
    // Populate the entire first tile with LCONST_1 (value 1.0)  
    for (uint row = 0; row < 32; row += 4) {  
        // Store LCONST_1 to all positions in the first tile  
        TT_SFPSTORE(p_sfpu::LCONST_1, 0, ADDR_MOD_3, first_tile_offset + row);  
        TT_SFPSTORE(p_sfpu::LCONST_1, 0, ADDR_MOD_3, first_tile_offset + row + 2);  
        TT_SFPSTORE(p_sfpu::LCONST_1, 0, ADDR_MOD_3, first_tile_offset + row + 32);  
        TT_SFPSTORE(p_sfpu::LCONST_1, 0, ADDR_MOD_3, first_tile_offset + row + 34);  
    }  
}
} // namespace ckernel::sfpu
