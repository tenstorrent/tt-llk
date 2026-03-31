// SPDX-FileCopyrightText: (c) 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <cstdint>

#include "cmath_common.h"
#include "llk_defs.h"

using namespace ckernel::math;

inline void _eltwise_ternary_sfpu_configure_addrmod_()
{
    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_7, csr_read<CSR::TRISC_ID>());
}

template <SfpuType sfpu_op>
inline void _llk_math_eltwise_ternary_sfpu_init_()
{
    _init_sfpu_config_reg_();
    _eltwise_ternary_sfpu_configure_addrmod_();
    _reset_counters_<p_setrwc::SET_ABD_F>();
}

template <DstSync Dst>
inline void _llk_math_eltwise_ternary_sfpu_start_(const std::uint32_t dst_index)
{
    _set_dst_write_addr_<DstTileShape::Tile32x32>(dst_index);
    TTI_STALLWAIT(p_stall::STALL_SFPU, 0, 0, p_stall::MATH);
}

inline void _llk_math_eltwise_ternary_sfpu_done_()
{
    _reset_counters_<p_setrwc::SET_D>();
}
