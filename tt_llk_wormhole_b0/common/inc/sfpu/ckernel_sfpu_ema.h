// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel.h"
#include "sfpi.h"


sfpi_inline void _ema_clear_prev_output_()
{
    TTI_SFPLOADI(ckernel::p_sfpu::LREG4, 0, 0);
}

sfpi_inline void _ema_load_prev_output_()
{
    TTI_SFPLOAD(p_sfpu::LREG4, 0, ADDR_MOD_3, 64);
}

sfpi_inline void _ema_load_alpha_beta_()
{
    // alpha = 0.25
    // beta = 1 - alpha = 0.75
    TTI_SFPLOADI(ckernel::p_sfpu::LREG5, 8, 0x3e800000 >> 16);
    TTI_SFPLOADI(ckernel::p_sfpu::LREG5, 10, 0x3e800000 & 0xFFFF); // 0.25
    TTI_SFPLOADI(ckernel::p_sfpu::LREG6, 8, 0x3f400000 >> 16); // 0.75
    TTI_SFPLOADI(ckernel::p_sfpu::LREG6, 10, 0x3f400000 & 0xFFFF); // 0.75
}

template <uint32_t I, uint32_t J>
sfpi_inline void _ema_load_curr_input_()
{
    constexpr uint32_t dst_reg_offset = 0;
    constexpr uint32_t offset0 = dst_reg_offset + (I * 32) + (4 * J);
    constexpr uint32_t offset1 = offset0 + 2;
    constexpr uint32_t offset2 = offset1 + 16;
    constexpr uint32_t offset3 = offset2 + 18;
    TTI_SFPLOAD(ckernel::p_sfpu::LREG0, 0, ckernel::ADDR_MOD_3, offset0); /*row0*/
    TTI_SFPLOAD(ckernel::p_sfpu::LREG1, 0, ckernel::ADDR_MOD_3, offset1); /*row1*/
    TTI_SFPLOAD(ckernel::p_sfpu::LREG2, 0, ckernel::ADDR_MOD_3, offset2); /*row2*/
    TTI_SFPLOAD(ckernel::p_sfpu::LREG3, 0, ckernel::ADDR_MOD_3, offset3); /*row3*/
    TTI_SFPTRANSP(0, 0, 0, 0);
}

sfpi_inline void _ema_save_output_for_next_()
{
    TTI_SFPSTORE(ckernel::p_sfpu::LREG4, 0, ckernel::ADDR_MOD_3, 64);
}

template <uint32_t I, uint32_t J>
sfpi_inline void _compute_ema_math_()
{
    constexpr uint32_t dst_reg_offset = 64 * 2;
    constexpr uint32_t offset0 = dst_reg_offset + (I * 32) + (4 * J);
    constexpr uint32_t offset1 = offset0 + 2;
    constexpr uint32_t offset2 = offset1 + 16;
    constexpr uint32_t offset3 = offset2 + 18;

    // EMA equation: EMA_new = α * EMA_old + (1-α) * input
    // Where: LREG0=input, LREG4=EMA_old, LREG5=α, LREG6=(1-α), LREG7=EMA_new
    // Thus, LREG7 = LREG5 * LREG4 + LREG6 * LREG0

    // Step 1: Calculate α * EMA_old in LREG7 input0
    // LREG7 = LREG5 * LREG4 (α * EMA_old)
    TTI_SFPMUL(ckernel::p_sfpu::LREG5, ckernel::p_sfpu::LREG4, ckernel::p_sfpu::LCONST_0, ckernel::p_sfpu::LREG7, 0);
    // Next cycle cannot read from LREG7 (2-cycle operation)

    TTI_SFPNOP; // Pipeline delay
  
    // Step 2: Calculate final EMA = (1-α) * input + α * EMA_old for input0
    // LREG7 = (LREG6 * LREG0) + LREG7 = (1-α) * input + α * EMA_old
    TTI_SFPMAD(ckernel::p_sfpu::LREG6, ckernel::p_sfpu::LREG0, ckernel::p_sfpu::LREG7, ckernel::p_sfpu::LREG7, 0);
    // Next cycle cannot read from LREG7 (2-cycle operation)

    TTI_SFPNOP; // Pipeline delay

    // Step 3: Store this value to the output register
    TTI_SFPSTORE(ckernel::p_sfpu::LREG7, 0, ckernel::ADDR_MOD_3, offset0);
    // TODO: Check if this is correct: Next cycle cannot read from LREG7 (2-cycle operation)

    TTI_SFPNOP; // Pipeline delay

    // Step 4: Calculate α * EMA_old in LREG7 for input1
    // LREG7 = LREG5 * LREG4 (α * EMA_old)
    TTI_SFPMUL(ckernel::p_sfpu::LREG5, ckernel::p_sfpu::LREG7, ckernel::p_sfpu::LCONST_0, ckernel::p_sfpu::LREG7, 0);
    // Next cycle cannot read from LREG7 (2-cycle operation)

    TTI_SFPNOP; // Pipeline delay

    // Step 5: Calculate final EMA = (1-α) * input + α * EMA_old for input1
    // LREG7 = (LREG6 * LREG0) + LREG7 = (1-α) * input + α * EMA_old
    TTI_SFPMAD(ckernel::p_sfpu::LREG6, ckernel::p_sfpu::LREG1, ckernel::p_sfpu::LREG7, ckernel::p_sfpu::LREG7, 0);
    // Next cycle cannot read from LREG7 (2-cycle operation)

    TTI_SFPNOP; // Pipeline delay

    // Step 6: Store this value to the output register
    TTI_SFPSTORE(ckernel::p_sfpu::LREG7, 0, ckernel::ADDR_MOD_3, offset1);
    // TODO: Check if this is correct: Next cycle cannot read from LREG7 (2-cycle operation)

    TTI_SFPNOP; // Pipeline delay

    // Step 7: Calculate α * EMA_old in LREG7 for input2
    // LREG7 = LREG5 * LREG4 (α * EMA_old)
    TTI_SFPMUL(ckernel::p_sfpu::LREG5, ckernel::p_sfpu::LREG7, ckernel::p_sfpu::LCONST_0, ckernel::p_sfpu::LREG7, 0);
    // Next cycle cannot read from LREG7 (2-cycle operation)

    TTI_SFPNOP; // Pipeline delay

    // Step 8: Calculate final EMA = (1-α) * input + α * EMA_old for input2
    // LREG7 = (LREG6 * LREG0) + LREG7 = (1-α) * input + α * EMA_old
    TTI_SFPMAD(ckernel::p_sfpu::LREG6, ckernel::p_sfpu::LREG2, ckernel::p_sfpu::LREG7, ckernel::p_sfpu::LREG7, 0);
    // Next cycle cannot read from LREG7 (2-cycle operation)

    TTI_SFPNOP; // Pipeline delay

    // Step 9: Store this value to the output register
    TTI_SFPSTORE(ckernel::p_sfpu::LREG7, 0, ckernel::ADDR_MOD_3, offset2);
    // TODO: Check if this is correct: Next cycle cannot read from LREG7 (2-cycle operation)

    TTI_SFPNOP; // Pipeline delay

    // Step 10: Calculate α * EMA_old in LREG7 for input3
    // LREG7 = LREG5 * LREG4 (α * EMA_old)
    TTI_SFPMUL(ckernel::p_sfpu::LREG5, ckernel::p_sfpu::LREG7, ckernel::p_sfpu::LCONST_0, ckernel::p_sfpu::LREG7, 0);
    // Next cycle cannot read from LREG7 (2-cycle operation)

    TTI_SFPNOP; // Pipeline delay

    // Step 11: Calculate final EMA = (1-α) * input + α * EMA_old for input3
    // LREG7 = (LREG6 * LREG0) + LREG7 = (1-α) * input + α * EMA_old
    TTI_SFPMAD(ckernel::p_sfpu::LREG6, ckernel::p_sfpu::LREG3, ckernel::p_sfpu::LREG7, ckernel::p_sfpu::LREG7, 0);
    // Next cycle cannot read from LREG7 (2-cycle operation)

    TTI_SFPNOP; // Pipeline delay

    // Step 12: Store this value to the output register
    TTI_SFPSTORE(ckernel::p_sfpu::LREG7, 0, ckernel::ADDR_MOD_3, offset3);
    // TODO: Check if this is correct: Next cycle cannot read from LREG7 (2-cycle operation)

    TTI_SFPNOP; // Pipeline delay

    // Step 13: Update EMA_old for next iteration
    // LREG4 = LREG7 (copy new EMA to old EMA register)
    TTI_SFPMUL(ckernel::p_sfpu::LCONST_1, ckernel::p_sfpu::LREG7, ckernel::p_sfpu::LCONST_0, ckernel::p_sfpu::LREG4, 0);
    // Next cycle cannot read from LREG4 (2-cycle operation)
  
    TTI_SFPNOP; // Pipeline delay
}

sfpi_inline void _ema_compute_()
{
    _ema_load_curr_input_<0, 0>();
    _compute_ema_math_<0, 0>();
    _ema_load_curr_input_<0, 1>();
    _compute_ema_math_<0, 1>();
    _ema_load_curr_input_<0, 2>();
    _compute_ema_math_<0, 2>();
    _ema_load_curr_input_<0, 3>();
    _compute_ema_math_<0, 3>();
    _ema_load_curr_input_<1, 0>();
    _compute_ema_math_<1, 0>();
    _ema_load_curr_input_<1, 1>();
    _compute_ema_math_<1, 1>();
    _ema_load_curr_input_<1, 2>();
    _compute_ema_math_<1, 2>();
    _ema_load_curr_input_<1, 3>();
    _compute_ema_math_<1, 3>();
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
    _ema_compute_();
    _ema_save_output_for_next_();
}
} // namespace sfpu
} // namespace ckernel
