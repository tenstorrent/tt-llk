// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu.h"
#include "ckernel_sfpu_add_top_row.h"
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
<<<<<<< HEAD
<<<<<<< HEAD
template <int ITERATIONS, bool dest_acc = is_fp32_dest_acc_en, bool approx_mode = APPROX_MODE>
=======
template <int ITERATIONS, bool dest_acc = is_fp32_dest_acc_en>
>>>>>>> 6c6799a7 (refactor: add dest_acc template parameter to call_sfpu_operation)
=======
template <int ITERATIONS, bool dest_acc = is_fp32_dest_acc_en, bool approx_mode = APPROX_MODE>
>>>>>>> 89a67e84 (added aprox_mode and unpacking_to_dest template parameters)
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
<<<<<<< HEAD
<<<<<<< HEAD
            _init_atanh_<approx_mode>();
            _calculate_atanh_<approx_mode, dest_acc, ITERATIONS>();
=======
            _init_atanh_<APPROX_MODE>();
            _calculate_atanh_<APPROX_MODE, dest_acc, ITERATIONS>();
>>>>>>> 6c6799a7 (refactor: add dest_acc template parameter to call_sfpu_operation)
=======
            _init_atanh_<approx_mode>();
            _calculate_atanh_<approx_mode, dest_acc, ITERATIONS>();
>>>>>>> 89a67e84 (added aprox_mode and unpacking_to_dest template parameters)
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
<<<<<<< HEAD
<<<<<<< HEAD
            _init_reciprocal_<approx_mode, dest_acc>();
            _calculate_reciprocal_<approx_mode, ITERATIONS, dest_acc>(ITERATIONS);
            break;
        case SfpuType::rsqrt:
            _init_rsqrt_<approx_mode>();
            _calculate_rsqrt_<approx_mode, ITERATIONS, dest_acc, false>(ITERATIONS);
=======
            _init_reciprocal_<APPROX_MODE>();
            _calculate_reciprocal_<APPROX_MODE, ITERATIONS, dest_acc>(ITERATIONS);
            break;
        case SfpuType::rsqrt:
            _init_rsqrt_<APPROX_MODE>();
            _calculate_rsqrt_<APPROX_MODE, ITERATIONS, dest_acc, false>(ITERATIONS);
>>>>>>> 6c6799a7 (refactor: add dest_acc template parameter to call_sfpu_operation)
=======
            _init_reciprocal_<approx_mode>();
            _calculate_reciprocal_<approx_mode, ITERATIONS, dest_acc>(ITERATIONS);
            break;
        case SfpuType::rsqrt:
            _init_rsqrt_<approx_mode>();
            _calculate_rsqrt_<approx_mode, ITERATIONS, dest_acc, false>(ITERATIONS);
>>>>>>> 89a67e84 (added aprox_mode and unpacking_to_dest template parameters)
            break;
        case SfpuType::silu:
            _calculate_silu_<approx_mode, ITERATIONS>();
            break;
        case SfpuType::sine:
            _calculate_sine_<approx_mode, ITERATIONS>(ITERATIONS);
            break;
        case SfpuType::sqrt:
<<<<<<< HEAD
<<<<<<< HEAD
            _init_sqrt_<approx_mode>();
            _calculate_sqrt_<approx_mode, ITERATIONS, dest_acc, false>(ITERATIONS);
=======
            _init_sqrt_<APPROX_MODE>();
            _calculate_sqrt_<APPROX_MODE, ITERATIONS, dest_acc, false>(ITERATIONS);
>>>>>>> 6c6799a7 (refactor: add dest_acc template parameter to call_sfpu_operation)
            break;
        case SfpuType::square:
            _calculate_square_<approx_mode, ITERATIONS>();
=======
            _init_sqrt_<approx_mode>();
            _calculate_sqrt_<approx_mode, ITERATIONS, dest_acc, false>(ITERATIONS);
            break;
        case SfpuType::square:
            _calculate_square_<approx_mode, ITERATIONS>(ITERATIONS);
>>>>>>> 89a67e84 (added aprox_mode and unpacking_to_dest template parameters)
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

template <bool APPROXIMATION_MODE = APPROX_MODE, BinaryOp BINOP, int ITERATIONS = 32, uint32_t MATH_FORMAT = 0>
void call_binary_sfpu_operation(const uint dst_index_in0 = 0, const uint dst_index_in1 = 1, const uint dst_index_out = 0)
{
    switch (BINOP)
    {
        case BinaryOp::ADD:
        case BinaryOp::SUB:
        case BinaryOp::MUL:
        case BinaryOp::XLOGY:
            _sfpu_binary_init_<APPROXIMATION_MODE, BINOP>();
            _calculate_sfpu_binary_<APPROXIMATION_MODE, BINOP, ITERATIONS>(dst_index_in0, dst_index_in1, dst_index_out);
            break;
        case BinaryOp::RSHFT:
            _calculate_binary_right_shift_<APPROXIMATION_MODE, ITERATIONS, INT32, false>(dst_index_in0, dst_index_in1, dst_index_out);
            break;
        case BinaryOp::LSHFT:
            _calculate_binary_left_shift_<APPROXIMATION_MODE, ITERATIONS, INT32, false>(dst_index_in0, dst_index_in1, dst_index_out);
            break;
        case BinaryOp::LOGICAL_RSHFT:
            _calculate_logical_right_shift_<APPROXIMATION_MODE, ITERATIONS, INT32, false>(dst_index_in0, dst_index_in1, dst_index_out);
            break;
        case BinaryOp::ADD_TOP_ROW:
            _init_add_top_row_();
            // Use actual format when compiling for ADD_TOP_ROW tests, otherwise use Float32 as safe default for static assert
            {
                constexpr DataFormat add_top_row_format = (BINOP == BinaryOp::ADD_TOP_ROW) ? static_cast<DataFormat>(MATH_FORMAT) : DataFormat::Float32;
                _calculate_add_top_row_<add_top_row_format>(dst_index_in0, dst_index_in1, dst_index_out);
            }
            break;
        default:
            return;
    }
}

} // namespace test_utils
