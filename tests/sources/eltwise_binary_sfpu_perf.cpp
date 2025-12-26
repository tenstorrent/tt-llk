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

        _llk_unpack_A_hw_configure_<is_fp32_dest_acc_en>(formats.unpack_src, formats.unpack_dst, FACE_R_DIM, UNPACK_TRANSPOSE_WITHIN_FACE, NUM_FACES);

        _llk_unpack_A_init_<BroadcastType::NONE, is_fp32_dest_acc_en, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
            UNPACK_TRANSPOSE_FACES, UNPACK_TRANSPOSE_WITHIN_FACE, FACE_R_DIM, NUM_FACES, formats.unpack_src, formats.unpack_dst);

        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")

        if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            if (!unpack_to_dest)
            {
                // Set valid for source A always.
                // Set valid for source B only if dest_acc is enabled.
                // Works only when unpacking to dest is not used.
                _perf_unpack_loop_set_valid<
                    /* src A */ true,
                    /* src B */ is_fp32_dest_acc_en>(
                    /* iterations*/ NUM_FACES * TILE_CNT * LOOP_FACTOR);
            }
        }
        else if constexpr (PERF_RUN_TYPE != PerfRunType::PACK_ISOLATE)
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
#include "build.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_binary_sfpu.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "sfpu_operations.h"

const int iterations = 32;

void run_kernel()
{
    {
        ZONE_SCOPED("INIT")

        _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en>(NUM_FACES, formats.math);
        _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
        _llk_math_hw_configure_<>(formats.math, formats.math);

        _llk_math_eltwise_binary_sfpu_init_<SfpuType::add1>();
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")

        if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE)
        {
            if constexpr (unpack_to_dest)
            {
                for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
                {
                    for (uint32_t i = 0; i < TILE_CNT; ++i)
                    {
                        // Only perform synchronization with unpacker, it does not copy
                        // the data when unpack_to_dest is true - as data is already in dest.
                        _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                            i, formats.math, formats.math);
                    }
                }
            }
            else
            {
                // Clear valid for sources A and B
                _perf_math_loop_clear_valid<
                    /* src A */ true,
                    /* src B */ true>(
                    /* iterations*/ NUM_FACES * TILE_CNT * LOOP_FACTOR);
            }
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::L1_CONGESTION)
        {
            if constexpr (unpack_to_dest)
            {
                for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
                {
                    for (uint32_t block_start = 0; block_start < TILE_CNT; block_start += MAX_TILES_DEST)
                    {
                        uint32_t block_tiles = std::min(TILE_CNT - block_start, MAX_TILES_DEST);

                        for (uint32_t block_tile = 0; block_tile < block_tiles; ++block_tile)
                        {
                            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                                block_start + block_tile, formats.math, formats.math);
                        }

                        _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
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

                        for (uint32_t block_tile = 0; block_tile < block_tiles; ++block_tile)
                        {
                            _perf_math_loop_clear_valid<
                                /* src A */ true,
                                /* src B */ true>(
                                /* iterations*/ NUM_FACES);
                        }

                        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
                        _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                    }
                }
            }
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
            {
                for (uint32_t block_start = 0; block_start < TILE_CNT; block_start += MAX_TILES_DEST)
                {
                    if constexpr (!unpack_to_dest)
                    {
                        uint32_t block_tiles = std::min(TILE_CNT - block_start, MAX_TILES_DEST);

                        for (uint32_t block_tile = 0; block_tile < block_tiles; ++block_tile)
                        {
                            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                                block_start + block_tile, formats.math, formats.math);
                        }
                    }

                    _llk_math_eltwise_binary_sfpu_start_<DstSync::SyncHalf>(/* dst_index */ block_start);

                    test_utils::call_binary_sfpu_operation<APPROX_MODE, SFPU_BINARY_OPERATION, iterations>(block_start, formats.math);

                    _llk_math_eltwise_binary_sfpu_done_();
                }
            }
        }
        else if constexpr (PERF_RUN_TYPE != PerfRunType::PACK_ISOLATE)
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

                    // Start SFPU binary operation
                    _llk_math_eltwise_binary_sfpu_start_<DstSync::SyncHalf>(/* dst_index */ block_start);

                    test_utils::call_binary_sfpu_operation<APPROX_MODE, SFPU_BINARY_OPERATION, iterations>(block_start, formats.math);

                    _llk_math_eltwise_binary_sfpu_done_();
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
        _llk_pack_hw_configure_<is_fp32_dest_acc_en>(formats.pack_src, formats.pack_dst, TILE_WIDTH * TILE_HEIGHT);

        _llk_pack_init_</* untilize */ false, /* zero_output */ false, DstTileFaceLayout::RowMajor, /* write_tile_header */ false>(formats.pack_dst);

        // Initialize destination for packing
        _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();

        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")

        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE)
        {
            for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
            {
                for (uint32_t block_start = 0; block_start < TILE_CNT; block_start += MAX_TILES_DEST)
                {
                    uint32_t block_tiles = std::min(TILE_CNT - block_start, MAX_TILES_DEST);

                    for (uint32_t block_tile = 0; block_tile < block_tiles; block_tile++)
                    {
                        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en>(block_tile, PERF_ADDRESS(PERF_OUTPUT, block_start + block_tile));
                    }
                }
            }
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::L1_TO_L1 || PERF_RUN_TYPE == PerfRunType::L1_CONGESTION)
        {
            for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
            {
                for (uint32_t block_start = 0; block_start < TILE_CNT; block_start += MAX_TILES_DEST)
                {
                    uint32_t block_tiles = std::min(TILE_CNT - block_start, MAX_TILES_DEST);

                    _llk_packer_wait_for_math_done_();
                    for (uint32_t block_tile = 0; block_tile < block_tiles; block_tile++)
                    {
                        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en>(block_tile, PERF_ADDRESS(PERF_OUTPUT, block_start + block_tile));
                    }
                    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                }
            }
        }

        PROFILER_SYNC();
    }
}

#endif // LLK_TRISC_PACK
