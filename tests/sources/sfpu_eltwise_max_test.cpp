// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

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

void run_kernel(const volatile struct RuntimeParams *params)
{
    // TILE_CNT = 2 * num_pairs (A tiles followed by B tiles in buffer_A)
    // For each pair i: unpack buffer_A[i] (A tile) and buffer_A[i + TILE_CNT/2] (B tile)
    const int num_pairs = params->TILE_CNT / 2;

    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);
    _llk_unpack_A_init_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
        0, 0, FACE_R_DIM, 4, formats.unpack_src, formats.unpack_dst);

    for (int i = 0; i < num_pairs; i++)
    {
        // Unpack A tile (index i) to dest[0]
        _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
            L1_ADDRESS(buffer_A[i]), formats.unpack_src, formats.unpack_dst);
        // Unpack B tile (index i + num_pairs) to dest[1]
        _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
            L1_ADDRESS(buffer_A[i + num_pairs]), formats.unpack_src, formats.unpack_dst);
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "ckernel_defs.h"
#include "ckernel_sfpu.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_binary_sfpu.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "params.h"

using namespace ckernel::sfpu;

void run_kernel(const volatile struct RuntimeParams *params)
{
    const bool is_int_fpu_en = false;

    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_(formats.math, formats.math);

#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false, is_int_fpu_en>(4, formats.math);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, is_int_fpu_en>(4, formats.math);
#endif

    const int num_pairs = params->TILE_CNT / 2;

    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (int i = 0; i < params->TILE_CNT; i++)
    {
        _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
            i, formats.math, formats.math);
    }

    _llk_math_eltwise_binary_sfpu_init_<SfpuType::add1>();
    _calculate_max_init_();

    // Process each pair: max(dest[i*2], dest[i*2+1]) -> dest[i*2]
    for (int i = 0; i < num_pairs; i++)
    {
        _llk_math_eltwise_binary_sfpu_start_<DstSync::SyncHalf>(i * 2);
        _calculate_max_<false, 32>(32);
        _llk_math_eltwise_binary_sfpu_done_();
    }

    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(formats.pack_src, formats.pack_dst, 16 * 16);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, 16 * 16);
#endif

    _llk_pack_init_<false, false>(formats.pack_dst);

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
#else
    _llk_pack_dest_init_<DstSync::SyncHalf, false, false>();
#endif

    const int num_pairs = params->TILE_CNT / 2;

    _llk_packer_wait_for_math_done_();
    // Pack results from tiles 0, 2, 4, ... (each pair produces one result at even indices)
    for (int i = 0; i < num_pairs; i++)
    {
        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(i * 2, L1_ADDRESS(buffer_Res[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif
