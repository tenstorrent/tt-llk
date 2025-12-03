// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <type_traits>

// Include auto-generated build configuration
#include "build.h"
#include "ckernel_defs.h"
#include "ckernel_sfpu.h"
// #include "data_format_inference.h"
#include "tensix_types.h"

inline uint32_t L1_ADDRESS(uint32_t buffer_address)
{
#ifdef ARCH_QUASAR
    return buffer_address / 16;
#else
    return (buffer_address / 16) - 1;
#endif
}

namespace
{
constexpr std::underlying_type_t<DataFormat> get_data_format(DataFormat format)
{
    return static_cast<std::underlying_type_t<DataFormat>>(format);
}
} // namespace

/*DATA FORMAT CONFIGURATION*/

/**
 * @struct FormatConfig
 * @brief Holds data format configurations for each stage of compute pipeline.
 * including unpacking, math operations, and packing.
 *
 * Each member represents the data format used at a specific stage:
 * - unpack_src: unpacker input format found in L1.
 * - unpack_dst: unpacker output format when unpacking from L1 to the register(s).
 * - math: math format used during compute operations and storing in dest register.
 * - pack_src: packer input format, when packing from dest register to L1.
 * - pack_dst: packer output format, desired result format in L1.
 */
struct FormatConfig
{
    const uint32_t unpack_src;
    const uint32_t unpack_dst;
    const uint32_t math;
    const uint32_t pack_src;
    const uint32_t pack_dst;

    constexpr FormatConfig(uint32_t unpack_src_, uint32_t unpack_dst_, uint32_t math_, uint32_t pack_src_, uint32_t pack_dst_) :
        unpack_src(unpack_src_), unpack_dst(unpack_dst_), math(math_), pack_src(pack_src_), pack_dst(pack_dst_)
    {
    }
};

// Build formats configurations L1-L1 run(s)
#if FUSED_MULTIPLE_RUNS
constexpr std::array<FormatConfig, L1_to_L1_ITERATIONS> formats_array = {
    {FormatConfig(UNPACK_A_IN_LIST[0], UNPACK_A_OUT_LIST[0], MATH_FORMAT_LIST[0], PACK_IN_LIST[0], PACK_OUT_LIST[0]),
     FormatConfig(UNPACK_A_IN_LIST[1], UNPACK_A_OUT_LIST[1], MATH_FORMAT_LIST[1], PACK_IN_LIST[1], PACK_OUT_LIST[1])}};
#else
constexpr FormatConfig formats = FormatConfig(UNPACK_A_IN, UNPACK_A_OUT, MATH_FORMAT, PACK_IN, PACK_OUT);
#endif

// Tile count validation - applies to all kernel variants (UNPACK, MATH, PACK)
#if defined(RT_DIM) && defined(CT_DIM)
constexpr uint32_t tile_count          = RT_DIM * CT_DIM;
constexpr uint32_t max_tiles_fp32_dest = 4; // 32-bit dest accumulation limit
constexpr uint32_t max_tiles_fp16_dest = 8; // 16-bit dest accumulation limit

static_assert(tile_count > 0, "Matrix dimensions invalid: RT_DIM and CT_DIM must be positive");

static_assert(tile_count <= max_tiles_fp16_dest, "Tile count exceeds hardware limit: RT_DIM * CT_DIM must be <= 8");

static_assert(
    !is_fp32_dest_acc_en || (tile_count <= max_tiles_fp32_dest),
    "FP32 dest accumulation requires RT_DIM * CT_DIM <= 4 (current configuration exceeds this limit)");
#endif
