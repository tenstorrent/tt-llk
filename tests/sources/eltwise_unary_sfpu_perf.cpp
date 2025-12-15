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
#include "perf.h"
#include "profiler.h"

// Globals
uint32_t unp_cfg_context                 = 0;
uint32_t pack_sync_tile_dst_ptr          = 0;
uint32_t math_sync_tile_dst_index        = 0;
static constexpr uint32_t MAX_TILES_DEST = is_fp32_dest_acc_en ? 4 : 8;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_common.h"

void run_kernel()
{
    {
        ZONE_SCOPED("INIT")

        _llk_unpack_A_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(
            formats.unpack_src, formats.unpack_dst, FACE_R_DIM, UNPACK_TRANSPOSE_WITHIN_FACE, NUM_FACES);

        _llk_unpack_A_init_<BroadcastType::NONE, is_fp32_dest_acc_en, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
            UNPACK_TRANSPOSE_FACES, UNPACK_TRANSPOSE_WITHIN_FACE, FACE_R_DIM, NUM_FACES, formats.unpack_src, formats.unpack_dst);

        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE)
        {
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            // Set valid for source A only (B is not used in this operation)
            // Works only when unpacking to dest is not used.
            return _perf_unpack_loop_set_valid<
                /* src A */ true,
                /* src B */ false>(
                /* iterations*/ LOOP_FACTOR * TILE_CNT);
        }
        else
        {
            for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
            {
                for (uint32_t i = 0; i < TILE_CNT; ++i)
                {
                    _llk_unpack_A_<BroadcastType::NONE, is_fp32_dest_acc_en, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
                        PERF_ADDRESS(PERF_INPUT_A, /* tile_idx */ i), formats.unpack_src, formats.unpack_dst);
                }
            }
        }
        PROFILER_SYNC();
    }
}

#endif // LLK_TRISC_UNPACK

#ifdef LLK_TRISC_MATH
#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "llk_math_eltwise_unary_sfpu.h"
#include "sfpu_operations.h"

const int iterations = 32;

void run_kernel()
{
    {
        ZONE_SCOPED("INIT")

        _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en>(NUM_FACES, formats.math);
        _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
        _llk_math_hw_configure_<>(formats.math, formats.math);

        _llk_math_eltwise_unary_sfpu_init_<SFPU_UNARY_OPERATION>();
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE)
        {
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE || PERF_RUN_TYPE == PerfRunType::L1_CONGESTION)
        {
            // Clear valid for source A only (B is not used)
            return _perf_math_loop_clear_valid<
                /* src A */ true,
                /* src B */ false>(
                /* iterations*/ LOOP_FACTOR * TILE_CNT);
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            // For MATH_ISOLATE, skip datacopy and only measure SFPU operations
            for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
            {
                for (uint32_t i = 0; i < TILE_CNT; ++i)
                {
                    _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(/* dst_index */ i);
                    test_utils::call_sfpu_operation<iterations>(SFPU_UNARY_OPERATION, formats.math);
                    _llk_math_eltwise_unary_sfpu_done_();

                    // Clear the valid flag set by unpacker
                    _perf_math_loop_clear_valid<
                        /* src A */ true,
                        /* src B */ false>(
                        /* iterations*/ 1);
                }
            }
        }
        else
        {
            for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
            {
                for (uint32_t block_start = 0; block_start < TILE_CNT; block_start += MAX_TILES_DEST)
                {
                    uint32_t block_tiles = std::min(TILE_CNT - block_start, MAX_TILES_DEST);

                    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();

                    // Copy from srcA to dest
                    for (uint32_t block_tile = 0; block_tile < block_tiles; ++block_tile)
                    {
                        _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                            block_start + block_tile, formats.math, formats.math);
                    }

                    // Start SFPU operation
                    _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(/* dst_index */ block_start);

                    test_utils::call_sfpu_operation<iterations>(SFPU_UNARY_OPERATION, formats.math);

                    _llk_math_eltwise_unary_sfpu_done_();
                    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                }
            }
        }
        PROFILER_SYNC();
    }
}

#endif // LLK_TRISC_MATH

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel()
{
    {
        ZONE_SCOPED("INIT")

        // Configure packer hardware
        _llk_pack_hw_configure_<is_fp32_dest_acc_en>(formats.pack_src, formats.pack_dst, FACE_R_DIM * FACE_C_DIM * NUM_FACES);

        _llk_pack_init_</* untilize */ false, /* zero_output */ false, DstTileFaceLayout::RowMajor, /* write_tile_header */ false>(formats.pack_dst);

        // Initialize destination for packing
        _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();

        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE || PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            return;
        }
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE || PERF_RUN_TYPE == PerfRunType::L1_CONGESTION)
        {
            for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
            {
                for (uint32_t block_start = 0; block_start < TILE_CNT; block_start += MAX_TILES_DEST)
                {
                    uint32_t block_tiles = std::min(TILE_CNT - block_start, MAX_TILES_DEST);

                    for (uint32_t block_tile = 0; block_tile < block_tiles; ++block_tile)
                    {
                        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, /* untilize */ false>(
                            block_tile, PERF_ADDRESS(PERF_OUTPUT, block_start + block_tile));
                    }
                }
            }
        }
        else
        {
            // Full L1-to-L1 operation
            for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
            {
                for (uint32_t block_start = 0; block_start < TILE_CNT; block_start += MAX_TILES_DEST)
                {
                    uint32_t block_tiles = std::min(TILE_CNT - block_start, MAX_TILES_DEST);

                    _llk_packer_wait_for_math_done_();
                    for (uint32_t block_tile = 0; block_tile < block_tiles; ++block_tile)
                    {
                        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, /* untilize */ false>(
                            block_tile, PERF_ADDRESS(PERF_OUTPUT, block_start + block_tile));
                    }
                    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                }
            }
        }
        PROFILER_SYNC();
    }
}

#endif // LLK_TRISC_PACK
