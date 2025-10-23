// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"

// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_binary_operands.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel()
{
    const uint BUF_DESC_ID_A = 0;
    const uint BUF_DESC_ID_B = 1;

    // Setup data valid scheme - eltwise binary always needs FPU coordination
    set_up_dest_dvalid_per_thread<dest_dvalid_client::UNPACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

    // Configure buffer descriptors for both operands
    tdma_descriptor_t td_val_A, td_val_B;

    // Configure Source A buffer descriptor
    buffer_descriptor_u bd_val_A;
    for (uint i = 0; i < BD_NUM_WORDS; i++)
    {
        bd_val_A.words[i] = 0;
    }
    bd_val_A.f.l1_addr_16B = buffer_A[0] / 16;
    bd_val_A.f.format      = static_cast<uint8_t>(formats.unpack_src);
    bd_val_A.f.x_dim       = 16;
    bd_val_A.f.y_dim       = 16;
    bd_val_A.f.z_dim       = num_faces;

    td_val_A.buf_desc        = bd_val_A;
    td_val_A.buf_desc_id     = BUF_DESC_ID_A;
    td_val_A.reg_data_format = static_cast<uint8_t>(formats.unpack_dst);

    // Configure Source B buffer descriptor
    buffer_descriptor_u bd_val_B;
    for (uint i = 0; i < BD_NUM_WORDS; i++)
    {
        bd_val_B.words[i] = 0;
    }
    bd_val_B.f.l1_addr_16B = buffer_B[0] / 16;
    bd_val_B.f.format      = static_cast<uint8_t>(formats.unpack_src);
    bd_val_B.f.x_dim       = 16;
    bd_val_B.f.y_dim       = 16;
    bd_val_B.f.z_dim       = num_faces;

    td_val_B.buf_desc        = bd_val_B;
    td_val_B.buf_desc_id     = BUF_DESC_ID_B;
    td_val_B.reg_data_format = static_cast<uint8_t>(formats.unpack_dst);

    // Configure hardware for binary operations
    _llk_unpack_configure_binary_<p_unpacr::UNP_A, p_unpacr::UNP_B>(td_val_A, td_val_B);

    // Configure MOP for binary operands
    _llk_unpack_binary_operands_mop_config_<BUF_DESC_ID_A, BUF_DESC_ID_B>(TILE_CNT);

    // Unpack all tiles for both operands
    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_unpack_binary_operands_<BUF_DESC_ID_A, BUF_DESC_ID_B>(i, i);
    }
    _llk_unpack_dest_dvalid_section_done_();
}

#endif

#ifdef LLK_TRISC_MATH

#ifdef FORMAT_INT32
const bool is_int_fpu_en = true;
#else
const bool is_int_fpu_en = false;
#endif

#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "params.h"

using namespace ckernel;

void run_kernel()
{
    // Setup synchronization for math operations
    set_up_dest_dvalid_per_thread<dest_dvalid_client::FPU>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

    // Initialize pack synchronization
    _llk_math_pack_sync_init_<DstSync::SyncHalf>();

    // Configure math hardware with proper Quasar API
    constexpr DataFormat src_format = static_cast<DataFormat>(formats.math);
    _llk_math_srcAB_hw_configure_<true /*math implied*/, is_fp32_dest_acc_en, is_int_fpu_en, src_format, src_format>();

    // Initialize eltwise binary operation with proper TileShape
    TileShape tile_shape = {.num_faces = num_faces, .face_r_dim = 16, .face_c_dim = 16, .narrow_tile = false};
    _llk_math_eltwise_binary_init_<ELTWISE_BINARY_OP, MATH_FIDELITY>(tile_shape);

    // Perform eltwise binary operation for each tile
    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_math_eltwise_binary_(i);
    }

    // Signal math completion
    _llk_math_dest_section_done_<DstSync::SyncHalf>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel()
{
    uint32_t const BUF_DESC = 8;

    // Setup synchronization for pack operations
    set_up_dest_dvalid_per_thread<dest_dvalid_client::PACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

    // Configure output buffer descriptor
    buffer_descriptor_u bd_val;
    for (uint i = 0; i < BD_NUM_WORDS; i++)
    {
        bd_val.words[i] = 0;
    }
    tdma_descriptor_t tdma_desc;

    bd_val.f.l1_addr_16B = buffer_Res[0] / 16;
    bd_val.f.format      = static_cast<uint8_t>(formats.pack_dst);
    bd_val.f.x_dim       = 16;
    bd_val.f.y_dim       = 16;
    bd_val.f.z_dim       = num_faces;

    tdma_desc.buf_desc        = bd_val;
    tdma_desc.buf_desc_id     = BUF_DESC;
    tdma_desc.reg_data_format = static_cast<uint8_t>(formats.pack_src);

    // Configure and initialize pack hardware
    _llk_pack_hw_configure_<p_pacr::PACK0>(tdma_desc);
    _llk_pack_init_<p_pacr::PACK0, BUF_DESC>(1);

    // Pack all result tiles
    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_pack_<p_pacr::PACK0>(i, i);
    }

    // Signal pack completion with proper synchronization
    _llk_pack_dest_dvalid_section_done_<dest_sync, is_fp32_dest_acc_en>();
}

#endif
