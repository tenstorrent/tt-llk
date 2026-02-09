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
// Stage Definitions
// ============================================================================

enum class Stage : int
{
    Values  = 0, // Stage for processing value tiles.
    Indices = 1  // Stage for processing index tiles.
};

constexpr int num_stages = 2;

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
    // We use 2 stages, one for unpacking value tiles and one for unpacking index tiles, because they have different source and destination formats.

    // For this test, we want to run the same unpack kernel twice with different source/destination formats:
    // Stage::Values: Unpack value tiles
    // Stage::Indices: Unpack index tiles.

    const std::uint32_t unpack_src_data_types[num_stages] = {formats.unpack_src, ckernel::to_underlying(DataFormat::UInt16)};
    const std::uint32_t unpack_dst_data_types[num_stages] = {formats.unpack_dst, ckernel::to_underlying(DataFormat::UInt16)};

    const int num_of_tiles_per_stage = params->TILE_CNT / num_stages; // First half of tiles are values, second half are indices.

    for (Stage stage : {Stage::Values, Stage::Indices})
    {
        const int stage_index                 = static_cast<int>(stage);
        const std::uint32_t unpack_src_format = unpack_src_data_types[stage_index];
        const std::uint32_t unpack_dst_format = unpack_dst_data_types[stage_index];

        if (stage == Stage::Values)
        {
            _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
                unpack_src_format, unpack_src_format, unpack_dst_format, unpack_dst_format, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);
        }
        else
        {
            // Stage::Indices we need to use reconfigure API to avoid race condition between hardware configuration in the second stage and unpacking in the
            // first stage.
            _llk_unpack_reconfig_data_format_srca_impl_<is_fp32_dest_acc_en, false /* to_from_int8 */>(
                unpack_src_format, unpack_dst_format, 16 * 16 * 4 /* tile_size */);
        }

        // Do transpose.
        _llk_unpack_A_init_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
            /* transpose_of_faces */ 1,
            /* within_face_16x16_transpose */ 1,
            /* face_r_dim     */ FACE_R_DIM,
            /* num_faces      */ 4,
            unpack_src_format,
            unpack_dst_format);

        const int tile_start_index = stage_index * num_of_tiles_per_stage;
        const int tile_end_index   = tile_start_index + num_of_tiles_per_stage;

        for (int tile_index = tile_start_index; tile_index < tile_end_index; ++tile_index)
        {
            _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
                L1_ADDRESS(buffer_A[tile_index]), unpack_src_format, unpack_dst_format);
        }
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

    // We use 2 stages, one for unpacking value tiles and one for unpacking index tiles, because they have different source and destination formats.
    const int num_of_tiles_per_stage = params->TILE_CNT / num_stages; // First half of tiles are values, second half are indices.

    const std::uint32_t math_data_types[num_stages] = {formats.math, ckernel::to_underlying(DataFormat::UInt16)};

    // Datacopy Initialization.
    _llk_math_pack_sync_init_<dest_sync, is_fp32_dest_acc_en>();

    _llk_math_wait_for_dest_available_<dest_sync>();

    // Datacopy phase done in two stages same as unpack.
    for (Stage stage : {Stage::Values, Stage::Indices})
    {
        const int stage_index           = static_cast<int>(stage);
        const std::uint32_t math_format = math_data_types[stage_index];

        if (stage == Stage::Values)
        {
            _llk_math_hw_configure_<is_fp32_dest_acc_en>(math_format, math_format);
        }
        else
        {
            // Stage::Indices we need to use reconfigure API to avoid race condition between hardware configuration in the second stage and math in the first
            // stage.
            _llk_math_reconfig_data_format_srca_<is_fp32_dest_acc_en, false /* to_from_int8 */>(math_format);
        }

#ifdef ARCH_BLACKHOLE
        _llk_math_eltwise_unary_datacopy_init_<
            DataCopyType::A2D,
            is_fp32_dest_acc_en,
            BroadcastType::NONE,
            false, // tilize
            false  // is_int_fpu_en
            >(
            /*num_rows_per_matrix=*/4,
            /*math_format=*/math_format);
#else
        _llk_math_eltwise_unary_datacopy_init_<
            DataCopyType::A2D,
            is_fp32_dest_acc_en,
            BroadcastType::NONE,
            false // is_int_fpu_en
            >(
            /*num_rows_per_matrix=*/4,
            /*math_format=*/math_format);
#endif

        const int tile_start_index = stage_index * num_of_tiles_per_stage;
        const int tile_end_index   = tile_start_index + num_of_tiles_per_stage;

        for (int tile_index = tile_start_index; tile_index < tile_end_index; ++tile_index)
        {
            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                /*src_tile_index=*/tile_index, math_format, math_format);
        }
    }

    // Reconfigure data formats for TopK SFPU phases.
    _llk_math_reconfig_data_format_srca_<is_fp32_dest_acc_en, false /* to_from_int8 */>(math_data_types[static_cast<int>(Stage::Values)]);

#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<
        DataCopyType::A2D,
        is_fp32_dest_acc_en,
        BroadcastType::NONE,
        false, // tilize
        false  // is_int_fpu_en
        >(
        /*num_rows_per_matrix=*/4,
        /*math_format=*/math_data_types[0] /* values are always in the first format */);
#else
    _llk_math_eltwise_unary_datacopy_init_<
        DataCopyType::A2D,
        is_fp32_dest_acc_en,
        BroadcastType::NONE,
        false // is_int_fpu_en
        >(
        /*num_rows_per_matrix=*/4,
        /*math_format=*/math_data_types[0] /* values are always in the first format */);
#endif

    // TopK phase.
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

    // same as calling ckernel::llk_math_eltwise_unary_sfpu_topk_local_sort from metal.
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
    const std::uint32_t pack_src_data_types[num_stages] = {formats.pack_src, ckernel::to_underlying(DataFormat::UInt16)};
    const std::uint32_t pack_dst_data_types[num_stages] = {formats.pack_dst, ckernel::to_underlying(DataFormat::UInt16)};

    const int num_of_tiles_per_stage = params->TILE_CNT / num_stages; // First half of tiles are values, second half are indices

    _llk_packer_wait_for_math_done_();

    // Standard pack path: pack DEST tiles back to L1 buffer_Res[i].
    for (Stage stage : {Stage::Values, Stage::Indices})
    {
        const int stage_index               = static_cast<int>(stage);
        const std::uint32_t pack_src_format = pack_src_data_types[stage_index];
        const std::uint32_t pack_dst_format = pack_dst_data_types[stage_index];

        if (stage == Stage::Values)
        {
#ifdef ARCH_BLACKHOLE
            _llk_pack_hw_configure_<
                is_fp32_dest_acc_en,
                false,  // untilize
                false>( // tilize
                pack_src_format,
                pack_dst_format,
                16 * 16 * 4);
#else
            _llk_pack_hw_configure_<
                is_fp32_dest_acc_en,
                false>( // untilize
                pack_src_format,
                pack_dst_format,
                16 * 16 * 4);
#endif
        }
        else
        {
            // Stage::Indices we need to use reconfigure API to avoid race condition between hardware configuration in the second stage and pack in the first
            // stage.
#ifdef ARCH_BLACKHOLE
            _llk_pack_reconfig_data_format_<is_fp32_dest_acc_en, false /* is_tile_dim_reconfig_en */>(
                pack_src_format,
                pack_dst_format,
                16 * 16 * 4,
                FACE_R_DIM,
                TILE_C_DIM,
                4 /* num_faces */,
                false /* partial_face */,
                false /* narrow_tile */,
                1 /* num_tiles */);
#else
            _llk_pack_reconfig_data_format_<is_fp32_dest_acc_en, false /* is_tile_dim_reconfig_en */>(
                pack_src_format, pack_dst_format, 16 * 16 * 4, FACE_R_DIM, 4 /* num_faces */, false /* partial_face */, false /* narrow_tile */);
#endif
        }

        _llk_pack_init_<false, false>(pack_dst_format);

#ifdef ARCH_BLACKHOLE
        _llk_pack_dest_init_<dest_sync, is_fp32_dest_acc_en>();
#else
        _llk_pack_dest_init_<dest_sync, false, false>();
#endif
        const int tile_start_index = stage_index * num_of_tiles_per_stage;
        const int tile_end_index   = tile_start_index + num_of_tiles_per_stage;

        for (int tile_index = tile_start_index; tile_index < tile_end_index; ++tile_index)
        {
            _llk_pack_<dest_sync, is_fp32_dest_acc_en, false>(tile_index, L1_ADDRESS(buffer_Res[tile_index]));
        }
    }

    _llk_pack_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
}
#endif // LLK_TRISC_PACK
