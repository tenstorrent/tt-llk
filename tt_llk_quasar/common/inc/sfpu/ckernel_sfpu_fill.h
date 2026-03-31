// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
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
// Stores fill value (pre-loaded in LREG1) to current Dest rows using implied format
inline void _calculate_fill_sfp_rows_()
{
    TTI_SFPSTORE(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
}

// Implements element-wise fill with a floating-point constant value
inline void _calculate_fill_(const int iterations, const std::uint32_t value)
{
    TTI_SFPLOADI(p_sfpu::LREG1, 0 /*FP16_B*/, (value >> 16)); // load fill constant into LREG1
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_fill_sfp_rows_();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
}

// Implements element-wise fill with an integer constant value
// store_mode must be a compile-time constant (template parameter) because TTI_SFPSTORE
// encodes it directly into the instruction word via inline assembly.
template <std::uint32_t store_mode>
inline void _calculate_fill_int_(const int iterations, const std::uint32_t value)
{
    // Load the integer constant into LREG1
    if constexpr (store_mode == p_sfpu::sfpmem::INT32)
    {
        // 32-bit integer: load in two halves
        TTI_SFPLOADI(p_sfpu::LREG1, 10, (value & 0xFFFF));        // LO16_ONLY: write lower 16 bits
        TTI_SFPLOADI(p_sfpu::LREG1, 8, ((value >> 16) & 0xFFFF)); // HI16_ONLY: write upper 16 bits
    }
    else
    {
        // 16-bit or 8-bit integer: load as UINT16 (zero-extended to 32 bits)
        TTI_SFPLOADI(p_sfpu::LREG1, 2, (value & 0xFFFF)); // UINT16: zero-extend to UINT32
    }

    // Store to all Dest rows with explicit integer format
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        TTI_SFPSTORE(p_sfpu::LREG1, store_mode, ADDR_MOD_7, 0, 0);
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
}

// Implements element-wise fill with a raw 32-bit bit pattern (bitcast to float)
// Loads the full 32-bit value via two SFPLOADI calls (LO16 + HI16) and stores
// with implied format, preserving all 32 bits in the destination.
inline void _calculate_fill_bitcast_(const int iterations, const std::uint32_t value_bit_mask)
{
    // Load the full 32-bit bit pattern into LREG1 via two partial writes
    TTI_SFPLOADI(p_sfpu::LREG1, 10, (value_bit_mask & 0xFFFF));        // LO16_ONLY: write lower 16 bits
    TTI_SFPLOADI(p_sfpu::LREG1, 8, ((value_bit_mask >> 16) & 0xFFFF)); // HI16_ONLY: write upper 16 bits

#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        TTI_SFPSTORE(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
}

} // namespace sfpu
} // namespace ckernel
