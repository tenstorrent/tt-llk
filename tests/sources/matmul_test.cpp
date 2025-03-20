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

#ifdef DEST_ACC
const bool is_fp32_dest_acc_en = true;
#else
const bool is_fp32_dest_acc_en = false;
#endif

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB_matmul.h"
#include "params.h"

void run_kernel()
{
    volatile uint32_t* const buffer_A = reinterpret_cast<volatile uint32_t*>(0x1a000);
    volatile uint32_t* const buffer_B = reinterpret_cast<volatile uint32_t*>(0x1b000);

    std::uint32_t ct_dim = 1;
    std::uint32_t rt_dim = 1;
    std::uint32_t kt_dim = 1;

    std::uint32_t tile_size = 128;

    _llk_unpack_AB_matmul_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(
        UNPACK_IN, UNPACK_IN, UNPACK_OUT, UNPACK_OUT); //, FACE_R_DIM, FACE_R_DIM, 0, 4, 4, tile_size, tile_size);
    _llk_unpack_AB_matmul_init_<>();
    _llk_unpack_AB_matmul_<>(
        L1_ADDRESS(buffer_A), L1_ADDRESS(buffer_B), 0, 0, tile_size, tile_size); 
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_matmul.h"
#include "params.h"

void run_kernel()
{
    _llk_math_matmul_init_<MATH_FIDELITY, DstTileFaceLayout::RowMajor>();
    _llk_math_pack_sync_init_<DstSync::SyncFull, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false, false>(MATH_FORMAT, MATH_FORMAT);
    _llk_math_wait_for_dest_available_<DstSync::SyncFull>();
    _llk_math_matmul_<MATH_FIDELITY, DstTileFaceLayout::RowMajor>(0);
    _llk_math_dest_section_done_<DstSync::SyncFull, is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel()
{
    volatile uint32_t* const buffer_Dest = reinterpret_cast<volatile uint32_t*>(0x1c000);

    std::fill(buffer_Dest, buffer_Dest + 16 * 16 * 4, 0xdeadbeef);

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<false, is_fp32_dest_acc_en, false>(PACK_IN, PACK_OUT, 16 * 16 * 4);
#else
    _llk_pack_hw_configure_<false, is_fp32_dest_acc_en>(PACK_IN, PACK_OUT, 16 * 16 * 4);
#endif

    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(PACK_OUT);

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncFull, DstTileFaceLayout::RowMajor, is_fp32_dest_acc_en>();
#else
    _llk_pack_dest_init_<DstSync::SyncFull, DstTileFaceLayout::RowMajor, false, false>();
#endif

    _llk_packer_wait_for_math_done_();
    _llk_pack_<DstSync::SyncFull, false, is_fp32_dest_acc_en>(0, L1_ADDRESS(buffer_Dest));
    _llk_pack_dest_section_done_<DstSync::SyncFull, is_fp32_dest_acc_en>();
}

#endif
