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
inline void _calculate_typecast_fp16b_to_uint16_(const int iterations, const std::uint32_t dst_index_in = 0, const std::uint32_t dst_index_out = 0)
{
    constexpr std::uint32_t dst_tile_size = 64; // Tile32x32 on Quasar
    const std::uint32_t in_offset         = dst_index_in * dst_tile_size;
    const std::uint32_t out_offset        = dst_index_out * dst_tile_size;

#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        TT_SFPLOAD(
            p_sfpu::LREG0, p_sfpu::sfpmem::FP16B, ADDR_MOD_7, 0, in_offset + (d << 1)); // load from dest into lreg[0], uses ADDR_MOD_7 (set to all zeroes)

        TTI_SFPENCC(0, 1);                 // CC_en <= 1, CC_res <= 1 # TODO - Luka: why do we need this? Why can't we just skip this?
        TTI_SFPSETCC(0, p_sfpu::LREG0, 0); // CC_res <= (LREG0 < 0)
        TTI_SFPLOADI(p_sfpu::LREG0, 0, 0); // loads zeros where lreg[0] is negative
        TTI_SFPENCC(0, 1);                 // CC_en <= 1, CC_res <= 1 (for all)

        TTI_SFP_STOCH_RND(
            p_sfpu::sfp_stochrnd_rnd_mod::NearEven, 0, 0, p_sfpu::LREG0, p_sfpu::LREG1, (1 << 3) | ckernel::p_sfpu::sfp_stochrnd_mod::FP32_TO_UINT16);

        TT_SFPSTORE(p_sfpu::LREG1, p_sfpu::sfpmem::UINT16, ADDR_MOD_7, 0, out_offset + (d << 1)); // Store from lreg[1] into dest register
    }
}

} // namespace sfpu
} // namespace ckernel
