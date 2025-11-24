// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu.h"
#include "llk_sfpu_types.h"

using namespace ckernel;
using namespace ckernel::sfpu;

namespace test_utils
{

/**
 * Template function to call SFPU operations with parameterized iteration count
 * and optional math format for type-specific behavior.
 *
 * @tparam ITERATIONS Number of SFPU iterations (typically 32 for full tile)
 * @tparam dest_acc Whether FP32 destination accumulation is enabled (default from build.h)
 * @tparam approx_mode Whether to use approximation mode for SFPU operations (default from build.h)
 * @param operation The SFPU operation type to execute
 * @param math_format Optional math format for operations that need format-specific behavior
 */
template <int ITERATIONS, bool dest_acc = is_fp32_dest_acc_en, bool approx_mode = APPROX_MODE>
void call_sfpu_operation(SfpuType operation, uint32_t math_format)
{
    switch (operation)
    {
        case SfpuType::abs:
            _calculate_abs_<approx_mode, ITERATIONS>(ITERATIONS);
            break;
        case SfpuType::acosh:
            _init_inverse_hyperbolic_<approx_mode>();
            _calculate_acosh_<approx_mode, ITERATIONS>();
            break;
        case SfpuType::asinh:
            _init_inverse_hyperbolic_<approx_mode>();
            _calculate_asinh_<approx_mode, ITERATIONS>();
            break;
        case SfpuType::atanh:
            _init_atanh_<approx_mode>();
            _calculate_atanh_<approx_mode, dest_acc, ITERATIONS>();
            break;
        case SfpuType::celu:
            _calculate_activation_<approx_mode, ActivationType::Celu, ITERATIONS>(10, 1.0f / 10.0f);
            break;
        case SfpuType::cosine:
            _calculate_cosine_<approx_mode, ITERATIONS>(ITERATIONS);
            break;
        case SfpuType::elu:
            _init_elu_<approx_mode>();
            _calculate_elu_<approx_mode, ITERATIONS>(1);
            break;
        case SfpuType::exp2:
            _init_exp2_<approx_mode>();
            _calculate_exp2_<approx_mode, ITERATIONS>();
            break;
        case SfpuType::exponential:
            _init_exponential_<approx_mode, false /*fast_mode*/, 0x3F800000 /* exp_base_scale_factor */>();
            _calculate_exponential_<approx_mode, false /* scale_en */, ITERATIONS, false /* fast_approx */, false /* skip_positive_check */>(
                p_sfpu::kCONST_1_FP16B /* exp_base_scale_factor */);
            break;
        case SfpuType::fill:
            if (math_format == static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Int32))
            {
                _calculate_fill_int_<approx_mode, ITERATIONS>(5);
            }
            else
            {
                _calculate_fill_<approx_mode, ITERATIONS>(1.0f);
            }
            break;
        case SfpuType::gelu:
            _init_gelu_<approx_mode>();
            _calculate_gelu_<approx_mode, ITERATIONS>();
            break;
        case SfpuType::hardsigmoid:
            _init_hardsigmoid_<approx_mode>();
            _calculate_activation_<approx_mode, ckernel::ActivationType::Hardsigmoid, ITERATIONS>();
            break;
        case SfpuType::log:
            _init_log_<approx_mode>();
            _calculate_log_<approx_mode, false, ITERATIONS>(ITERATIONS, 0);
            break;
        case SfpuType::neg:
        case SfpuType::negative:
            if (math_format == static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Int32))
            {
                _calculate_negative_int_<approx_mode, ITERATIONS>();
            }
            else
            {
                _calculate_negative_<approx_mode, ITERATIONS>();
            }
            break;
        case SfpuType::reciprocal:
            _init_reciprocal_<approx_mode, dest_acc>();
            _calculate_reciprocal_<approx_mode, ITERATIONS, dest_acc>(ITERATIONS);
            break;
        case SfpuType::rsqrt:
            _init_rsqrt_<approx_mode>();
            _calculate_rsqrt_<approx_mode, ITERATIONS, dest_acc, false>(ITERATIONS);
            break;
        case SfpuType::silu:
            _calculate_silu_<approx_mode, ITERATIONS>();
            break;
        case SfpuType::sine:
            _calculate_sine_<approx_mode, ITERATIONS>(ITERATIONS);
            break;
        case SfpuType::sqrt:
            _init_sqrt_<approx_mode>();
            _calculate_sqrt_<approx_mode, ITERATIONS, dest_acc, false>(ITERATIONS);
            break;
        case SfpuType::square:
            _calculate_square_<approx_mode, ITERATIONS>();
            break;
        case SfpuType::threshold:
            _calculate_threshold_<approx_mode, ITERATIONS>(5.0f, 10.0f);
            break;
        default:
            return; // Unsupported op – should never happen
    }
}

/**
 * Convenience function for the common case of 32 iterations (full tile)
 */
inline void call_sfpu_operation_32(SfpuType operation, uint32_t math_format = 0)
{
    call_sfpu_operation<32>(operation, math_format);
}

} // namespace test_utils
