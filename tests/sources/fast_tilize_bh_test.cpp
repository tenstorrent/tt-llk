// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

// BH full fast-tilize: unpack + math + pack → standard tilized output.
// ct_dim must be divisible by 4, unit_dim=4 always.
// Supports PERF_RUN_TYPE for performance measurements.

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
    constexpr std::uint32_t unit_dim = 4;

    {
        ZONE_SCOPED("INIT")
        _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
            formats.unpack_A_src, formats.unpack_B_src, formats.unpack_A_dst, formats.unpack_B_dst, FACE_R_DIM, FACE_R_DIM, 4, 4);
        _llk_unpack_fast_tilize_init_(formats.unpack_A_dst, BLOCK_CT_DIM);
        // Set L1 base address once — block function uses only counters.
        volatile std::uint32_t tt_reg_ptr* cfg   = get_cfg_pointer();
        cfg[THCON_SEC0_REG3_Base_address_ADDR32] = L1_ADDRESS(buffer_A[0]);
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE)
        {
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            std::uint32_t num_units = BLOCK_RT_DIM * (BLOCK_CT_DIM / unit_dim);
            return _perf_unpack_loop_set_valid<true, false>(num_units * 4 * LOOP_FACTOR);
        }
        else
        {
            std::uint32_t num_units_total = BLOCK_RT_DIM * (BLOCK_CT_DIM / unit_dim);
            for (std::uint32_t loop = 0; loop < LOOP_FACTOR; loop++)
            {
                _llk_unpack_fast_tilize_block_(L1_ADDRESS(buffer_A[0]), 0, formats.unpack_A_src, unit_dim, num_units_total, BLOCK_CT_DIM, 4);
            }
        }
    }
    {
        ZONE_SCOPED("UNINIT")
        _llk_unpack_fast_tilize_uninit_<is_fp32_dest_acc_en>();
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

    {
        ZONE_SCOPED("INIT")
        // Enable remap BEFORE sync_init (reconfig_remap waits for MATH_PACK semaphore)
        _llk_math_reconfig_remap_(true);
        _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
        _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);
        _llk_math_fast_tilize_init_<is_fp32_dest_acc_en>(formats.math, unit_dim);
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        std::uint32_t num_units = BLOCK_RT_DIM * (BLOCK_CT_DIM / unit_dim);

        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE)
        {
            // Release one dest section so pack can start, then exit
            _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE)
        {
            return _perf_math_loop_clear_valid<true, false>(num_units * 4 * LOOP_FACTOR);
        }
        else
        {
            for (std::uint32_t loop = 0; loop < LOOP_FACTOR; loop++)
            {
                for (std::uint32_t u = 0; u < num_units; u++)
                {
                    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
                    _llk_math_fast_tilize_block_<is_fp32_dest_acc_en>(0, formats.math, unit_dim, 1, 4);
                    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                }
            }
        }
    }
    {
        ZONE_SCOPED("UNINIT")
        _llk_math_fast_tilize_uninit_<is_fp32_dest_acc_en>(formats.math);
        // NOTE: _llk_math_reconfig_remap_(false) removed — it calls tensix_sync()
        // which deadlocks when pack T2 hasn't finished. Warm reset fixture
        // handles remap cleanup between tests.
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

    {
        ZONE_SCOPED("INIT")
        _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
        _llk_pack_hw_configure_<is_fp32_dest_acc_en>(formats.pack_src, formats.pack_dst, SCALE_DATUM_SIZE(formats.pack_dst, TILE_C_DIM * TILE_R_DIM));
        _llk_pack_fast_tilize_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>(0, formats.pack_dst, unit_dim, 4);
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        std::uint32_t num_units = BLOCK_RT_DIM * (BLOCK_CT_DIM / unit_dim);

        if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE)
        {
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
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
                for (std::uint32_t u = 0; u < num_units; u++)
                {
                    _llk_packer_wait_for_math_done_();
                    std::uint32_t row         = u / (BLOCK_CT_DIM / unit_dim);
                    std::uint32_t col         = u % (BLOCK_CT_DIM / unit_dim);
                    std::uint32_t tile_offset = row * BLOCK_CT_DIM + col * unit_dim;
                    _llk_pack_fast_tilize_block_(0, L1_ADDRESS(buffer_Res[tile_offset]), unit_dim, 1, 4);
                    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                }
            }
        }
    }
    {
        ZONE_SCOPED("UNINIT")
        _llk_pack_fast_tilize_uninit_<DstSync::SyncHalf, is_fp32_dest_acc_en>(formats.pack_dst, FACE_R_DIM, 4);
    }
}

#endif
