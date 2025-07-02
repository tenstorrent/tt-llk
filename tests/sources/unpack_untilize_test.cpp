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

#include "llk_unpack_A.h"
#include "llk_unpack_common.h"
#include "llk_unpack_untilize.h"
#include "params.h"

void run_kernel()
{
    _llk_unpack_untilize_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(UNPACK_A_IN, UNPACK_A_OUT, FACE_R_DIM, 0, 4);
    _llk_unpack_untilize_init_(UNPACK_A_IN, 1024, FACE_R_DIM, 4);

    _llk_unpack_untilize_pass_<true>(L1_ADDRESS(buffer_A[0]), BLOCK_CT_DIM);
    _llk_unpack_untilize_pass_<false>(L1_ADDRESS(buffer_A[0]), BLOCK_CT_DIM);

    // std::uint32_t unpA_ch1_x_stride = UNPACK_A_IN & 0x3 == 0 ? 4 : UNPACK_A_IN & 0x3 == 1 ? 2 : 1;
    // std::uint32_t unpA_ch1_y_stride = FACE_C_DIM * FACE_R_DIM * unpA_ch1_x_stride;

    // // Check that unpacker is done (all contexts freed up) before starting hw configuration
    // wait_for_idle();

    // // Reset address counters
    // unpacker_addr_counter_init();

    // // Wait for cfg to be free to edit
    // TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::UNPACK);

    // // Reset the values to default in unpack AB common.
    // TT_SETADCXX(p_setadc::UNP_A, FACE_R_DIM * FACE_C_DIM - 1, 0x0);
    // TTI_REG2FLOP(
    //     1, 0, 0, 0, THCON_SEC0_REG5_Tile_x_dim_cntx0_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::FACE_DIM_16x16);
    // cfg_reg_rmw_tensix<THCON_SEC0_REG0_TileDescriptor_ADDR32 + 1, 0, 0xFFFF>(1);
    // cfg_reg_rmw_tensix<
    //     UNP0_ADDR_CTRL_XY_REG_1_Ystride_ADDR32,
    //     UNP0_ADDR_CTRL_XY_REG_0_Ystride_SHAMT,
    //     UNP0_ADDR_CTRL_XY_REG_1_Ystride_MASK>(unpA_ch1_y_stride);
    // TTI_NOP;
    // TTI_NOP;  // Do we need this for WH?
}

#endif

#ifdef LLK_TRISC_MATH

const bool is_int_fpu_en = false;

#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "params.h"

using namespace ckernel;

void run_kernel()
{
// copy srca to dest
#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false, is_int_fpu_en>(0, 0, 4, MATH_FORMAT);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, is_int_fpu_en>(0, 0, 4, MATH_FORMAT);
#endif
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false, false>(MATH_FORMAT, MATH_FORMAT);
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, false>(i, MATH_FORMAT, MATH_FORMAT);
    }
    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel()
{
    const bool UNTILIIZE = false;

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, UNTILIIZE, false>(PACK_IN, PACK_OUT, 16 * 16 * 4);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, UNTILIIZE>(PACK_IN, PACK_OUT, 16 * 16 * 4);
#endif

    _llk_pack_init_<UNTILIIZE, false, DstTileFaceLayout::RowMajor, false>(PACK_OUT);

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();
#else
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor, UNTILIIZE>();
#endif

    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < TILE_CNT; ++i)
    {
        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(i, L1_ADDRESS(buffer_Res[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif
