// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdarg>
#include <type_traits>

#include "ckernel_sfpu_log.h"
#include "ckernel_sfpu_sqrt.h"
#include "ckernel_sfpu_square.h"
#include "llk_defs.h"
#include "llk_sfpu_types.h"
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

#define UNPACK_SRC_CASE(data_format) constexpr auto UNPACK_IN = get_data_format(DataFormat::data_format);

#ifdef UNPACK_SRC_FLOAT16_B
UNPACK_SRC_CASE(Float16_b)
#endif
#ifdef UNPACK_SRC_FLOAT16
UNPACK_SRC_CASE(Float16)
#endif
#ifdef UNPACK_SRC_FLOAT32
UNPACK_SRC_CASE(Float32)
#endif
#ifdef UNPACK_SRC_INT32
UNPACK_SRC_CASE(Int32)
#endif
#ifdef UNPACK_SRC_BFP8_B
UNPACK_SRC_CASE(Bfp8_b)
#endif

#undef UNPACK_SRC_CASE

#define UNPACK_DST_CASE(data_format) constexpr auto UNPACK_OUT = get_data_format(DataFormat::data_format);

#ifdef UNPACK_DST_FLOAT16_B
UNPACK_DST_CASE(Float16_b)
#endif
#ifdef UNPACK_DST_FLOAT16
UNPACK_DST_CASE(Float16)
#endif
#ifdef UNPACK_DST_FLOAT32
UNPACK_DST_CASE(Float32)
#endif
#ifdef UNPACK_DST_INT32
UNPACK_DST_CASE(Int32)
#endif
#ifdef UNPACK_DST_BFP8_B
UNPACK_DST_CASE(Bfp8_b)
#endif

#undef UNPACK_DST_CASE

#define PACK_SRC_CASE(data_format) constexpr auto PACK_IN = get_data_format(DataFormat::data_format);

#ifdef PACK_SRC_FLOAT16_B
PACK_SRC_CASE(Float16_b)
#endif
#ifdef PACK_SRC_FLOAT16
PACK_SRC_CASE(Float16)
#endif
#ifdef PACK_SRC_FLOAT32
PACK_SRC_CASE(Float32)
#endif
#ifdef PACK_SRC_INT32
PACK_SRC_CASE(Int32)
#endif
#ifdef PACK_SRC_BFP8_B
PACK_SRC_CASE(Bfp8_b)
#endif

#undef PACK_SRC_CASE

#define PACK_DST_CASE(data_format) constexpr auto PACK_OUT = get_data_format(DataFormat::data_format);

#ifdef PACK_DST_FLOAT16_B
PACK_DST_CASE(Float16_b)
#endif
#ifdef PACK_DST_FLOAT16
PACK_DST_CASE(Float16)
#endif
#ifdef PACK_DST_FLOAT32
PACK_DST_CASE(Float32)
#endif
#ifdef PACK_DST_INT32
PACK_DST_CASE(Int32)
#endif
#ifdef PACK_DST_BFP8_B
PACK_DST_CASE(Bfp8_b)
#endif

#undef PACK_DST_CASE

#define MATH_CASE(data_format) constexpr auto MATH_FORMAT = get_data_format(DataFormat::data_format);

#ifdef MATH_FLOAT16_B
MATH_CASE(Float16_b)
#endif
#ifdef MATH_FLOAT16
MATH_CASE(Float16)
#endif
#ifdef MATH_FLOAT32
MATH_CASE(Float32)
#endif
#ifdef MATH_INT32
MATH_CASE(Int32)
#endif
#ifdef MATH_BFP8_B
MATH_CASE(Bfp8_b)
#endif

#undef MATH_CASE

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

#ifdef SFPU_OP_SQRT
constexpr auto SFPU_OPERATION = SfpuType::sqrt;
#endif
#ifdef SFPU_OP_LOG
constexpr auto SFPU_OPERATION = SfpuType::log;
#endif
#ifdef SFPU_OP_SQUARE
constexpr auto SFPU_OPERATION = SfpuType::square;
#endif

inline void process_addresses(volatile uint32_t* buffer_Dest[], int n, int first, ...)
{
    buffer_Dest[0] = reinterpret_cast<volatile uint32_t*>(first);

    va_list args;
    va_start(args, first);
    for (int i = 1; i < n; ++i)
    {
        int num        = va_arg(args, int);
        buffer_Dest[i] = reinterpret_cast<volatile uint32_t*>(num);
    }
    va_end(args);
}
