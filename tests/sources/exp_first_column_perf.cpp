// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
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
                // Set valid for source B only if dest_acc is enabled.
                // Works only when unpacking to dest is not used.
                _perf_unpack_loop_set_valid<
                    /* src A */ true,
                    /* src B */ is_fp32_dest_acc_en>(
                    /* iterations*/ params->num_faces * params->TILE_CNT * params->LOOP_FACTOR);
            }
        }
        else if constexpr (PERF_RUN_TYPE != PerfRunType::PACK_ISOLATE)
        {
            for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
            {
                for (int i = 0; i < params->TILE_CNT; ++i)
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
#include "ckernel_sfpu_exp.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "llk_math_eltwise_unary_sfpu.h"

// Implementation of exp_tile_first_column from SDPA
// This tests the exponential operation on only the first 8 columns of each face
//
// TILE_LOOP behavior:
// - Processes ALL tiles (params->TILE_CNT, e.g., 8 or 16 tiles)
// - For each block of up to MAX_TILES_DEST tiles (4 or 8):
//   1. Copy tiles from src to dest registers
//   2. Call exp_tile_first_column for EACH tile in the block
//      (exp operates on first 8 columns only, processes 4 faces per tile)
//   3. Pack tiles from dest registers to output
// - Total operations: params->TILE_CNT tiles × params->LOOP_FACTOR iterations
//
// Note: In SDPA, exp_tile_first_column is called with idst=0 because they
// acquire/release dest for each tile. Here, we process blocks of tiles, so
// we call it with block_tile (0 to MAX_TILES_DEST-1) for each tile in the block.
namespace
{

// Simplified exponential calculation for first column only
// Based on SDPA's calculate_exponential_first_column
template <bool SDPA_EXP_APPROX_MODE, uint16_t scale_bf16>
void exp_tile_first_column(uint32_t idst)
{
    constexpr int ITERATIONS_HALF_FACE = 4;

    _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(idst);

    if constexpr (SDPA_EXP_APPROX_MODE)
    {
        // Use piecewise approximation (fast mode)
        for (int d = 0; d < ITERATIONS_HALF_FACE; d++)
        {
            sfpi::vFloat val = sfpi::dst_reg[0];
            sfpi::vFloat result =
                ckernel::sfpu::_calculate_exponential_piecewise_<true /*APPROXIMATION_MODE*/, true /*SCALE_EN*/, true /*SKIP_POSITIVE_CHECK*/>(val, scale_bf16);
            sfpi::dst_reg[0] = result;

            // Stride by 2 to skip columns 8:16 of the face
            sfpi::dst_reg += 2;
        }
    }
    else
    {
        // Use standard exp implementation (polynomial-based, more accurate)
        for (int d = 0; d < ITERATIONS_HALF_FACE; d++)
        {
            sfpi::vFloat val = sfpi::dst_reg[0];

            // Scale the input
            val = val * sfpi::s2vFloat16b(scale_bf16);

            // Use standard exp (internally uses polynomial approximation)
            sfpi::vFloat result =
                ckernel::sfpu::_calculate_exponential_piecewise_<false /*APPROXIMATION_MODE*/, false /*SCALE_EN*/, true /*SKIP_POSITIVE_CHECK*/>(
                    val, scale_bf16);

            sfpi::dst_reg[0] = result;

            // Stride by 2 to skip columns 8:16 of the face
            sfpi::dst_reg += 2;
        }
    }

    _llk_math_eltwise_unary_sfpu_done_();
}

} // namespace

void run_kernel(const volatile struct RuntimeParams* params)
{
    {
        ZONE_SCOPED("INIT")

        _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en>(params->num_faces, formats.math);
        _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
        _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);

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
                        _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                            i, formats.math, formats.math);
                    }
                }
            }
            else
            {
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
                    int block_tiles = std::min(params->TILE_CNT - block_start, MAX_TILES_DEST);

                    if constexpr (!unpack_to_dest)
                    {
                        for (int block_tile = 0; block_tile < block_tiles; ++block_tile)
                        {
                            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                                block_start + block_tile, formats.math, formats.math);
                        }
                    }

                    // Call exp_tile_first_column for EACH tile in the block
                    // Note: In SDPA, they call this once per tile with idst=0 because they acquire/release dest per tile
                    // In our test, we process blocks of tiles, so we call it for each tile with the appropriate dest index
                    constexpr uint16_t scale_bf16 = 0x3F80; // 1.0 in bfloat16
                    for (int block_tile = 0; block_tile < block_tiles; ++block_tile)
                    {
                        exp_tile_first_column<SDPA_EXP_APPROX_MODE, scale_bf16>(block_tile);
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

                    // Call exp_tile_first_column for EACH tile in the block
                    constexpr uint16_t scale_bf16 = 0x3F80; // 1.0 in bfloat16
                    for (int block_tile = 0; block_tile < block_tiles; ++block_tile)
                    {
                        exp_tile_first_column<SDPA_EXP_APPROX_MODE, scale_bf16>(block_tile);
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
    {
        ZONE_SCOPED("INIT")

        _llk_pack_hw_configure_<is_fp32_dest_acc_en>(formats.pack_src, formats.pack_dst, FACE_R_DIM * FACE_C_DIM * params->num_faces);

#ifdef ARCH_BLACKHOLE
        _llk_pack_init_<false, false>(formats.pack_dst, FACE_R_DIM, TILE_C_DIM, params->num_faces);
#else
        _llk_pack_init_<false, false>(formats.pack_dst, FACE_R_DIM, params->num_faces);
#endif
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
                    int block_tiles = std::min(params->TILE_CNT - block_start, MAX_TILES_DEST);

                    for (int block_tile = 0; block_tile < block_tiles; ++block_tile)
                    {
                        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, /* untilize */ false>(
                            block_tile, PERF_ADDRESS(PERF_OUTPUT, block_start + block_tile));
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
                    int block_tiles = std::min(params->TILE_CNT - block_start, MAX_TILES_DEST);

                    _llk_packer_wait_for_math_done_();
                    for (int block_tile = 0; block_tile < block_tiles; ++block_tile)
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
