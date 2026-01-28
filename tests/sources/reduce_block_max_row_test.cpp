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

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB_reduce_custom.h"
#include "llk_unpack_common.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);

    _llk_unpack_AB_reduce_block_max_row_init_<BLOCK_CT_DIM, is_fp32_dest_acc_en>();

    // Unpack block_ct_dim tiles from input A (operand) starting at address 0
    // and 1 tile from input B (scaler with 1.0 values) at address 0
    _llk_unpack_AB_reduce_block_max_row_(L1_ADDRESS(buffer_A[0]), L1_ADDRESS(buffer_B[0]));

    _llk_unpack_AB_reduce_block_max_row_uninit_(FACE_R_DIM, FACE_R_DIM);
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_reduce_custom.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    _llk_math_pack_sync_init_<DstSync::SyncFull, is_fp32_dest_acc_en>();
    _llk_math_wait_for_dest_available_<DstSync::SyncFull>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);

    _llk_math_reduce_block_max_row_init_<BLOCK_CT_DIM, is_fp32_dest_acc_en>();

    // Perform block-based max row reduction
    // Process BLOCK_CT_DIM tiles in the width dimension
    // Result goes to destination register index 0
    _llk_math_reduce_block_max_row_<BLOCK_CT_DIM, is_fp32_dest_acc_en>(0);

    _llk_math_reduce_block_max_row_uninit_();

    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    _llk_pack_init_<false, false>(formats.pack_dst);

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
#endif

    // Configure pack with reduce mask for ROW reduction
    _llk_pack_reduce_mask_config_<false, ckernel::ReduceDim::REDUCE_ROW>();

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
#else
    _llk_pack_dest_init_<DstSync::SyncFull, is_fp32_dest_acc_en, false>();
#endif

    _llk_packer_wait_for_math_done_();
    _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(0, L1_ADDRESS(buffer_Res[0]));
    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

    _llk_pack_reduce_mask_clear_();
}

#endif
