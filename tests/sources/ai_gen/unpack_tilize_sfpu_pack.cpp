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
#include "llk_unpack_tilize.h"
#include "params.h"

void run_kernel()
{
    // Configure unpack for tilize operation (row-major input -> tiled format)
    // This handles both A and B inputs which need to be tilized before binary ops
    _llk_unpack_tilize_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(formats.unpack_src, formats.unpack_dst, FACE_R_DIM, 0, 4);
    _llk_unpack_tilize_init_(formats.unpack_src, formats.unpack_dst, BLOCK_CT_DIM, FACE_R_DIM, false);

    // Unpack and tilize single tile A (stored in src A register - index 0)
    _llk_unpack_tilize_(L1_ADDRESS(buffer_A[0]), 0, formats.unpack_src, BLOCK_CT_DIM, FACE_R_DIM, 4, false);
}

#endif

#ifdef LLK_TRISC_MATH

#include "ckernel_sfpu.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "llk_math_eltwise_unary_sfpu.h"
#include "params.h"

using namespace ckernel;
using namespace ckernel::sfpu;

const int iterations = 32;

namespace
{
void call_sfpu_operation(SfpuType operation)
{
    switch (operation)
    {
        case SfpuType::abs:
            ckernel::sfpu::_calculate_abs_<APPROX_MODE, iterations>(iterations);
            break;
        case SfpuType::cosine:
            ckernel::sfpu::_calculate_cosine_<APPROX_MODE, iterations>(iterations);
            break;
        case SfpuType::log:
            ckernel::sfpu::_init_log_<APPROX_MODE>();
            ckernel::sfpu::_calculate_log_<APPROX_MODE, false, iterations>(iterations, 0);
            break;
        case SfpuType::reciprocal:
            ckernel::sfpu::_init_reciprocal_<APPROX_MODE>();
            ckernel::sfpu::_calculate_reciprocal_<APPROX_MODE, iterations, is_fp32_dest_acc_en>(iterations);
            break;
        case SfpuType::sine:
            ckernel::sfpu::_calculate_sine_<APPROX_MODE, iterations>(iterations);
            break;
        case SfpuType::sqrt:
            ckernel::sfpu::_init_sqrt_<APPROX_MODE>();
            ckernel::sfpu::_calculate_sqrt_<APPROX_MODE, 0, iterations>(iterations);
            break;
        case SfpuType::square:
            ckernel::sfpu::_calculate_square_<APPROX_MODE, iterations>(iterations);
            break;
        case SfpuType::gelu:
            ckernel::sfpu::_init_gelu_<APPROX_MODE>();
            ckernel::sfpu::_calculate_gelu_<APPROX_MODE, iterations>();
            break;
        case SfpuType::celu:
            ckernel::sfpu::_calculate_activation_<APPROX_MODE, ActivationType::Celu, iterations>(10, 1.0f / 10.0f);
            break;
        case SfpuType::silu:
            ckernel::sfpu::_calculate_silu_<APPROX_MODE, iterations>();
            break;
        case SfpuType::neg:
            ckernel::sfpu::_calculate_negative_<APPROX_MODE, iterations>();
            break;
        case SfpuType::exponential:
            ckernel::sfpu::_init_exponential_<APPROX_MODE, false /*fast_mode*/, 0x3F800000 /* exp_base_scale_factor */>();
            ckernel::sfpu::_calculate_exponential_<APPROX_MODE, false /* scale_en */, iterations, false /* fast_approx */, false /* skip_positive_check */>(
                p_sfpu::kCONST_1_FP16B /* exp_base_scale_factor */);
            break;
        case SfpuType::exp2:
            ckernel::sfpu::_init_exp2_<APPROX_MODE>();
            ckernel::sfpu::_calculate_exp2_<APPROX_MODE, iterations>();
            break;
        case SfpuType::elu:
            ckernel::sfpu::_init_elu_<APPROX_MODE>();
            ckernel::sfpu::_calculate_elu_<APPROX_MODE, iterations>(1);
            break;
        case SfpuType::hardsigmoid:
            ckernel::sfpu::_init_hardsigmoid_<APPROX_MODE>();
            ckernel::sfpu::_calculate_activation_<APPROX_MODE, ckernel::ActivationType::Hardsigmoid, iterations>();
            break;
        default:
            return;
    }
}
} // namespace

void run_kernel()
{
    // Initialize math operations for datacopy->unary pipeline

    // Initialize datacopy operation (copy src A to dest)
#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false, false>(0, 0, 4, formats.math);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false>(0, 0, 4, formats.math);
#endif

    _llk_math_pack_sync_init_<DST_SYNC, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false, false>(formats.math, formats.math);

    // Wait for destination to be available
    _llk_math_wait_for_dest_available_<DST_SYNC>();

    // Step 1: Copy tilized input from src A to dest
    _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DST_SYNC, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(0, formats.math, formats.math);

    // Step 2: Initialize and perform SFPU unary operation on the copied data
    _llk_math_eltwise_unary_sfpu_init_<SFPU_UNARY_OPERATION>();
    _llk_math_eltwise_unary_sfpu_start_<DST_SYNC>(0);

    // Execute the specific SFPU operation
    call_sfpu_operation(SFPU_UNARY_OPERATION);

    // Complete SFPU operation
    _llk_math_eltwise_unary_sfpu_done_();

    // Signal completion to packer
    _llk_math_dest_section_done_<DST_SYNC, is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel()
{
    // Configure packer hardware for standard pack (no untilize)
    const bool UNTILIZE = false;
    const bool TILIZE   = false; // Input to pack is already in tile format

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, UNTILIZE, TILIZE>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
    _llk_pack_init_<UNTILIZE, false, DstTileFaceLayout::RowMajor, false, TILIZE>(formats.pack_dst);
    _llk_pack_dest_init_<DST_SYNC, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, UNTILIZE>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
    _llk_pack_init_<UNTILIZE, false, DstTileFaceLayout::RowMajor, false>(formats.pack_dst);
    _llk_pack_dest_init_<DST_SYNC, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor, UNTILIZE>();
#endif

    // Pack the single result tile from destination register to output buffer
    _llk_packer_wait_for_math_done_();
    _llk_pack_<DST_SYNC, is_fp32_dest_acc_en, UNTILIZE>(0, L1_ADDRESS(buffer_Res[0]));
    _llk_pack_dest_section_done_<DST_SYNC, is_fp32_dest_acc_en>();
}

#endif
