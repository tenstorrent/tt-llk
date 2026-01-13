// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Simple perf test for unpackAB with column broadcast on srcB
 * Uses functional test structure with profiling zones
 */

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"
#include "params.h"
#include "profiler.h"

// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    // Configure hardware for unpacking AB with column broadcast
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);

    // Initialize unpack AB with column broadcast on srcB
    _llk_unpack_AB_init_<BroadcastType::COL>(FACE_R_DIM, 4 /* num_faces */, false /* narrow_tile */, 0 /* transpose */);

    PROFILER_SYNC();

    // Measure unpack with column broadcast
    ZONE_SCOPED("TILE_LOOP")
    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_unpack_AB_<BroadcastType::COL>(L1_ADDRESS(buffer_A[i]), L1_ADDRESS(buffer_B[i]));
    }
    PROFILER_SYNC();
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    // Initialize math for binary operation with column broadcast
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_(formats.math, formats.math);
    _llk_math_eltwise_binary_init_<ELTWISE_BINARY_OP, BroadcastType::COL, MATH_FIDELITY>(4 /* num_faces */, false);

    PROFILER_SYNC();

    // Perform math operations
    ZONE_SCOPED("TILE_LOOP")
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_math_eltwise_binary_<ELTWISE_BINARY_OP, BroadcastType::COL, DstSync::SyncHalf, is_fp32_dest_acc_en, MATH_FIDELITY>(
            4 /* num_faces */, i /* dst_index */, false /* clear_fp32_dst_acc */);
    }
    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    PROFILER_SYNC();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>();
#endif

    _llk_pack_init_<false, false>(formats.pack_dst);

    PROFILER_SYNC();

    _llk_packer_wait_for_math_done_();
    ZONE_SCOPED("TILE_LOOP")
    for (int i = 0; i < params->TILE_CNT; i++)
    {
        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(i, L1_ADDRESS(buffer_Res[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    PROFILER_SYNC();
}

#endif
