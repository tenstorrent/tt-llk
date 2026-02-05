// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"
#include "llk_memory_checks.h"

// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src,
        formats.unpack_src,
        formats.unpack_dst,
        formats.unpack_dst,
        FACE_R_DIM,
        FACE_R_DIM,
        params->num_faces_A,
        params->num_faces_B,
        TILE_SIZE_UNPACK_A,
        TILE_SIZE_UNPACK_B);

    _llk_unpack_AB_init_<false>(0, 0, 1);

    // Unpack input tiles (2 rows x 4 cols = 8 tiles)
    // We're unpacking them into operand A for the reduce operation
    for (uint32_t i = 0; i < params->TILE_CNT_INPUT; i++)
    {
        _llk_unpack_AB_<false, false, false>(
            L1_ADDRESS(buffer_A[i]),
            L1_ADDRESS(buffer_B[0]), // scale tile (1.0)
            i,
            0,
            TILE_SIZE_UNPACK_A,
            TILE_SIZE_UNPACK_B);
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_reduce_custom.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    // Initialize reduce_block_max_row with block_ct_dim = 4 (4 tiles in width)
    _llk_math_reduce_block_max_row_init_<4, is_fp32_dest_acc_en>();

    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();

    // Perform reduce for each row of tiles
    // Row 0: tiles 0-3 -> output tile 0
    // Row 1: tiles 4-7 -> output tile 1
    for (uint32_t row = 0; row < 2; row++)
    {
        _llk_math_reduce_block_max_row_<4, is_fp32_dest_acc_en>(row);
    }

    _llk_math_reduce_block_max_row_uninit_();
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
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(formats.pack_src, formats.pack_dst, TILE_SIZE_PACK);
    _llk_pack_init_<false, false, false>(formats.pack_dst);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, TILE_SIZE_PACK);
    _llk_pack_init_<false, false>(formats.pack_dst);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>();
#endif
    _llk_packer_wait_for_math_done_();

    // Pack 2 output tiles (tile 0 of first row, tile 0 of second row)
    for (int i = 0; i < params->TILE_CNT; i++)
    {
        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(i, L1_ADDRESS(buffer_Res[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif
