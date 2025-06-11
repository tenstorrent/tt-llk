// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_ops.h"
#include "llk_defs.h"

namespace ckernel
{
namespace sfpu
{

inline constexpr bool is_valid_instruction_mode(InstrModLoadStore mode)
{
    return mode == InstrModLoadStore::INT32_2S_COMP || mode == InstrModLoadStore::INT32 || mode == InstrModLoadStore::LO16;
}

template <bool SIGN_MAGNITUDE_FORMAT>
inline void apply_sign_magnitude_conversion(uint src, uint dst, InstrModCast cast_mode)
{
    if constexpr (SIGN_MAGNITUDE_FORMAT)
    {
        TTI_SFPCAST(src /*lreg*/, dst /*ldest*/, cast_mode);
        // Required after cast due to a bug in Blackhole RTL (Refer tenstorrent/tt-llk-bh#16)
        TTI_SFPSETSGN(0 /* imm */, dst /*lreg_c*/, src /*ldest*/, 0 /*imod*/);
    }
}

} // namespace sfpu
} // namespace ckernel
