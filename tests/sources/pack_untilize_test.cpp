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
#include "params.h"

#if defined(ARCH_BLACKHOLE) || defined(ARCH_WORMHOLE)

#include "llk_unpack_A.h"

#else // ARCH_QUASAR

#include "llk_unpack_unary_operand.h"

#endif

void run_kernel()
{
#if defined(ARCH_BLACKHOLE) || defined(ARCH_WORMHOLE)

    _llk_unpack_A_init_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
        0, 0, FACE_R_DIM, 4, formats.unpack_src, formats.unpack_dst);
    _llk_unpack_A_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(formats.unpack_src, formats.unpack_dst, FACE_R_DIM, 0, 4);
    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
            L1_ADDRESS(buffer_A[i]), 0, formats.unpack_src, formats.unpack_dst);
    }

#else // ARCH_QUASAR

    tdma_descriptor_t td_val;
    const uint BUF_DESC_ID          = 0;
    const uint num_tiles_per_unpack = 1;

    // Setup data valid scheme
    set_up_dest_dvalid_per_thread<dest_dvalid_client::UNPACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

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

#endif
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "params.h"

using namespace ckernel;

void run_kernel()
{
#if defined(ARCH_BLACKHOLE) || defined(ARCH_WORMHOLE)

    const bool is_int_fpu_en = false;

// copy srca to dest
#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false, is_int_fpu_en>(0, 0, 4, formats.math);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, is_int_fpu_en>(0, 0, 4, formats.math);
#endif
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<true, false>(formats.math, formats.math);
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
            i, formats.math, formats.math);
    }
    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

#else // ARCH_QUASAR

#ifdef FORMAT_INT32
    const bool is_int_fpu_en = true;
#else
    const bool is_int_fpu_en = false;
#endif

    set_up_dest_dvalid_per_thread<dest_dvalid_client::FPU>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

    constexpr DataFormat src_format = static_cast<DataFormat>(formats.math);
    _llk_math_srcAB_hw_configure_<true /*math implied*/, is_fp32_dest_acc_en, is_int_fpu_en, src_format, src_format>();
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en>(64, 1);
    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_math_eltwise_unary_datacopy_<64>(i);
    }
    _llk_math_set_dvalid_<p_cleardvalid::FPU>();

#endif
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack_common.h"
#include "llk_pack_untilize.h"
#include "params.h"

void run_kernel()
{
#if defined(ARCH_BLACKHOLE) || defined(ARCH_WORMHOLE)

    const bool UNTILIZE = true;

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, UNTILIZE, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();
    _llk_pack_untilize_init_<BLOCK_CT_DIM>(formats.pack_src, formats.pack_dst, FACE_R_DIM, 4);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, UNTILIZE>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor, UNTILIZE>();
    _llk_pack_untilize_init_<BLOCK_CT_DIM>(formats.pack_dst, FACE_R_DIM, 4);
#endif

    _llk_packer_wait_for_math_done_();

    _llk_pack_untilize_<BLOCK_CT_DIM>(L1_ADDRESS(buffer_Res[0]), formats.pack_dst, FACE_R_DIM, 4, 0);
    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

#else // ARCH_QUASAR

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

    constexpr TileShape tile_shape = {.num_faces = 4, .face_r_dim = 16, .face_c_dim = 16, .narrow_tile = 0};

    constexpr uint32_t C_DIM_FACES = (tile_shape.narrow_tile ? 1 : 2);

    _llk_pack_hw_configure_<p_pacr::PACK0>(tdma_desc);
    _llk_pack_untilize_init_<BUF_DESC, 1 /*full_ct_dim*/, 1 /*block_ct_dim */, C_DIM_FACES>(tile_shape);

    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_pack_untilize_();
    }
    _llk_pack_dest_dvalid_section_done_<dest_sync, is_fp32_dest_acc_en>();

#endif
}

#endif
