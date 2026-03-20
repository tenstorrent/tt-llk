// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "ckernel_debug.h"
#include "llk_defs.h"

// Globals
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#if defined(RUNTIME_FORMATS) && !defined(SPEED_OF_LIGHT)
    const FormatConfig& formats = params.formats;
#endif
    _llk_unpack_hw_configure_<false>(
        formats.unpack_A_src, formats.unpack_B_src, formats.unpack_A_dst, formats.unpack_B_dst, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);
    _llk_unpack_bcastA_B_init_();

    const int num_blocks         = params.NUM_BLOCKS;
    const int num_tiles_in_block = params.NUM_TILES_IN_BLOCK;

    // Process tiles in blocks
    // Each call to _llk_unpack_bcastA_B_ unpacks SRCA_REUSE_COUNT tiles:
    // one srcA tile is broadcast/reused across SRCA_REUSE_COUNT srcB tiles
    for (int block = 0; block < num_blocks; block++)
    {
        for (int tile = 0; tile < num_tiles_in_block / params.SRCA_REUSE_COUNT; tile++)
        {
            const int tile_base_index = (block * num_tiles_in_block) + (tile * params.SRCA_REUSE_COUNT);
            const int srca_index      = (block * num_tiles_in_block / params.SRCA_REUSE_COUNT) + tile;
            const int srcb_base_index = tile_base_index;
            _llk_unpack_bcastA_B_(L1_ADDRESS(params.buffer_A[srca_index]), L1_ADDRESS(params.buffer_B[srcb_base_index]), params.SRCA_REUSE_COUNT);
        }
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "params.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#if defined(RUNTIME_FORMATS) && !defined(SPEED_OF_LIGHT)
    const FormatConfig& formats = params.formats;
#endif
    _llk_math_pack_sync_init_<dest_sync, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);
    _llk_math_eltwise_binary_init_<ELTWISE_BINARY_OP, ckernel::MathFidelity::LoFi>(params.SRCA_REUSE_COUNT);

    const int num_blocks         = params.NUM_BLOCKS;
    const int num_tiles_in_block = params.NUM_TILES_IN_BLOCK;

    // Process tiles in blocks with proper dest sync
    // Each call to _llk_math_eltwise_binary_ processes SRCA_REUSE_COUNT tiles
    // (configured in _llk_math_eltwise_binary_init_)
    for (int block = 0; block < num_blocks; block++)
    {
        _llk_math_wait_for_dest_available_<dest_sync>();

        for (int tile = 0; tile < num_tiles_in_block / params.SRCA_REUSE_COUNT; tile++)
        {
            const int tile_index = (tile * params.SRCA_REUSE_COUNT);
            LLK_ASSERT(
                (static_cast<std::uint32_t>(tile_index) < get_dest_max_tiles<dest_sync, is_fp32_dest_acc_en, DstTileShape::Tile32x32>()),
                "tile index exceeds max dest tiles");
            _llk_math_eltwise_binary_(tile_index /* dst_index */);
        }

        _llk_math_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
    }
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#if defined(RUNTIME_FORMATS) && !defined(SPEED_OF_LIGHT)
    const FormatConfig& formats = params.formats;
#endif
#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
#endif

    _llk_pack_init_<false, false>(formats.pack_dst);

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<dest_sync, is_fp32_dest_acc_en>();
#else
    _llk_pack_dest_init_<dest_sync, false, false>();
#endif

    const int num_blocks         = params.NUM_BLOCKS;
    const int num_tiles_in_block = params.NUM_TILES_IN_BLOCK;

    // Pack tiles in blocks with proper dest sync
    for (int block = 0; block < num_blocks; block++)
    {
        _llk_packer_wait_for_math_done_();

        for (int tile = 0; tile < num_tiles_in_block; tile++)
        {
            const int res_tile_idx = (block * num_tiles_in_block) + tile;
            LLK_ASSERT(
                (static_cast<std::uint32_t>(tile) < get_dest_max_tiles<dest_sync, is_fp32_dest_acc_en, DstTileShape::Tile32x32>()),
                "tile index exceeds max dest tiles");
            _llk_pack_<dest_sync, is_fp32_dest_acc_en, false>(tile, L1_ADDRESS(params.buffer_Res[res_tile_idx]));
        }

        _llk_pack_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
    }
}

#endif
