// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"

// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst, FACE_R_DIM, FACE_R_DIM, params->num_faces, params->num_faces);
    _llk_unpack_A_init_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
        0, 0, FACE_R_DIM, params->num_faces, formats.unpack_src, formats.unpack_dst);
    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
            L1_ADDRESS(buffer_A[i]), formats.unpack_src, formats.unpack_dst);
    }
}

#endif

#ifdef LLK_TRISC_MATH

#ifdef FORMAT_INT32
const bool is_int_fpu_en = true;
#else
const bool is_int_fpu_en = false;
#endif

#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "params.h"

using namespace ckernel;

void run_kernel(const volatile struct RuntimeParams* params)
{
// copy srca to dest
#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false, is_int_fpu_en>(params->num_faces, formats.math);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, is_int_fpu_en>(params->num_faces, formats.math);
#endif
    _llk_math_pack_sync_init_<dest_sync, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_(formats.math, formats.math);

    uint32_t dest_tile_capacity = is_fp32_dest_acc_en ? (dest_sync == DstSync::Full ? 8 : 4) : (dest_sync == DstSync::Full ? 16 : 8);
    ;

    // Calculate total iterations needed to process all tiles in dest capacity chunks
    uint32_t number_of_tile_chunks = (params->TILE_CNT + dest_tile_capacity - 1) / dest_tile_capacity;

    for (int i = 0; i < number_of_tile_chunks; ++i)
    {
        _llk_math_wait_for_dest_available_<dest_sync>();

        uint32_t num_tiles_this_chunk = (i == number_of_tile_chunks - 1) ? (params->TILE_CNT - i * dest_tile_capacity) : dest_tile_capacity;

        for (int j = 0; j < num_tiles_this_chunk; ++j)
        {
#ifdef ARCH_BLACKHOLE
            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, dest_sync, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                params->DST_INDEX + j, formats.math, formats.math, params->num_faces);
#else
            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, dest_sync, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                params->DST_INDEX + j, formats.math, formats.math);
#endif
        }
        _llk_math_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
    }
}

#endif
#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, tilize_en>(
        formats.pack_src, formats.pack_dst, 16 * 16 * 4, FACE_R_DIM, TILE_C_DIM, params->num_faces, false, false, params->RELU_CONFIG);
    _llk_pack_init_<false, false, tilize_en>(formats.pack_dst, FACE_R_DIM, TILE_C_DIM, params->num_faces);
    _llk_pack_dest_init_<dest_sync, is_fp32_dest_acc_en>();
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(
        formats.pack_src, formats.pack_dst, 16 * 16 * 4, FACE_R_DIM, params->num_faces, false, false, params->RELU_CONFIG);
    _llk_pack_init_<false, false>(formats.pack_dst, FACE_R_DIM, params->num_faces);
    _llk_pack_dest_init_<dest_sync, is_fp32_dest_acc_en, false>();
#endif

    uint32_t dest_tile_capacity = is_fp32_dest_acc_en ? (dest_sync == DstSync::Full ? 8 : 4) : (dest_sync == DstSync::Full ? 16 : 8);
    ;

    // Calculate total iterations needed to process all tiles in dest capacity chunks
    uint32_t number_of_tile_chunks = (params->TILE_CNT + dest_tile_capacity - 1) / dest_tile_capacity;

    for (int i = 0; i < number_of_tile_chunks; ++i)
    {
        _llk_packer_wait_for_math_done_();

        uint32_t num_tiles_this_chunk = (i == number_of_tile_chunks - 1) ? (params->TILE_CNT - i * dest_tile_capacity) : dest_tile_capacity;

        for (int j = 0; j < num_tiles_this_chunk; ++j)
        {
            _llk_pack_<dest_sync, is_fp32_dest_acc_en, false>(params->DST_INDEX + j, L1_ADDRESS(buffer_Res[i * dest_tile_capacity + j]));
        }
        _llk_pack_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
    }
}
#endif
