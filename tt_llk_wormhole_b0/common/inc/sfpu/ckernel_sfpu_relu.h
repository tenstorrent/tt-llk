// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu_converter.h"
#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel
{
namespace sfpu
{

template <typename T>
constexpr bool is_supported_relu_type_v = std::is_same_v<T, float> || std::is_same_v<T, uint32_t>;

template <bool APPROXIMATION_MODE>
inline void _calculate_lrelu_(const int iterations, uint slope)
{
    sfpi::vFloat s = Converter::as_float(slope);

#pragma GCC unroll 0
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat v = sfpi::dst_reg[0];

        v_if (v < 0.0f)
        {
            v *= s;
        }
        v_endif;

        sfpi::dst_reg[0] = v;

        sfpi::dst_reg++;
    }
}

sfpi_inline sfpi::vFloat _relu_max_body_(sfpi::vFloat val, sfpi::vFloat threshold)
{
    sfpi::vFloat result = val;
    v_if (result > threshold)
    {
        result = threshold;
    }
    v_endif;
    v_if (result < 0.0f)
    {
        result = 0.0f;
    }
    v_endif;
    return result;
}

template <bool APPROXIMATION_MODE, int ITERATIONS, typename VecType>
inline void _relu_max_impl_(const int iterations, VecType threshold)
{
    for (int d = 0; d < iterations; d++)
    {
        VecType result = sfpi::dst_reg[0];
        v_if (result > threshold)
        {
            result = threshold;
        }
        v_endif;
        v_if (result < 0)
        {
            result = 0;
        }
        v_endif;
        sfpi::dst_reg[0] = result;
        sfpi::dst_reg++;
    }
}

// Wrappers
template <bool APPROXIMATION_MODE, int ITERATIONS, typename T>
inline void _relu_max_(T threshold)
{
    sfpi::vFloat v_threshold;
    if constexpr (std::is_same_v<T, float>)
    {
        v_threshold = threshold;
    }
    else if constexpr (std::is_same_v<T, uint32_t>)
    {
        v_threshold = Converter::as_float(threshold);
    }
    _relu_max_impl_<APPROXIMATION_MODE, ITERATIONS, sfpi::vFloat>(ITERATIONS, v_threshold);
}

template <bool APPROXIMATION_MODE, int ITERATIONS, typename T>
inline void _relu_max_int_(T threshold)
{
    sfpi::vInt v_threshold;
    if constexpr (std::is_same_v<T, float>)
    {
        v_threshold = threshold;
    }
    else if constexpr (std::is_same_v<T, uint32_t>)
    {
        v_threshold = Converter::as_float(threshold);
    }
    _relu_max_impl_<APPROXIMATION_MODE, ITERATIONS, sfpi::vInt>(ITERATIONS, v_threshold);
}

template <bool APPROXIMATION_MODE, int ITERATIONS, typename VecType>
inline void _relu_min_impl_(const int iterations, VecType threshold)
{
    for (int d = 0; d < iterations; d++)
    {
        VecType a = sfpi::dst_reg[0];
        v_if (a < threshold)
        {
            a = threshold;
        }
        v_endif;
        sfpi::dst_reg[0] = a;
        sfpi::dst_reg++;
    }
}

// Wrappers
template <bool APPROXIMATION_MODE, int ITERATIONS, typename T>
inline void _relu_min_(T threshold)
{
    sfpi::vFloat v_threshold;
    if constexpr (std::is_same_v<T, float>)
    {
        v_threshold = threshold;
    }
    else if constexpr (std::is_same_v<T, uint32_t>)
    {
        v_threshold = Converter::as_float(threshold);
    }
    _relu_min_impl_<APPROXIMATION_MODE, ITERATIONS, sfpi::vFloat>(ITERATIONS, v_threshold);
}

template <bool APPROXIMATION_MODE, int ITERATIONS, typename T>
inline void _relu_min_int_(T threshold)
{
    sfpi::vInt v_threshold;
    if constexpr (std::is_same_v<T, float>)
    {
        v_threshold = threshold;
    }
    else if constexpr (std::is_same_v<T, uint32_t>)
    {
        v_threshold = Converter::as_float(threshold);
    }
    _relu_min_impl_<APPROXIMATION_MODE, ITERATIONS, sfpi::vInt>(ITERATIONS, v_threshold);
}

} // namespace sfpu
} // namespace ckernel
