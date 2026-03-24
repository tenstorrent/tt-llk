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
// Implements leaky relu which return x when x > 0 and x*slope when x < 0
inline void _calculate_lrelu_(const int iterations, const std::uint32_t slope, const std::uint32_t dst_index_in = 0, const std::uint32_t dst_index_out = 0)
{
    constexpr std::uint32_t dst_tile_size = 64; // Tile32x32 on Quasar
    const std::uint32_t in_offset         = dst_index_in * dst_tile_size;
    const std::uint32_t out_offset        = dst_index_out * dst_tile_size;

    TTI_SFPLOADI(p_sfpu::LREG2, 0 /*Float16_b*/, (slope >> 16)); // store slope in LREG2
    TTI_SFPENCC(1, 2);                                           // enable cc
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        TT_SFPLOAD(
            p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, in_offset + (d << 1)); // load from dest into lreg[0], uses ADDR_MOD_7 (set to all zeroes)

        TTI_SFPSETCC(0, p_sfpu::LREG0, 0); // condition - if value in LREG0 is negative //will set cc result reg

        TTI_SFPMAD(p_sfpu::LREG0, p_sfpu::LREG2, p_sfpu::LCONST_0, p_sfpu::LREG0, 0); // Multiply and add - LREG0 * LREG2 + LCONST_0 (x * slope + 0)

        TTI_SFPENCC(0, 0); // clear cc result reg

        // Store from lreg0 into dest register
        TT_SFPSTORE(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, out_offset + (d << 1));
    }
    TTI_SFPENCC(0, 2); // disable cc
}

// Implements relu min which returns x when x > threshold, otherwise return 0
inline void _calculate_relu_min_(
    const int iterations, const std::uint32_t threshold, const std::uint32_t dst_index_in = 0, const std::uint32_t dst_index_out = 0)
{
    constexpr std::uint32_t dst_tile_size = 64; // Tile32x32 on Quasar
    const std::uint32_t in_offset         = dst_index_in * dst_tile_size;
    const std::uint32_t out_offset        = dst_index_out * dst_tile_size;

    TTI_SFPLOADI(p_sfpu::LREG2, 0 /*Float16_b*/, (threshold >> 16)); // store slope in LREG2
    TTI_SFPENCC(1, 2);                                               // enable cc
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        TT_SFPLOAD(
            p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, in_offset + (d << 1)); // load from dest into lreg[0], uses ADDR_MOD_7 (set to all zeroes)

        // where lreg0 < threshold, load 0 into LREG0[x]
        TTI_SFPGT(0, p_sfpu::LREG0, p_sfpu::LREG2, 0x1 /*cc mode*/);

        TTI_SFPLOADI(p_sfpu::LREG0, 0, 0); // Load 0 into lreg0[x]

        TTI_SFPENCC(0, 0); // clear cc result reg

        // Store from lreg0 into dest register
        TT_SFPSTORE(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, out_offset + (d << 1));
    }
    TTI_SFPENCC(0, 2); // disable cc
}

// Implements relu max
inline void _calculate_relu_max_(
    const int iterations, const std::uint32_t threshold, const std::uint32_t dst_index_in = 0, const std::uint32_t dst_index_out = 0)
{
    constexpr std::uint32_t dst_tile_size = 64; // Tile32x32 on Quasar
    const std::uint32_t in_offset         = dst_index_in * dst_tile_size;
    const std::uint32_t out_offset        = dst_index_out * dst_tile_size;

    TTI_SFPLOADI(p_sfpu::LREG2, 0 /*Float16_b*/, (threshold >> 16)); // store slope in LREG2
    TTI_SFPENCC(1, 2);                                               // enable cc
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        TT_SFPLOAD(
            p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, in_offset + (d << 1)); // load from dest into lreg[0], uses ADDR_MOD_7 (set to all zeroes)

        // If x > threshold, x = threshold
        TTI_SFPGT(0, p_sfpu::LREG2, p_sfpu::LREG0, 0x1 /*cc mode*/);

        TTI_SFPMOV(p_sfpu::LREG2 /*src*/, p_sfpu::LREG0 /*dest*/, 0); // copy threshold value into LREG0 where LREG0 > threshold

        // Classic Relu: Max(x, 0)
        TTI_SFPSETCC(0, p_sfpu::LREG0, 0); // condition - if value in LREG0 is negative

        TTI_SFPLOADI(p_sfpu::LREG0, 0, 0); // Load 0 into lreg0[x]

        TTI_SFPENCC(0, 0); // reset cc

        // Store from lreg0 into dest register
        TT_SFPSTORE(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, out_offset + (d << 1));
    }
    TTI_SFPENCC(0, 2); // disable cc
}

} // namespace sfpu
} // namespace ckernel
