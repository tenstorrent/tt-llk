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
inline void _calculate_negative_(const std::uint32_t dst_index_in, const std::uint32_t dst_index_out)
{
    constexpr std::uint32_t dst_tile_size_sfpi = 32;
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat val                                  = sfpi::dst_reg[dst_index_in * dst_tile_size_sfpi];
        sfpi::dst_reg[dst_index_out * dst_tile_size_sfpi] = -val;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_negative_int_(const std::uint32_t dst_index_in, const std::uint32_t dst_index_out)
{
    constexpr std::uint32_t dst_tile_size_sfpi = 32;
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vInt val = sfpi::dst_reg[dst_index_in * dst_tile_size_sfpi];
        v_if (val != 0)
        {
            sfpi::dst_reg[dst_index_out * dst_tile_size_sfpi] = sfpi::reinterpret<sfpi::vInt>(-sfpi::reinterpret<sfpi::vFloat>(val));
        }
        v_endif;
        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
