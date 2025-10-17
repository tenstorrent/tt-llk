// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <type_traits>

#include "ckernel_globals.h"
#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "ckernel_sfpu.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_math_common.h"
#include "llk_sfpu_types.h"
#include "sfpu/ckernel_sfpu_ema.h"

// local function declarations
inline void ema_sfpu_configure_addrmod()
{
    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_7);
}

inline void _llk_math_ema_sfpu_init_()
{
    sfpu::_init_sfpu_config_reg();
    ema_sfpu_configure_addrmod();
    math::reset_counters(p_setrwc::SET_ABD_F);
    lltt::record(0, 8 + 6);
    // _ema_clear_prev_output_();
    TTI_SFPLOADI(ckernel::p_sfpu::LREG4, 0, 0);
    // _ema_load_prev_output_();
    TTI_SFPLOAD(p_sfpu::LREG4, 0, ADDR_MOD_3, 64);
    // _ema_load_alpha_beta_();
    // alpha = 0.25
    TTI_SFPLOADI(ckernel::p_sfpu::LREG5, 8, 0x3e800000 >> 16);
    TTI_SFPLOADI(ckernel::p_sfpu::LREG5, 10, 0x3e800000 & 0xFFFF);
    TTI_SFPLOADI(ckernel::p_sfpu::LREG6, 8, 3f400000 >> 16);
    TTI_SFPLOADI(ckernel::p_sfpu::LREG6, 10, 3f400000 & 0xFFFF);
    // _ema_load_curr_input_();
    TTI_SFPTRANSP(0, 0, 0, 0);
    // _ema_save_curr_output_();
    TTI_SFPSTORE(ckernel::p_sfpu::LREG4, 0, ckernel::ADDR_MOD_3, 64);
    /*past_values = 0*/ 
    _compute_ema_math_();      // 6 TTI instructions
}
