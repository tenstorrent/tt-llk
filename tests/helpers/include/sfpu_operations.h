// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_sfpu.h"
#include "ckernel_sfpu_add_top_row.h"
#include "ckernel_sfpu_binary.h"
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
#define CALL_SFPU_OPERATION_INIT() \
do { \
    if constexpr (SFPU_UNARY_OPERATION == SfpuType::acosh || \
                  SFPU_UNARY_OPERATION == SfpuType::asinh) \
    { \
        _init_inverse_hyperbolic_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::atanh) \
    { \
        _init_atanh_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::elu) \
    { \
        _init_elu_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::exp2) \
    { \
        _init_exp2_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::exponential) \
    { \
        _init_exponential_<APPROX_MODE, FAST_MODE, 0x3F800000 /* exp_base_scale_factor */, CLAMP_NEGATIVE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::gelu) \
    { \
        _init_gelu_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::hardsigmoid) \
    { \
        _init_hardsigmoid_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::log) \
    { \
        _init_log_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::log1p) \
    { \
        ckernel::sfpu::log1p_init<APPROX_MODE, FAST_MODE, is_fp32_dest_acc_en>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::reciprocal) \
    { \
        _init_reciprocal_<APPROX_MODE, is_fp32_dest_acc_en>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::rsqrt) \
    { \
        _init_rsqrt_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::sqrt) \
    { \
        _init_sqrt_<APPROX_MODE>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::tanh) \
    { \
        ckernel::sfpu::tanh_init<APPROX_MODE, is_fp32_dest_acc_en>(); \
    } \
} while (false)

/**
 * Runs the per-tile SFPU calculate call for the given operation.
 * Call once per tile inside the tile loop, after CALL_SFPU_OPERATION_INIT().
 *
 * Required compile-time names in scope:
 *   SFPU_UNARY_OPERATION, APPROX_MODE, is_fp32_dest_acc_en, ITERATIONS,
 *   FAST_MODE, STABLE_SORT, CLAMP_NEGATIVE
 *
 * @param math_format  Runtime data format (e.g. formats.math); selects integer
 *                     vs float paths for fill and neg operations.
 */
#define CALL_SFPU_OPERATION(math_format, fill_const_value) \
do { \
    _llk_math_eltwise_unary_sfpu_start_<DST_SYNC>(block_tile); \
    if constexpr (SFPU_UNARY_OPERATION == SfpuType::abs) \
    { \
        _calculate_abs_<APPROX_MODE, ITERATIONS>(ITERATIONS); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::acosh) \
    { \
        _calculate_acosh_<APPROX_MODE, ITERATIONS>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::asinh) \
    { \
        _calculate_asinh_<APPROX_MODE, ITERATIONS>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::atanh) \
    { \
        _calculate_atanh_<APPROX_MODE, is_fp32_dest_acc_en, ITERATIONS>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::celu) \
    { \
        _calculate_activation_<APPROX_MODE, ckernel::ActivationType::Celu, ITERATIONS>(10, 1.0f / 10.0f); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::cosine) \
    { \
        _calculate_cosine_<APPROX_MODE, ITERATIONS>(ITERATIONS); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::elu) \
    { \
        _calculate_elu_<APPROX_MODE, ITERATIONS>(1); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::exp2) \
    { \
        _calculate_exp2_<APPROX_MODE, ITERATIONS>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::exponential) \
    { \
        if constexpr (FAST_MODE && APPROX_MODE && CLAMP_NEGATIVE) \
        { \
            /* Each call processes 8 iterations; 4 faces => 32 total. */ \
            static_assert(ITERATIONS == 32); \
            for (int i = 0; i < 4; i++) \
            { \
                _calculate_exponential_<APPROX_MODE, false, ITERATIONS, FAST_MODE, false, CLAMP_NEGATIVE>( \
                    p_sfpu::kCONST_1_FP16B); \
                TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D); \
                TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D); \
            } \
        } \
        else if constexpr (FAST_MODE && APPROX_MODE) \
        { \
            /* Each call can process either 8 or 32 iterations. */ \
            static_assert(ITERATIONS == 8 || ITERATIONS == 32); \
            _calculate_exponential_<APPROX_MODE, false, ITERATIONS, FAST_MODE, false, CLAMP_NEGATIVE>( \
                p_sfpu::kCONST_1_FP16B); \
        } \
        else \
        { \
            _calculate_exponential_<APPROX_MODE, false, ITERATIONS, FAST_MODE, false, CLAMP_NEGATIVE>( \
                p_sfpu::kCONST_1_FP16B); \
        } \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::fill) \
    { \
        if ((math_format) == ckernel::to_underlying(DataFormat::Int32)) \
        { \
            _calculate_fill_int_<APPROX_MODE, ckernel::InstrModLoadStore::INT32, ITERATIONS>(static_cast<std::uint32_t>(fill_const_value)); \
        } \
        else if ((math_format) == ckernel::to_underlying(DataFormat::UInt16)) \
        { \
            _calculate_fill_int_<APPROX_MODE, ckernel::InstrModLoadStore::LO16, ITERATIONS>(static_cast<std::uint32_t>(fill_const_value)); \
        } \
        else if ((math_format) == ckernel::to_underlying(DataFormat::UInt32)) \
        { \
            _calculate_fill_int_<APPROX_MODE, ckernel::InstrModLoadStore::INT32, ITERATIONS>(static_cast<std::uint32_t>(fill_const_value)); \
        } \
        else \
        { \
            _calculate_fill_<APPROX_MODE, ITERATIONS>(fill_const_value); \
        } \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::gelu) \
    { \
        _calculate_gelu_<APPROX_MODE, ITERATIONS>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::hardsigmoid) \
    { \
        _calculate_activation_<APPROX_MODE, ckernel::ActivationType::Hardsigmoid, ITERATIONS>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::log) \
    { \
        _calculate_log_<APPROX_MODE, false, ITERATIONS>(ITERATIONS, 0); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::log1p) \
    { \
        ckernel::sfpu::calculate_log1p<APPROX_MODE, FAST_MODE, is_fp32_dest_acc_en, ITERATIONS>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::neg || SFPU_UNARY_OPERATION == SfpuType::negative) \
    { \
        if ((math_format) == ckernel::to_underlying(DataFormat::Int32)) \
        { \
            _calculate_negative_int_<APPROX_MODE, ITERATIONS>(); \
        } \
        else \
        { \
            _calculate_negative_<APPROX_MODE, ITERATIONS>(); \
        } \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::reciprocal) \
    { \
        _calculate_reciprocal_<APPROX_MODE, ITERATIONS, is_fp32_dest_acc_en>(ITERATIONS); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::rsqrt) \
    { \
        _calculate_rsqrt_<APPROX_MODE, ITERATIONS, is_fp32_dest_acc_en, FAST_MODE>(ITERATIONS); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::silu) \
    { \
        _calculate_silu_<APPROX_MODE, ITERATIONS>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::sine) \
    { \
        _calculate_sine_<APPROX_MODE, ITERATIONS>(ITERATIONS); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::sqrt) \
    { \
        _calculate_sqrt_<APPROX_MODE, ITERATIONS, is_fp32_dest_acc_en, FAST_MODE>(ITERATIONS); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::square) \
    { \
        _calculate_square_<APPROX_MODE, ITERATIONS>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::tanh) \
    { \
        ckernel::sfpu::calculate_tanh<APPROX_MODE, is_fp32_dest_acc_en, ITERATIONS>(); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::threshold) \
    { \
        _calculate_threshold_<APPROX_MODE, ITERATIONS>(5.0f, 10.0f); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::topk_local_sort) \
    { \
        _bitonic_topk_phases_steps<APPROX_MODE, is_fp32_dest_acc_en, STABLE_SORT>( \
            /* idir */ 0, \
            /* i_end_phase */ 5, \
            /* i_start_phase */ 0, \
            /* i_end_step */ 10, \
            /* i_start_step */ 0); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::topk_merge) \
    { \
        _bitonic_topk_merge<APPROX_MODE, is_fp32_dest_acc_en, STABLE_SORT>( \
            /* m_iter */ 5, \
            /* k */ 10); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::topk_rebuild) \
    { \
        _bitonic_topk_rebuild<APPROX_MODE, is_fp32_dest_acc_en, STABLE_SORT>( \
            /* idir */ 0, \
            /* m_iter */ 5, \
            /* k */ 10, \
            /* logk */ 3, \
            /* skip_second */ 0); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::relu_max) \
    { \
        _relu_max_<sfpi::vFloat, APPROX_MODE, ITERATIONS>(5.0f); \
    } \
    else if constexpr (SFPU_UNARY_OPERATION == SfpuType::relu_min) \
    { \
        _relu_min_<sfpi::vFloat, APPROX_MODE, ITERATIONS>(5.0f); \
    } \
    _llk_math_eltwise_unary_sfpu_done_(); \
} while (false)
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
