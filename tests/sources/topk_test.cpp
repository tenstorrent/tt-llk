// topk_sfpu_test.cpp
// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
// SPDX-License-Identifier: Apache-2.0
//
// Minimal LLK test kernel for Blackhole SFPU TopK.
// Purpose:
//   - Take stimuli unpacked into SRC_A
//   - Copy SRC_A → DEST
//   - Run TopK pipeline on DEST:
//       1) topk_local_sort
//       2) topk_merge
//       3) topk_rebuild
//   - Pack DEST back to L1 as result
//
// Assumptions / contract for correctness tests:
//   - Each DEST instance encodes exactly one 1D sequence of length N (e.g. N=64).
//   - Values and indices are already laid out in DEST-compatible form when
//     the TopK SFPU runs (values + indices share DEST, indices at fixed offset
//     as defined by ckernel_sfpu_topk.h on Blackhole).
//   - Host-side test harness is responsible for generating:
//       vals[i]    = input_row[i]
//       indices[i] = i
//     before calling this kernel (via unpack + datacopy).
//
// This kernel just wires SFPU and LLK plumbing; it does not generate indices
// itself. That keeps it focused on exercising the TopK SFPU pipeline.

#include <algorithm>
#include <cstdint>
#include <type_traits>

#include "ckernel.h"
#include "llk_defs.h"

// Globals required by the test framework
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

// ============================================================================
// UNPACK TRISC
// ============================================================================

#ifdef LLK_TRISC_UNPACK
#include "llk_unpack_A.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    // We treat input as generic unary SFPU stimuli: tiles in L1 → SRC_A.
    // Format selection comes from `formats` / params, same as other SFPU tests.
    //
    // This does *not* know anything about TopK indices; it just moves raw data.

    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);

    _llk_unpack_A_init_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
        /* src_index_base */ 0,
        /* dst_index_base */ 0,
        /* face_r_dim     */ FACE_R_DIM,
        /* num_faces      */ 4,
        formats.unpack_src,
        formats.unpack_dst);

    // Unpack all tiles from L1 buffer_A[i] to SRC_A (or DEST if unpack_to_dest)
    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
            L1_ADDRESS(buffer_A[i]), formats.unpack_src, formats.unpack_dst);
    }
}
#endif // LLK_TRISC_UNPACK

// ============================================================================
// MATH TRISC (TopK SFPU pipeline)
// ============================================================================

#ifdef LLK_TRISC_MATH
#include "ckernel_sfpu.h"
#include "ckernel_sfpu_topk.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "llk_math_eltwise_unary_sfpu.h"
#include "params.h"
#include "sfpu_operations.h" // test_utils::call_sfpu_operation, SfpuType

using namespace ckernel;
using namespace ckernel::sfpu;

// We use 32 SFPU iterations (same as other unary SFPU tests).
constexpr int TOPK_ITERATIONS = 32;

// Template knobs are picked up from params/templates on the Python side
// via APPROX_MODE, FAST_MODE, etc. Here we only care about APPROX_MODE and
// is_fp32_dest_acc_en; FAST_MODE/STABLE_SORT are compile-time params to
// call_sfpu_operation.
void run_kernel(const volatile struct RuntimeParams *params)
{
    // 1) Copy SRC_A → DEST, like in eltwise_unary_sfpu_test.cpp
    //    This gives us DEST initialized with the input payload that TopK
    //    expects (values + indices encoded by the host test harness).
#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<
        DataCopyType::A2D,
        is_fp32_dest_acc_en,
        BroadcastType::NONE,
        false,  // tilize
        false>( // is_int_fpu_en
        /*num_rows_per_matrix=*/4,
        /*math_format=*/formats.math);
#else
    _llk_math_eltwise_unary_datacopy_init_<
        DataCopyType::A2D,
        is_fp32_dest_acc_en,
        BroadcastType::NONE,
        false>( // is_int_fpu_en
        /*num_rows_per_matrix=*/4,
        /*math_format=*/formats.math);
#endif

    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);

    for (int tile = 0; tile < params->TILE_CNT; ++tile)
    {
        // Copy srcA tile → DEST
        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
        _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
            tile, formats.math, formats.math);

        constexpr bool APPROX_MODE = false; // override via template if needed
        constexpr bool FAST_MODE   = false; // TopK ignores FAST_MODE
        constexpr bool STABLE_SORT = false; // or true if you want to test it

        // Initialize TopK (sets up SFPU control register for index tracking)
        _init_topk();

        // Start SFPU section for this tile
        _llk_math_eltwise_unary_sfpu_init_<SfpuType::topk_local_sort>();
        _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(tile);

        // // 2.1) Local sort
        _bitonic_topk_phases_steps<APPROX_MODE, is_fp32_dest_acc_en, STABLE_SORT>(
            /* idir */ 0,
            /* i_end_phase */ 5,
            /* i_start_phase */ 0,
            /* i_end_step */ 10,
            /* i_start_step */ 0);

        // 2.2) Merge
        _bitonic_topk_merge<APPROX_MODE, is_fp32_dest_acc_en, STABLE_SORT>(
            /* m_iter */ 5,
            /* k */ 10);

        // 2.3) Rebuild (final top-k)
        _bitonic_topk_rebuild<APPROX_MODE, is_fp32_dest_acc_en, STABLE_SORT>(
            /* idir */ 0,
            /* m_iter */ 5,
            /* k */ 10,
            /* logk */ 3,
            /* skip_second */ 0);

        _llk_math_eltwise_unary_sfpu_done_();
    }

    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}
#endif // LLK_TRISC_MATH

// ============================================================================
// PACK TRISC
// ============================================================================

#ifdef LLK_TRISC_PACK
#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    // Standard pack path: pack DEST tiles back to L1 buffer_Res[i].
#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<
        is_fp32_dest_acc_en,
        false,  // untilize
        false>( // tilize
        formats.pack_src,
        formats.pack_dst,
        16 * 16 * 4);
#else
    _llk_pack_hw_configure_<
        is_fp32_dest_acc_en,
        false>( // untilize
        formats.pack_src,
        formats.pack_dst,
        16 * 16 * 4);
#endif

    _llk_pack_init_<false, false>(formats.pack_dst);

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
#else
    _llk_pack_dest_init_<DstSync::SyncHalf, false, false>();
#endif

    _llk_packer_wait_for_math_done_();

    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(i, L1_ADDRESS(buffer_Res[i]));
    }

    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}
#endif // LLK_TRISC_PACK
