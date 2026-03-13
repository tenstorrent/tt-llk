// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
//
// Debug: build with -DUNPACK_DEBUG_SKIP_MATH_BROADCAST=1 to skip the math D2B/B2D phase.
// Pack then reads dest exactly as the unpacker wrote it. Use this to isolate failures:
// - If with the flag you get scalar in row 0 and zeros elsewhere -> unpack is correct, bug is in math D2B/B2D.
// - If with the flag you get a different pattern -> unpack or dest layout is wrong.
// Golden comparison will fail when the flag is on (expected: "scalar in row 0, rest zeros"); it is for manual inspection only.

#ifndef UNPACK_DEBUG_SKIP_MATH_BROADCAST
#define UNPACK_DEBUG_SKIP_MATH_BROADCAST 0
#endif

#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"

#ifdef LLK_TRISC_UNPACK

#include "llk_math_common.h"
#include "llk_unpack_common.h"
#include "llk_unpack_unary_broadcast_operands.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
#ifdef RUNTIME_FORMATS
    const volatile FormatConfig& formats = params->formats;
#endif
    tdma_descriptor_t td_val;
    const std::uint32_t buf_desc_id          = 0;
    const std::uint32_t num_tiles_per_unpack = params->TILE_CNT;

    // Setup data valid scheme
    // When unpack_to_dest is false: unpacker writes to srcB, math does MOVB2D→dest, packer reads from dest
    // When unpack_to_dest is true: unpacker writes to dest, math does MOVD2B→MOVB2D→dest (workaround), packer reads from dest
    if (unpack_to_dest)
    {
        set_up_dest_dvalid_per_thread<dest_dvalid_client::UNPACK>({dest_dvalid_client::UNPACK, dest_dvalid_client::PACK});
        _llk_math_upk_to_dest_hw_configure_<IMPLIED_MATH_FORMAT, is_fp32_dest_acc_en, false /*is_int_fpu_en*/>();
    }
    else
    {
        set_up_dest_dvalid_per_thread<dest_dvalid_client::UNPACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});
    }

    buffer_descriptor_u bd_val = {0};
    // Unpacker always reads from buffer_B (test stimulus is src_B / buffer_B)
    bd_val.f.l1_addr_16B = params->buffer_B[0] / 16;
    bd_val.f.format      = static_cast<std::uint8_t>(formats.unpack_A_src);
    bd_val.f.x_dim       = params->TEST_FACE_C_DIM;
    bd_val.f.y_dim       = params->TEST_FACE_R_DIM;
    bd_val.f.z_dim       = params->num_faces;

    td_val.buf_desc        = bd_val;
    td_val.buf_desc_id     = buf_desc_id;
    td_val.reg_data_format = static_cast<std::uint8_t>(formats.unpack_A_dst);

    _configure_buf_desc_table_(td_val.buf_desc_id, td_val.buf_desc);

    if (is_fp32_dest_acc_en)
    {
        // If Dst fmt is 32b and operation uses unpack_to_dest, we need both SrcA/B fmts configured
        if (unpack_to_dest)
        {
            // Configure both UNP_A and UNP_B format/descriptor so math's dest→srcB→dest workaround has SrcB format set.
            // Only UNP_A actually runs (unpack L1→Dest); UNP_B does not read from L1. Both use same buffer_B descriptor.
            tdma_descriptor_t td_val_B;
            buffer_descriptor_u bd_val_B = {0};
            bd_val_B.f.l1_addr_16B       = params->buffer_B[0] / 16;
            bd_val_B.f.format            = static_cast<std::uint8_t>(formats.unpack_A_src);
            bd_val_B.f.x_dim             = params->TEST_FACE_C_DIM;
            bd_val_B.f.y_dim             = params->TEST_FACE_R_DIM;
            bd_val_B.f.z_dim             = params->num_faces;
            td_val_B.buf_desc            = bd_val_B;
            td_val_B.buf_desc_id = buf_desc_id; // Reuse buf_desc_id: UNPACKER1 doesn't read from L1, but its descriptor is needed to hold intermediate data in
                                                // the dest→srcB→dest workaround
            td_val_B.reg_data_format = static_cast<std::uint8_t>(formats.unpack_A_dst);
            _llk_unpack_configure_binary_<p_unpacr::UNP_A, p_unpacr::UNP_B>(td_val, td_val_B);
        }
        else
        {
            _llk_unpack_configure_unary_<p_unpacr::UNP_B>(td_val);
        }
    }
    else
    {
        // When unpack_to_dest is true, use UNP_A (UNPACKER0); when false, use UNP_B (UNPACKER1)
        if constexpr (unpack_to_dest)
        {
            _llk_unpack_configure_unary_<p_unpacr::UNP_A>(td_val);
        }
        else
        {
            _llk_unpack_configure_unary_<p_unpacr::UNP_B>(td_val);
        }
    }

    if constexpr (unpack_to_dest)
    {
        _llk_unpack_unary_broadcast_operands_init_<p_unpacr::UNP_A, BROADCAST_TYPE, unpack_to_dest, is_fp32_dest_acc_en>(buf_desc_id, num_tiles_per_unpack);
        _llk_unpack_unary_broadcast_operands_<p_unpacr::UNP_A, unpack_to_dest>(0);
        _llk_unpack_dest_dvalid_section_done_();
    }
    else
    {
        _llk_unpack_unary_broadcast_operands_init_<p_unpacr::UNP_B, BROADCAST_TYPE, unpack_to_dest, is_fp32_dest_acc_en>(buf_desc_id, num_tiles_per_unpack);
        _llk_unpack_unary_broadcast_operands_<p_unpacr::UNP_B, unpack_to_dest>(0);
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_unary_broadcast.h"
#include "params.h"

using namespace ckernel;

void run_kernel(const volatile struct RuntimeParams* params)
{
#ifdef RUNTIME_FORMATS
    const volatile FormatConfig& formats = params->formats;
#endif
    if constexpr (unpack_to_dest)
    {
        // When unpack_to_dest: unpacker wrote seed data to dest, math runs D2B→B2D to broadcast
        set_up_dest_dvalid_per_thread<dest_dvalid_client::FPU>({dest_dvalid_client::UNPACK, dest_dvalid_client::PACK});
    }
    else
    {
        set_up_dest_dvalid_per_thread<dest_dvalid_client::FPU>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});
    }

    DataFormat src_format = static_cast<DataFormat>(formats.math);
    _llk_math_srcAB_hw_configure_<IMPLIED_MATH_FORMAT, is_fp32_dest_acc_en, false /*int32_dest*/>(src_format, src_format);

#if UNPACK_DEBUG_SKIP_MATH_BROADCAST
    // Skip D2B/B2D: leave dest as unpack wrote it and signal dvalid so pack reads it.
    // Run test and inspect result to see whether unpack output is correct (row 0 = scalar, rest = 0).
    (void)params;
    _llk_math_set_dvalid_<p_cleardvalid::FPU>();
    return;
#endif

    TileShape tile_shape = {
        .num_faces   = static_cast<std::uint32_t>(params->num_faces),
        .face_r_dim  = static_cast<std::uint32_t>(params->TEST_FACE_R_DIM),
        .face_c_dim  = static_cast<std::uint32_t>(params->TEST_FACE_C_DIM),
        .narrow_tile = false};
    _llk_math_eltwise_unary_broadcast_init_<BROADCAST_TYPE, unpack_to_dest, is_fp32_dest_acc_en>(tile_shape);

    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_math_eltwise_unary_broadcast_<BROADCAST_TYPE, unpack_to_dest, is_fp32_dest_acc_en>(static_cast<std::uint32_t>(i), tile_shape);
    }
    _llk_math_set_dvalid_<p_cleardvalid::FPU>();
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

    std::uint32_t const buf_desc_id        = 8;
    const std::uint32_t num_tiles_per_pack = params->TILE_CNT;

    // Setup data valid scheme - must match unpack/math sections
    if (unpack_to_dest)
    {
        set_up_dest_dvalid_per_thread<dest_dvalid_client::PACK>({dest_dvalid_client::UNPACK, dest_dvalid_client::PACK});
    }
    else
    {
        set_up_dest_dvalid_per_thread<dest_dvalid_client::PACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});
    }

    buffer_descriptor_u bd_val = {0};
    bd_val.f.l1_addr_16B       = params->buffer_Res[0] / 16;
    bd_val.f.format            = static_cast<std::uint8_t>(formats.pack_dst);
    bd_val.f.x_dim             = params->TEST_FACE_C_DIM;
    bd_val.f.y_dim             = params->TEST_FACE_R_DIM;
    bd_val.f.z_dim             = params->num_faces;

    tdma_descriptor_t tdma_desc;

    tdma_desc.buf_desc        = bd_val;
    tdma_desc.buf_desc_id     = buf_desc_id;
    tdma_desc.reg_data_format = static_cast<std::uint8_t>(formats.pack_src);

    _configure_buf_desc_table_(tdma_desc.buf_desc_id, tdma_desc.buf_desc);
    _llk_pack_hw_configure_<p_pacr::PACK0>(tdma_desc);
    _llk_pack_init_<p_pacr::PACK0>(buf_desc_id, num_tiles_per_pack);
    _llk_pack_<p_pacr::PACK0>(0, 0);
    _llk_pack_dest_dvalid_section_done_<dest_sync, is_fp32_dest_acc_en>();
}

#endif
