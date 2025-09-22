
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

#include "llk_unpack_common.h"
#include "llk_unpack_unary_operand.h"
#include "params.h"

void run_kernel()
{
    tdma_descriptor_t td_val;
    const uint BUF_DESC_ID          = 0;
    const uint num_tiles_per_unpack = 1;

    // Setup data valid scheme
    set_up_dest_dvalid_per_thread<dest_dvalid_client::UNPACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

    _zerosrc_();

    buffer_descriptor_u bd_val;
    for (uint i = 0; i < BD_NUM_WORDS; i++)
    {
        bd_val.words[i] = 0;
    }
    // Now just plug everything in
    bd_val.f.l1_addr_16B = buffer_A[0] / 16;
    bd_val.f.format      = static_cast<uint8_t>(formats.unpack_src);
    bd_val.f.x_dim       = 16;
    bd_val.f.y_dim       = 16;
    bd_val.f.z_dim       = num_faces;

    td_val.buf_desc        = bd_val;
    td_val.buf_desc_id     = BUF_DESC_ID;
    td_val.reg_data_format = static_cast<uint8_t>(formats.unpack_dst);

    _llk_unpack_configure_binary_<p_unpacr::UNP_A, p_unpacr::UNP_B>(td_val, td_val);
    _llk_unpack_unary_operand_init_<p_unpacr::UNP_A, BUF_DESC_ID, false /*transpose*/, is_fp32_dest_acc_en>(num_tiles_per_unpack);

    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_unpack_unary_operand_<p_unpacr::UNP_A>(i);
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

    constexpr DataFormat src_format = static_cast<DataFormat>(formats.math);
    _llk_math_srcAB_hw_configure_<true /*math implied*/, is_fp32_dest_acc_en, is_int_fpu_en, src_format, src_format>();

    _zero_dest_reg_();

    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en>(64, 1);
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
    uint32_t const BUF_DESC = 31;

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
    _llk_pack_init_<p_pacr::PACK0, BUF_DESC>(1);

    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_pack_<p_pacr::PACK0>(i, i);
    }
    _llk_pack_dest_dvalid_section_done_<dest_sync, is_fp32_dest_acc_en>();
}
#endif
