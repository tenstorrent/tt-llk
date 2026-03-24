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
// Calculates SIGMOID for results already in registers, used as a helper function.
// Input of sigmoid is expected to already be in src_reg, and the result will be stored in dest_reg.
// work_reg is an intermediate register used for calculations.
inline void _calculate_sigmoid_regs_(const std::uint32_t src_reg, const std::uint32_t work_reg, const std::uint32_t dest_reg)
{
    // ALthough SFPMUL/SFPADD are 2 cycle instructions, we don't need a TTI_NOP
    // because the hardware implicitly stalls if the next instruction depends on results
    TTI_SFPMOV(src_reg, work_reg, 1);                                      // Copy src_reg to work_reg and invert sign bit (take negative of input)
    TTI_SFPNONLINEAR(work_reg, dest_reg, p_sfpnonlinear::EXP_MODE);        // Read value from work_reg, take exponential, load back into dest_reg
    TTI_SFPADD(p_sfpu::LCONST_1, dest_reg, p_sfpu::LCONST_1, work_reg, 0); // Add 1 to dest_reg, store in work_reg, takes 2 cycles
    TTI_SFPNONLINEAR(work_reg, dest_reg, p_sfpnonlinear::RECIP_MODE);      // Read value from work_reg, approximate recip, load back into dest_reg
}

inline void _calculate_sigmoid_(const int iterations, const std::uint32_t dst_index_in = 0, const std::uint32_t dst_index_out = 0)
{
    constexpr std::uint32_t dst_tile_size = 64; // Tile32x32 on Quasar
    const std::uint32_t in_offset         = dst_index_in * dst_tile_size;
    const std::uint32_t out_offset        = dst_index_out * dst_tile_size;

#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        TT_SFPLOAD(
            p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, in_offset + (d << 1)); // load from dest into lreg[0], uses ADDR_MOD_7 (set to all zeroes)

        _calculate_sigmoid_regs_(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LREG0); // calculate sigmoid using lreg[0] as src and dest, and lreg[1] as work register

        TT_SFPSTORE(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, out_offset + (d << 1)); // store from lreg[0] into dest register
    }
}

} // namespace sfpu
} // namespace ckernel
