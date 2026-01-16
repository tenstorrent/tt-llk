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
uint32_t unp_cfg_context            = 0;
uint32_t pack_sync_tile_dst_ptr     = 0;
uint32_t math_sync_tile_dst_index   = 0;
static constexpr int MAX_TILES_DEST = is_fp32_dest_acc_en ? 4 : 8;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_common.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
    // TILE_CNT = 2 * num_pairs (A tiles followed by B tiles in buffer_A)
    const int num_pairs = params->TILE_CNT / 2;

    {
        ZONE_SCOPED("INIT")

        _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
            formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst, FACE_R_DIM, FACE_R_DIM, params->num_faces, params->num_faces);

        _llk_unpack_A_init_<BroadcastType::NONE, is_fp32_dest_acc_en, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
            params->UNPACK_TRANSPOSE_FACES, params->UNPACK_TRANSPOSE_WITHIN_FACE, FACE_R_DIM, params->num_faces, formats.unpack_src, formats.unpack_dst);
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")

        if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            if (!unpack_to_dest)
            {
                // Set valid for source A always.
                // Works only when unpacking to dest is not used.
                _perf_unpack_loop_set_valid<
                    /* src A */ true,
                    /* src B */ false>(
                    /* iterations*/ params->num_faces * params->TILE_CNT * params->LOOP_FACTOR);
            }
        }
        else if constexpr (PERF_RUN_TYPE != PerfRunType::PACK_ISOLATE)
        {
            for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
            {
                for (int i = 0; i < num_pairs; ++i)
                {
                    // Unpack A tile (index i)
                    _llk_unpack_A_<BroadcastType::NONE, is_fp32_dest_acc_en, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
                        PERF_ADDRESS(PERF_INPUT_A, /* tile_idx */ i), formats.unpack_src, formats.unpack_dst);
                    // Unpack B tile (index i + num_pairs)
                    _llk_unpack_A_<BroadcastType::NONE, is_fp32_dest_acc_en, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
                        PERF_ADDRESS(PERF_INPUT_A, /* tile_idx */ i + num_pairs), formats.unpack_src, formats.unpack_dst);
                }
            }
        }
        PROFILER_SYNC();
    }
}

#endif // LLK_TRISC_UNPACK

#ifdef LLK_TRISC_MATH
#include "build.h"
#include "ckernel_sfpu.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_binary_sfpu.h"
#include "llk_math_eltwise_unary_datacopy.h"

using namespace ckernel::sfpu;

void run_kernel(const volatile struct RuntimeParams* params)
{
    const bool is_int_fpu_en = false;
    // TILE_CNT = 2 * num_pairs (A tiles followed by B tiles in buffer_A)
    const int num_pairs = params->TILE_CNT / 2;

    {
        ZONE_SCOPED("INIT")

#ifdef ARCH_BLACKHOLE
        _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false, is_int_fpu_en>(
            params->num_faces, formats.math);
#else
        _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, is_int_fpu_en>(params->num_faces, formats.math);
#endif
        _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
        _llk_math_hw_configure_(formats.math, formats.math);

        _llk_math_eltwise_binary_sfpu_init_<SfpuType::add1>();
        _calculate_max_init_();
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")

        if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE)
        {
            if constexpr (unpack_to_dest)
            {
                for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
                {
                    for (int i = 0; i < params->TILE_CNT; ++i)
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
                    /* iterations*/ params->num_faces * params->TILE_CNT * params->LOOP_FACTOR);
            }
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::L1_CONGESTION)
        {
            if constexpr (unpack_to_dest)
            {
                for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
                {
                    for (int block_start = 0; block_start < params->TILE_CNT; block_start += MAX_TILES_DEST)
                    {
                        int block_tiles = std::min(params->TILE_CNT - block_start, MAX_TILES_DEST);

                        for (int block_tile = 0; block_tile < block_tiles; ++block_tile)
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
                for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
                {
                    for (int block_start = 0; block_start < params->TILE_CNT; block_start += MAX_TILES_DEST)
                    {
                        int block_tiles = std::min(params->TILE_CNT - block_start, MAX_TILES_DEST);

                        for (int block_tile = 0; block_tile < block_tiles; ++block_tile)
                        {
                            _perf_math_loop_clear_valid<
                                /* src A */ true,
                                /* src B */ true>(
                                /* iterations*/ params->num_faces);
                        }

                        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
                        _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                    }
                }
            }
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
            {
                for (int block_start = 0; block_start < params->TILE_CNT; block_start += MAX_TILES_DEST)
                {
                    if constexpr (!unpack_to_dest)
                    {
                        int block_tiles = std::min(params->TILE_CNT - block_start, MAX_TILES_DEST);

                        for (int block_tile = 0; block_tile < block_tiles; ++block_tile)
                        {
                            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                                block_start + block_tile, formats.math, formats.math);
                        }
                    }

                    // Process pairs: max(dest[i*2], dest[i*2+1]) -> dest[i*2]
                    // block_start corresponds to paired tiles, so we process block_start/2 pairs
                    int pairs_in_block = std::min((params->TILE_CNT - block_start) / 2, MAX_TILES_DEST / 2);
                    for (int pair = 0; pair < pairs_in_block; ++pair)
                    {
                        int pair_idx = (block_start / 2) + pair;
                        _llk_math_eltwise_binary_sfpu_start_<DstSync::SyncHalf>(pair_idx * 2);
                        _calculate_max_<false, 32>(32);
                        _llk_math_eltwise_binary_sfpu_done_();
                    }
                }
            }
        }
        else if constexpr (PERF_RUN_TYPE != PerfRunType::PACK_ISOLATE)
        {
            for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
            {
                for (int block_start = 0; block_start < params->TILE_CNT; block_start += MAX_TILES_DEST)
                {
                    int block_tiles = std::min(params->TILE_CNT - block_start, MAX_TILES_DEST);

                    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();

                    // Copy from srcA to dest
                    for (int block_tile = 0; block_tile < block_tiles; ++block_tile)
                    {
                        _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                            block_start + block_tile, formats.math, formats.math);
                    }

                    // Process pairs: max(dest[i*2], dest[i*2+1]) -> dest[i*2]
                    int pairs_in_block = std::min((params->TILE_CNT - block_start) / 2, MAX_TILES_DEST / 2);
                    for (int pair = 0; pair < pairs_in_block; ++pair)
                    {
                        int pair_idx = (block_start / 2) + pair;
                        _llk_math_eltwise_binary_sfpu_start_<DstSync::SyncHalf>(pair_idx * 2);
                        _calculate_max_<false, 32>(32);
                        _llk_math_eltwise_binary_sfpu_done_();
                    }

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

void run_kernel(const volatile struct RuntimeParams* params)
{
    // TILE_CNT = 2 * num_pairs (A tiles followed by B tiles in buffer_A)
    const int num_pairs = params->TILE_CNT / 2;

    {
        ZONE_SCOPED("INIT")

        // Configure packer hardware
        _llk_pack_hw_configure_<is_fp32_dest_acc_en>(formats.pack_src, formats.pack_dst, FACE_R_DIM * FACE_C_DIM * params->num_faces);

#ifdef ARCH_BLACKHOLE
        _llk_pack_init_<false, false>(formats.pack_dst, FACE_R_DIM, TILE_C_DIM, params->num_faces);
#else
        _llk_pack_init_<false, false>(formats.pack_dst, FACE_R_DIM, params->num_faces);
#endif
        // Initialize destination for packing
        _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")

        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE)
        {
            for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
            {
                for (int block_start = 0; block_start < params->TILE_CNT; block_start += MAX_TILES_DEST)
                {
                    // Pack results from tiles 0, 2, 4, ... (each pair produces one result at even indices)
                    int pairs_in_block = std::min((params->TILE_CNT - block_start) / 2, MAX_TILES_DEST / 2);
                    for (int pair = 0; pair < pairs_in_block; ++pair)
                    {
                        int pair_idx = (block_start / 2) + pair;
                        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en>(pair * 2, PERF_ADDRESS(PERF_OUTPUT, pair_idx));
                    }
                }
            }
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::L1_TO_L1 || PERF_RUN_TYPE == PerfRunType::L1_CONGESTION)
        {
            for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
            {
                for (int block_start = 0; block_start < params->TILE_CNT; block_start += MAX_TILES_DEST)
                {
                    _llk_packer_wait_for_math_done_();

                    // Pack results from tiles 0, 2, 4, ... (each pair produces one result at even indices)
                    int pairs_in_block = std::min((params->TILE_CNT - block_start) / 2, MAX_TILES_DEST / 2);
                    for (int pair = 0; pair < pairs_in_block; ++pair)
                    {
                        int pair_idx = (block_start / 2) + pair;
                        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en>(pair * 2, PERF_ADDRESS(PERF_OUTPUT, pair_idx));
                    }

                    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                }
            }
        }

        PROFILER_SYNC();
    }
}

#endif // LLK_TRISC_PACK
