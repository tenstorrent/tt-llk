// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "ckernel_defs.h"
#include "data_format_inference.h"
#include "llk_defs.h"
#include "params.h"
#include "perf.h"
#include "profiler.h"

// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;
uint32_t tile_size                = 128;
const int iterations              = 32;

static constexpr uint32_t MAX_TILES_DEST = is_fp32_dest_acc_en ? 4 : 8;
constexpr uint32_t buffer_A_tilized      = 0x17000;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_AB_matmul.h"
#include "llk_unpack_common.h"

void run_kernel()
{
    {
        ZONE_SCOPED("INIT")
        int run = 0;
        _llk_unpack_AB_matmul_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(
            formats_array[run].unpack_src, formats_array[run].unpack_src, formats_array[run].unpack_dst, formats_array[run].unpack_dst);
        _llk_unpack_AB_matmul_init_<>();
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
            for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
            {
                // Matmul only - skip unpack and only measure math
                _perf_unpack_loop_set_valid</* src A */ true, /* src B */ true>(1);

                // Unary SFPU only - skip unpack, only measure SFPU operations
                _perf_unpack_loop_set_valid</* src A */ true, /* src B */ false>(1);
            }
        }
        else
        {
            for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
            {
                int run = 0;
                _llk_unpack_AB_matmul_<>(
                    PERF_ADDRESS(PERF_INPUT_A, 0),
                    PERF_ADDRESS(PERF_INPUT_B, 0),
                    /* tile_index_a */ 0,
                    /* tile_index_b */ 0,
                    /* tile_size_a */ tile_size,
                    /* tile_size_b */ tile_size);

                t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
                t6_semaphore_get<>(semaphore::PACK_DONE);

                run = 1;
                _llk_unpack_reconfig_data_format_srca_impl_<is_fp32_dest_acc_en, /* to_from_int8 */ false>(
                    formats_array[run].unpack_src,
                    formats_array[run].unpack_dst,
                    /* tile_size */ tile_size);
                _llk_unpack_A_init_<BroadcastType::NONE, is_fp32_dest_acc_en, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
                    /* transpose_of_faces */ 0,
                    /* within_face_16x16_transpose */ 0,
                    FACE_R_DIM,
                    /* tile_size */ 4,
                    formats_array[run].unpack_src,
                    formats_array[run].unpack_dst);
                _llk_unpack_A_<BroadcastType::NONE, is_fp32_dest_acc_en, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(
                    PERF_ADDRESS(PERF_INPUT_A, 0), formats_array[run].unpack_src, formats_array[run].unpack_dst);
            }
        }
        PROFILER_SYNC();
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "ckernel_sfpu.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "llk_math_eltwise_unary_sfpu.h"
#include "llk_math_matmul.h"
#include "sfpu_operations.h"

void run_kernel()
{
    {
        ZONE_SCOPED("INIT")
        int run = 0;
        _llk_math_matmul_init_<MATH_FIDELITY, DstTileFaceLayout::RowMajor>();
        _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
        _llk_math_hw_configure_<>(formats_array[run].math, formats_array[run].math);
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
            return _perf_math_loop_clear_valid<true, true>(LOOP_FACTOR);
        }
        else
        {
            for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
            {
                // int run = 0;
                //_llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
                _llk_math_matmul_<MATH_FIDELITY, DstTileFaceLayout::RowMajor>(/* dst_index */ 0);
                //_llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

                // run = 1;
                //  _llk_math_reconfig_data_format_srca_<is_fp32_dest_acc_en, /* to_from_int8 */ false>(
                //      formats_array[run].math);
                // cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_RMW>(formats_array[run].math);

                // _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE>(
                //     NUM_FACES,
                //     formats_array[run].math);

                // //_llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                // //_llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
                // _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                //     /* tile_idx */ 0,
                //     formats_array[run].math,
                //     formats_array[run].math);

                _llk_math_eltwise_unary_sfpu_init_<SFPU_UNARY_OPERATION>();
                _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(/* dst_index */ 0);
                test_utils::call_sfpu_operation_32(SFPU_UNARY_OPERATION);

                _llk_math_eltwise_unary_sfpu_done_();
                //_llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            }
        }
        PROFILER_SYNC();
    }
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel()
{
    {
        ZONE_SCOPED("INIT")
        int run = 0;
#ifdef ARCH_BLACKHOLE
        _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(
            formats_array[run].pack_src, formats_array[run].pack_dst, TEST_FACE_R_DIM * TEST_FACE_C_DIM * NUM_FACES);
        _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false, false>(formats_array[run].pack_dst);
        _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();
#else
        _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(
            formats_array[run].pack_src, formats_array[run].pack_dst, TEST_FACE_R_DIM * TEST_FACE_C_DIM * NUM_FACES);
        _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(formats_array[run].pack_dst);
        _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor, false>();
#endif
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE || PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            return;
        }
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE || PERF_RUN_TYPE == PerfRunType::L1_CONGESTION)
        {
            for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
            {
                int run = 0;
                _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(0, L1_ADDRESS(buffer_A_tilized));

                t6_semaphore_post<>(semaphore::PACK_DONE);

                run = 1;
                _llk_pack_reconfig_data_format_<is_fp32_dest_acc_en>(formats_array[run].pack_src, formats_array[run].pack_dst, tile_size);
                _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(formats_array[run].pack_dst);

#ifdef ARCH_BLACKHOLE
                _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();
#else
                _llk_pack_dest_init_<DstSync::SyncHalf, false, DstTileFaceLayout::RowMajor, false>();
#endif

                _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(0, PERF_ADDRESS(PERF_OUTPUT, 0));
            }
        }
        else
        {
            for (uint32_t loop = 0; loop < LOOP_FACTOR; ++loop)
            {
                int run = 0;
                _llk_packer_wait_for_math_done_();
                _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(0, L1_ADDRESS(buffer_A_tilized));
                _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

                t6_semaphore_post<>(semaphore::PACK_DONE);

                run = 1;
                _llk_pack_reconfig_data_format_<is_fp32_dest_acc_en>(formats_array[run].pack_src, formats_array[run].pack_dst, tile_size);
                _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(formats_array[run].pack_dst);

#ifdef ARCH_BLACKHOLE
                _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();
#else
                _llk_pack_dest_init_<DstSync::SyncHalf, false, DstTileFaceLayout::RowMajor, false>();
#endif

                _llk_packer_wait_for_math_done_();
                _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(0, PERF_ADDRESS(PERF_OUTPUT, 0));
                _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            }
        }
        PROFILER_SYNC();
    }
}

#endif
