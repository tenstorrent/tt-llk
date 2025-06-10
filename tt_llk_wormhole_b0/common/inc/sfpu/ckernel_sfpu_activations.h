// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_defs.h"
#include "sfpi.h"
#include "sfpi_fp16.h"
#include "sfpu/ckernel_sfpu_exp.h"

namespace ckernel::sfpu
{

// General template structure to implement activations
template <bool APPROXIMATION_MODE, ActivationType ACTIVATION_TYPE>
struct ActivationImpl;

// Specialization for CELU activation
template <bool APPROXIMATION_MODE>
struct ActivationImpl<APPROXIMATION_MODE, ActivationType::Celu>
{
    static inline void apply(sfpi::vFloat& v, uint param0, uint param1)
    {
        // All params are in FP16_B format
        // param0 = alpha
        // param1 = alpha_recip
        sfpi::vFloat result        = 56.0f;
        sfpi::vFloat s2v_result    = sfpi::s2vFloat16(param0);
        sfpi::vFloat direct_result = param0;

        v = result;
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
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat v = sfpi::dst_reg[0];
        apply_activation<APPROXIMATION_MODE, ACTIVATION_TYPE>(v, param0, param1);
        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}

} // namespace ckernel::sfpu
