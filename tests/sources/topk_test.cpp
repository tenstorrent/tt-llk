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
#include "llk_unpack_tilize.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);

    _llk_unpack_A_init_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
        /* transpose_of_faces */ 1,
        /* within_face_16x16_transpose */ 1,
        /* face_r_dim     */ FACE_R_DIM,
        /* num_faces      */ 4,
        formats.unpack_src,
        formats.unpack_dst);

    // Unpack all tiles from L1 buffer_A[i] to SRC_A (or DEST if unpack_to_dest) with transpose
    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
            L1_ADDRESS(buffer_A[i]), formats.unpack_src, formats.unpack_dst);
    }

    // _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
    //     formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);
    // _llk_unpack_tilize_init_(formats.unpack_src, formats.unpack_dst, BLOCK_CT_DIM, FACE_R_DIM, false);

    // uint32_t read_offset = 0;

    // const std::uint32_t block_ct_dim = is_blackhole ? 0 : BLOCK_CT_DIM;

    // for (uint32_t i = 0; i < BLOCK_RT_DIM; i++)
    // {
    //     for (uint32_t j = 0; j < BLOCK_CT_DIM; j++)
    //     {
    //         _llk_unpack_tilize_(L1_ADDRESS(buffer_A[read_offset]), j, formats.unpack_src, formats.unpack_dst, block_ct_dim, FACE_R_DIM, 4, false);
    //     }
    //     read_offset += BLOCK_RT_DIM;
    // }
}
#endif // LLK_TRISC_UNPACK

// const bool TILIZE = true;

#ifdef LLK_TRISC_MATH
#include "ckernel_sfpu.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"

using namespace ckernel;

// Define DST_SYNC_MODE so LLK SFPU params helpers compile in this TU.
// This must be done BEFORE including the TopK LLK API header.
#define DST_SYNC_MODE DstSync::SyncHalf
#include "llk_math_eltwise_unary_sfpu_topk.h"
#undef DST_SYNC_MODE

#include "params.h"

using namespace ckernel;

void run_kernel(const volatile struct RuntimeParams *params)
{
    constexpr int GROUP_TILES = 4;
    // const bool is_int_fpu_en = false;

    // --------------------------------------------------------------------
    // 0) Generic math + datacopy setup
    // --------------------------------------------------------------------
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

    // copy srca to dest
    // #ifdef ARCH_BLACKHOLE
    //     // set tilize flag to true
    //     _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, TILIZE, is_int_fpu_en>(4, formats.math);
    // #else
    //     _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, is_int_fpu_en>(4, formats.math);
    // #endif

    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);

    // --------------------------------------------------------------------
    // 1) TopK LLK init (SFPU + TopK index tracking)
    // --------------------------------------------------------------------
    constexpr bool APPROX      = false;
    constexpr bool STABLE_SORT = false;

    constexpr int K      = 32;
    constexpr int M_ITER = 0;
    constexpr int LOGK   = 5;

    // These two calls are essentially the same as calling ckernel::llk_math_eltwise_unary_sfpu_topk_init<APPROX>(); from metal.
    _llk_math_eltwise_unary_sfpu_init_<SfpuType::topk_local_sort>();
    ckernel::sfpu::_init_topk();

    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();

    // Copy all tiles in this group into DEST with matching indices
    for (int tile = 0; tile < params->TILE_CNT; ++tile)
    {
        _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
            /*src_tile_index=*/tile, formats.math, formats.math);
    }

    // --------------------------------------------------------------------
    // 3) Run TopK pipeline ONCE over the group starting at dst_index = 0
    // --------------------------------------------------------------------
    const uint dst_index = 0;        // base DEST index for the 4-tile group
    const int idir       = 0;        // 0 = ArgMax, 1 = ArgMin
    const int end_phase  = LOGK - 1; // same as other TopK call sites

    const int start_phase = 0;
    const int end_step    = 0;
    const int start_step  = 0;
    const int vector_mode = (int)VectorMode::RC_custom;

    // same as calling ckernel::llk_math_eltwise_unary_sfpu_topk_merge from metal.
    _llk_math_eltwise_unary_sfpu_params_<APPROX>(
        ckernel::sfpu::calculate_bitonic_topk_phases_steps<APPROX, is_fp32_dest_acc_en, STABLE_SORT>,
        dst_index,
        vector_mode,
        idir,
        end_phase,
        start_phase,
        end_step,
        start_step);

    // Same as calling ckernel::llk_math_eltwise_unary_sfpu_topk_merge from metal.
    _llk_math_eltwise_unary_sfpu_params_<APPROX>(
        ckernel::sfpu::calculate_bitonic_topk_merge<APPROX, is_fp32_dest_acc_en, idir, STABLE_SORT>, dst_index, vector_mode, M_ITER, K);

    // Same as calling ckernel::llk_math_eltwise_unary_sfpu_topk_rebuild from metal.
    _llk_math_eltwise_unary_sfpu_params_<APPROX>(
        ckernel::sfpu::calculate_bitonic_topk_rebuild<APPROX, is_fp32_dest_acc_en, STABLE_SORT>, dst_index, vector_mode, idir, M_ITER, K, LOGK, 0);

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
