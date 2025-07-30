// SPDX-FileCopyrightText: © 2025 Jason Davies <jason@jasondavies.com>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu_sqrt.h"
#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS, bool layernorm_compat>
inline void _calculate_rsqrt_(int iterations)
{
    if constexpr (layernorm_compat)
    {
        return _calculate_sqrt_internal_<APPROXIMATION_MODE, ITERATIONS, true>(iterations);
    }
    else
    {
        return _calculate_rsqrt_compat_<APPROXIMATION_MODE, ITERATIONS>(iterations);
    }
}

template <bool APPROXIMATION_MODE, bool layernorm_compat>
inline void _init_rsqrt_()
{
    if constexpr (layernorm_compat)
    {
        _init_sqrt_compat_<APPROXIMATION_MODE>();
    }
    else
    {
        _init_sqrt_<APPROXIMATION_MODE>();
    }
}

} // namespace sfpu
} // namespace ckernel
