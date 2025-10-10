// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>

#include "build.h"
#include "ckernel.h"
#include "ckernel_defs.h"
#include "data_format_inference.h"
#include "llk_defs.h"
#include "params.h"
#include "perf.h"
#include "profiler.h"
#include "tensix_types.h"

// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

static_assert(PERF_RUN_TYPE == PerfRunType::L1_TO_L1, "Only L1 to L1 is supported for this benchmark");

static constexpr uint32_t MAX_TILES_DEST = dest_datum_width ? 4 : 8;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_common.h"

void run_kernel()
{
    {
        ZONE_SCOPED("INIT")

        _llk_unpack_A_hw_configure_<dest_datum_width, StochRndType::None, unpack_to_dest>(
            formats.unpack_src, formats.unpack_dst, FACE_R_DIM, false, TILE_NUM_FACES);
        _llk_unpack_A_init_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
            UNPACK_TRANSPOSE_FACES, false, FACE_R_DIM, TILE_NUM_FACES, formats.unpack_src, formats.unpack_dst);
        PROFILER_SYNC();
    }

    {
        ZONE_SCOPED("TILE_LOOP")

        for (uint32_t tile = 0; tile < TILE_CNT; tile++)
        {
            _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
                PERF_ADDRESS(PERF_INPUT_A, tile), UNPACK_TRANSPOSE_FACES, formats.unpack_src, formats.unpack_dst);
            _llk_unpack_set_srcb_dummy_valid_();
        }
        PROFILER_SYNC();
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "llk_math_transpose_dest.h"

void run_kernel()
{
    {
        ZONE_SCOPED("INIT")

        _llk_math_pack_sync_init_<DstSync::SyncHalf, dest_datum_width>();
        _llk_math_hw_configure_<>(formats.math, formats.math);
        PROFILER_SYNC();
    }

    {
        ZONE_SCOPED("TILE_LOOP")

        for (uint32_t block_start = 0; block_start < TILE_CNT; block_start += MAX_TILES_DEST)
        {
            uint32_t block_tiles = std::min(TILE_CNT - block_start, MAX_TILES_DEST);

            _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();

#ifdef ARCH_BLACKHOLE
            _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, dest_datum_width, BroadcastType::NONE, false, false>(
                UNPACK_TRANSPOSE_FACES, false, TILE_NUM_FACES, formats.math);
#else
            _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, dest_datum_width, BroadcastType::NONE, false>(
                UNPACK_TRANSPOSE_FACES, false, TILE_NUM_FACES, formats.math);
#endif
            for (uint32_t block_tile = 0; block_tile < block_tiles; block_tile++)
            {
                _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, dest_datum_width, BroadcastType::NONE, unpack_to_dest>(
                    block_tile, formats.math, formats.math);
            }

            _llk_math_transpose_dest_init_<MATH_TRANSPOSE_FACES, dest_datum_width>();
            for (uint32_t block_tile = 0; block_tile < block_tiles; block_tile++)
            {
                _llk_math_transpose_dest_<MATH_TRANSPOSE_FACES, dest_datum_width>(block_tile);
            }

            _llk_math_dest_section_done_<DstSync::SyncHalf, dest_datum_width>();
        }

        PROFILER_SYNC();
    }
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel()
{
    {
        ZONE_SCOPED("INIT")
        _llk_pack_hw_configure_<dest_datum_width>(formats.pack_src, formats.pack_dst, TILE_WIDTH * TILE_HEIGHT);
        _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(formats.pack_dst);
        _llk_pack_dest_init_<DstSync::SyncHalf, dest_datum_width>();
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")

        for (uint32_t block_start = 0; block_start < TILE_CNT; block_start += MAX_TILES_DEST)
        {
            uint32_t block_tiles = std::min(TILE_CNT - block_start, MAX_TILES_DEST);

            _llk_packer_wait_for_math_done_();
            for (uint32_t block_tile = 0; block_tile < block_tiles; block_tile++)
            {
                _llk_pack_<DstSync::SyncHalf, dest_datum_width>(block_tile, PERF_ADDRESS(PERF_OUTPUT, block_start + block_tile));
            }
            _llk_pack_dest_section_done_<DstSync::SyncHalf, dest_datum_width>();
        }
        PROFILER_SYNC();
    }
}

#endif
