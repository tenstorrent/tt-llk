// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_addrmod.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _mul_int_(const std::uint32_t dst_index_in0, const std::uint32_t dst_index_in1, const std::uint32_t dst_index_out)
{
    // Non-LOADMACRO implementation using SFPMUL24 directly.
    // Throughput is lower than LOADMACRO version but correctness is preserved.

    constexpr int a = p_sfpu::LREG0;
    constexpr int b = p_sfpu::LREG1;

    const int offset0   = (dst_index_in0 * 32) << 1;
    const int offset1   = (dst_index_in1 * 32) << 1;
    const int offset_out = (dst_index_out * 32) << 1;

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        TT_SFPLOAD(a, LO16, ADDR_MOD_7, offset0);
        TT_SFPLOAD(b, LO16, ADDR_MOD_7, offset1);
        TTI_SFPMUL24(a, 0, b, a, 0);
        TT_SFPSTORE(a, LO16, ADDR_MOD_6, offset_out);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
    TTI_SFPNOP;
}

template <bool APPROXIMATION_MODE>
inline void _init_mul_int_()
{
    // No constants or macro sequences needed for SFPMUL24-based implementation.
}

} // namespace sfpu
} // namespace ckernel
