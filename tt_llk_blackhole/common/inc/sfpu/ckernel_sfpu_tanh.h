// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel.h"
#include "ckernel_defs.h"
#include "ckernel_sfpu_load_config.h"
#include "sfpi.h"

using namespace sfpi;

namespace ckernel {
namespace sfpu {

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_tanh_(const int iterations) {
    // SFPU microcode
    vstd::uint32_tl0 = l_reg[LRegs::LReg0];
    vstd::uint32_tl1 = l_reg[LRegs::LReg1];
    vstd::uint32_tl2 = l_reg[LRegs::LReg2];

#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++) {
        vFloat val = dst_reg[0];
        val        = lut(val, l0, l1, l2);
        dst_reg[0] = val;

        dst_reg++;
    }

    l_reg[LRegs::LReg0] = l0;
    l_reg[LRegs::LReg1] = l1;
    l_reg[LRegs::LReg2] = l2;
}

template <bool APPROXIMATION_MODE>
inline void _init_tanh_() {
    std::uint32_timm0;
    std::uint32_timm1;
    std::uint32_timm2;
    imm0 = 0x1DFF; // 0.90625*x
    imm1 = 0x481A; // 0.09375*x + 0.8125
    imm2 = 0xFF00; // 1
    _sfpu_load_imm16_(0, imm0);
    _sfpu_load_imm16_(1, imm1);
    _sfpu_load_imm16_(2, imm2);
}

} // namespace sfpu
} // namespace ckernel
