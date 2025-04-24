
// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"

// Globals
uint32_t unp_cfg_context        = 0;
uint32_t pack_sync_tile_dst_ptr = 0;

#ifdef DEST_ACC
const bool is_fp32_dest_acc_en = true;
#else
const bool is_fp32_dest_acc_en = false;
#endif

#if defined(UNPACK_A_SRC_INT32) || defined(UNPACK_A_SRC_FLOAT32) || defined(IN_FLOAT32) || defined(IN_INT32)
const bool unpack_to_dest = true;
#else
const bool unpack_to_dest = false;
#endif

struct Formats {
    const uint32_t unpack_src; 
    const uint32_t unpack_dst;
    const uint32_t pack_src;
    const uint32_t pack_dst;
};

constexpr bool is_exponentB(uint32_t format){
    return (format == static_cast<uint32_t>(DataFormat::Float16_b) || format == static_cast<uint32_t>(DataFormat::Bfp8_b) || format == static_cast<uint32_t>(DataFormat::Tf32));
}

constexpr bool format_combo_is_outlier(uint32_t input, uint32_t output){
    return (is_exponentB(input) && output  == (uint32_t)DataFormat::Float16 && !is_fp32_dest_acc_en);
}

constexpr Formats get_data_formats(uint32_t input, uint32_t output)
{
    uint32_t unpack_in = input;
    uint32_t unpack_out = input;
    uint32_t pack_out   = output;
    uint32_t pack_in;

    if (input == (uint32_t)DataFormat::Float16_b && output == (uint32_t)DataFormat::Bfp8_b && !is_fp32_dest_acc_en) {
        pack_in = static_cast<uint32_t>(DataFormat::Bfp8);
    } else if (is_fp32_dest_acc_en) {
        pack_in = output;
    } else if (format_combo_is_outlier(input, output)) {
        pack_in = output;
    } else {
        pack_in = input;
    }
    
    return {unpack_in, unpack_out, pack_in, pack_out};
}

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel()
{
    volatile uint32_t* const buffer_A = reinterpret_cast<volatile uint32_t*>(0x1a000);
    constexpr bool dest_acc = is_fp32_dest_acc_en || format_combo_is_outlier(IN_FORMAT, OUT_FORMAT);
    constexpr Formats pipeline_formats = get_data_formats(IN_FORMAT, OUT_FORMAT);
    constexpr uint32_t unpack_a_in = pipeline_formats.unpack_src;
    constexpr uint32_t unpack_a_out = pipeline_formats.unpack_dst;  

    _llk_unpack_A_init_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(0, 0, FACE_R_DIM, 4, unpack_a_in, unpack_a_out);
    _llk_unpack_A_hw_configure_<dest_acc, StochRndType::None>(unpack_a_in, unpack_a_out, FACE_R_DIM, 0, 4);
    _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(L1_ADDRESS(buffer_A), 0, unpack_a_in, unpack_a_out);
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
    constexpr bool dest_acc = is_fp32_dest_acc_en || format_combo_is_outlier(IN_FORMAT, OUT_FORMAT);
    constexpr Formats pipeline_formats = get_data_formats(IN_FORMAT, OUT_FORMAT);
    constexpr uint32_t unpack_a_in = pipeline_formats.unpack_src;
    constexpr uint32_t unpack_a_out = pipeline_formats.unpack_dst;  
// copy srca to dest
#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, BroadcastType::NONE, false, dest_acc, is_int_fpu_en>(0, 0, 4, unpack_a_out);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, BroadcastType::NONE, dest_acc, is_int_fpu_en>(0, 0, 4, unpack_a_out);
#endif
    _llk_math_pack_sync_init_<DstSync::SyncFull, dest_acc>();
    _llk_math_hw_configure_<false, false>(unpack_a_out, unpack_a_out);
    _llk_math_wait_for_dest_available_<DstSync::SyncFull>();
    _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncFull, BroadcastType::NONE, dest_acc, unpack_to_dest>(
        0, unpack_a_out, unpack_a_out);
    _llk_math_dest_section_done_<DstSync::SyncFull, dest_acc>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel()
{
    constexpr bool dest_acc = is_fp32_dest_acc_en || format_combo_is_outlier(IN_FORMAT, OUT_FORMAT);
    constexpr Formats pipeline_formats = get_data_formats(IN_FORMAT, OUT_FORMAT);
    constexpr uint32_t pack_in = pipeline_formats.pack_src;
    constexpr uint32_t pack_out = pipeline_formats.pack_dst;
    volatile uint32_t* const buffer_Dest = reinterpret_cast<volatile uint32_t*>(0x1c000);

    std::fill(buffer_Dest, buffer_Dest + 16 * 16 * 4, 0xdeadbeef);

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<false, dest_acc, false>(pack_in, pack_out, 16 * 16 * 4);
#else
    _llk_pack_hw_configure_<false, dest_acc>(pack_in, pack_out, 16 * 16 * 4);
#endif

    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(pack_out);

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncFull, DstTileFaceLayout::RowMajor, dest_acc>();
#else
    _llk_pack_dest_init_<DstSync::SyncFull, DstTileFaceLayout::RowMajor, false, false>();
#endif

    _llk_packer_wait_for_math_done_();
    _llk_pack_<DstSync::SyncFull, false, dest_acc>(0, L1_ADDRESS(buffer_Dest));
    _llk_pack_dest_section_done_<DstSync::SyncFull, dest_acc>();
}

#endif
