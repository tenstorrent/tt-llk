// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_sfpu.h"
#include "ckernel_sfpu_add_top_row.h"
#include "ckernel_sfpu_binary.h"
#include "llk_assert.h"
#include "llk_math_eltwise_unary_sfpu_params.h"
#include "llk_sfpu_types.h"

// Metal SFPU operations from tt-metal repository
// To add a new metal SFPU operation:
// 1. Include the metal header below: #include "metal_sfpu/<operation>.h"
// 2. Add the operation enum to SfpuType in llk_sfpu_types.h
// 3. Add branches in both CALL_SFPU_OPERATION_INIT and CALL_SFPU_OPERATION below
#include "metal_sfpu/ckernel_sfpu_exp.h"
#include "metal_sfpu/ckernel_sfpu_log1p.h"
#include "metal_sfpu/ckernel_sfpu_tanh.h"

/**
 * Runs the one-time per-operation SFPU initialisation.
 * Call once before the tile loop, paired with CALL_SFPU_OPERATION inside the loop.
 *
 * Required compile-time names in scope:
 *   SFPU_UNARY_OPERATION, APPROX_MODE, is_fp32_dest_acc_en, FAST_MODE, CLAMP_NEGATIVE
 */
// clang-format off
#define CALL_SFPU_OPERATION_INIT \
do { \
    if constexpr (SFPU_UNARY_OPERATION == SfpuType::acosh || \
                  SFPU_UNARY_OPERATION == SfpuType::asinh) \
    { \
        ckernel::sfpu::_init_inverse_hyperbolic_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::atanh) \
    { \
        ckernel::sfpu::_init_atanh_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::elu) \
    { \
        ckernel::sfpu::_init_elu_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::exp2) \
    { \
        ckernel::sfpu::_init_exp2_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::exponential) \
    { \
        ckernel::sfpu::_init_exponential_<APPROX_MODE, FAST_MODE, 0x3F800000 /* exp_base_scale_factor */, CLAMP_NEGATIVE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::gelu) \
    { \
        ckernel::sfpu::_init_gelu_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::hardsigmoid) \
    { \
        ckernel::sfpu::_init_hardsigmoid_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::log) \
    { \
        ckernel::sfpu::_init_log_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::log1p) \
    { \
        ckernel::sfpu::log1p_init<APPROX_MODE, FAST_MODE, is_fp32_dest_acc_en>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::reciprocal) \
    { \
        ckernel::sfpu::_init_reciprocal_<APPROX_MODE, is_fp32_dest_acc_en, false>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::rsqrt) \
    { \
        ckernel::sfpu::_init_rsqrt_<APPROX_MODE, false>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::sqrt) \
    { \
        ckernel::sfpu::_init_sqrt_<APPROX_MODE, false>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::tanh) \
    { \
        ckernel::sfpu::tanh_init<APPROX_MODE, is_fp32_dest_acc_en>(); \
    } \
} while (false);

/**
 * Dispatches the per-tile SFPU calculate call via _llk_math_eltwise_unary_sfpu_params_,
 * which handles dst setup, SFPU stalling, face iteration (VectorMode::RC), and cleanup.
 * Replaces the _llk_math_eltwise_unary_sfpu_start_ / _done_ pair at the call site.
 *
 * Follows the same calling convention as llk_math_eltwise_unary_sfpu_typecast.h:
 * a fully-specialized function pointer is passed as the first argument to
 * _llk_math_eltwise_unary_sfpu_params_; runtime arguments the function requires
 * are forwarded via the variadic Args&&... parameter after vector_mode.
 * Functions with no runtime arguments use the default VectorMode::RC.
 *
 * Call inside the tile loop after CALL_SFPU_OPERATION_INIT().
 *
 * Required compile-time names in scope:
 *   SFPU_UNARY_OPERATION, APPROX_MODE, is_fp32_dest_acc_en, ITERATIONS,
 *   FAST_MODE, STABLE_SORT, CLAMP_NEGATIVE
 *
 * @param dst_index        Destination register index for this tile.
 * @param math_format      Runtime data format (e.g. formats.math); selects integer
 *                         vs float paths for fill and neg operations.
 * @param fill_const_value Constant value used by the fill operation.
 */
#define CALL_SFPU_OPERATION(dst_index, math_format, vector_mode) \
do { \
    if constexpr (SFPU_UNARY_OPERATION == SfpuType::abs) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_abs_<APPROX_MODE, ITERATIONS>, dst_index, static_cast<int>(vector_mode), ITERATIONS); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::acosh) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_acosh_<APPROX_MODE, ITERATIONS>, dst_index, static_cast<int>(vector_mode)); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::asinh) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_asinh_<APPROX_MODE, ITERATIONS>, dst_index, static_cast<int>(vector_mode)); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::atanh) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_atanh_<APPROX_MODE, is_fp32_dest_acc_en, ITERATIONS>, dst_index, static_cast<int>(vector_mode)); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::celu) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(static_cast<void(*)(std::uint32_t, std::uint32_t)>(ckernel::sfpu::_calculate_activation_<APPROX_MODE, ckernel::ActivationType::Celu, ITERATIONS>), dst_index, static_cast<int>(vector_mode), static_cast<std::uint32_t>(10), static_cast<std::uint32_t>(0)); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::cosine) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_cosine_<APPROX_MODE, ITERATIONS>, dst_index, static_cast<int>(vector_mode), ITERATIONS); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::elu) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_elu_<APPROX_MODE, ITERATIONS>, dst_index, static_cast<int>(vector_mode), 1); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::exp2) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_exp2_<APPROX_MODE, ITERATIONS>, dst_index, static_cast<int>(vector_mode)); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::exponential) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_exponential_<APPROX_MODE, false, ITERATIONS, FAST_MODE, false, CLAMP_NEGATIVE>, dst_index, static_cast<int>(vector_mode), p_sfpu::kCONST_1_FP16B); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::fill) \
    { \
        if ((math_format) == ckernel::to_underlying(DataFormat::Int32)) \
            _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_fill_int_<APPROX_MODE, ckernel::InstrModLoadStore::INT32, ITERATIONS>, dst_index, static_cast<int>(vector_mode), 5); \
        else if ((math_format) == ckernel::to_underlying(DataFormat::UInt16)) \
            _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_fill_int_<APPROX_MODE, ckernel::InstrModLoadStore::LO16, ITERATIONS>, dst_index, static_cast<int>(vector_mode), 5); \
        else if ((math_format) == ckernel::to_underlying(DataFormat::UInt32)) \
            _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_fill_int_<APPROX_MODE, ckernel::InstrModLoadStore::INT32, ITERATIONS>, dst_index, static_cast<int>(vector_mode), 5); \
        else \
            _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_fill_<APPROX_MODE, ITERATIONS>, dst_index, static_cast<int>(vector_mode), 0); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::gelu) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_gelu_<APPROX_MODE, ITERATIONS>, dst_index, static_cast<int>(vector_mode)); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::hardsigmoid) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(static_cast<void(*)()>(ckernel::sfpu::_calculate_activation_<APPROX_MODE, ckernel::ActivationType::Hardsigmoid, ITERATIONS>), dst_index, static_cast<int>(vector_mode)); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::log) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_log_<APPROX_MODE, false, ITERATIONS>, dst_index, static_cast<int>(vector_mode), ITERATIONS, 0); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::log1p) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::calculate_log1p<APPROX_MODE, FAST_MODE, is_fp32_dest_acc_en, ITERATIONS>, dst_index); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::neg || SFPU_UNARY_OPERATION == SfpuType::negative) \
    { \
        if ((math_format) == ckernel::to_underlying(DataFormat::Int32)) \
            _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_negative_int_<APPROX_MODE, ITERATIONS>, dst_index, static_cast<int>(vector_mode)); \
        else \
            _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_negative_<APPROX_MODE, ITERATIONS>, dst_index, static_cast<int>(vector_mode)); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::reciprocal) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_reciprocal_<APPROX_MODE, ITERATIONS, is_fp32_dest_acc_en>, dst_index, static_cast<int>(vector_mode), ITERATIONS); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::rsqrt) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_rsqrt_<APPROX_MODE, ITERATIONS, is_fp32_dest_acc_en, FAST_MODE>, dst_index, static_cast<int>(vector_mode), ITERATIONS); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::silu) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_silu_<APPROX_MODE, ITERATIONS>, dst_index, static_cast<int>(vector_mode)); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::sine) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_sine_<APPROX_MODE, ITERATIONS>, dst_index, static_cast<int>(vector_mode), ITERATIONS); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::sqrt) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_sqrt_<APPROX_MODE, ITERATIONS, is_fp32_dest_acc_en, FAST_MODE>, dst_index, static_cast<int>(vector_mode), ITERATIONS); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::square) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_square_<APPROX_MODE, ITERATIONS>, dst_index, static_cast<int>(vector_mode)); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::tanh) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::calculate_tanh<APPROX_MODE, is_fp32_dest_acc_en, ITERATIONS>, dst_index, static_cast<int>(vector_mode)); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::threshold) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_calculate_threshold_<APPROX_MODE, ITERATIONS, float>, dst_index, static_cast<int>(vector_mode), 5.0f, 10.0f); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::topk_local_sort) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_bitonic_topk_phases_steps<APPROX_MODE, is_fp32_dest_acc_en, STABLE_SORT>, dst_index, static_cast<int>(vector_mode), 0, 5, 0, 10, 0); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::topk_merge) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_bitonic_topk_merge<APPROX_MODE, is_fp32_dest_acc_en, STABLE_SORT>, dst_index, static_cast<int>(vector_mode), 5, 10); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::topk_rebuild) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_bitonic_topk_rebuild<APPROX_MODE, is_fp32_dest_acc_en, STABLE_SORT>, dst_index, static_cast<int>(vector_mode), 0, 5, 10, 3, 0); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::relu_max) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_relu_max_<sfpi::vFloat, APPROX_MODE, ITERATIONS, float>, dst_index, static_cast<int>(vector_mode), 5.0f); \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::relu_min) \
        _llk_math_eltwise_unary_sfpu_params_<APPROX_MODE>(ckernel::sfpu::_relu_min_<sfpi::vFloat, APPROX_MODE, ITERATIONS, float>, dst_index, static_cast<int>(vector_mode), 5.0f); \
} while (false);

// clang-format on

namespace test_utils
{
using namespace ckernel;
using namespace ckernel::sfpu;

template <bool APPROXIMATION_MODE, BinaryOp BINOP, int ITERATIONS = 32, std::uint32_t MATH_FORMAT = 0>
void call_binary_sfpu_operation(const std::uint32_t dst_index_in0 = 0, const std::uint32_t dst_index_in1 = 1, const std::uint32_t dst_index_out = 0)
{
    switch (BINOP)
    {
        case BinaryOp::ADD:
        case BinaryOp::SUB:
        case BinaryOp::MUL:
        case BinaryOp::DIV:
        case BinaryOp::RSUB:
        case BinaryOp::XLOGY:
        case BinaryOp::POW:
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
