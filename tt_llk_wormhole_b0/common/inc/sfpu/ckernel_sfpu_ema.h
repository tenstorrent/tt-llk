// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstdint>
#include <type_traits>

#include "ckernel.h"
#include "ckernel_defs.h"
#include "sfpi.h"

sfpi_inline void _ema_clear_prev_output_()
{
    lltt::replay(0, 1);
}

sfpi_inline void _ema_load_prev_output_()
{
    lltt::replay(1, 1);
}

sfpi_inline void _ema_load_alpha_beta_()
{
    lltt::replay(2, 4);
}

template <uint32_t I, uint32_t J>
sfpi_inline void _ema_load_curr_input_()
{
    constexpr uint32_t offset1 = (I * 32) + (4 * J);
    constexpr uint32_t offset2 = offset1 + 2;
    constexpr uint32_t offset3 = offset1 + 16;
    constexpr uint32_t offset4 = offset1 + 18;
    TTI_SFPLOAD(ckernel::p_sfpu::LREG0, 0, ckernel::ADDR_MOD_3, offset1); /*row1*/
    TTI_SFPLOAD(ckernel::p_sfpu::LREG1, 0, ckernel::ADDR_MOD_3, offset2); /*row2*/
    TTI_SFPLOAD(ckernel::p_sfpu::LREG2, 0, ckernel::ADDR_MOD_3, offset3); /*row3*/
    TTI_SFPLOAD(ckernel::p_sfpu::LREG3, 0, ckernel::ADDR_MOD_3, offset4); /*row4*/
    lltt::replay(6, 1);
}

sfpi_inline void _ema_save_curr_output_()
{
    // saves data raw to dst reg
    lltt::replay(7, 1);
}

sfpi_inline void _compute_ema_math_()
{
    // EMA equation: EMA_new = α * EMA_old + (1-α) * input
    // Where: LREG0=input, LREG4=EMA_old, LREG5=α, LREG6=(1-α), LREG7=EMA_new
    // Thus, LREG7 = LREG5 * LREG4 + LREG6 * LREG0

    // Step 1: Calculate α * EMA_old in LREG7
    // LREG7 = LREG5 * LREG4 (α * EMA_old)
    TTI_SFPMUL(ckernel::p_sfpu::LREG5, ckernel::p_sfpu::LREG4, ckernel::p_sfpu::LCONST_0, ckernel::p_sfpu::LREG7, 0);
    // Next cycle cannot read from LREG7 (2-cycle operation)

    TTI_SFPNOP; // Pipeline delay
  
    // Step 2: Calculate final EMA = (1-α) * input + α * EMA_old
    // LREG7 = (LREG6 * LREG0) + LREG7 = (1-α) * input + α * EMA_old
    TTI_SFPMAD(ckernel::p_sfpu::LREG6, ckernel::p_sfpu::LREG0, ckernel::p_sfpu::LREG7, ckernel::p_sfpu::LREG7, 0);
    // Next cycle cannot read from LREG7 (2-cycle operation)
  
    TTI_SFPNOP; // Pipeline delay
  
    // Step 3: Update EMA_old for next iteration
    // LREG4 = LREG7 (copy new EMA to old EMA register)
    TTI_SFPMUL(ckernel::p_sfpu::LCONST_1, ckernel::p_sfpu::LREG7, ckernel::p_sfpu::LCONST_0, ckernel::p_sfpu::LREG4, 0);
    // Next cycle cannot read from LREG4 (2-cycle operation)
  
    TTI_SFPNOP; // Pipeline delay
}

sfpi_inline void _ema_main_()
{
    _ema_load_curr_input_<0, 0>();
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    _ema_load_curr_input_<0, 1>();
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    _ema_load_curr_input_<0, 2>();
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    _ema_load_curr_input_<0, 3>();
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    _ema_load_curr_input_<1, 0>();
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    _ema_load_curr_input_<1, 1>();
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    _ema_load_curr_input_<1, 2>();
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    _ema_load_curr_input_<1, 3>();
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
    lltt::replay(8, 6);
}

namespace ckernel
{
namespace sfpu
{
sfpi_inline void _calculate_ema_online_(bool first_sample)
{
  
    if (first_sample)
    {
        _ema_clear_prev_output_();
    } else {
        _ema_load_prev_output_();
    }
    _ema_load_alpha_beta_();
    _ema_main_();
    _ema_save_curr_output_();
}
} // namespace sfpu
} // namespace ckernel
