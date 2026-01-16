// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <sys/_stdint.h>

#include "ckernel_sfpu_load_config.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_tanh_(const int iterations)
{
    if constexpr (APPROXIMATION_MODE)
    {
        sfpi::vUInt l0 = sfpi::l_reg[sfpi::LRegs::LReg0];
        sfpi::vUInt l1 = sfpi::l_reg[sfpi::LRegs::LReg1];
        sfpi::vUInt l2 = sfpi::l_reg[sfpi::LRegs::LReg2];

#pragma GCC unroll 8
        for (int d = 0; d < iterations; d++)
        {
            sfpi::vFloat val = sfpi::dst_reg[0];
            val              = lut(val, l0, l1, l2);
            sfpi::dst_reg[0] = val;
            sfpi::dst_reg++;
        }

        sfpi::l_reg[sfpi::LRegs::LReg0] = l0;
        sfpi::l_reg[sfpi::LRegs::LReg1] = l1;
        sfpi::l_reg[sfpi::LRegs::LReg2] = l2;
    }
    else
    {
        constexpr float C0 = 0.999f;
        constexpr float C1 = -1.0f / 3.0f;
        constexpr float C2 = 2.0f / 15.0f;
        constexpr float C3 = -17.0f / 315.0f;

#pragma GCC unroll 8
        for (int d = 0; d < iterations; d++)
        {
            sfpi::vFloat x  = sfpi::dst_reg[0];
            sfpi::vFloat x2 = x * x;

            // sfpi::vFloat p = (C2 * x2 + C1) * x2 + C0;
            sfpi::vFloat p = ((C3 * x2 + C2) * x2 + C1) * x2 + C0;

            sfpi::vFloat y = x * p;

            sfpi::dst_reg[0] = y;
            sfpi::dst_reg++;
        }
    }
}

template <bool APPROXIMATION_MODE>
inline void _init_tanh_()
{
    if constexpr (APPROXIMATION_MODE)
    {
        uint32_t imm0 = 0x1DFF; // 0.90625*x
        uint32_t imm1 = 0x481A; // 0.09375*x + 0.8125
        uint32_t imm2 = 0xFF00; // 1
        _sfpu_load_imm16_(0, imm0);
        _sfpu_load_imm16_(1, imm1);
        _sfpu_load_imm16_(2, imm2);
    }
}

} // namespace sfpu
} // namespace ckernel
