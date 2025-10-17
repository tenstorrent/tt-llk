// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_include.h"
#include "ckernel_sfpu.h"
#include "cmath_common.h"


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
}
