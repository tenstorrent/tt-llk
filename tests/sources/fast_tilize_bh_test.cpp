// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

// BH full fast-tilize: unpack + math + pack → standard tilized output.
// Supports arbitrary ct_dim via fast 4-wide prefix + standard tilize tail.
// Supports PERF_RUN_TYPE for performance measurements (4-wide only).

#include <cstdint>

#include "ckernel.h"
#include "llk_defs.h"
#include "params.h"
#include "profiler.h"

#ifndef PERF_RUN_TYPE
#define PERF_RUN_TYPE PerfRunType::L1_TO_L1
#endif
#include "perf.h"

std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_common.h"
#include "llk_unpack_tilize.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#ifndef SPEED_OF_LIGHT
    const std::uint32_t BLOCK_CT_DIM = params.BLOCK_CT_DIM;
    const std::uint32_t BLOCK_RT_DIM = params.BLOCK_RT_DIM;
    const std::uint32_t LOOP_FACTOR  = params.LOOP_FACTOR;
    const Operand& buffer_A          = params.buffer_A;
#endif

    const std::uint32_t prefix_tiles = (BLOCK_CT_DIM / 4) * 4;
    const std::uint32_t tail_tiles   = BLOCK_CT_DIM % 4;
    const bool has_tail              = tail_tiles > 0;

    {
        ZONE_SCOPED("INIT")
        _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
            formats.unpack_A_src, formats.unpack_B_src, formats.unpack_A_dst, formats.unpack_B_dst, FACE_R_DIM, FACE_R_DIM, 4, 4);

        if (prefix_tiles > 0)
        {
            _llk_unpack_fast_tilize_init_(formats.unpack_A_dst, BLOCK_CT_DIM);
            volatile std::uint32_t tt_reg_ptr* cfg   = get_cfg_pointer();
            cfg[THCON_SEC0_REG3_Base_address_ADDR32] = L1_ADDRESS(buffer_A[0]);
        }
        else
        {
            _llk_unpack_tilize_init_(formats.unpack_A_src, formats.unpack_A_dst, BLOCK_CT_DIM, FACE_R_DIM, false);
        }
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE)
        {
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            // For perf isolate: only count fast-prefix dvalids
            std::uint32_t num_units = BLOCK_RT_DIM * (prefix_tiles / 4);
            return _perf_unpack_loop_set_valid<true, false>(num_units * 4 * LOOP_FACTOR);
        }
        else
        {
            // Unpack processes all fast units first (all rows), then all tails.
            // Math/pack interleave via section_done synchronization.
            const std::uint32_t num_fast_units_total = BLOCK_RT_DIM * (prefix_tiles / 4);

            for (std::uint32_t loop = 0; loop < LOOP_FACTOR; loop++)
            {
                // Phase 1: all fast-prefix units across all rows
                if (prefix_tiles > 0)
                {
                    if (loop > 0 && has_tail)
                    {
                        _llk_unpack_fast_tilize_init_(formats.unpack_A_dst, BLOCK_CT_DIM);
                        volatile std::uint32_t tt_reg_ptr* cfg   = get_cfg_pointer();
                        cfg[THCON_SEC0_REG3_Base_address_ADDR32] = L1_ADDRESS(buffer_A[0]);
                    }

                    _llk_unpack_fast_tilize_block_(L1_ADDRESS(buffer_A[0]), 0, formats.unpack_A_src, 4, num_fast_units_total, BLOCK_CT_DIM, 4);

                    if (has_tail)
                    {
                        _llk_unpack_fast_tilize_uninit_<is_fp32_dest_acc_en>();
                    }
                }

                // Phase 2: all standard-tail tiles across all rows
                if (has_tail)
                {
                    if (!(loop == 0 && prefix_tiles == 0))
                    {
                        _llk_unpack_tilize_init_(formats.unpack_A_src, formats.unpack_A_dst, BLOCK_CT_DIM, FACE_R_DIM, false);
                    }

                    for (std::uint32_t row = 0; row < BLOCK_RT_DIM; row++)
                    {
                        std::uint32_t tile_base = row * BLOCK_CT_DIM * TILE_R_DIM;
                        for (std::uint32_t t = 0; t < tail_tiles; t++)
                        {
                            _llk_unpack_tilize_(
                                L1_ADDRESS(buffer_A[0]), tile_base + prefix_tiles + t, formats.unpack_A_src, formats.unpack_A_dst, 0, FACE_R_DIM, 4, false);
                        }
                    }
                }
            }
        }
    }
    {
        ZONE_SCOPED("UNINIT")
        if (has_tail || prefix_tiles == 0)
        {
            _llk_unpack_tilize_uninit_(formats.unpack_A_dst, 4, FACE_R_DIM);
        }
        else
        {
            _llk_unpack_fast_tilize_uninit_<is_fp32_dest_acc_en>();
        }
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#ifndef SPEED_OF_LIGHT
    const std::uint32_t BLOCK_CT_DIM = params.BLOCK_CT_DIM;
    const std::uint32_t BLOCK_RT_DIM = params.BLOCK_RT_DIM;
    const std::uint32_t LOOP_FACTOR  = params.LOOP_FACTOR;
#endif
    constexpr std::uint32_t unit_dim = 4;

    const std::uint32_t prefix_tiles   = (BLOCK_CT_DIM / 4) * 4;
    const std::uint32_t tail_tiles     = BLOCK_CT_DIM % 4;
    const bool has_tail                = tail_tiles > 0;
    const std::uint32_t num_fast_units = prefix_tiles / 4;

    {
        ZONE_SCOPED("INIT")
        _llk_math_reconfig_remap_(true);
        _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
        _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);

        if (prefix_tiles > 0)
        {
            _llk_math_fast_tilize_init_<is_fp32_dest_acc_en>(formats.math, unit_dim);
        }
        else
        {
            _llk_math_eltwise_unary_datacopy_init_<A2D, is_fp32_dest_acc_en, BroadcastType::NONE, true>(4, formats.math);
        }
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE)
        {
            _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE)
        {
            std::uint32_t num_units = BLOCK_RT_DIM * num_fast_units;
            return _perf_math_loop_clear_valid<true, false>(num_units * 4 * LOOP_FACTOR);
        }
        else
        {
            const std::uint32_t num_fast_units_total = BLOCK_RT_DIM * num_fast_units;
            const std::uint32_t num_tail_tiles_total = BLOCK_RT_DIM * tail_tiles;

            for (std::uint32_t loop = 0; loop < LOOP_FACTOR; loop++)
            {
                // Phase 1: all fast-prefix units
                if (prefix_tiles > 0)
                {
                    if (loop > 0 && has_tail)
                    {
                        _llk_math_fast_tilize_init_<is_fp32_dest_acc_en>(formats.math, unit_dim);
                    }

                    for (std::uint32_t u = 0; u < num_fast_units_total; u++)
                    {
                        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
                        _llk_math_fast_tilize_block_<is_fp32_dest_acc_en>(0, formats.math, unit_dim, 1, 4);
                        _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                    }

                    if (has_tail)
                    {
                        _llk_math_fast_tilize_uninit_<is_fp32_dest_acc_en>(formats.math);
                    }
                }

                // Phase 2: all standard-tail tiles
                if (has_tail)
                {
                    if (!(loop == 0 && prefix_tiles == 0))
                    {
                        _llk_math_eltwise_unary_datacopy_init_<A2D, is_fp32_dest_acc_en, BroadcastType::NONE, true>(4, formats.math);
                    }

                    for (std::uint32_t t = 0; t < num_tail_tiles_total; t++)
                    {
                        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
                        _llk_math_eltwise_unary_datacopy_<A2D, DstSync::SyncHalf, is_fp32_dest_acc_en>(0, formats.math, formats.math, 4);
                        _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                    }
                }
            }
        }
    }
    {
        ZONE_SCOPED("UNINIT")
        if (prefix_tiles > 0 && !has_tail)
        {
            _llk_math_fast_tilize_uninit_<is_fp32_dest_acc_en>(formats.math);
        }
    }
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack_common.h"
#include "llk_pack_fast_tilize.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#ifndef SPEED_OF_LIGHT
    const std::uint32_t BLOCK_CT_DIM = params.BLOCK_CT_DIM;
    const std::uint32_t BLOCK_RT_DIM = params.BLOCK_RT_DIM;
    const std::uint32_t LOOP_FACTOR  = params.LOOP_FACTOR;
    const Operand& buffer_Res        = params.buffer_Res;
#endif
    constexpr std::uint32_t unit_dim = 4;

    const std::uint32_t prefix_tiles   = (BLOCK_CT_DIM / 4) * 4;
    const std::uint32_t tail_tiles     = BLOCK_CT_DIM % 4;
    const bool has_tail                = tail_tiles > 0;
    const std::uint32_t num_fast_units = prefix_tiles / 4;

    {
        ZONE_SCOPED("INIT")
        _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
        _llk_pack_hw_configure_<is_fp32_dest_acc_en>(formats.pack_src, formats.pack_dst, SCALE_DATUM_SIZE(formats.pack_dst, TILE_C_DIM * TILE_R_DIM));

        if (prefix_tiles > 0)
        {
            _llk_pack_fast_tilize_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>(0, formats.pack_dst, unit_dim, 4);
        }
        else
        {
            _llk_pack_init_<false, false, true>(formats.pack_src, formats.pack_dst, FACE_R_DIM, TILE_C_DIM, 4, false, false, 1, false);
        }
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE)
        {
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            std::uint32_t num_units = BLOCK_RT_DIM * num_fast_units;
            for (std::uint32_t i = 0; i < num_units * LOOP_FACTOR; i++)
            {
                _llk_packer_wait_for_math_done_();
                _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            }
            return;
        }
        else
        {
            for (std::uint32_t loop = 0; loop < LOOP_FACTOR; loop++)
            {
                // Phase 1: all fast-prefix units across all rows
                if (prefix_tiles > 0)
                {
                    if (loop > 0 && has_tail)
                    {
                        _llk_pack_fast_tilize_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>(0, formats.pack_dst, unit_dim, 4);
                    }

                    for (std::uint32_t row = 0; row < BLOCK_RT_DIM; row++)
                    {
                        for (std::uint32_t u = 0; u < num_fast_units; u++)
                        {
                            _llk_packer_wait_for_math_done_();
                            std::uint32_t tile_offset = row * BLOCK_CT_DIM + u * 4;
                            _llk_pack_fast_tilize_block_(0, L1_ADDRESS(buffer_Res[tile_offset]), unit_dim, 1, 4);
                            _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                        }
                    }

                    if (has_tail)
                    {
                        _llk_pack_fast_tilize_uninit_<DstSync::SyncHalf, is_fp32_dest_acc_en>(formats.pack_dst, FACE_R_DIM, 4);
                    }
                }

                // Phase 2: all standard-tail tiles across all rows
                if (has_tail)
                {
                    if (!(loop == 0 && prefix_tiles == 0))
                    {
                        _llk_pack_init_<false, false, true>(formats.pack_src, formats.pack_dst, FACE_R_DIM, TILE_C_DIM, 4, false, false, 1, false);
                    }

                    for (std::uint32_t row = 0; row < BLOCK_RT_DIM; row++)
                    {
                        for (std::uint32_t t = 0; t < tail_tiles; t++)
                        {
                            _llk_packer_wait_for_math_done_();
                            std::uint32_t tile_offset = row * BLOCK_CT_DIM + prefix_tiles + t;
                            _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en>(0, L1_ADDRESS(buffer_Res[tile_offset]));
                            _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                        }
                    }
                }
            }
        }
    }
    {
        ZONE_SCOPED("UNINIT")
        if (prefix_tiles > 0 && !has_tail)
        {
            _llk_pack_fast_tilize_uninit_<DstSync::SyncHalf, is_fp32_dest_acc_en>(formats.pack_dst, FACE_R_DIM, 4);
        }
    }
}

#endif
