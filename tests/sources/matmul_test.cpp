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

#include "llk_unpack_AB_matmul.h"
#include "params.h"

void run_kernel()
{
    std::uint32_t ct_dim = CT_DIM;
    std::uint32_t rt_dim = RT_DIM;
    std::uint32_t kt_dim = KT_DIM;

#if defined ARCH_BLACKHOLE || defined ARCH_WORMHOLE

    _llk_unpack_AB_matmul_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(
        formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst, FACE_R_DIM, FACE_R_DIM, 0, 4, 4, TILE_SIZE_UNPACK, TILE_SIZE_UNPACK);
    _llk_unpack_AB_matmul_init_<>(0, ct_dim, rt_dim, kt_dim, FACE_R_DIM, FACE_R_DIM);
    for (uint32_t j = 0; j < kt_dim; j++)
    {
        _llk_unpack_AB_matmul_<>(
            L1_ADDRESS(buffer_A[0]),
            L1_ADDRESS(buffer_B[0]),
            j,
            j * ct_dim,
            TILE_SIZE_UNPACK,
            TILE_SIZE_UNPACK,
            FACE_R_DIM,
            FACE_R_DIM,
            false,
            false,
            ct_dim,
            rt_dim,
            kt_dim);
    }

#endif

#ifdef ARCH_QUASAR

    const uint BUF_DESC_ID_0 = 0;
    const uint BUF_DESC_ID_1 = 1;

    // Set up the descriptor
    // Start with a buffer descriptor with all zeroes
    buffer_descriptor_u bd_val_srca;
    for (uint i = 0; i < BD_NUM_WORDS /* or 4? */; i++)
    {
        bd_val_srca.words[i] = 0;
    }
    // Now just plug everything in
    bd_val_srca.f.l1_addr_16B = L1_ADDRESS(buffer_A[0]);                  //
    bd_val_srca.f.format      = static_cast<uint8_t>(formats.unpack_src); //
    bd_val_srca.f.x_dim       = 16;                                       // FACE_C_DIM
    bd_val_srca.f.y_dim       = 16;                                       // FACE_R_DIM
    bd_val_srca.f.z_dim       = 4;                                        // NUM_OF_FACES

    td_val_srca.buf_desc        = bd_val_srca;
    td_val_srca.buf_desc_id     = BUF_DESC_ID_0;                            //
    td_val_srca.reg_data_format = static_cast<uint8_t>(formats.unpack_dst); //

    // Set up the descriptor
    // Start with a buffer descriptor with all zeroes
    buffer_descriptor_u bd_val_srcb;
    for (uint i = 0; i < BD_NUM_WORDS /* or 4? */; i++)
    {
        bd_val_srcb.words[i] = 0;
    }

    // Now just plug everything in
    bd_val_srcb.f.l1_addr_16B = L1_ADDRESS(buffer_B[0]);                  //
    bd_val_srcb.f.format      = static_cast<uint8_t>(formats.unpack_src); //
    bd_val_srcb.f.x_dim       = 16;                                       // FACE_C_DIM
    bd_val_srcb.f.y_dim       = 16;                                       // FACE_R_DIM
    bd_val_srcb.f.z_dim       = 4;                                        // NUM_OF_FACES

    td_val_srcb.buf_desc        = bd_val_srcb;
    td_val_srcb.buf_desc_id     = BUF_DESC_ID_1;                            //
    td_val_srcb.reg_data_format = static_cast<uint8_t>(formats.unpack_dst); //

    // This kernel swaps A & B, reason is the math kernel does SrcB * SrcA, where SrcB access is done as column major
    // But the expectation from users is a matrix multiplication of A * B (input 0 * input 1).
    // So we flip input 0 into SrcB and input 1 into SrcA
    _llk_unpack_configure_binary_<p_unpacr::UNP_A, p_unpacr::UNP_B>(td_val_srcb, td_val_srca);
    _llk_unpack_matmul_init_<>(BUF_DESC_ID_0, BUF_DESC_ID_1, false /*transpose*/, ct_dim, rt_dim, kt_dim);
    for (uint32_t j = 0; j < kt_dim; j++)
    {
        _llk_unpack_matmul_<BUF_DESC_ID_0, BUF_DESC_ID_1, ct_dim, rt_dim, kt_dim>();
    }

#endif
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_matmul.h"
#include "params.h"

void run_kernel()
{
    std::uint32_t ct_dim = CT_DIM;
    std::uint32_t rt_dim = RT_DIM;
    std::uint32_t kt_dim = KT_DIM;

#if defined ARCH_BLACKHOLE || defined ARCH_WORMHOLE

    _llk_math_matmul_init_<MATH_FIDELITY, DstTileFaceLayout::RowMajor>(TILE_R_DIM, TILE_C_DIM, TILE_R_DIM, TILE_C_DIM, false, 0, ct_dim, rt_dim, kt_dim);
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false, false>(formats.math, formats.math);
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (uint32_t j = 0; j < kt_dim; j++)
    {
        _llk_math_matmul_<MATH_FIDELITY, DstTileFaceLayout::RowMajor>(0, 0, ct_dim, rt_dim, kt_dim);
    }
    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

#endif

#ifdef ARCH_QUASAR

    _llk_math_matmul_init_<MATH_FIDELITY, ct_dim, rt_dim, false /*EN_DI*/, false /*EN_X2*/>();
    _llk_math_pack_sync_init_<DstSync::SyncHalf>(); // params OK
    //_llk_math_hw_configure_ does not exist for QSR
    _llk_math_wait_for_dest_available_(); // params OK
    for (uint32_t j = 0; j < kt_dim; j++)
    {
        _llk_math_matmul_block_<MATH_FIDELITY, ct_dim, rt_dim>(); // do we iterate like this? Is there a need to iterate through init? //Is it not going to
                                                                  // overwrite itself
    }
    _llk_math_dest_section_done_<DstSync::SyncHalf>(); // params OK

#endif
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel()
{
#if defined ARCH_BLACKHOLE || defined ARCH_WORMHOLE

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(formats.pack_src, formats.pack_dst, TILE_SIZE_PACK);
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false, false>(formats.pack_dst);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, TILE_SIZE_PACK);
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(formats.pack_dst);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor, false>();
#endif
    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < TILE_CNT; i++)
    {
        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(i, L1_ADDRESS(buffer_Res[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

#endif

#ifdef ARCH_QUASAR

    _llk_pack_hw_configure_<p_pacr::PACK0>(tdma_desc /*???*/);
    _llk_pack_init_<p_pacr::PACK0, BUF_DESC /* which one? */>(4 /*num_tiles*/);
    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < TILE_CNT; i++)
    {
        _llk_pack_<p_pacr::PACK0>(i, L1_ADDRESS(buffer_Res[i])); // are these indexes okay
    }
    _llk_pack_dest_dvalid_section_done_<DST_MODE, IS_DEST_FPU_EN>(); /// maybe

#endif
}

#endif
