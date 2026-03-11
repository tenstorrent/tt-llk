// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"

// Globals
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
#ifdef RUNTIME_FORMATS
    const volatile FormatConfig& formats = params->formats;
#endif
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_A_src, formats.unpack_B_src, formats.unpack_A_dst, formats.unpack_B_dst, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);

    _llk_unpackA_bcastB_row_as_col_init_<UNPACK_TRANSPOSE_FACES, UNPACK_TRANSPOSE_WITHIN_FACE>();

    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_unpackA_bcastB_row_as_col_<UNPACK_TRANSPOSE_FACES>(L1_ADDRESS(params->buffer_A[i]), L1_ADDRESS(params->buffer_B[i]), params->ROW_INDEX);
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "params.h"

using namespace ckernel;

void run_kernel(const volatile struct RuntimeParams* params)
{
#ifdef RUNTIME_FORMATS
    const volatile FormatConfig& formats = params->formats;
#endif
    _llk_math_pack_sync_init_<dest_sync, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);

    // Use UNPACK_TRANSPOSE_FACES to control face transpose on math side as well
    // When transpose_faces=true, faces are written as: F0->dest0, F1->dest32, F2->dest16, F3->dest48
    // This swaps F1 and F2 positions to achieve the face transpose effect
    _llk_math_eltwise_binary_bcastB_row_as_col_init_<ELTWISE_BINARY_OP, UNPACK_TRANSPOSE_FACES>();

    _llk_math_wait_for_dest_available_<dest_sync>();

    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_math_eltwise_binary_bcastB_row_as_col_(i);
    }

    _llk_math_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
#ifdef RUNTIME_FORMATS
    const volatile FormatConfig& formats = params->formats;
#endif
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
        _llk_pack_<dest_sync, is_fp32_dest_acc_en, false /* untilize */>(i, L1_ADDRESS(params->buffer_Res[i]));
    }
    _llk_pack_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
}

#endif
