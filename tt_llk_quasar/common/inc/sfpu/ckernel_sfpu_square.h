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
// Calculates SQUARE for number of rows of output SFPU ops (Quasar = 2 rows)
template <bool APPROXIMATION_MODE>
inline void _calculate_square_sfp_rows_(const std::uint32_t dst_offset_in, const std::uint32_t dst_offset_out)
{
    TT_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, dst_offset_in); // load from dest into lreg[0]
    // Multiply LREG0 * LREG0, store result in LREG0
    TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG0, p_sfpu::LCONST_0, p_sfpu::LREG0, 0);
    // Store result back to destination
    TT_SFPSTORE(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, dst_offset_out);
}

inline void _calculate_square_(const int iterations, const std::uint32_t dst_index_in, const std::uint32_t dst_index_out)
{
    constexpr std::uint32_t dst_tile_size = 64;
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_square_sfp_rows_<false>(dst_index_in * dst_tile_size, dst_index_out * dst_tile_size);
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
}

} // namespace sfpu
} // namespace ckernel
