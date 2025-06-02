// SPDX-FileCopyrightText: © 2025 Jason Davies <jason@jasondavies.com>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"
#include "sfpi_fp16.h"
#include "ckernel_sfpu_sqrt.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_rsqrt_(int iterations)
{
    return _calculate_sqrt_internal_<APPROXIMATION_MODE, ITERATIONS, true>(iterations);
}

template <bool APPROXIMATION_MODE>
inline void _init_rsqrt_()
{
    _init_sqrt_<APPROXIMATION_MODE>();
}

} // namespace sfpu
} // namespace ckernel
