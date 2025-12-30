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
#include "llk_unpack_unary_operand.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    tdma_descriptor_t td_val;
    const uint buf_desc_id          = 0;
    const uint num_tiles_per_unpack = params->TILE_CNT;

    // Setup data valid scheme
    set_up_dest_dvalid_per_thread<dest_dvalid_client::UNPACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});

    buffer_descriptor_u bd_val = {0};

    bd_val.f.l1_addr_16B = buffer_A[0] / 16;
    bd_val.f.format      = static_cast<uint8_t>(formats.unpack_src);
    bd_val.f.x_dim       = params->TEST_FACE_C_DIM;
    bd_val.f.y_dim       = params->TEST_FACE_R_DIM;
    bd_val.f.z_dim       = params->num_faces;

    td_val.buf_desc        = bd_val;
    td_val.buf_desc_id     = buf_desc_id;
    td_val.reg_data_format = static_cast<uint8_t>(formats.unpack_dst);

    if (is_fp32_dest_acc_en)
    {
        // If Dst fmt is 32b and operation is Mov2D, we need both SrcA/B fmts to be configured since Mov2D will be implemented via ELWADD
        _llk_unpack_configure_binary_<p_unpacr::UNP_A, p_unpacr::UNP_B>(td_val, td_val);
    }
    else
    {
        _llk_unpack_configure_unary_<UNPACKER_ENGINE_SEL>(td_val);
    }

    _llk_unpack_unary_operand_init_<UNPACKER_ENGINE_SEL, false /*transpose*/, is_fp32_dest_acc_en>(buf_desc_id, num_tiles_per_unpack);
    _llk_unpack_unary_operand_<UNPACKER_ENGINE_SEL>(0);
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
#include "llk_math_eltwise_unary_sfpu_common.h"
#include "params.h"
#include "sfpu/ckernel_sfpu_rsqrt.h"

using namespace ckernel;
using namespace ckernel::math;
using namespace ckernel::sfpu;

void run_kernel(const volatile struct RuntimeParams *params)
{
    // Process all rows in a tile: num_faces * TEST_FACE_R_DIM = 4 * 16 = 64 rows
    // Each SFPU iteration processes SFP_ROWS = 2 rows
    // So we need 64 / 2 = 32 iterations to process a full tile
    const int iterations = params->num_faces * params->TEST_FACE_R_DIM / ckernel::math::SFP_ROWS;

    set_up_dest_dvalid_per_thread<dest_dvalid_client::FPU>({dest_dvalid_client::FPU, dest_dvalid_client::SFPU, dest_dvalid_client::PACK});

    DataFormat src_format = static_cast<DataFormat>(formats.math);
    _llk_math_srcAB_hw_configure_<IMPLIED_MATH_FORMAT, is_fp32_dest_acc_en, is_int_fpu_en>(src_format, src_format);

    // Initialize datacopy
    _llk_math_eltwise_unary_datacopy_init_<DATA_COPY_TYPE, is_fp32_dest_acc_en>(
        params->num_faces * params->TEST_FACE_R_DIM /*num_rows_per_matrix*/, 1 /*num_matrices*/);

    // Initialize SFPU once before processing all tiles (configures addrmods and resets counters)
    _llk_math_eltwise_unary_sfpu_init_();

    // Process each tile: datacopy then SFPU
    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        // Copy srcA to dest for this tile
        _llk_math_eltwise_unary_datacopy_(params->num_faces * params->TEST_FACE_R_DIM /*num_rows_per_tile*/, params->DST_INDEX + i);

        // Wait for MOP to complete to ensure datacopy has finished writing to dest register
        wait_mop_idle();

        // Start SFPU for this tile - sets base address and waits for readiness
        _llk_math_eltwise_unary_sfpu_start_(i);

        // Call rsqrt function directly from Quasar implementation
        _calculate_rsqrt_<APPROX_MODE>(iterations);

        // Wait for SFPU operations to complete
        wait_sfpu_idle();

        // Signal this tile is done - resets dest counter
        _llk_math_eltwise_unary_sfpu_done_();
    }

    // Set data valid flag to signal packer that SFPU is complete
    _llk_math_set_dvalid_<p_cleardvalid::SFPU>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    uint32_t const buf_desc_id    = 8;
    const uint num_tiles_per_pack = params->TILE_CNT;

    set_up_dest_dvalid_per_thread<dest_dvalid_client::PACK>({dest_dvalid_client::FPU, dest_dvalid_client::SFPU, dest_dvalid_client::PACK});

    buffer_descriptor_u bd_val = {0};
    tdma_descriptor_t tdma_desc;

    bd_val.f.l1_addr_16B = buffer_Res[0] / 16;
    bd_val.f.format      = static_cast<uint8_t>(formats.pack_dst);
    bd_val.f.x_dim       = params->TEST_FACE_C_DIM;
    bd_val.f.y_dim       = params->TEST_FACE_R_DIM;
    bd_val.f.z_dim       = params->num_faces;

    tdma_desc.buf_desc        = bd_val;
    tdma_desc.buf_desc_id     = buf_desc_id;
    tdma_desc.reg_data_format = static_cast<uint8_t>(formats.pack_src);

    _llk_pack_hw_configure_<p_pacr::PACK0>(tdma_desc);
    _llk_pack_init_<p_pacr::PACK0>(buf_desc_id, num_tiles_per_pack);
    _llk_pack_<p_pacr::PACK0>(params->DST_INDEX, 0);
    _llk_pack_dest_dvalid_section_done_<dest_sync, is_fp32_dest_acc_en>();
}
#endif
