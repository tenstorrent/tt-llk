// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_defs.h"
#include "sfpi.h"
#include "sfpu/ckernel_sfpu_converter.h"
#include "sfpu/ckernel_sfpu_relu.h"

namespace ckernel::sfpu
{

// General template structure to implement activations
template <bool APPROXIMATION_MODE, ActivationType ACTIVATION_TYPE>
struct ActivationImpl;

// CELU specialization removed — depends on _calculate_exponential_body_ (Family 1 primitive)
template <bool APPROXIMATION_MODE>
struct ActivationImpl<APPROXIMATION_MODE, ActivationType::Celu>
{
    static inline void apply(sfpi::vFloat& v, std::uint32_t param0, std::uint32_t param1)
    {
        // Implementation removed — exp primitive not available
    }
};

// Specialization for HARDSIGMOID activation
template <bool APPROXIMATION_MODE>
struct ActivationImpl<APPROXIMATION_MODE, ActivationType::Hardsigmoid>
{
    static inline void apply(sfpi::vFloat& v)
    {
        sfpi::vFloat tmp = (v * sfpi::vConstFloatPrgm0) + sfpi::vConstFloatPrgm1;
        v                = _relu_max_body_(tmp, 1.0f);
    }
};

// Dispatch wrapper function
template <bool APPROXIMATION_MODE, ActivationType ACTIVATION_TYPE>
inline void apply_activation(sfpi::vFloat& v, std::uint32_t param0, std::uint32_t param1)
{
    ActivationImpl<APPROXIMATION_MODE, ACTIVATION_TYPE>::apply(v, param0, param1);
}

// Dispatch wrapper function
template <bool APPROXIMATION_MODE, ActivationType ACTIVATION_TYPE>
inline void apply_activation(sfpi::vFloat& v)
{
    ActivationImpl<APPROXIMATION_MODE, ACTIVATION_TYPE>::apply(v);
}

template <bool APPROXIMATION_MODE, ActivationType ACTIVATION_TYPE, int ITERATIONS = 8>
inline void _calculate_activation_(std::uint32_t param0, std::uint32_t param1)
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

template <bool APPROXIMATION_MODE, ActivationType ACTIVATION_TYPE, int ITERATIONS>
inline void _calculate_activation_()
{
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat v = sfpi::dst_reg[0];
        apply_activation<APPROXIMATION_MODE, ACTIVATION_TYPE>(v);
        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
void _init_hardsigmoid_()
{
    // For hardsigmoid slope is 1/6, FP32 IEEE 754 representation.
    sfpi::vConstFloatPrgm0 = 0.1666666716337204f;
    sfpi::vConstFloatPrgm1 = 0.5f;
}

} // namespace ckernel::sfpu
