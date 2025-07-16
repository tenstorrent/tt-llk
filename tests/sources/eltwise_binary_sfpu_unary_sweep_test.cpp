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

#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel()
{
    // Configure hardware for unpacking AB (two inputs for binary elementwise operation)
    _llk_unpack_AB_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst);
    _llk_unpack_AB_init_<>();

    // Unpack one tile from each input buffer
    _llk_unpack_AB_<>(L1_ADDRESS(buffer_A[0]), L1_ADDRESS(buffer_B[0]));
}

#endif

#ifdef LLK_TRISC_MATH

#include "ckernel_sfpu.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "llk_math_eltwise_unary_sfpu.h"
#include "params.h"

using namespace ckernel;
using namespace ckernel::sfpu;

const int iterations = 32;

namespace
{
void call_sfpu_operation(SfpuType operation, uint32_t math_format)
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
        default:
            return;
    }
}
} // namespace

void run_kernel()
{
    // Initialize math operations
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false, false>(formats.math, formats.math);
    _llk_math_eltwise_binary_init_<ELTWISE_BINARY_OP, BroadcastType::NONE, MATH_FIDELITY>(4, 0, 0);

    // Wait for destination to be available
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();

    // Perform elementwise binary operation (ELWADD, ELWMUL, or ELWSUB)
    _llk_math_eltwise_binary_<ELTWISE_BINARY_OP, BroadcastType::NONE, DstSync::SyncHalf, is_fp32_dest_acc_en, MATH_FIDELITY, EltwiseBinaryReuseDestType::NONE>(
        4, 0, false);

    // Now perform SFPU unary operation on the result in dest
    _llk_math_eltwise_unary_sfpu_init_<SFPU_UNARY_OPERATION>();
    _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(0);

    // Call the specific SFPU operation
    call_sfpu_operation(SFPU_UNARY_OPERATION, formats.math);

    _llk_math_eltwise_unary_sfpu_done_();
    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel()
{
    // Configure packer hardware
#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
#endif

    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(formats.pack_dst);

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();
#else
    _llk_pack_dest_init_<DstSync::SyncHalf, false, DstTileFaceLayout::RowMajor, false>();
#endif

    // Pack the result from destination register to output buffer
    _llk_packer_wait_for_math_done_();
    _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(0, L1_ADDRESS(buffer_Res[0]));
    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif
