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
// 3. Add the case statement in call_sfpu_operation() switch below
// Metal SFPU includes removed — primitives nuked for generator evaluation
#include "metal_sfpu/ckernel_sfpu_log1p.h"

namespace test_utils
{
using namespace ckernel;
using namespace ckernel::sfpu;

/**
 * Template function to call SFPU operations with parameterized iteration count
 * and optional math format for type-specific behavior.
 *
 * @tparam APPROX_MODE Whether to use approximation mode for the SFPU operation
 * @tparam is_fp32_dest_acc_en Whether the destination accumulator is in FP32 mode
 * @tparam ITERATIONS Number of SFPU iterations (typically 32 for full tile)
 * @param operation The SFPU operation type to execute
 * @param math_format Optional math format for operations that need format-specific behavior
 */
template <bool APPROX_MODE, bool is_fp32_dest_acc_en, int ITERATIONS, bool FAST_MODE = false, bool STABLE_SORT = false, bool CLAMP_NEGATIVE = false>
void call_sfpu_operation(SfpuType operation, std::uint32_t math_format = 0, float fill_const_value = 5.0f)
{
    switch (operation)
    {
        case SfpuType::abs:
            _calculate_abs_<APPROX_MODE, ITERATIONS>(ITERATIONS);
            break;
        case SfpuType::acosh:
            _init_inverse_hyperbolic_<APPROX_MODE>();
            _calculate_acosh_<APPROX_MODE, ITERATIONS>();
            break;
        case SfpuType::asinh:
            _init_inverse_hyperbolic_<APPROX_MODE>();
            _calculate_asinh_<APPROX_MODE, ITERATIONS>();
            break;
        case SfpuType::atanh:
            _init_atanh_<APPROX_MODE>();
            _calculate_atanh_<APPROX_MODE, is_fp32_dest_acc_en, ITERATIONS>();
            break;
        case SfpuType::cosine:
            _calculate_cosine_<APPROX_MODE, ITERATIONS>(ITERATIONS);
            break;
        case SfpuType::exp2:
            _init_exp2_<APPROX_MODE>();
            _calculate_exp2_<APPROX_MODE, is_fp32_dest_acc_en, ITERATIONS>();
            break;
        case SfpuType::fill:
            if (math_format == ckernel::to_underlying(DataFormat::Int32))
            {
                _calculate_fill_int_<APPROX_MODE, ckernel::InstrModLoadStore::INT32, ITERATIONS>(static_cast<std::uint32_t>(fill_const_value));
            }
            else if (math_format == ckernel::to_underlying(DataFormat::UInt16))
            {
                _calculate_fill_int_<APPROX_MODE, ckernel::InstrModLoadStore::LO16, ITERATIONS>(static_cast<std::uint32_t>(fill_const_value));
            }
            else if (math_format == ckernel::to_underlying(DataFormat::UInt32))
            {
                _calculate_fill_int_<APPROX_MODE, ckernel::InstrModLoadStore::INT32, ITERATIONS>(static_cast<std::uint32_t>(fill_const_value));
            }
            else
            {
                _calculate_fill_<APPROX_MODE, ITERATIONS>(fill_const_value);
            }
            break;
        case SfpuType::hardsigmoid:
            _init_hardsigmoid_<APPROX_MODE>();
            _calculate_activation_<APPROX_MODE, ckernel::ActivationType::Hardsigmoid, ITERATIONS>();
            break;
        case SfpuType::log1p:
            log1p_init<APPROX_MODE, FAST_MODE, is_fp32_dest_acc_en>();
            calculate_log1p<APPROX_MODE, FAST_MODE, is_fp32_dest_acc_en, ITERATIONS>();
            break;
        case SfpuType::neg:
        case SfpuType::negative:
            if (math_format == ckernel::to_underlying(DataFormat::Int32))
            {
                _calculate_negative_int_<APPROX_MODE, ITERATIONS>();
            }
            else
            {
                _calculate_negative_<APPROX_MODE, ITERATIONS>();
            }
            break;
        case SfpuType::rsqrt:
            _init_rsqrt_<APPROX_MODE>();
            _calculate_rsqrt_<APPROX_MODE, ITERATIONS, is_fp32_dest_acc_en, FAST_MODE>(ITERATIONS);
            break;
        case SfpuType::sine:
            _calculate_sine_<APPROX_MODE, ITERATIONS>(ITERATIONS);
            break;
        case SfpuType::sqrt:
            _init_sqrt_<APPROX_MODE>();
            _calculate_sqrt_<APPROX_MODE, ITERATIONS, is_fp32_dest_acc_en, FAST_MODE>(ITERATIONS);
            break;
        case SfpuType::square:
            _calculate_square_<APPROX_MODE, ITERATIONS>();
            break;
        case SfpuType::threshold:
            _calculate_threshold_<APPROX_MODE, ITERATIONS>(5.0f, 10.0f);
            break;
        case SfpuType::topk_local_sort:
            _bitonic_topk_phases_steps<APPROX_MODE, is_fp32_dest_acc_en, STABLE_SORT>(
                /* idir */ 0,
                /* i_end_phase */ 5,
                /* i_start_phase */ 0,
                /* i_end_step */ 10,
                /* i_start_step */ 0);
            break;
        case SfpuType::topk_merge:
            _bitonic_topk_merge<APPROX_MODE, is_fp32_dest_acc_en, STABLE_SORT>(
                /* m_iter */ 5,
                /* k */ 10);
            break;
        case SfpuType::topk_rebuild:
            _bitonic_topk_rebuild<APPROX_MODE, is_fp32_dest_acc_en, STABLE_SORT>(
                /* idir */ 0,
                /* m_iter */ 5,
                /* k */ 10,
                /* logk */ 3,
                /* skip_second */ 0);
            break;
        default:
            return; // Unsupported op – should never happen
    }
}

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
