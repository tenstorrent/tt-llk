// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"
#include "sfpi_fp16.h"
#include "sfpu/ckernel_sfpu_exp.h"

namespace ckernel
{
namespace sfpu
{

constexpr bool is_supported_activation_type(ActivationType activation_type)
{
    return activation_type == ActivationType::Celu;
}

// General template structure to implement activations
template <bool APPROXIMATION_MODE, ActivationType ACTIVATION_TYPE>
struct ActivationImpl
{
    static inline void apply(sfpi::vFloat& v, uint param0, uint param1);
};

// Specialization for CELU activation
template <bool APPROXIMATION_MODE>
struct ActivationImpl<APPROXIMATION_MODE, ActivationType::Celu>
{
    static inline void apply(sfpi::vFloat& v, uint param0, uint param1)
    {
        // All params are in FP16_B format
        // param0 = alpha
        // param1 = alpha_recip

        sfpi::vFloat alpha       = sfpi::s2vFloat16(param0);
        sfpi::vFloat alpha_recip = sfpi::s2vFloat16(param1);

        v_if (v < 0.0f)
        {
            sfpi::vFloat exp_val      = _calculate_exponential_body_<APPROXIMATION_MODE>(v * alpha_recip);
            sfpi::vFloat result       = v * exp_val;
            sfpi::vFloat result_alpha = result * alpha;
            v                         = result_alpha - 1;
        }
        v_elseif (v == 0.0f)
        {
            v = 0.0f;
        }
        v_endif;
    }
};

// Dispatch wrapper function
template <bool APPROXIMATION_MODE, ActivationType ACTIVATION_TYPE>
inline void apply_activation(sfpi::vFloat& v, uint param0, uint param1)
{
    ActivationImpl<APPROXIMATION_MODE, ACTIVATION_TYPE>::apply(v, param0, param1);
}

template <bool APPROXIMATION_MODE, ActivationType ACTIVATION_TYPE, int ITERATIONS = 8>
inline void _calculate_activation_(uint param0, uint param1)
{
    static_assert(is_supported_activation_type(ACTIVATION_TYPE), "Invalid activation type");
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat v = sfpi::dst_reg[0];
        apply_activation<APPROXIMATION_MODE, ACTIVATION_TYPE>(v, param0, param1);
        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
