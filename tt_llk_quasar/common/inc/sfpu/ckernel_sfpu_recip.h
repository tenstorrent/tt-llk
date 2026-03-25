// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>

#include "ckernel_trisc_common.h"
#include "cmath_common.h"

namespace ckernel
{
namespace sfpu
{
template <bool APPROXIMATION_MODE>
inline void _calculate_reciprocal_(const int iterations, const std::uint32_t dst_index_in = 0, const std::uint32_t dst_index_out = 0)
{
    constexpr std::uint32_t dst_tile_size = 64; // Tile32x32 on Quasar
    const std::uint32_t in_offset         = dst_index_in * dst_tile_size;
    const std::uint32_t out_offset        = dst_index_out * dst_tile_size;

#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        TT_SFPLOAD(
            p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, in_offset + (d << 1)); // load from dest into lreg[0], uses ADDR_MOD_7 (set to all zeroes)

        // SFPARECIP, approx version of reciprocal
        if constexpr (APPROXIMATION_MODE)
        {
            TTI_SFPNONLINEAR(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpnonlinear::RECIP_MODE); // Read value from lreg[0], approximate recip, load back into lreg[1]
        }

        // Store from lreg[1] into dest register
        TT_SFPSTORE(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, out_offset + (d << 1));
    }
}

} // namespace sfpu
} // namespace ckernel
