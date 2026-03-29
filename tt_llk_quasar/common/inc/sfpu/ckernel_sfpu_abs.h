// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_ops.h"
#include "ckernel_trisc_common.h"
#include "cmath_common.h"

namespace ckernel
{
namespace sfpu
{
// Calculates ABS for number of rows of output SFPU ops (Quasar = 2 rows)
template <bool APPROXIMATION_MODE>
inline void _calculate_abs_sfp_rows_()
{
    TTI_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0);  // load from dest into lreg[0]
    TTI_SFPABS(p_sfpu::LREG0, p_sfpu::LREG0, 1);                            // FP32 abs: lreg[0] = abs(lreg[0]), instr_mod1=1 for FP32
    TTI_SFPSTORE(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, 0); // store back to dest
}

template <bool APPROXIMATION_MODE>
inline void _calculate_abs_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        _calculate_abs_sfp_rows_<APPROXIMATION_MODE>();
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>(); // does the dest_reg++ (increments by 2 rows)
    }
}

template <bool APPROXIMATION_MODE>
inline void _init_abs_()
{
    // No initialization needed for abs kernel — SFPU setup is handled by common infrastructure
}

} // namespace sfpu
} // namespace ckernel
