
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

void run_kernel()
{
    _llk_unpack_A_init_<BROADCAST_TYPE, ACC_TO_DEST, REUSE_DEST_TYPE, unpack_to_dest>(
        UNPACK_TRANSPOSE_FACES, UNPACK_TRANSPOSE_WITHIN_FACE, FACE_R_DIM, NUM_FACES, formats.unpack_src, formats.unpack_dst);
    _llk_unpack_A_hw_configure_<is_fp32_dest_acc_en, STOCHASTIC_RND, disable_src_zero_flag>(
        formats.unpack_src, formats.unpack_dst, FACE_R_DIM, UNPACK_TRANSPOSE_WITHIN_FACE, NUM_FACES);

    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_unpack_A_<BROADCAST_TYPE, ACC_TO_DEST, REUSE_DEST_TYPE, unpack_to_dest>(
            L1_ADDRESS(buffer_A[i]), UNPACK_TRANSPOSE_FACES, formats.unpack_src, formats.unpack_dst);
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

void run_kernel()
{
// copy srca to dest
#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BROADCAST_TYPE, false, is_int_fpu_en>(0, 0, NUM_FACES, formats.math);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BROADCAST_TYPE, is_int_fpu_en>(0, 0, NUM_FACES, formats.math);
#endif
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false, false>(formats.math, formats.math);
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BROADCAST_TYPE, unpack_to_dest>(
            i, formats.math, formats.math);
    }
    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel()
{
    // Use proper tile size calculation based on NUM_FACES
    const uint32_t tile_size = 16 * 16 * NUM_FACES;

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(formats.pack_src, formats.pack_dst, tile_size);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, tile_size);
#endif

    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(formats.pack_dst);

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();
#else
    _llk_pack_dest_init_<DstSync::SyncHalf, false, DstTileFaceLayout::RowMajor, false>();
#endif

    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(i, L1_ADDRESS(buffer_Res[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}
#endif
