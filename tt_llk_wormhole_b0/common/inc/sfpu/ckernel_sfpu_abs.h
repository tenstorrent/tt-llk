// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_abs_(const std::uint32_t dst_index_in, const std::uint32_t dst_index_out, const int iterations)
{
    constexpr std::uint32_t dst_tile_size_sfpi = 32;
    // SFPU microcode
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat v                                    = sfpi::dst_reg[dst_index_in * dst_tile_size_sfpi];
        sfpi::dst_reg[dst_index_out * dst_tile_size_sfpi] = sfpi::abs(v);
        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
