
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

#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel()
{
    // Configure hardware for unpacking:
    // - srcA with transpose enabled
    // - srcB with column broadcast
    _llk_unpack_AB_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src,
        formats.unpack_src,
        formats.unpack_dst,
        formats.unpack_dst,
        FACE_R_DIM,
        UNPACK_TRANSPOSE_WITHIN_FACE, // Enable within-face transpose for srcA
        NUM_FACES);

    // Initialize unpack with column broadcast on srcB and transpose on srcA
    _llk_unpack_AB_init_<BROADCAST_TYPE>(
        FACE_R_DIM,
        NUM_FACES,
        false,                   // narrow_tile
        UNPACK_TRANSPOSE_FACES); // Enable face rearrangement for srcA

    // Unpack tiles: srcA will be transposed, srcB will be column broadcasted
    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_unpack_AB_<BROADCAST_TYPE>(L1_ADDRESS(buffer_A[i]), L1_ADDRESS(buffer_B[i]));
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "params.h"

using namespace ckernel;

void run_kernel()
{
    // Initialize math for element-wise subtraction
    _llk_math_pack_sync_init_<dest_sync, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false, false>(formats.math, formats.math);
    _llk_math_eltwise_binary_init_<EltwiseBinaryType::ELWSUB, BROADCAST_TYPE>(NUM_FACES, 0);

    _llk_math_wait_for_dest_available_<dest_sync>();

    // Perform element-wise subtraction: result = transposed(srcA) - column_broadcast(srcB)
    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_math_eltwise_binary_<EltwiseBinaryType::ELWSUB, BROADCAST_TYPE, dest_sync, is_fp32_dest_acc_en>(
            NUM_FACES, i /* dst_index */, false /* clear_fp32_dst_acc */);
    }

    _llk_math_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel()
{
#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
#endif

    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(formats.pack_dst);

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<dest_sync, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();
#else
    _llk_pack_dest_init_<dest_sync, false, DstTileFaceLayout::RowMajor, false>();
#endif

    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < TILE_CNT; i++)
    {
        _llk_pack_<dest_sync, is_fp32_dest_acc_en, false>(i, L1_ADDRESS(buffer_Res[i]));
    }
    _llk_pack_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
}

#endif
