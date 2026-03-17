// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"

// Globals
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_common.h"
#include "llk_unpack_tilize.h"
#include "params.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#if defined(RUNTIME_FORMATS) && !defined(SPEED_OF_LIGHT)
    const FormatConfig& formats = params.formats;
#endif
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_A_src, formats.unpack_B_src, formats.unpack_A_dst, formats.unpack_B_dst, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);
    _llk_unpack_tilize_init_(formats.unpack_A_src, formats.unpack_A_dst, params.BLOCK_CT_DIM, FACE_R_DIM, false);

    std::uint32_t read_offset = 0;
    const bool is_int8        = (formats.unpack_A_src == to_underlying(DataFormat::Int8)) || (formats.unpack_A_src == to_underlying(DataFormat::UInt8));

#ifdef ARCH_BLACKHOLE
    const std::uint32_t block_ct_dim = 0;
#else
    const std::uint32_t block_ct_dim = params.BLOCK_CT_DIM;
#endif

    for (std::uint32_t i = 0; i < params.BLOCK_RT_DIM; i++)
    {
        for (std::uint32_t j = 0; j < params.BLOCK_CT_DIM; j++)
        {
            if (is_int8)
            {
                _llk_unpack_tilize_int8_workaround_(L1_ADDRESS(params->buffer_A[read_offset]), j, formats.unpack_A_src, FACE_R_DIM, 4, params->BLOCK_CT_DIM);
            }
            else
            {
                _llk_unpack_tilize_(
                    L1_ADDRESS(params->buffer_A[read_offset]), j, formats.unpack_A_src, formats.unpack_A_dst, block_ct_dim, FACE_R_DIM, 4, false);
            }
        }
        read_offset += params.BLOCK_RT_DIM;
    }
}

#endif

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
    const bool is_int_fpu_en  = false;
    const bool tilize_runtime = !(formats.unpack_A_src == to_underlying(DataFormat::Int8) || formats.unpack_A_dst == to_underlying(DataFormat::UInt8));

    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);
// copy srca to dest
#ifdef ARCH_BLACKHOLE
    if (tilize_runtime)
    {
        _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, true, is_int_fpu_en>(4, formats.math);
    }
    else
    {
        _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false, is_int_fpu_en>(4, formats.math);
    }
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, is_int_fpu_en>(4, formats.math);
#endif
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

    const int tiles_in_block = params->NUM_TILES_IN_BLOCK;
    const int num_blocks     = params->NUM_BLOCKS;

    for (int block = 0; block < num_blocks; ++block)
    {
        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
        for (int tile = 0; tile < tiles_in_block; ++tile)
        {
            LLK_ASSERT(
                (static_cast<std::uint32_t>(tile) < get_dest_max_tiles<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileShape::Tile32x32>()),
                "Block tile index exceeds maximum destination tiles");
            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                tile, formats.math, formats.math);
        }
        _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
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
    const bool UNTILIZE       = false;
    const bool tilize_runtime = !(formats.unpack_A_src == to_underlying(DataFormat::Int8) || formats.unpack_A_dst == to_underlying(DataFormat::UInt8));

#ifdef ARCH_BLACKHOLE
    if (tilize_runtime)
    {
        _llk_pack_hw_configure_<is_fp32_dest_acc_en, UNTILIZE, true>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
        _llk_pack_init_<UNTILIZE, false, true>(formats.pack_dst);
    }
    else
    {
        _llk_pack_hw_configure_<is_fp32_dest_acc_en, UNTILIZE, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
        _llk_pack_init_<UNTILIZE, false, false>(formats.pack_dst);
    }
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, UNTILIZE>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
    _llk_pack_init_<UNTILIZE, false>(formats.pack_dst);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, UNTILIZE>();
#endif

    _llk_packer_wait_for_math_done_();
    const int tiles_in_block = params->NUM_TILES_IN_BLOCK;
    const int num_blocks     = params->NUM_BLOCKS;

    for (int block = 0; block < num_blocks; ++block)
    {
        _llk_packer_wait_for_math_done_();
        for (int tile = 0; tile < tiles_in_block; ++tile)
        {
            const int res_tile_idx = (block * tiles_in_block) + tile;
            LLK_ASSERT(
                (static_cast<std::uint32_t>(tile) < get_dest_max_tiles<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileShape::Tile32x32>()),
                "Block tile index exceeds maximum destination tiles");
            _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, UNTILIZE>(tile, L1_ADDRESS(params->buffer_Res[res_tile_idx]));
        }
        _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }
}

#endif
