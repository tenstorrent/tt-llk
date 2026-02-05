// topk_test.cpp
// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
// SPDX-License-Identifier: Apache-2.0

//
// TopK Algorithm Description:
// ===========================
// This test implements a bitonic TopK algorithm that extracts the top K=32 values
// from a group of 4 tiles (4096 elements total) in descending order (largest first).
//
// Algorithm Stages:
// 1. UNPACK: Transpose input tiles (both face-level and within-face transpose)
//    - Swaps face positions within each tile
//    - Transposes 16x16 elements within each face
//    - Copies transposed tiles from L1 to SRC_A or DEST registers
//
// 2. MATH (TopK Pipeline):
//    a) Local Sort (topk_local_sort): Initialize TopK SFPU state.
//    b) Phases/Steps (bitonic_topk_phases_steps): Perform bitonic sort phases.
//       - Uses bitonic merge network to sort across the 4-tile group.
//       - end_phase = LOGK-1 = 4 (for K=32, log2(32) = 5).
//    c) Merge (bitonic_topk_merge): Extract top K values from sorted sequence.
//       - TOPK_SORT_DIRECTION=0 means ArgMax mode (descending order, largest values first).
//    d) Rebuild (bitonic_topk_rebuild): Reconstruct final TopK output.
//       - Organizes top K values and their corresponding indices.
//
// 3. PACK: Write result tiles from DEST back to L1 buffer.
//
// Example:
//
// Input Format (32x128 tensor, 4 tiles):
//  - First 64 columns: Values to search for top K.
//  - Second 64 columns: Indices (1-64) that track original positions.
//
// Output Format:
//  - First K values: Top K values in descending order.
//  - Corresponding K indices: Original positions of those top K values.
//
// Sort Direction: Descending (TOPK_SORT_DIRECTION=0 → ArgMax → largest values first).
//                 Ascending (TOPK_SORT_DIRECTION=1 → ArgMin → smallest values first).

#include <algorithm>
#include <cstdint>
#include <type_traits>

#include "ckernel.h"
#include "llk_defs.h"

// Globals required by the test framework.
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

// ============================================================================
// UNPACK TRISC
// ============================================================================

#ifdef LLK_TRISC_UNPACK
#include "llk_unpack_A.h"
#include "llk_unpack_common.h"
#include "llk_unpack_tilize.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);

    // Do transpose.
    _llk_unpack_A_init_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
        /* transpose_of_faces */ 1,
        /* within_face_16x16_transpose */ 1,
        /* face_r_dim     */ FACE_R_DIM,
        /* num_faces      */ 4,
        formats.unpack_src,
        formats.unpack_dst);

    // Unpack all tiles from L1 buffer_A[i] to SRC_A (or DEST if unpack_to_dest) with transpose.
    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
            L1_ADDRESS(buffer_A[i]), formats.unpack_src, formats.unpack_dst);
    }
}
#endif // LLK_TRISC_UNPACK

// ============================================================================
// UNPACK MATH
// ============================================================================

#ifdef LLK_TRISC_MATH
#include "ckernel_sfpu.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "llk_math_transpose_dest.h"
#include "params.h"
using namespace ckernel;

// Define DST_SYNC_MODE so LLK SFPU params helpers compile in this TU.
// This must be done BEFORE including the TopK LLK API header.
#define DST_SYNC_MODE dest_sync
#include "llk_math_eltwise_unary_sfpu_topk.h"
#undef DST_SYNC_MODE

void run_kernel(const volatile struct RuntimeParams *params)
{
    const bool is_int_fpu_en = false;

    _llk_math_pack_sync_init_<dest_sync, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);

#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<
        DataCopyType::A2D,
        is_fp32_dest_acc_en,
        BroadcastType::NONE,
        false, // tilize
        false  // is_int_fpu_en
        >(
        /*num_rows_per_matrix=*/4,
        /*math_format=*/formats.math);
#else
    _llk_math_eltwise_unary_datacopy_init_<
        DataCopyType::A2D,
        is_fp32_dest_acc_en,
        BroadcastType::NONE,
        false // is_int_fpu_en
        >(
        /*num_rows_per_matrix=*/4,
        /*math_format=*/formats.math);
#endif

    _llk_math_wait_for_dest_available_<dest_sync>();

    for (int tile = 0; tile < params->TILE_CNT; ++tile)
    {
        _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
            /*src_tile_index=*/tile, formats.math, formats.math);
    }

    constexpr bool APPROX             = false;
    constexpr bool STABLE_SORT        = false;
    constexpr int M_ITER              = 0;
    constexpr std::uint32_t dst_index = 0;             // base DEST index for the 4-tile group.
    const int end_phase               = TOPK_LOGK - 1; // same as other TopK call sites.
    constexpr int start_phase         = 0;
    constexpr int end_step            = 0;
    constexpr int start_step          = 0;
    constexpr int vector_mode         = (int)VectorMode::RC_custom;

    // These two calls are essentially the same as calling ckernel::llk_math_eltwise_unary_sfpu_topk_init<APPROX>(); from metal.
    _llk_math_eltwise_unary_sfpu_init_<SfpuType::topk_local_sort>();
    ckernel::sfpu::_init_topk();

    // same as calling ckernel::llk_math_eltwise_unary_sfpu_topk_merge from metal.
    _llk_math_eltwise_unary_sfpu_params_<APPROX>(
        ckernel::sfpu::calculate_bitonic_topk_phases_steps<APPROX, is_fp32_dest_acc_en, STABLE_SORT>,
        dst_index,
        vector_mode,
        TOPK_SORT_DIRECTION,
        end_phase,
        start_phase,
        end_step,
        start_step);

    // Same as calling ckernel::llk_math_eltwise_unary_sfpu_topk_merge from metal.
    _llk_math_eltwise_unary_sfpu_params_<APPROX>(
        ckernel::sfpu::calculate_bitonic_topk_merge<APPROX, is_fp32_dest_acc_en, TOPK_SORT_DIRECTION, STABLE_SORT>, dst_index, vector_mode, M_ITER, TOPK_K);

    // Same as calling ckernel::llk_math_eltwise_unary_sfpu_topk_rebuild from metal.
    _llk_math_eltwise_unary_sfpu_params_<APPROX>(
        ckernel::sfpu::calculate_bitonic_topk_rebuild<APPROX, is_fp32_dest_acc_en, STABLE_SORT>,
        dst_index,
        vector_mode,
        TOPK_SORT_DIRECTION,
        M_ITER,
        TOPK_K,
        TOPK_LOGK,
        0);

    _llk_math_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
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
