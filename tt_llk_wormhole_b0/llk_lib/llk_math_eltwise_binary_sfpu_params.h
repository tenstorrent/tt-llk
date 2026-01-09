// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <utility>

#include "llk_math_eltwise_binary_sfpu.h"
#include "llk_sfpu_types.h"

template <bool APPROXIMATE, typename Callable, typename... Args>
inline void _llk_math_eltwise_binary_sfpu_params_(
    Callable&& sfpu_func,
    const uint dst_index_in0,
    const uint dst_index_in1,
    const uint dst_index_out,
    const int vector_mode = (int)VectorMode::RC,
    Args&&... args)
{
    // Unpack tile dimensions from args if provided. Defaults to 32x32 if not provided.
    // Taking the first two args as tile dimensions (as whole circular buffer is full with the same values anyway).
    // This is to support different tile sizes in the future if needed.
    constexpr uint8_t TILE_R_DIM          = unpack_tile_r_dim[0];
    constexpr uint8_t TILE_C_DIM          = unpack_tile_c_dim[0];
    constexpr uint8_t TILE_FACE_NUMBER    = unpack_tile_num_faces[0];
    constexpr uint8_t TILE_FACE_R_DIM     = unpack_tile_face_r_dim[0];
    constexpr DstTileShape dst_tile_shape = TILE_R_DIM == 32 && TILE_C_DIM == 32   ? DstTileShape::Tile32x32
                                            : TILE_R_DIM == 32 && TILE_C_DIM == 16 ? DstTileShape::Tile32x16
                                            : TILE_R_DIM == 16 && TILE_C_DIM == 16 ? DstTileShape::Tile16x16
                                                                                   : DstTileShape::Tile32x32;

    _llk_math_eltwise_binary_sfpu_start_<DST_SYNC_MODE, DST_ACCUM_MODE, dst_tile_shape>(0); // Is 0 ok or dst_index_in0?

    // Calculate max tile index based on DEST register size, dest sync mode, dest accum mode,
    // and tile size in rows (TILE_NUM_FACES * TILE_FACE_R_DIM)
    // DST_SYNC_MODE:
    // - when SyncFull, that means we can use full DEST register size => as many tiles as fit in full DEST register
    // - when SyncHalf, that means we can use half DEST register size => as many tiles as fit in half DEST register
    // DST_ACCUM_MODE:
    // - when true, we are using FP32 destination accumulation => use DEST_REGISTER_HALF_SIZE
    // - when false, we are using normal destination => use DEST_REGISTER_FULL_SIZE
    // Typical examples:
    // DEST_REGISTER_FULL_SIZE = 1024 bytes, DEST_REGISTER_HALF_SIZE = 512 bytes, BIT32_DEST_REGISTER_HALF_SIZE = 256 bytes
    // TILE_FACE_NUMBER = 4, TILE_FACE_R_DIM = 16 (32x32 tile)
    // - SyncFull + AccumMode false => 1024 / (4*16) = 16 tiles
    // - SyncFull + AccumMode true  => 512 / (4*16)  = 8 tiles
    // - SyncHalf + AccumMode false => 512 / (4*16)  = 8 tiles
    // - SyncHalf + AccumMode true  => 256 / (4*16)  = 4 tiles
    constexpr uint DESTREG_MAX_INDEX = DST_SYNC_MODE == DstSync::SyncFull
                                           ? (DST_ACCUM_MODE ? (DEST_REGISTER_HALF_SIZE / (uint32_t)(TILE_FACE_NUMBER * TILE_FACE_R_DIM))
                                                             : (DEST_REGISTER_FULL_SIZE / (uint32_t)(TILE_FACE_NUMBER * TILE_FACE_R_DIM)))
                                           : (DST_ACCUM_MODE ? (BIT32_DEST_REGISTER_HALF_SIZE / (uint32_t)(TILE_FACE_NUMBER * TILE_FACE_R_DIM))
                                                             : (DEST_REGISTER_HALF_SIZE / (uint32_t)(TILE_FACE_NUMBER * TILE_FACE_R_DIM)));

    LLK_ASSERT(dst_index_in0 < DESTREG_MAX_INDEX, "dst_index_in0 out of range");
    LLK_ASSERT(dst_index_in1 < DESTREG_MAX_INDEX, "dst_index_in1 out of range");
    LLK_ASSERT(dst_index_out < DESTREG_MAX_INDEX, "dst_index_out out of range");

    VectorMode mode = static_cast<VectorMode>(vector_mode);

    if (mode == VectorMode::R)
    {
        // Do a row vector, Face0 + Face1 -- first iteration (first row)
#pragma GCC unroll 0
        for (int face = 0; face < 2; face++)
        {
            std::forward<Callable>(sfpu_func)(dst_index_in0, dst_index_in1, dst_index_out, std::forward<Args>(args)...);
            _llk_math_eltwise_binary_sfpu_inc_dst_face_addr_();
        }
        // Skip the next 2 faces
        _llk_math_eltwise_binary_sfpu_inc_dst_face_addr_();
        _llk_math_eltwise_binary_sfpu_inc_dst_face_addr_();
    }
    else if (mode == VectorMode::C)
    {
        // Do a column vector, Face0 + Face2 -- All iterations for full face
#pragma GCC unroll 0
        for (int face = 0; face < 2; face++)
        {
            std::forward<Callable>(sfpu_func)(dst_index_in0, dst_index_in1, dst_index_out, std::forward<Args>(args)...);
            _llk_math_eltwise_binary_sfpu_inc_dst_face_addr_();
            _llk_math_eltwise_binary_sfpu_inc_dst_face_addr_();
        }
    }
    else if (mode == VectorMode::RC)
    {
        // Do all four faces, and iterate through all 4 blocks of 4 rows each
#pragma GCC unroll 0
        for (int face = 0; face < 4; face++)
        {
            std::forward<Callable>(sfpu_func)(dst_index_in0, dst_index_in1, dst_index_out, std::forward<Args>(args)...);
            _llk_math_eltwise_binary_sfpu_inc_dst_face_addr_();
        }
    }
    else
    {
        std::forward<Callable>(sfpu_func)(dst_index_in0, dst_index_in1, dst_index_out, std::forward<Args>(args)...);
    }
    _llk_math_eltwise_binary_sfpu_done_();
}
