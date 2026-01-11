// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "ckernel.h"
#include "ckernel_defs.h"
#include "llk_defs.h"
#include "params.h"

// Globals
uint32_t unp_cfg_context                              = 0;
uint32_t pack_sync_tile_dst_ptr                       = 0;
uint32_t math_sync_tile_dst_index                     = 0;
static constexpr ckernel::DstSync DST_SYNC            = ckernel::DstSync::SyncHalf;
static constexpr ckernel::DstTileShape dst_tile_shape = ckernel::DstTileShape::Tile32x32;
// Use half of dest register because of DST_SYNC = SyncHalf:
static constexpr int MAX_TILES_DEST = is_fp32_dest_acc_en ? (BIT32_DEST_REGISTER_HALF_SIZE / 64) : (DEST_REGISTER_HALF_SIZE / 64);
#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_common.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst, FACE_R_DIM, FACE_R_DIM, TILE_NUM_FACES, TILE_NUM_FACES);

    _llk_unpack_A_init_<BroadcastType::NONE, is_fp32_dest_acc_en, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
        0 /* transpose_of_faces */, 0 /* within_face_16x16_transpose */, FACE_R_DIM, TILE_NUM_FACES, formats.unpack_src, formats.unpack_dst);

    for (int block_start = 0; block_start < params->TILE_CNT; block_start += MAX_TILES_DEST)
    {
        int block_tiles = std::min(params->TILE_CNT - block_start, MAX_TILES_DEST);

        for (int block_tile = 0; block_tile < block_tiles; ++block_tile)
        {
            _llk_unpack_A_<BroadcastType::NONE, is_fp32_dest_acc_en, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
                L1_ADDRESS(buffer_A[block_start + block_tile]), formats.unpack_src, formats.unpack_dst);
        }
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "ckernel_sfpu.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "llk_math_eltwise_unary_sfpu.h"
#include "sfpu_operations.h"

using namespace ckernel;
using namespace ckernel::sfpu;

const int iterations = 32;

void run_kernel(const volatile struct RuntimeParams *params)
{
// copy srca to dest
#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false, false>(TILE_NUM_FACES, formats.math);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false>(TILE_NUM_FACES, formats.math);
#endif
    _llk_math_pack_sync_init_<DST_SYNC, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_(formats.math, formats.math);

    _llk_math_eltwise_unary_sfpu_init_<SFPU_UNARY_OPERATION>();

    for (int block_start = 0; block_start < params->TILE_CNT; block_start += MAX_TILES_DEST)
    {
        int block_tiles = std::min(params->TILE_CNT - block_start, MAX_TILES_DEST);

        _llk_math_wait_for_dest_available_<DST_SYNC>();
        for (int block_tile = 0; block_tile < block_tiles; ++block_tile)
        {
            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DST_SYNC, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest, dst_tile_shape>(
                block_tile, formats.math, formats.math);

            // calculation of sfpu operation on dest
            _llk_math_eltwise_unary_sfpu_start_<DST_SYNC, is_fp32_dest_acc_en, dst_tile_shape>(block_tile);
            // calling sfpu function from ckernel
            // this part is where parametrization of operation takes part
            test_utils::call_sfpu_operation<APPROX_MODE, is_fp32_dest_acc_en, iterations>(SFPU_UNARY_OPERATION, formats.math);

            _llk_math_eltwise_unary_sfpu_done_();
        }

        _llk_math_dest_section_done_<DST_SYNC, is_fp32_dest_acc_en>();
    }
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(formats.pack_src, formats.pack_dst, FACE_R_DIM * FACE_C_DIM * TILE_NUM_FACES);
    _llk_pack_init_<false, false>(formats.pack_dst, FACE_R_DIM, TILE_C_DIM, TILE_NUM_FACES);
    _llk_pack_dest_init_<DST_SYNC, is_fp32_dest_acc_en>();
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, FACE_R_DIM * FACE_C_DIM * TILE_NUM_FACES);
    _llk_pack_init_<false, false>(formats.pack_dst, FACE_R_DIM, TILE_NUM_FACES);
    _llk_pack_dest_init_<DST_SYNC, is_fp32_dest_acc_en, false>();
#endif

    for (int block_start = 0; block_start < params->TILE_CNT; block_start += MAX_TILES_DEST)
    {
        int block_tiles = std::min(params->TILE_CNT - block_start, MAX_TILES_DEST);

        _llk_packer_wait_for_math_done_();
        for (int block_tile = 0; block_tile < block_tiles; ++block_tile)
        {
            _llk_pack_<DST_SYNC, is_fp32_dest_acc_en, /* untilize */ false, dst_tile_shape>(block_tile, L1_ADDRESS(buffer_Res[block_start + block_tile]));
        }
        _llk_pack_dest_section_done_<DST_SYNC, is_fp32_dest_acc_en>();
    }
}

#endif
