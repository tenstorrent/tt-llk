// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <type_traits>

#include "ckernel_defs.h"
#include "sfpi.h"
#include "sfpi_fp16.h"
#include "sfpu/ckernel_sfpu_converter.h"

namespace ckernel::sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS, typename T>
inline void _calculate_threshold_(T param0, T param1)
{
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat in = sfpi::dst_reg[0];

        sfpi::vFloat t;
        sfpi::vFloat v;

        if constexpr (std::is_same_v<T, float>)
        {
            t = param0;
            v = param1;
        }
        else if constexpr (std::is_same_v<T, uint32_t>)
        {
            t = Converter::as_float(param0);
            v = Converter::as_float(param1);
        }
        else
        {
            static_assert(!sizeof(T*), "Unsupported type for _calculate_threshold_");
        }

        sfpi::vFloat result = v;
        v_if (in > t)
        {
            result = in;
        }
        v_endif;

        sfpi::dst_reg[0] = result;
        sfpi::dst_reg++;
    }
}

} // namespace ckernel::sfpu
