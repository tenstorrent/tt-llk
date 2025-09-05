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

#include "llk_unpack_matmul.h"
#include "params.h"

void run_kernel()
{
#ifdef ARCH_QUASAR

    set_up_dest_dvalid_per_thread<dest_dvalid_client::UNPACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

    _zerosrc_();

    const uint BUF_DESC_ID_0 = 0;
    const uint BUF_DESC_ID_1 = 1;
    tdma_descriptor_t td_val_srca;
    tdma_descriptor_t td_val_srcb;

    // Set up the descriptor
    // Start with a buffer descriptor with all zeroes
    buffer_descriptor_u bd_val_srca;
    for (uint i = 0; i < BD_NUM_WORDS; i++)
    {
        bd_val_srca.words[i] = 0;
    }
    // Now just plug everything in
    bd_val_srca.f.l1_addr_16B = buffer_A[0] / 16;
    bd_val_srca.f.format      = static_cast<uint8_t>(formats.unpack_src);
    bd_val_srca.f.x_dim       = 16; // FACE_C_DIM
    bd_val_srca.f.y_dim       = 16; // FACE_R_DIM
    bd_val_srca.f.z_dim       = 4;  // NUM_OF_FACES

    td_val_srca.buf_desc        = bd_val_srca;
    td_val_srca.buf_desc_id     = BUF_DESC_ID_0;
    td_val_srca.reg_data_format = static_cast<uint8_t>(formats.unpack_dst);

    // Set up the descriptor
    // Start with a buffer descriptor with all zeroes
    buffer_descriptor_u bd_val_srcb;
    for (uint i = 0; i < BD_NUM_WORDS; i++)
    {
        bd_val_srcb.words[i] = 0;
    }

    // Now just plug everything in
    bd_val_srcb.f.l1_addr_16B = buffer_B[0] / 16;
    bd_val_srcb.f.format      = static_cast<uint8_t>(formats.unpack_src);
    bd_val_srcb.f.x_dim       = 16; // FACE_C_DIM
    bd_val_srcb.f.y_dim       = 16; // FACE_R_DIM
    bd_val_srcb.f.z_dim       = 4;  // NUM_OF_FACES

    td_val_srcb.buf_desc        = bd_val_srcb;
    td_val_srcb.buf_desc_id     = BUF_DESC_ID_1;
    td_val_srcb.reg_data_format = static_cast<uint8_t>(formats.unpack_dst);

    // This kernel swaps A & B, reason is the math kernel does SrcB * SrcA, where SrcB access is done as column major
    // But the expectation from users is a matrix multiplication of A * B (input 0 * input 1).
    // So we flip input 0 into SrcB and input 1 into SrcA
    _llk_unpack_configure_binary_<p_unpacr::UNP_A, p_unpacr::UNP_B>(td_val_srcb, td_val_srca);
    _llk_unpack_matmul_init_<BUF_DESC_ID_0, BUF_DESC_ID_1, false /*transpose*/, CT_DIM, RT_DIM, KT_DIM>();
    for (uint32_t j = 0; j < KT_DIM; j++)
    {
        _llk_unpack_matmul_<BUF_DESC_ID_0, BUF_DESC_ID_1, CT_DIM, RT_DIM, KT_DIM>(j, j * CT_DIM);
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
#ifdef ARCH_QUASAR

    set_up_dest_dvalid_per_thread<dest_dvalid_client::FPU>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

    _zero_dest_reg_();

    constexpr bool EN_IMPLIED_MATH_FORMAT = true;
    constexpr bool IS_FP32_DEST_EN        = (formats.math == (uint)DataFormat::Float32) ? true : false;
    constexpr bool IS_INT32_DEST_EN       = (formats.math == (uint)DataFormat::Int32) ? true : false;
    _llk_math_srcAB_hw_configure_<
        EN_IMPLIED_MATH_FORMAT,
        IS_FP32_DEST_EN,
        IS_INT32_DEST_EN,
        (DataFormat)formats.math,
        (DataFormat)formats.math>(); // dont need formats if
                                     // EN_IMPLIED_MATH_FORMAT
    _llk_math_matmul_init_<(MathFidelity)MATH_FIDELITY, CT_DIM, RT_DIM, false /*EN_DI*/, false /*EN_X2*/>();
    for (uint32_t j = 0; j < KT_DIM; j++)
    {
        _llk_math_matmul_block_<CT_DIM, RT_DIM>();
    }
    _llk_math_set_dvalid_<p_cleardvalid::FPU>();

#endif
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel()
{
#ifdef ARCH_QUASAR

    set_up_dest_dvalid_per_thread<dest_dvalid_client::PACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

    const uint BUF_DESC = 8;

    // Set up the descriptor
    // Start with a buffer descriptor with all zeroes
    buffer_descriptor_u bd_val;
    for (uint i = 0; i < BD_NUM_WORDS; i++)
    {
        bd_val.words[i] = 0;
    }
    tdma_descriptor_t tdma_desc;
    // Now just plug everything in
    bd_val.f.l1_addr_16B = buffer_Res[0] / 16;
    bd_val.f.format      = static_cast<uint8_t>(formats.pack_dst);
    bd_val.f.x_dim       = 16;
    bd_val.f.y_dim       = 16;
    bd_val.f.z_dim       = 4;

    tdma_desc.buf_desc        = bd_val;
    tdma_desc.buf_desc_id     = BUF_DESC;
    tdma_desc.reg_data_format = static_cast<uint8_t>(formats.pack_src);

    _llk_pack_hw_configure_<p_pacr::PACK0>(tdma_desc);
    _llk_pack_init_<p_pacr::PACK0, BUF_DESC>(1 /*num_tiles_per_pack*/);
    for (int i = 0; i < TILE_CNT; i++)
    {
        _llk_pack_<p_pacr::PACK0>(i, i);
    }
    _llk_pack_dest_dvalid_section_done_<ckernel::DstSync::SyncHalf, is_fp32_dest_acc_en>();

#endif
}

#endif
