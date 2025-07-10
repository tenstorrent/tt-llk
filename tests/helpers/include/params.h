// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdarg>
#include <type_traits>

// Include auto-generated build configuration
#include "build.h"
#include "ckernel_defs.h"
#include "ckernel_sfpu_binary.h"
#include "ckernel_sfpu_log.h"
#include "ckernel_sfpu_sqrt.h"
#include "ckernel_sfpu_square.h"
#include "data_format_inference.h"
#include "llk_defs.h"
#include "llk_sfpu_types.h"
#include "perf.h"
#include "tensix_types.h"

inline uint32_t L1_ADDRESS(const volatile void* buffer)
{
    return (reinterpret_cast<uint32_t>(buffer) / 16) - 1;
}

namespace
{
constexpr std::underlying_type_t<DataFormat> get_data_format(DataFormat format)
{
    return static_cast<std::underlying_type_t<DataFormat>>(format);
}
} // namespace

constexpr bool dest_acc_en_input =
#ifdef DEST_ACC
    true;
#else
    false;
#endif

constexpr bool unpack_to_dest = UNPACKING_TO_DEST;

/*DATA FORMAT CONFIGURATION*/

// Given input and output formats, infer the rest of the format configuration
#if DATA_FORMAT_INFERENCE_MODEL

// If the input is exponentB, we cannot convert it to Float16 without enabling fp32 mode in dest;
// this is considered a format combination outlier, so we enable dest_acc
constexpr bool is_fp32_dest_acc_en =
    dest_acc_en_input || is_format_combination_outlier(static_cast<DataFormat>(UNPACK_A_IN), static_cast<DataFormat>(PACK_OUT), dest_acc_en_input);

// Get Data Formats :)
inline constexpr std::array<FormatConfig, L1_to_L1_ITERATIONS> formats_array =
    data_formats<static_cast<DataFormat>(UNPACK_A_IN), static_cast<DataFormat>(PACK_OUT), dest_acc_en_input, L1_to_L1_ITERATIONS>();

// Not fusing: single L1-to-L1 iteration, so we retrieve one format configuration
// L1_to_L1_iterations is the number of times we perform llk operations from L1 input tensor to L1 output tensor 
// If L1_to_L1_ITERATIONS is 1, we take input tensor from L1 -> unpack -> math -> pack -> L1
// If L1_to_L1_ITERATIONS is greater than 1, we perform multiple iterations of unpack -> math -> pack, by taking results tensor in L1 to be input tensor of next iteration 
#if L1_to_L1_ITERATIONS == 1
constexpr auto& formats = formats_array[0];
#else
// Fusing: multiple L1-to-L1 iterations, so we retrieve an array of format configurations
// (organized in the same order as the iterations)
constexpr auto& formats = formats_array; 
#endif

#else // Not inferring formats — all formats are pre-defined. Set format configuration directly.
constexpr bool is_fp32_dest_acc_en = dest_acc_en_input; // dest_acc doesn't require adjustment; configuration is hard-coded
constexpr FormatConfig formats     = get_data_formats(
    static_cast<DataFormat>(UNPACK_A_IN),
    static_cast<DataFormat>(UNPACK_A_OUT),
    static_cast<DataFormat>(MATH),
    static_cast<DataFormat>(PACK_IN),
    static_cast<DataFormat>(PACK_OUT));
#endif

#ifdef ELTWISE_BINARY_ADD
constexpr auto ELTWISE_BINARY_OP = ckernel::EltwiseBinaryType::ELWADD;
#endif
#ifdef ELTWISE_BINARY_SUB
constexpr auto ELTWISE_BINARY_OP = ckernel::EltwiseBinaryType::ELWSUB;
#endif
#ifdef ELTWISE_BINARY_MUL
constexpr auto ELTWISE_BINARY_OP = ckernel::EltwiseBinaryType::ELWMUL;
#endif
// TO BE IMPLEMENTED IN LLKs
#ifdef ELTWISE_BINARY_DIV
constexpr auto ELTWISE_BINARY_OP = ckernel::EltwiseBinaryType::ELWDIV;
#endif
#ifdef ELTWISE_BINARY_LESS
constexpr auto ELTWISE_BINARY_OP = ckernel::EltwiseBinaryType::ELWLESS;
#endif

#ifdef SFPU_ELWADD
constexpr auto SFPU_BINARY_OPERATION = ckernel::BinaryOp::ADD;
#endif
#ifdef SFPU_ELWSUB
constexpr auto SFPU_BINARY_OPERATION = ckernel::BinaryOp::SUB;
#endif
#ifdef SFPU_ELWMUL
constexpr auto SFPU_BINARY_OPERATION = ckernel::BinaryOp::MUL;
#endif
#ifdef SFPU_OP_XLOGY
constexpr auto SFPU_BINARY_OPERATION = ckernel::BinaryOp::XLOGY;
#endif
#ifdef SFPU_OP_RSHFT
constexpr auto SFPU_BINARY_OPERATION = ckernel::BinaryOp::RSHFT;
#endif
#ifdef SFPU_OP_LSHFT
constexpr auto SFPU_BINARY_OPERATION = ckernel::BinaryOp::LSHFT;
#endif
#ifdef SFPU_OP_LOGICAL_RSHFT
constexpr auto SFPU_BINARY_OPERATION = ckernel::BinaryOp::LOGICAL_RSHFT;
#endif
#ifdef SFPU_OP_NEG
constexpr auto SFPU_OPERATION = SfpuType::neg;
#endif

#ifdef SFPU_OP_SQRT
constexpr auto SFPU_OPERATION = SfpuType::sqrt;
#endif
#ifdef SFPU_OP_LOG
constexpr auto SFPU_OPERATION = SfpuType::log;
#endif
#ifdef SFPU_OP_SQUARE
constexpr auto SFPU_OPERATION = SfpuType::square;
#endif
#ifdef SFPU_OP_SINE
constexpr auto SFPU_OPERATION = SfpuType::sine;
#endif
#ifdef SFPU_OP_COSINE
constexpr auto SFPU_OPERATION = SfpuType::cosine;
#endif
#ifdef SFPU_OP_ABS
constexpr auto SFPU_OPERATION = SfpuType::abs;
#endif
#ifdef SFPU_OP_RECIPROCAL
constexpr auto SFPU_OPERATION = SfpuType::reciprocal;
#endif
#ifdef SFPU_OP_CELU
constexpr auto SFPU_OPERATION = SfpuType::celu;
#endif
#ifdef SFPU_OP_SILU
constexpr auto SFPU_OPERATION = SfpuType::silu;
#endif
#ifdef SFPU_OP_GELU
constexpr auto SFPU_OPERATION = SfpuType::gelu;
#endif
