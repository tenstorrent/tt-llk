// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"
#include "params.h"

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
    const std::uint32_t row_index_in_tile_b   = 31;
    constexpr std::uint32_t BYTES_PER_ELEMENT = 2;  // For now only support 16 bit wide data
    constexpr std::uint32_t TILE_WIDTH        = 32; // Full tile width for row-major layout

    std::uint32_t offset_in_elements = row_index_in_tile_b * TILE_WIDTH;

    (*((volatile uint32_t*)0x18000)) = 0x1a800 + offset_in_elements * BYTES_PER_ELEMENT;

    _llk_unpack_AB_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst);
    _llk_unpack_AB_init_<BroadcastType::ROW>();

    for (int i = 0; i < TILE_CNT; i++)
    {
        _llk_unpack_AB_<BroadcastType::ROW>(L1_ADDRESS(buffer_A[i]), L1_ADDRESS(buffer_B[i] + offset_in_elements * BYTES_PER_ELEMENT));
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "ckernel_debug.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "params.h"

void run_kernel()
{
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false, false>(formats.math, formats.math);
    _llk_math_eltwise_binary_init_<ELTWISE_BINARY_OP, BroadcastType::ROW, MATH_FIDELITY>(4, 0, 0);

    for (int i = 0; i < TILE_CNT; i++)
    {
        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
        _llk_math_eltwise_binary_<
            ELTWISE_BINARY_OP,
            BroadcastType::ROW,
            DstSync::SyncHalf,
            is_fp32_dest_acc_en,
            MATH_FIDELITY,
            EltwiseBinaryReuseDestType::NONE>(4, 0, false);
        _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel()
{
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

    for (int i = 0; i < TILE_CNT; i++)
    {
        _llk_packer_wait_for_math_done_();
        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(0, L1_ADDRESS(buffer_Res[i]));
        _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }
}

#endif
