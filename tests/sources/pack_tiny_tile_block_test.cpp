// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

// Test: Pack tiny tiles (1x32, 8x32, 16x32, 16x16, 32x32) in contiguous
// L1 blocks using _llk_pack_mop_config_ with num_tiles > 1.
//
// The math thread places tiles contiguously in DEST at face granularity
// (using the correct DstTileShape), then pack reads them all in one MOP
// execution via the Z counter.

#include <cstdint>

#include "ckernel.h"
#include "llk_defs.h"

// Globals
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

// ---------------------------------------------------------------------------
// TRISC0 — UNPACK
// ---------------------------------------------------------------------------
#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#if defined(RUNTIME_FORMATS) && !defined(SPEED_OF_LIGHT)
    const FormatConfig& formats = params.formats;
#endif
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_A_src,
        formats.unpack_B_src,
        formats.unpack_A_dst,
        formats.unpack_B_dst,
        params.TEST_FACE_R_DIM,
        params.TEST_FACE_R_DIM,
        params.num_faces,
        params.num_faces);

    _llk_unpack_A_init_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, false>(
        0, 0, params.TEST_FACE_R_DIM, params.num_faces, formats.unpack_A_src, formats.unpack_A_dst);

    const int total_tiles = params.NUM_TILES_IN_BLOCK * params.NUM_BLOCKS;
    for (int i = 0; i < total_tiles; ++i)
    {
        _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, false>(
            L1_ADDRESS(params.buffer_A[i]), formats.unpack_A_src, formats.unpack_A_dst);
    }
}

#endif // LLK_TRISC_UNPACK

// ---------------------------------------------------------------------------
// TRISC1 — MATH (datacopy with correct DstTileShape for tiny tiles)
// ---------------------------------------------------------------------------
#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "params.h"

using namespace ckernel;

void run_kernel(RUNTIME_PARAMETERS params)
{
#if defined(RUNTIME_FORMATS) && !defined(SPEED_OF_LIGHT)
    const FormatConfig& formats = params.formats;
#endif

    // Init datacopy MOP — num_faces configures how many faces the MOP copies
#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false, false>(params.num_faces, formats.math);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false>(params.num_faces, formats.math);
#endif
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);

    const int num_tiles_in_block = params.NUM_TILES_IN_BLOCK;
    const int num_blocks         = params.NUM_BLOCKS;

    for (int block = 0; block < num_blocks; block++)
    {
        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();

        for (int tile = 0; tile < num_tiles_in_block; tile++)
        {
            // Use standard datacopy (Tile32x32 addressing).
            // For num_faces=4 multi-tile, this produces contiguous faces in DEST.
            // For num_faces<4, we pack one tile at a time (num_tiles=1 in MOP).
#ifdef ARCH_BLACKHOLE
            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, false>(
                tile, formats.math, formats.math, params.num_faces);
#else
            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, false>(
                tile, formats.math, formats.math);
#endif
        }

        _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }
}

#endif // LLK_TRISC_MATH

// ---------------------------------------------------------------------------
// TRISC2 — PACK (tiny tile block packing via MOP with num_tiles)
// ---------------------------------------------------------------------------
#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#if defined(RUNTIME_FORMATS) && !defined(SPEED_OF_LIGHT)
    const FormatConfig& formats = params.formats;
#endif
    const int num_tiles_in_block = params.NUM_TILES_IN_BLOCK;
    const int num_blocks         = params.NUM_BLOCKS;

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(
        formats.pack_src, formats.pack_dst, 16 * 16 * 4, params.TEST_FACE_R_DIM, params.in0_tile_c_dim, params.num_faces);

    // Init with num_tiles=1 first; we reconfigure to multi-tile MOP below.
    _llk_pack_init_<false, false, false>(formats.pack_src, formats.pack_dst, params.TEST_FACE_R_DIM, params.in0_tile_c_dim, params.num_faces, false, false, 1);

    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

    reconfigure_packer_l1_acc(params.L1_ACC);

    // For full tiles (num_faces=4), use multi-tile MOP:
    // With Tile32x32 DEST layout, consecutive tiles have contiguous faces
    // (tile 0 = faces 0-3, tile 1 = faces 4-7), so Z counter works.
    //
    // For tiny tiles (num_faces<4), we must pack one tile at a time since
    // Tile32x32 leaves unused face gaps in DEST between tiles.
    const bool use_multi_tile_mop = (params.num_faces == 4);
    if (use_multi_tile_mop)
    {
        _llk_pack_mop_config_<false, false, false>(
            formats.pack_dst, params.TEST_FACE_R_DIM, params.in0_tile_c_dim, params.num_faces, false, false, num_tiles_in_block);
    }
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4, params.TEST_FACE_R_DIM, params.num_faces);
    _llk_pack_init_<false, false>(formats.pack_dst, params.TEST_FACE_R_DIM, params.num_faces);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>();
#endif

    for (int block = 0; block < num_blocks; block++)
    {
        _llk_packer_wait_for_math_done_();

#ifdef ARCH_BLACKHOLE
        if (use_multi_tile_mop)
        {
            // Single _llk_pack_ call packs all tiles contiguously to L1.
            // The MOP outer loop = num_faces * num_tiles_in_block.
            _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(0, L1_ADDRESS(params.buffer_Res[block * num_tiles_in_block]));
        }
        else
        {
            // Tiny tiles: pack one tile at a time using Tile32x32 DEST offsets.
            for (int tile = 0; tile < num_tiles_in_block; ++tile)
            {
                int res_idx = block * num_tiles_in_block + tile;
                _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(tile, L1_ADDRESS(params.buffer_Res[res_idx]));
            }
        }
#else
        // Non-Blackhole: pack tiles one at a time
        for (int tile = 0; tile < num_tiles_in_block; ++tile)
        {
            int res_idx = block * num_tiles_in_block + tile;
            _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(tile, L1_ADDRESS(params.buffer_Res[res_idx]));
        }
#endif

        _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }
}

#endif // LLK_TRISC_PACK
