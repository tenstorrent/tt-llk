// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"
#include "params.h"

// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

constexpr std::uint32_t within_face_16x16_transpose = (REDUCE_DIM == ckernel::ReduceDim::REDUCE_ROW) ? 1 : 0;
constexpr bool row_pool                             = (REDUCE_DIM == ckernel::ReduceDim::REDUCE_ROW);

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel()
{
    _llk_unpack_AB_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(
        formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst, FACE_R_DIM, within_face_16x16_transpose);
    _llk_unpack_AB_init_<>(FACE_R_DIM, 4, false, within_face_16x16_transpose, 0);
    _llk_unpack_AB_<>(L1_ADDRESS(buffer_A[0]), L1_ADDRESS(buffer_B[0]), within_face_16x16_transpose);
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_reduce.h"
#include "params.h"

void run_kernel()
{
    const std::uint32_t math_fid = 4;
    const bool is_int_fpu_en     = false;
    const bool fp32_transpose    = false;
    _llk_math_pack_sync_init_<DstSync::SyncFull, is_fp32_dest_acc_en>();
    _llk_math_wait_for_dest_available_<DstSync::SyncFull>();
    _llk_math_hw_configure_<false, row_pool>(formats.math, formats.math);
    _llk_math_reduce_init_<POOL_TYPE, REDUCE_DIM, math_fid>(within_face_16x16_transpose);
    _llk_math_reduce_<POOL_TYPE, REDUCE_DIM, is_fp32_dest_acc_en, math_fid, is_int_fpu_en, fp32_transpose>(0);
    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel()
{
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(formats.pack_dst);

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
#endif

    _llk_pack_reduce_mask_config_<false, REDUCE_DIM>();

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();
#else
    _llk_pack_dest_init_<DstSync::SyncFull, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor, false>();
#endif

    _llk_packer_wait_for_math_done_();
    _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(0, L1_ADDRESS(buffer_Res[0]));
    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

    _llk_pack_reduce_mask_clear_();
}

#endif
