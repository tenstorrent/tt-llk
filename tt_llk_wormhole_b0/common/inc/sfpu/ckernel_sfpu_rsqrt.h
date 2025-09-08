// SPDX-FileCopyrightText: © 2025 Jason Davies <jason@jasondavies.com>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu_rsqrt_compat.h"
#include "ckernel_sfpu_sqrt.h"
#include "llk_defs.h"
#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel
{
namespace sfpu
{

template <ApproximationMode APPROX_MODE, int ITERATIONS, bool fp32_dest_acc_en, bool legacy_compat = false>
inline void _calculate_rsqrt_(int iterations)
{
    if constexpr (legacy_compat)
    {
        return _calculate_rsqrt_compat_<APPROX_MODE, ITERATIONS, fp32_dest_acc_en>(iterations);
    }
    else
    {
        return _calculate_sqrt_internal_<APPROX_MODE, ITERATIONS, fp32_dest_acc_en, true>(iterations);
    }
}

template <ApproximationMode APPROX_MODE, bool legacy_compat = false>
inline void _init_rsqrt_()
{
    if constexpr (!legacy_compat)
    {
        _init_sqrt_<APPROX_MODE>();
    }
}

} // namespace sfpu
} // namespace ckernel
