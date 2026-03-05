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

void run_kernel(const volatile struct RuntimeParams* params)
{
#ifdef RUNTIME_FORMATS
    const volatile FormatConfig& formats = params->formats;
#endif
    _llk_unpack_hw_configure_<false>(
        formats.unpack_A_src, formats.unpack_B_src, formats.unpack_A_dst, formats.unpack_B_dst, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);
    _llk_unpack_bcastA_B_init_();

    const int num_blocks     = params->NUM_BLOCKS;
    const int tiles_in_block = params->NUM_TILES_IN_BLOCK;

    // Process data in blocks
    for (int block = 0; block < num_blocks; block++)
    {
        const int block_offset   = block * tiles_in_block;
        const int num_iterations = tiles_in_block / params->SRCA_REUSE_COUNT;

        for (int i = 0; i < num_iterations; i++)
        {
            const int tile_idx   = block_offset / params->SRCA_REUSE_COUNT + i;
            const int b_tile_idx = block_offset + i * params->SRCA_REUSE_COUNT;
            _llk_unpack_bcastA_B_(L1_ADDRESS(params->buffer_A[tile_idx]), L1_ADDRESS(params->buffer_B[b_tile_idx]), params->SRCA_REUSE_COUNT);
        }
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
#ifdef RUNTIME_FORMATS
    const volatile FormatConfig& formats = params->formats;
#endif
    _llk_math_pack_sync_init_<dest_sync, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);
    _llk_math_eltwise_binary_init_<ELTWISE_BINARY_OP, ckernel::MathFidelity::LoFi>(params->SRCA_REUSE_COUNT);

    const int num_blocks     = params->NUM_BLOCKS;
    const int tiles_in_block = params->NUM_TILES_IN_BLOCK;

    for (int block = 0; block < num_blocks; block++)
    {
        _llk_math_wait_for_dest_available_<dest_sync>();

        const int num_iterations = tiles_in_block / params->SRCA_REUSE_COUNT;

        for (int i = 0; i < num_iterations; i++)
        {
            const std::uint32_t tile_index = i * params->SRCA_REUSE_COUNT;
            LLK_ASSERT((tile_index < get_dest_max_tiles<dest_sync, is_fp32_dest_acc_en, DstTileShape::Tile32x32>()), "tile_index exceeds max dest tiles");
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

void run_kernel(const volatile struct RuntimeParams* params)
{
#ifdef RUNTIME_FORMATS
    const volatile FormatConfig& formats = params->formats;
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

    const int num_blocks     = params->NUM_BLOCKS;
    const int tiles_in_block = params->NUM_TILES_IN_BLOCK;

    for (int block = 0; block < num_blocks; block++)
    {
        _llk_packer_wait_for_math_done_();

        const int block_offset = block * tiles_in_block;

        for (int i = 0; i < tiles_in_block; i++)
        {
            const int tile_idx = block_offset + i;
            LLK_ASSERT((i < get_dest_max_tiles<dest_sync, is_fp32_dest_acc_en, DstTileShape::Tile32x32>()), "i exceeds max dest tiles");
            _llk_pack_<dest_sync, is_fp32_dest_acc_en, false>(i, L1_ADDRESS(params->buffer_Res[tile_idx]));
        }

        _llk_pack_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
    }
}

#endif
