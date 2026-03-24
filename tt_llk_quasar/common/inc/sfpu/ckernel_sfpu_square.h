// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_ops.h"
#include "ckernel_trisc_common.h"
#include "cmath_common.h"

namespace ckernel
{
namespace sfpu
{
inline void _calculate_square_(const int iterations, const std::uint32_t dst_index_in = 0, const std::uint32_t dst_index_out = 0)
{
    constexpr std::uint32_t dst_tile_size = 64; // Tile32x32 on Quasar
    const std::uint32_t in_offset         = dst_index_in * dst_tile_size;
    const std::uint32_t out_offset        = dst_index_out * dst_tile_size;

#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        TT_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, in_offset + (d << 1)); // load from dest into lreg[0]
        // Multiply LREG0 * LREG0, store result in LREG0
        TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG0, p_sfpu::LCONST_0, p_sfpu::LREG0, 0);
        // Store result back to destination
        TT_SFPSTORE(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, out_offset + (d << 1));
    }
}

} // namespace sfpu
} // namespace ckernel
