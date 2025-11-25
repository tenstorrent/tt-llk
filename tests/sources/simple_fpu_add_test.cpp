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

void run_kernel()
{
    _llk_unpack_AB_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst);

    _llk_unpack_AB_init_<>();

    for (int i = 0; i < TILE_CNT; i++)
    {
        _llk_unpack_AB_<>(L1_ADDRESS(buffer_A[i]), L1_ADDRESS(buffer_B[i]));
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "ckernel.h"
#include "ckernel_globals.h"
#include "ckernel_include.h"
#include "cmath_common.h"
#include "llk_math_common.h"

using namespace ckernel;

void run_kernel()
{
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

    _llk_math_hw_configure_<false, false>(formats.math, formats.math);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 8},
        .dest = {.incr = 8},
    }
        .set(ADDR_MOD_0);

    addr_mod_t {
        .srca = {.incr = 8},
        .srcb = {.incr = 8},
        .dest = {.incr = 8},
    }
        .set(ADDR_MOD_1);

    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();

    for (int i = 0; i < TILE_CNT; i++)
    {
        math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(i);

        math::reset_counters(p_setrwc::SET_ABD_F);

        TTI_ELWADD(0, 0, 0, ADDR_MOD_0, 0); // Face 0, redovi 0-7
        TTI_ELWADD(0, 0, 0, ADDR_MOD_1, 0); // Face 0, redovi 8-15
        TTI_ELWADD(0, 0, 0, ADDR_MOD_0, 0); // Face 1, redovi 0-7
        TTI_ELWADD(0, 0, 0, ADDR_MOD_1, 0); // Face 1, redovi 8-15

        TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_A);

        TTI_ELWADD(0, 0, 0, ADDR_MOD_0, 0); // Face 2, redovi 0-7
        TTI_ELWADD(0, 0, 0, ADDR_MOD_1, 0); // Face 2, redovi 8-15
        TTI_ELWADD(0, 0, 0, ADDR_MOD_0, 0); // Face 3, redovi 0-7
        TTI_ELWADD(0, 0, 0, ADDR_MOD_1, 0); // Face 3, redovi 8-15

        TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_AB);

        math::clear_dst_reg_addr();
    }

    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
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
