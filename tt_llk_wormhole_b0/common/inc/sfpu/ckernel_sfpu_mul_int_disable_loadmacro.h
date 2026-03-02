// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
// SPDX-FileCopyrightText: © 2026 Jason Davies <jason@jasondavies.com>
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
    // Non-LOADMACRO implementation of uint16 x uint16 → uint16 (low 16 bits of product).
    //
    // Splits u16 inputs a and b into a = (a1 << 8) | a0 and b = (b1 << 8) | b0 (u8 bytes),
    // casts to FP32, then computes:
    //   lo  = a0*b0 + 2^23   (exact integer via mantissa bits)
    //   hi  = a0*b1 + 2^23 + a1*b0  (exact integer via mantissa bits)
    //   result_L16 = exman(lo) | (exman(hi) << 8)
    //
    // LREG12 = 0xff, LREG13 = 2^23 (set by _init_mul_int_).

    const int offset0   = (dst_index_in0 * 32) << 1;
    const int offset1   = (dst_index_in1 * 32) << 1;
    const int offset_out = (dst_index_out * 32) << 1;

    constexpr int a0  = p_sfpu::LREG0;
    constexpr int b0  = p_sfpu::LREG1;
    constexpr int a1  = p_sfpu::LREG2;
    constexpr int b1  = p_sfpu::LREG3;
    constexpr int out = p_sfpu::LREG4;
    constexpr int tmp = p_sfpu::LREG5;

#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        // Load 16-bit inputs
        TT_SFPLOAD(b1, InstrModLoadStore::LO16, ADDR_MOD_3, offset1);
        TT_SFPLOAD(a0, InstrModLoadStore::LO16, ADDR_MOD_3, offset0);

        // Split b into bytes: b1 = hi byte, b0 = lo byte
        TTI_SFPSHFT2(-8 & 0xfff, 0, b1, 6); // b1 >>= 8 (SFPSHFT2_MOD1_SHFT_IMM)
        TTI_SFPAND(0, p_sfpu::LREG12, b1, 0); // b1 &= 0xff  (clamp to byte before cast)
        TTI_SFPCAST(b1, b1, 0);               // b1 = float(b1)

        // Split a into bytes: a0 = lo, a1 = hi
        TT_SFPLOAD(b0, InstrModLoadStore::LO16, ADDR_MOD_3, offset1);
        TTI_SFPAND(0, p_sfpu::LREG12, a0, 0); // a0 &= 0xff
        TTI_SFPCAST(a0, a0, 0);               // a0 = float(a0)
        TTI_SFPAND(0, p_sfpu::LREG12, b0, 0); // b0 &= 0xff
        TTI_SFPCAST(b0, b0, 0);               // b0 = float(b0)

        TT_SFPLOAD(a1, InstrModLoadStore::LO16, ADDR_MOD_3, offset0);
        TTI_SFPSHFT2(-8 & 0xfff, 0, a1, 6); // a1 >>= 8
        TTI_SFPAND(0, p_sfpu::LREG12, a1, 0); // a1 &= 0xff
        TTI_SFPCAST(a1, a1, 0);              // a1 = float(a1)

        // Compute products using magic constant 2^23:
        // b1 (hi) = a0*b1 + 2^23
        TTI_SFPMAD(a0, b1, p_sfpu::LREG13, b1, 0);
        // a0 (lo) = a0*b0 + 2^23
        TTI_SFPMAD(a0, b0, p_sfpu::LREG13, a0, 0);
        // b1 (hi) += a1*b0
        TTI_SFPMAD(a1, b0, b1, b1, 0);

        // Extract low 16 bits and hi 16 bits via mantissa
        TTI_SFPEXMAN(0, a0, tmp, sfpi::SFPEXMAN_MOD1_PAD9); // tmp = mantissa(lo)
        TTI_SFPEXMAN(0, b1, out, sfpi::SFPEXMAN_MOD1_PAD9); // out = mantissa(hi)

        TTI_SFPSHFT2(8, 0, out, 6);                         // out <<= 8 (hi byte into upper byte position)
        TTI_SFPIADD(0, tmp, out, sfpi::SFPIADD_MOD1_CC_NONE); // out |= tmp

        TT_SFPSTORE(out, InstrModLoadStore::LO16, ADDR_MOD_2, offset_out);
    }
    TTI_SFPNOP;
    TTI_SFPNOP;
    TTI_SFPNOP;
}

template <bool APPROXIMATION_MODE>
inline void _init_mul_int_()
{
    // Set constants used by _mul_int_; no macro sequences needed.
    sfpi::vConstIntPrgm0   = 0xff;      // LREG12: byte mask
    sfpi::vConstFloatPrgm1 = 8388608.0; // LREG13: 2^23 magic constant
}

} // namespace sfpu
} // namespace ckernel
