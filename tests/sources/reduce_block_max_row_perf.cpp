// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "ckernel_defs.h"
#include "llk_defs.h"
#include "params.h"
#include "perf.h"
#include "profiler.h"

// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

static constexpr uint32_t MAX_TILES_DEST = is_fp32_dest_acc_en ? 4 : 8;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB_reduce_custom.h"
#include "llk_unpack_common.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src,
        formats.unpack_src,
        formats.unpack_dst,
        formats.unpack_dst,
        FACE_R_DIM,
        FACE_R_DIM,
        /* num_faces */ 4,
        /* num_faces */ 4);
    {
        ZONE_SCOPED("INIT")

        _llk_unpack_AB_reduce_block_max_row_init_<BLOCK_CT_DIM, is_fp32_dest_acc_en>();
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE)
        {
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            // Set valid flags matching the pattern expected by the math kernel
            // Use TTI_UNPACR to match the exact pattern used by the unpack function
            for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
            {
                // Set SrcB valid once for the entire block (scaler stays the same)
                // This matches line 128 in llk_unpack_AB_reduce_custom.h
                TTI_STALLWAIT(ckernel::p_stall::STALL_TDMA, ckernel::p_stall::SRCB_CLR);
                TTI_UNPACR(SrcB, 0b00000000 /* Z_ch0_inc and Z_ch1_inc */, 0, 0, 0, 1, 1 /* Set Dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

                // Set SrcA valid for each tile in the block
                // This matches the MOP template pattern (line 41 in llk_unpack_AB_reduce_custom.h)
                for (int i = 0; i < BLOCK_CT_DIM; ++i)
                {
                    TTI_STALLWAIT(ckernel::p_stall::STALL_TDMA, ckernel::p_stall::SRCA_CLR);
                    TTI_UNPACR(SrcA, 0b00000001 /* Z_ch0_inc and Z_ch1_inc */, 0, 0, 0, 1, 1 /* Set Dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
                }
            }
            return;
        }
        else
        {
            // Run the unpack operation multiple times to amortize profiler overhead
            for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
            {
                // Unpack block_ct_dim tiles from input A (operand) starting at address 0
                // and 1 tile from input B (scaler with 1.0 values) at address 0
                _llk_unpack_AB_reduce_block_max_row_(PERF_ADDRESS(PERF_INPUT_A, 0), PERF_ADDRESS(PERF_INPUT_B, 0));
            }
        }
        PROFILER_SYNC();
    }
    // Uninit outside of profiled zone to avoid zone mismatch with pack
    _llk_unpack_AB_reduce_block_max_row_uninit_(FACE_R_DIM, FACE_R_DIM);
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_reduce_custom.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);
    {
        ZONE_SCOPED("INIT")
        _llk_math_reduce_block_max_row_init_<BLOCK_CT_DIM, is_fp32_dest_acc_en>();
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE)
        {
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE || PERF_RUN_TYPE == PerfRunType::L1_CONGESTION)
        {
            // Clear valid flags matching the pattern used by the math kernel
            // SrcA is cleared after each tile, SrcB only after the entire block
            for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
            {
                for (int i = 0; i < BLOCK_CT_DIM; ++i)
                {
                    _perf_math_loop_clear_valid<true /* clear SrcA valid */, false /* don't clear SrcB valid */>(1);
                }
                _perf_math_loop_clear_valid<false /* don't clear SrcA valid */, true /* clear SrcB valid */>(1);
            }
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            // Run the math operation multiple times to amortize profiler overhead
            for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
            {
                // _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
                _llk_math_reduce_block_max_row_<BLOCK_CT_DIM, is_fp32_dest_acc_en>(0);
                // _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            }
        }
        else
        {
            // Run the full operation multiple times to amortize profiler overhead
            for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
            {
                _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
                _llk_math_reduce_block_max_row_<BLOCK_CT_DIM, is_fp32_dest_acc_en>(0);
                _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            }
        }
        PROFILER_SYNC();
    }
    // Uninit outside of profiled zone to avoid zone mismatch with pack
    _llk_math_reduce_block_max_row_uninit_();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
    {
        ZONE_SCOPED("INIT")
        _llk_pack_hw_configure_<is_fp32_dest_acc_en>(formats.pack_src, formats.pack_dst, TILE_WIDTH * TILE_HEIGHT);
        _llk_pack_init_<
            /* untilize */ false,
            /* zero output */ false>(formats.pack_dst);
        _llk_pack_reduce_mask_config_<
            /* untilize */ false,
            ckernel::ReduceDim::REDUCE_ROW>();
        _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE || PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            // _llk_pack_reduce_mask_clear_();
            return;
        }
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE || PERF_RUN_TYPE == PerfRunType::L1_CONGESTION)
        {
            // Run the pack operation multiple times to amortize profiler overhead
            for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
            {
                _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en>(0, PERF_ADDRESS(PERF_OUTPUT, 0));
            }
        }
        else
        {
            // Run the full operation multiple times to amortize profiler overhead
            for (int loop = 0; loop < params->LOOP_FACTOR; ++loop)
            {
                _llk_packer_wait_for_math_done_();
                _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en>(0, PERF_ADDRESS(PERF_OUTPUT, 0));
                _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            }
        }

        _llk_pack_reduce_mask_clear_();

        PROFILER_SYNC();
    }
}

#endif
