#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "ckernel.h"
#include "llk_defs.h"
// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

template <ckernel::ThreadId thread_id>
inline void dbg_thread_halt()
{
    static_assert(
        (thread_id == ckernel::ThreadId::MathThreadId) || (thread_id == ckernel::ThreadId::UnpackThreadId) || (thread_id == ckernel::ThreadId::PackThreadId),
        "Invalid thread id set in dbg_wait_for_thread_idle(...)");

    if constexpr (thread_id == ckernel::ThreadId::UnpackThreadId)
    {
        // Wait for all instructions on the running thread to complete
        ckernel::tensix_sync();
        // Notify math thread that unpack thread is idle
        ckernel::mailbox_write(ckernel::ThreadId::MathThreadId, 1);
        // Wait for math thread to complete debug dump
        volatile uint32_t temp = ckernel::mailbox_read(ckernel::ThreadId::MathThreadId);
    }
    else if constexpr (thread_id == ckernel::ThreadId::MathThreadId)
    {
        // Wait for all instructions on the running thread to complete
        ckernel::tensix_sync();
        // Wait for unpack thread to complete
        volatile uint32_t temp = ckernel::mailbox_read(ckernel::ThreadId::UnpackThreadId);
        // Wait for previous packs to finish
        while (ckernel::semaphore_read(ckernel::semaphore::MATH_PACK) > 0)
        {
        };
    }
}

void dbg_halt() {
    dbg_thread_halt<ckernel::MathThreadId>();
}




// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0


#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel()
{
    volatile uint32_t* const buffer_A = reinterpret_cast<volatile uint32_t*>(0x1a000);

    _llk_unpack_A_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(UNPACK_A_IN, UNPACK_A_OUT, FACE_R_DIM, 0, 4);
    _llk_unpack_A_init_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(0, 0, FACE_R_DIM, 4, UNPACK_A_IN, UNPACK_A_OUT);
    _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(L1_ADDRESS(buffer_A), 0, UNPACK_A_IN, UNPACK_A_OUT);
}

#endif

#ifdef LLK_TRISC_MATH

#include "ckernel_sfpu.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "llk_math_eltwise_unary_sfpu.h"
#include "params.h"
#include "ckernel_sfpu_where.h"

using namespace ckernel;
using namespace ckernel::sfpu;

void run_kernel()
{

    const uint32_t DST_TILE = 2;

// copy srca to dest
#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, BroadcastType::NONE, false, is_fp32_dest_acc_en, false>(0, 0, 4, MATH_FORMAT);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, BroadcastType::NONE, is_fp32_dest_acc_en, false>(0, 0, 4, MATH_FORMAT);
#endif
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false, false>(MATH_FORMAT, MATH_FORMAT);
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, BroadcastType::NONE, is_fp32_dest_acc_en, unpack_to_dest>(
        DST_TILE , MATH_FORMAT, MATH_FORMAT);

    // calculation of sfpu operation on dest
    _llk_math_eltwise_unary_sfpu_init_<SFPU_OPERATION>();
    _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(0);

    _calculate_where_<32>(DST_TILE,1,8);

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

#ifdef PACK_DST_BFP8_B
    constexpr auto PACK_OUT = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
#endif

    volatile uint32_t* const buffer_Dest = reinterpret_cast<volatile uint32_t*>(0x1c000);

    std::fill(buffer_Dest, buffer_Dest + 16 * 16 * 4, 0xdeadbeef);

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<false, is_fp32_dest_acc_en, false>(PACK_IN, PACK_OUT, 16 * 16 * 4);
#else
    _llk_pack_hw_configure_<false, is_fp32_dest_acc_en>(PACK_IN, PACK_OUT, 16 * 16 * 4);
#endif

    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(PACK_OUT);

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncHalf, DstTileFaceLayout::RowMajor, is_fp32_dest_acc_en>();
#else
    _llk_pack_dest_init_<DstSync::SyncHalf, DstTileFaceLayout::RowMajor, false, false>();
#endif

    _llk_packer_wait_for_math_done_();
    _llk_pack_<DstSync::SyncHalf, false, is_fp32_dest_acc_en>(0, L1_ADDRESS(buffer_Dest));
    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif
