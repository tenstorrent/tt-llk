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
    _llk_unpack_tilize_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(formats.unpack_src, formats.unpack_dst, FACE_R_DIM, 0, 4);
    _llk_unpack_tilize_init_(formats.unpack_src, formats.unpack_dst, BLOCK_CT_DIM, FACE_R_DIM, false);

    // Unpack and tilize input A (stored in src A register - index 0)
    _llk_unpack_tilize_(L1_ADDRESS(buffer_A[0]), 0, formats.unpack_src, BLOCK_CT_DIM, FACE_R_DIM, 4, false);
}

#endif

#ifdef LLK_TRISC_MATH

#include "ckernel_sfpu.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "llk_math_eltwise_unary_sfpu.h"
#include "llk_math_reduce.h"
#include "params.h"

using namespace ckernel;
using namespace ckernel::sfpu;

const int sfpu_iterations = 32;

namespace
{
void call_sfpu_operation(SfpuType operation)
{
    switch (operation)
    {
        case SfpuType::abs:
            _calculate_abs_<APPROX_MODE, sfpu_iterations>(sfpu_iterations);
            break;
        case SfpuType::gelu:
            _init_gelu_<APPROX_MODE>();
            _calculate_gelu_<APPROX_MODE, sfpu_iterations>();
            break;
        case SfpuType::neg:
            _calculate_negative_<APPROX_MODE, sfpu_iterations>();
            break;
        case SfpuType::silu:
            _calculate_silu_<APPROX_MODE, sfpu_iterations>();
            break;
        case SfpuType::sqrt:
            _init_sqrt_<APPROX_MODE>();
            _calculate_sqrt_<APPROX_MODE, 0, sfpu_iterations>(sfpu_iterations);
            break;
        case SfpuType::square:
            _calculate_square_<APPROX_MODE, sfpu_iterations>(sfpu_iterations);
            break;
        default:
            return; // Unsupported op – should never happen
    }
}
} // namespace

void run_kernel()
{
    //------------------------------------------------------------------
    // Synchronisation & HW configuration (following reduce_sfpu_unary pattern)
    //------------------------------------------------------------------
    _llk_math_pack_sync_init_<ckernel::DstSync::SyncFull, is_fp32_dest_acc_en>();
    _llk_math_wait_for_dest_available_<ckernel::DstSync::SyncFull>();

    // row_pool tells HW if pooling is performed across rows (affects transpose path)
    _llk_math_hw_configure_<false, false>(formats.math, formats.math);

    //------------------------------------------------------------------
    // 1) Datacopy from input to dest (tilized input already available)
    //------------------------------------------------------------------
#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false, false>(0, 0, 4, formats.math);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false>(0, 0, 4, formats.math);
#endif

    _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, ckernel::DstSync::SyncFull, is_fp32_dest_acc_en, BroadcastType::NONE, false>(
        0, formats.math, formats.math);

    //------------------------------------------------------------------
    // 2) Reduce operation on the data in dest registers
    //------------------------------------------------------------------
    const bool is_int_fpu_en    = false; // Not using int-FPU path
    const bool fp32_transpose   = false; // No fp32 transpose on reduce path
    constexpr uint32_t math_fid = 4;

    _llk_math_reduce_init_<POOL_TYPE, REDUCE_DIM, math_fid>(0);
    _llk_math_reduce_<POOL_TYPE, REDUCE_DIM, is_fp32_dest_acc_en, math_fid, is_int_fpu_en, fp32_transpose>(0);

    //------------------------------------------------------------------
    // 3) SFPU unary directly on the reduced result in dest regs
    //------------------------------------------------------------------
    _llk_math_eltwise_unary_sfpu_init_<SFPU_UNARY_OPERATION>();
    _llk_math_eltwise_unary_sfpu_start_<ckernel::DstSync::SyncFull>(0);

    call_sfpu_operation(SFPU_UNARY_OPERATION);

    _llk_math_eltwise_unary_sfpu_done_();
    _llk_math_dest_section_done_<ckernel::DstSync::SyncFull, is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel()
{
    // Configure packer hardware for untilize operation (tile format -> row-major)
    const bool UNTILIZE = true;
    const bool TILIZE   = false; // Input to pack is already in tile format

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, UNTILIZE, TILIZE>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
    _llk_pack_dest_init_<ckernel::DstSync::SyncFull, is_fp32_dest_acc_en, ckernel::DstTileFaceLayout::RowMajor>();
    _llk_pack_untilize_init_<BLOCK_CT_DIM>(formats.pack_src, formats.pack_dst, FACE_R_DIM, 4);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, UNTILIZE>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
    _llk_pack_dest_init_<ckernel::DstSync::SyncFull, is_fp32_dest_acc_en, ckernel::DstTileFaceLayout::RowMajor, UNTILIZE>();
    _llk_pack_untilize_init_<BLOCK_CT_DIM>(formats.pack_dst, FACE_R_DIM, 4);
#endif

    // Pack the result tile with untilization to output buffer
    _llk_packer_wait_for_math_done_();
    _llk_pack_untilize_<BLOCK_CT_DIM>(L1_ADDRESS(buffer_Res[0]), formats.pack_dst, FACE_R_DIM, 4, 0);
    _llk_pack_dest_section_done_<ckernel::DstSync::SyncFull, is_fp32_dest_acc_en>();
}

#endif
