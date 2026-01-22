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
#include "tensix_types.h"

namespace
{
constexpr std::underlying_type_t<DataFormat> get_data_format(DataFormat format)
{
    return static_cast<std::underlying_type_t<DataFormat>>(format);
}
} // namespace

/*DATA FORMAT CONFIGURATION*/

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
