// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "llk_defs.h"

constexpr std::int32_t llk_san_x = -1;

enum class llk_san_op
{
    UnpackA,
    UnpackABMatmul,
    UnpackUntilize,
    EltwiseUnaryDatacopy,
    Matmul,
    Pack,
    PackUntilize
};

enum class llk_san_cfg
{
    Addrmod,
    Mop,
    DvalidDisable,
    CH0Strides,
    CH1Strides,
    TileDesc,
    AdcXX,
    Transpose,
    L1Offset
};

enum class llk_san_operand
{
    SrcA,
    SrcB,
    Dst
};

struct llk_san_operand_desc
{
    std::int32_t face_width;
    std::int32_t face_height;
    std::int32_t tile_width_faces;
    std::int32_t tile_height_faces;
    std::int32_t src_fmt;
    std::int32_t dst_fmt;
};

// Derived values:
// tile_width
// tile_height
// tile_size
// num_faces
// narrow_tile
// narrow_row
// partial_face

// Sanitizer state
struct llk_san_state // Per thread
{
    // Operand state
    llk_san_operand_desc SrcA;
    llk_san_operand_desc SrcB;
    llk_san_operand_desc Dst;
    bool dest_width;
};
