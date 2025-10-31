// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_common.h"
#include "llk_unpack_tilize.h"
#include "params.h"

void run_kernel()
{
    tdma_descriptor_t td_val_A, td_val_B;
    const uint BUF_DESC_ID_A = 0;
    const uint BUF_DESC_ID_B = 1;

    // Setup data valid scheme
    set_up_dest_dvalid_per_thread<dest_dvalid_client::UNPACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

    buffer_descriptor_u bd_val_A, bd_val_B;
    for (uint i = 0; i < BD_NUM_WORDS; i++)
    {
        bd_val_A.words[i] = 0;
        bd_val_B.words[i] = 0;
    }

    unsigned l1_addr_16B_A = buffer_A[0] / 16;
    unsigned l1_addr_16B_B = buffer_B[0] / 16;

    bd_val_A.f.l1_addr_16B = l1_addr_16B_A;
    bd_val_A.f.format      = static_cast<uint8_t>(formats.unpack_src);
    bd_val_A.f.x_dim       = 16;
    bd_val_A.f.y_dim       = 16;
    bd_val_A.f.z_dim       = num_faces;

    td_val_A.buf_desc        = bd_val_A;
    td_val_A.buf_desc_id     = BUF_DESC_ID_A;
    td_val_A.reg_data_format = static_cast<uint8_t>(formats.unpack_dst);

    bd_val_B.f.l1_addr_16B = l1_addr_16B_B;
    bd_val_B.f.format      = static_cast<uint8_t>(formats.unpack_src);
    bd_val_B.f.x_dim       = 16;
    bd_val_B.f.y_dim       = 16;
    bd_val_B.f.z_dim       = num_faces;

    td_val_B.buf_desc        = bd_val_B;
    td_val_B.buf_desc_id     = BUF_DESC_ID_B;
    td_val_B.reg_data_format = static_cast<uint8_t>(formats.unpack_dst);

    constexpr TileShape tile_shape = {.num_faces = 4, .face_r_dim = 16, .face_c_dim = 16, .narrow_tile = 0};
    constexpr uint32_t C_DIM_FACES = (tile_shape.narrow_tile ? 1 : 2);

    if (is_fp32_dest_acc_en)
    {
        _llk_unpack_configure_binary_<p_unpacr::UNP_A, p_unpacr::UNP_B>(td_val_A, td_val_B);
    }
    else
    {
        _llk_unpack_configure_unary_<UNPACKER_ENGINE_SEL>(UNPACKER_ENGINE_SEL == p_unpacr::UNP_A ? td_val_A : td_val_B);
    }
    _llk_unpack_tilize_init_<
        UNPACKER_ENGINE_SEL,
        UNPACKER_ENGINE_SEL == p_unpacr::UNP_A ? BUF_DESC_ID_A : BUF_DESC_ID_B,
        is_fp32_dest_acc_en,
        FULL_CT_DIM,
        BLOCK_CT_DIM,
        C_DIM_FACES>();
    for (uint block_rt = 0; block_rt < BLOCK_RT_DIM; block_rt++)
    {
        _llk_unpack_tilize_<UNPACKER_ENGINE_SEL>(block_rt * FULL_CT_DIM * 32, 0);
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
    set_up_dest_dvalid_per_thread<dest_dvalid_client::FPU>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

    constexpr DataCopyType DATA_COPY_TYPE = (UNPACKER_ENGINE_SEL == p_unpacr::UNP_A) ? DataCopyType::A2D : DataCopyType::B2D;

    constexpr DataFormat src_format = static_cast<DataFormat>(formats.math);
    _llk_math_srcAB_hw_configure_<true /*math implied*/, is_fp32_dest_acc_en, is_int_fpu_en, src_format, src_format>();

    _llk_math_eltwise_unary_datacopy_init_<DATA_COPY_TYPE, is_fp32_dest_acc_en>(64, 1);
    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_math_eltwise_unary_datacopy_<64>(i);
    }
    _llk_math_set_dvalid_<p_cleardvalid::FPU>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel()
{
    uint32_t const BUF_DESC       = 8;
    const uint num_tiles_per_pack = TILE_CNT;

    set_up_dest_dvalid_per_thread<dest_dvalid_client::PACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

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
    bd_val.f.z_dim       = num_faces;

    tdma_desc.buf_desc        = bd_val;
    tdma_desc.buf_desc_id     = BUF_DESC;
    tdma_desc.reg_data_format = static_cast<uint8_t>(formats.pack_src);

    _llk_pack_hw_configure_<p_pacr::PACK0>(tdma_desc);
    _llk_pack_init_<p_pacr::PACK0, BUF_DESC>(num_tiles_per_pack);
    _llk_pack_<p_pacr::PACK0>(0, 0);
    _llk_pack_dest_dvalid_section_done_<dest_sync, is_fp32_dest_acc_en>();
}
#endif
