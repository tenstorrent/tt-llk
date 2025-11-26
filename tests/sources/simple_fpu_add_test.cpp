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

#include "ckernel_template.h"
#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"

void run_kernel()
{
    using namespace ckernel;

    _llk_unpack_AB_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst);

    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(0);

    constexpr std::uint32_t UNP_SEL = p_setadc::UNP_AB;
    config_unpacker_x_end<UNP_SEL>(FACE_R_DIM);

    volatile uint tt_reg_ptr *cfg = get_cfg_pointer();

    constexpr uint unpacr_A = TT_OP_UNPACR(SrcA, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    constexpr uint unpacr_B = TT_OP_UNPACR(SrcB, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    ckernel_template unpack_mop(1, 4, unpacr_A, unpacr_B);

    unpack_mop.program();

    for (int i = 0; i < TILE_CNT; i++)
    {
        TTI_SETADCZW(0b011, 0, 0, 0, 0, 0b1111);

        wait_for_next_context(2);

        if (0 == unp_cfg_context)
        {
            cfg[THCON_SEC0_REG3_Base_address_ADDR32] = L1_ADDRESS(buffer_A[i]);
            cfg[THCON_SEC1_REG3_Base_address_ADDR32] = L1_ADDRESS(buffer_B[i]);
        }
        else
        {
            cfg[THCON_SEC0_REG3_Base_cntx1_address_ADDR32] = L1_ADDRESS(buffer_A[i]);
            cfg[THCON_SEC1_REG3_Base_cntx1_address_ADDR32] = L1_ADDRESS(buffer_B[i]);
        }

        semaphore_post(semaphore::UNPACK_SYNC);

        TTI_STALLWAIT(p_stall::STALL_UNPACK, p_stall::TRISC_CFG);

        ckernel_template::run();

        t6_semaphore_get(semaphore::UNPACK_SYNC);

        switch_config_context(unp_cfg_context);
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "ckernel.h"
#include "ckernel_include.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_math_common.h"

using namespace ckernel;

void run_kernel()
{
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false, false>(formats.math, formats.math);

    addr_mod_t {
        .srca = {.incr = 8},
        .srcb = {.incr = 8},
        .dest = {.incr = 8},
    }
        .set(ADDR_MOD_0);

    constexpr uint outerloop   = 4;
    constexpr uint innerloop   = 16 >> 3;
    const uint addr_mod        = ADDR_MOD_0;
    const uint32_t acc_to_dest = 0;
    auto broadcast_type        = p_elwise::SRCB_NO_BCAST;

    ckernel_template math_mop(outerloop, innerloop, TT_OP_ELWADD(0, acc_to_dest, broadcast_type, addr_mod, 0));

    math_mop.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_AB, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));

    math_mop.program();

    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);

    for (int i = 0; i < TILE_CNT; i++)
    {
        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
        math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(i);
        math::reset_counters(p_setrwc::SET_ABD_F);
        ckernel_template::run();
        _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

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
        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(i, L1_ADDRESS(buffer_Res[i]));
        _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }
}

#endif
