// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

// Test for eltwise binary operations with reuse_dest on Quasar.

#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"

// Globals
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_common.h"
#include "llk_unpack_unary_operand.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    tdma_descriptor_t td_val_A, td_val_B;
    const std::uint32_t buf_desc_id_a = 0;
    const std::uint32_t buf_desc_id_b = 1;

    // Setup data valid scheme
    set_up_dest_dvalid_per_thread<dest_dvalid_client::UNPACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

    // Configure Source A buffer descriptor (for datacopy to DEST in phase 1)
    buffer_descriptor_u bd_val_A {};
    bd_val_A.f.l1_addr_16B = buffer_A[0] / 16;
    bd_val_A.f.format      = static_cast<std::uint8_t>(formats.unpack_src);
    bd_val_A.f.x_dim       = params->TEST_FACE_C_DIM;
    bd_val_A.f.y_dim       = params->TEST_FACE_R_DIM;
    bd_val_A.f.z_dim       = params->num_faces;

    td_val_A.buf_desc        = bd_val_A;
    td_val_A.buf_desc_id     = buf_desc_id_a;
    td_val_A.reg_data_format = static_cast<std::uint8_t>(formats.unpack_dst);

    // Configure Source B buffer descriptor (CB operand for eltwise binary in phase 2)
    buffer_descriptor_u bd_val_B {};
    bd_val_B.f.l1_addr_16B = buffer_B[0] / 16;
    bd_val_B.f.format      = static_cast<std::uint8_t>(formats.unpack_src);
    bd_val_B.f.x_dim       = params->TEST_FACE_C_DIM;
    bd_val_B.f.y_dim       = params->TEST_FACE_R_DIM;
    bd_val_B.f.z_dim       = params->num_faces;

    td_val_B.buf_desc        = bd_val_B;
    td_val_B.buf_desc_id     = buf_desc_id_b;
    td_val_B.reg_data_format = static_cast<std::uint8_t>(formats.unpack_dst);

    // Configure both unpackers upfront (need both for reuse_dest phase)
    _configure_buf_desc_table_(td_val_A.buf_desc_id, td_val_A.buf_desc);
    _configure_buf_desc_table_(td_val_B.buf_desc_id, td_val_B.buf_desc);
    _llk_unpack_configure_binary_<p_unpacr::UNP_A, p_unpacr::UNP_B>(td_val_A, td_val_B);

    // Unpack src_A to SrcA for datacopy into DEST
    _llk_unpack_unary_operand_init_<p_unpacr::UNP_A, false /*transpose*/, false /*32b_dest*/>(buf_desc_id_a, 1);
    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_unpack_unary_operand_<p_unpacr::UNP_A>(i);
    }

    // Unpack src_B with reuse_dest mode
    // The reuse_dest MOP internally determines which unpacker reads from CB and which gets dummy dvalid:
    //   DEST_TO_SRCA: UNP_B reads from CB (src_B), UNP_A gets dummy dvalid (filled by MOVD2A)
    //   DEST_TO_SRCB: UNP_A reads from CB (src_B), UNP_B gets dummy dvalid (filled by MOVD2B)
    _llk_unpack_unary_operand_init_<p_unpacr::UNP_A, false /*transpose*/, false /*32b_dest*/, REUSE_DEST_TYPE>(buf_desc_id_b, 1);
    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_unpack_unary_operand_<p_unpacr::UNP_A, REUSE_DEST_TYPE>(i);
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "params.h"

using namespace ckernel;

void run_kernel(const volatile struct RuntimeParams *params)
{
    set_up_dest_dvalid_per_thread<dest_dvalid_client::FPU>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

    DataFormat src_format = static_cast<DataFormat>(formats.math);
    _llk_math_srcAB_hw_configure_<IMPLIED_MATH_FORMAT, is_fp32_dest_acc_en, false /*int32*/>(src_format, src_format);

    TileShape tile_shape = {.num_faces = params->num_faces, .face_r_dim = params->TEST_FACE_R_DIM, .face_c_dim = params->TEST_FACE_C_DIM, .narrow_tile = false};

    // Datacopy src_A from SrcA to DEST
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en>(params->num_faces * params->TEST_FACE_R_DIM, 1);
    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_math_eltwise_unary_datacopy_(params->num_faces * params->TEST_FACE_R_DIM, i);
    }

    // Eltwise binary with reuse_dest
    // DEST already contains src_A data from phase 1.
    // MOVD copies DEST face → SrcA/SrcB (depending on reuse_dest type),
    // while the other source is unpacked from CB (src_B).
    _llk_math_eltwise_binary_init_<ELTWISE_BINARY_OP, MATH_FIDELITY, false /*EN_DI*/, REUSE_DEST_TYPE>(tile_shape);
    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_math_eltwise_binary_<REUSE_DEST_TYPE>(i);
    }

    _llk_math_set_dvalid_<p_cleardvalid::FPU>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    std::uint32_t const buf_desc_id = 8;

    // Setup synchronization - PACK waits for FPU to write to DEST
    set_up_dest_dvalid_per_thread<dest_dvalid_client::PACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

    buffer_descriptor_u bd_val {};
    bd_val.f.l1_addr_16B = buffer_Res[0] / 16;
    bd_val.f.format      = static_cast<std::uint8_t>(formats.pack_dst);
    bd_val.f.x_dim       = params->TEST_FACE_C_DIM;
    bd_val.f.y_dim       = params->TEST_FACE_R_DIM;
    bd_val.f.z_dim       = params->num_faces;

    tdma_descriptor_t tdma_desc;
    tdma_desc.buf_desc        = bd_val;
    tdma_desc.buf_desc_id     = buf_desc_id;
    tdma_desc.reg_data_format = static_cast<std::uint8_t>(formats.pack_src);

    _configure_buf_desc_table_(tdma_desc.buf_desc_id, tdma_desc.buf_desc);
    _llk_pack_hw_configure_<p_pacr::PACK0>(tdma_desc);
    _llk_pack_init_<p_pacr::PACK0>(buf_desc_id, 1);

    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_pack_<p_pacr::PACK0>(i, i);
    }

    _llk_pack_dest_dvalid_section_done_<dest_sync, is_fp32_dest_acc_en>();
}

#endif
