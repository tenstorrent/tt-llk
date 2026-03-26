// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

// Test: Pack tiny tiles (1x32, 8x32, 16x32, 16x16, 32x32) in contiguous
// L1 blocks using _llk_pack_mop_config_ with num_tiles > 1.
//
// The math thread places tiles contiguously in DEST at face granularity
// (using the correct DstTileShape shift), then pack reads them all in a
// single _llk_pack_ call whose MOP outer loop = num_faces * num_tiles.

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
// TRISC1 — MATH
// ---------------------------------------------------------------------------
#ifdef LLK_TRISC_MATH

#include "cmath_common.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "params.h"

using namespace ckernel;

void run_kernel(RUNTIME_PARAMETERS params)
{
#if defined(RUNTIME_FORMATS) && !defined(SPEED_OF_LIGHT)
    const FormatConfig& formats = params.formats;
#endif

#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false, false>(params.num_faces, formats.math);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false>(params.num_faces, formats.math);
#endif
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);

    const int num_tiles_in_block = params.NUM_TILES_IN_BLOCK;
    const int num_blocks         = params.NUM_BLOCKS;

#ifdef ARCH_BLACKHOLE
    // DstTileSizeLog2 shift for contiguous DEST placement.
    // Must match the tile's actual face count so the packer's Z counter
    // walks through tiles without hitting unused face slots.
    //   num_faces=4 -> Tile32x32 -> shift 6 (64 rows)
    //   num_faces=2 -> Tile32x16 -> shift 5 (32 rows)
    //   num_faces=1 -> Tile16x16 -> shift 4 (16 rows)
    const std::uint32_t tile_shift = (params.num_faces == 1) ? 4 : (params.num_faces == 2) ? 5 : 6;
#endif

    for (int block = 0; block < num_blocks; block++)
    {
        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();

        for (int tile = 0; tile < num_tiles_in_block; tile++)
        {
#ifdef ARCH_BLACKHOLE
            // Place tiles contiguously in DEST using the correct tile-shape
            // shift instead of datacopy's hardcoded Tile32x32 (shift 6).
            // This replicates the normal A2D datacopy path with a custom
            // DEST offset.
            std::uint32_t dest_offset = (tile << tile_shift) + ckernel::get_dest_buffer_base();
            TT_SETC16(DEST_TARGET_REG_CFG_MATH_Offset_ADDR32, dest_offset);
            ckernel::ckernel_template::run();
            math::clear_dst_reg_addr();
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
// TRISC2 — PACK
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

    // Init pack MOP with num_tiles=1 (sets addr_mods, strides, INTF_SEL).
    _llk_pack_init_<false, false, false>(formats.pack_src, formats.pack_dst, params.TEST_FACE_R_DIM, params.in0_tile_c_dim, params.num_faces, false, false, 1);

    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    reconfigure_packer_l1_acc(params.L1_ACC);

    // Reconfigure MOP outer loop for the full block of tiles.
    // This is the pattern compute kernels use:
    //   _llk_pack_reconfig_data_format_  (optional, sets formats)
    //   _llk_pack_mop_config_            (sets MOP outer = num_faces * num_tiles)
    //   _llk_pack_                       (single call packs all tiles)
    _llk_pack_mop_config_<false, false, false>(
        formats.pack_dst, params.TEST_FACE_R_DIM, params.in0_tile_c_dim, params.num_faces, false, false, num_tiles_in_block);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4, params.TEST_FACE_R_DIM, params.num_faces);
    _llk_pack_init_<false, false>(formats.pack_dst, params.TEST_FACE_R_DIM, params.num_faces);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>();
#endif

    for (int block = 0; block < num_blocks; block++)
    {
        _llk_packer_wait_for_math_done_();

#ifdef ARCH_BLACKHOLE
        // One _llk_pack_ call packs all tiles contiguously to L1.
        // tile_index=0 because tiles are contiguous from DEST face 0.
        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(0, L1_ADDRESS(params.buffer_Res[block * num_tiles_in_block]));
#else
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
