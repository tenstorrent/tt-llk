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

void run_kernel(const volatile struct RuntimeParams *params)
{
    // Configure hardware for unpacking
    // Both srcA and srcB are already tilized in L1
    // srcB will have column broadcast applied
    // NOTE: Transpose on srcB is NOT applied here initially - test will fail
    //       User will implement transpose on srcB later
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);

    sdpa_optimization_init_();

    // Unpack tiles: srcA normal, srcB with column broadcast (needs transpose to pass)
    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        sdpa_optimization_(L1_ADDRESS(buffer_A[i]), L1_ADDRESS(buffer_B[i]));
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "params.h"

using namespace ckernel;

void run_kernel(const volatile struct RuntimeParams *params)
{
    // Initialize math for element-wise subtraction with column broadcast
    _llk_math_pack_sync_init_<dest_sync, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);

    sdpa_optimization_new_init_();

    _llk_math_wait_for_dest_available_<dest_sync>();

    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        sdpa_optimization_new_(i);
    }

    _llk_math_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false /* untilize */, false /* tilize */>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false /* untilize */>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
#endif

    _llk_pack_init_<false /* untilize */, false /* zero_output */>(formats.pack_dst);

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<dest_sync, is_fp32_dest_acc_en>();
#else
    _llk_pack_dest_init_<dest_sync, is_fp32_dest_acc_en, false /* untilize */>();
#endif

    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < params->TILE_CNT; i++)
    {
        _llk_pack_<dest_sync, is_fp32_dest_acc_en, false /* untilize */>(i, L1_ADDRESS(buffer_Res[i]));
    }
    _llk_pack_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
}

#endif
