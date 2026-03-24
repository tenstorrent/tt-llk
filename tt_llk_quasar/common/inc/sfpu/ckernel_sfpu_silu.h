// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_ops.h"
#include "ckernel_sfpu_sigmoid.h"
#include "ckernel_trisc_common.h"
#include "cmath_common.h"

namespace ckernel
{
namespace sfpu
{
inline void _calculate_silu_(const int iterations, const std::uint32_t dst_index_in = 0, const std::uint32_t dst_index_out = 0)
{
    constexpr std::uint32_t dst_tile_size = 64; // Tile32x32 on Quasar
    const std::uint32_t in_offset         = dst_index_in * dst_tile_size;
    const std::uint32_t out_offset        = dst_index_out * dst_tile_size;

#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        TT_SFPLOAD(
            p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, in_offset + (d << 1)); // load from dest into lreg[0], uses ADDR_MOD_7 (set to all zeroes)

        // Calculate sigmoid using lreg[0] as src, lreg[1] as work register, and lreg[2] as dest (since we need the original value for the final multiply)
        _calculate_sigmoid_regs_(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LREG2);
        TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG2, p_sfpu::LCONST_0, p_sfpu::LREG1, 0); // Multiply lreg[0] * lreg[2], store result in lreg[1]

        TT_SFPSTORE(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, out_offset + (d << 1)); // store from lreg[1] into dest register
    }
}

} // namespace sfpu
} // namespace ckernel
