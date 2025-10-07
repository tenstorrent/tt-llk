// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_defs.h"
#include "tensix_types.h"
#include "unified_assert.hpp"

struct UnifiedOperand
{
    uint8_t face_width;
    uint8_t face_height;
    uint8_t tile_width_faces;
    uint8_t tile_height_faces;
    DataFormat l1_format;
    DataFormat src_format;
    DataFormat dest_format;
    uint8_t hint;
    uint16_t tensor_width_tiles;
    uint16_t tensor_height_tiles;
    uint32_t l1_address;

    inline uint16_t tile_size() const
    {
        // TODO LP do something better here
        return ckernel::GET_L1_HEADERLESS_TILE_SIZE((uint)l1_format);
    }

    inline uint8_t num_faces() const
    {
        return tile_width_faces * tile_height_faces;
    }

    inline uint8_t tile_width() const
    {
        return face_width * tile_width_faces;
    }

    inline uint8_t tile_height() const
    {
        return face_height * tile_height_faces;
    }

    inline bool narrow_tile() const
    {
        return tile_width_faces != 2;
    }

    inline bool partial_face() const
    {
        return face_width != 16 || face_height != 16;
    }

    inline bool narrow_row() const
    {
        return face_width != 16;
    }
};

inline uint16_t matmul_ct_dim([[maybe_unused]] UnifiedOperand lhs, UnifiedOperand rhs)
{
    LLK_ASSERT(lhs.tensor_width_tiles == rhs.tensor_height_tiles, "Invalid tensor dimensions for matmul");
    return rhs.tensor_width_tiles;
}

inline uint16_t matmul_rt_dim(UnifiedOperand lhs, [[maybe_unused]] UnifiedOperand rhs)
{
    LLK_ASSERT(lhs.tensor_width_tiles == rhs.tensor_height_tiles, "Invalid tensor dimensions for matmul");
    return lhs.tensor_height_tiles;
}

inline uint16_t matmul_kt_dim(UnifiedOperand lhs, [[maybe_unused]] UnifiedOperand rhs)
{
    LLK_ASSERT(lhs.tensor_width_tiles == rhs.tensor_height_tiles, "Invalid tensor dimensions for matmul");
    return lhs.tensor_width_tiles;
}
