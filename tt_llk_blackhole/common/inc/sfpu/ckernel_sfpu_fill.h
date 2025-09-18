// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_ops.h"
#include "ckernel_sfpu_converter.h"
#include "ckernel_sfpu_load_config.h"
#include "llk_defs.h"

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
inline void _calculate_fill_int_(const int value)
{
    // SFPU microcode
    sfpi::vInt fill_val = value;

    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::dst_reg[0] = fill_val;
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

<<<<<<< Updated upstream
inline void _populate_first_tile_with_ones_()
{
    // ADDR_MOD_1 has .dest = {.incr = 1}, so we use this auto-increment behavior
    // Instead of manually calculating row offsets, let the auto-increment handle addressing
    
    // Store to all positions in the first tile
    // Each SFPSTORE handles 4 consecutive rows with even/odd column pattern
    // We need 16 stores total: 8 for even columns + 8 for odd columns = 32 rows covered
    
    // Store to even columns (8 SFPSTORE operations for 32 rows)
    // for (uint i = 0; i < 8; i++) {
    //     TTI_SFPSTORE(p_sfpu::LCONST_1, ckernel::InstrModLoadStore::FP16A, ADDR_MOD_1, i * 4);
    // }
    
    // Store to odd columns (8 SFPSTORE operations for 32 rows)
    // for (uint i = 0; i < 8; i++) {
    //     TTI_SFPSTORE(p_sfpu::LCONST_1, ckernel::InstrModLoadStore::FP16A, ADDR_MOD_1, i * 4 + 2);
    // }

    // since the code above does not work, we are using the already existing replacement
    // todo: fix this method to use hardware constant 1.0
=======
inline void _populate_first_tile_with_ones_() {
>>>>>>> Stashed changes
    _calculate_fill_<false, 1024>(1.0f);
}
} // namespace ckernel::sfpu
