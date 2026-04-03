// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
//
// Generic unary SFPU accuracy test kernel.
//
// The op to execute is selected at compile time via SFPU_UNARY_OPERATION (set by
// MATH_OP template parameter in the Python harness).  All other knobs (APPROX_MODE,
// FAST_MODE, CLAMP_NEGATIVE) are also compile-time constants injected by the Python
// test runner through build.h.
//
// Runtime parameters (from StimuliConfig + NUM_BLOCKS / NUM_TILES_IN_BLOCK):
//   params.buffer_A[i]   – L1 address of input tile i
//   params.buffer_Res[i] – L1 address of result tile i
//   params.NUM_BLOCKS       – number of blocks to process (= tile_count when
//                             NUM_TILES_IN_BLOCK == 1)
//   params.NUM_TILES_IN_BLOCK – tiles per block (kept at 1 for accuracy tests)

#include <algorithm>
#include <cstdint>
#include <type_traits>

#include "ckernel.h"
#include "ckernel_defs.h"
#include "llk_defs.h"
#include "params.h"

// Globals required by the LLK infrastructure
std::uint32_t unp_cfg_context              = 0;
std::uint32_t pack_sync_tile_dst_ptr       = 0;
std::uint32_t math_sync_tile_dst_index     = 0;
static constexpr ckernel::DstSync DST_SYNC = ckernel::DstSync::SyncHalf;

// ─── UNPACK thread ────────────────────────────────────────────────────────────
#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_common.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#if defined(RUNTIME_FORMATS) && !defined(SPEED_OF_LIGHT)
    const FormatConfig& formats = params.formats;
#endif
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_A_src, formats.unpack_B_src, formats.unpack_A_dst, formats.unpack_B_dst, FACE_R_DIM, FACE_R_DIM, TILE_NUM_FACES, TILE_NUM_FACES);

    _llk_unpack_A_init_<
        BroadcastType::NONE,
        false, // is_fp32_dest_acc_en (false avoids wh b0 regression)
        EltwiseBinaryReuseDestType::NONE,
        unpack_to_dest>(0 /*transpose_of_faces*/, 0 /*within_face_16x16_transpose*/, FACE_R_DIM, TILE_NUM_FACES, formats.unpack_A_src, formats.unpack_A_dst);

    for (std::uint32_t i = 0; i < params.NUM_BLOCKS * params.NUM_TILES_IN_BLOCK; ++i)
    {
        _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
            L1_ADDRESS(params.buffer_A[i]), formats.unpack_A_src, formats.unpack_A_dst);
    }
}

#endif // LLK_TRISC_UNPACK

// ─── MATH thread ──────────────────────────────────────────────────────────────
#ifdef LLK_TRISC_MATH

#include "ckernel_sfpu.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "llk_math_eltwise_unary_sfpu.h"
#include "sfpu_operations.h"

using namespace ckernel;
using namespace ckernel::sfpu;

// 32 SFPU iterations cover one full 32x32 tile (4 faces × 8 rows × 16 lanes)
static constexpr int SFPU_ITERATIONS = 32;

void run_kernel(RUNTIME_PARAMETERS params)
{
#if defined(RUNTIME_FORMATS) && !defined(SPEED_OF_LIGHT)
    const FormatConfig& formats = params.formats;
#endif

#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false, false>(TILE_NUM_FACES, formats.math);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false>(TILE_NUM_FACES, formats.math);
#endif

    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);
    _llk_math_pack_sync_init_<DST_SYNC, is_fp32_dest_acc_en>();
    _llk_math_eltwise_unary_sfpu_init_<SFPU_UNARY_OPERATION>();

    LLK_ASSERT(
        (params.NUM_TILES_IN_BLOCK <= get_dest_max_tiles<DST_SYNC, is_fp32_dest_acc_en, DstTileShape::Tile32x32>()),
        "NUM_TILES_IN_BLOCK exceeds max dest tiles");

    for (int block = 0; block < params.NUM_BLOCKS; ++block)
    {
        _llk_math_wait_for_dest_available_<DST_SYNC>();

        for (std::uint32_t tile = 0; tile < params.NUM_TILES_IN_BLOCK; ++tile)
        {
            // Copy SrcA → DEST
            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DST_SYNC, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                tile, formats.math, formats.math);

            // Run SFPU op in-place on DEST
            _llk_math_eltwise_unary_sfpu_start_<DST_SYNC>(tile);
            test_utils::call_sfpu_operation<APPROX_MODE, is_fp32_dest_acc_en, SFPU_ITERATIONS, FAST_MODE, false /*STABLE_SORT*/, CLAMP_NEGATIVE>(
                SFPU_UNARY_OPERATION, formats.math);
            _llk_math_eltwise_unary_sfpu_done_();
        }

        _llk_math_dest_section_done_<DST_SYNC, is_fp32_dest_acc_en>();
    }
}

#endif // LLK_TRISC_MATH

// ─── PACK thread ──────────────────────────────────────────────────────────────
#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#if defined(RUNTIME_FORMATS) && !defined(SPEED_OF_LIGHT)
    const FormatConfig& formats = params.formats;
#endif

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(formats.pack_src, formats.pack_dst, FACE_R_DIM * FACE_C_DIM * TILE_NUM_FACES);
    _llk_pack_init_<false, false>(formats.pack_dst, FACE_R_DIM, TILE_C_DIM, TILE_NUM_FACES);
    _llk_pack_dest_init_<DST_SYNC, is_fp32_dest_acc_en>();
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, FACE_R_DIM * FACE_C_DIM * TILE_NUM_FACES);
    _llk_pack_init_<false, false>(formats.pack_dst, FACE_R_DIM, TILE_NUM_FACES);
    _llk_pack_dest_init_<DST_SYNC, is_fp32_dest_acc_en, false>();
#endif

    LLK_ASSERT(
        (params.NUM_TILES_IN_BLOCK <= get_dest_max_tiles<DST_SYNC, is_fp32_dest_acc_en, DstTileShape::Tile32x32>()),
        "NUM_TILES_IN_BLOCK exceeds max dest tiles");

    for (int block = 0; block < params.NUM_BLOCKS; ++block)
    {
        _llk_packer_wait_for_math_done_();

        for (std::uint32_t tile = 0; tile < params.NUM_TILES_IN_BLOCK; ++tile)
        {
            _llk_pack_<DST_SYNC, is_fp32_dest_acc_en, false>(tile, L1_ADDRESS(params.buffer_Res[block * params.NUM_TILES_IN_BLOCK + tile]));
        }

        _llk_pack_dest_section_done_<DST_SYNC, is_fp32_dest_acc_en>();
    }
}

#endif // LLK_TRISC_PACK
