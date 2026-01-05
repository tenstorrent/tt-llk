// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "counters.h"
#include "llk_defs.h"

using namespace llk_perf;

// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

#include "counters.h"
#include "llk_unpack_AB_matmul.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    llk_perf::PerfCounters counters;
    counters.add(llk_perf::CounterBank::INSTRN_THREAD, llk_perf::CounterId::InstrnThread::INST_UNPACK);
    counters.add(llk_perf::CounterBank::INSTRN_THREAD, llk_perf::CounterId::InstrnThread::INST_CFG);
    counters.add(llk_perf::CounterBank::FPU, llk_perf::CounterId::Fpu::FPU_OP_VALID);
    counters.add(llk_perf::CounterBank::FPU, llk_perf::CounterId::Fpu::SFPU_OP_VALID);
    counters.add(llk_perf::CounterBank::TDMA_UNPACK, llk_perf::CounterId::TdmaUnpack::UNPACK_BUSY_0);
    counters.add(llk_perf::CounterBank::TDMA_UNPACK, llk_perf::CounterId::TdmaUnpack::UNPACK_BUSY_1);
    counters.add(llk_perf::CounterBank::TDMA_PACK, llk_perf::CounterId::TdmaPack::PACK_NOT_DEST_STALL);
    counters.add(llk_perf::CounterBank::L1, llk_perf::CounterId::L1::NOC_RING0_INCOMING_1, 0);
    counters.start();

    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src,
        formats.unpack_src,
        formats.unpack_dst,
        formats.unpack_dst,
        FACE_R_DIM,
        FACE_R_DIM,
        params->num_faces_A,
        params->num_faces_B,
        TILE_SIZE_UNPACK_A,
        TILE_SIZE_UNPACK_B);
    _llk_unpack_AB_matmul_init_<>(0, params->CT_DIM, params->RT_DIM, params->KT_DIM, FACE_R_DIM, FACE_R_DIM, 4, 4, false, false);
    for (uint32_t j = 0; j < params->KT_DIM; j++)
    {
        _llk_unpack_AB_matmul_<>(
            L1_ADDRESS(buffer_A[0]),
            L1_ADDRESS(buffer_B[0]),
            j,
            j * params->CT_DIM,
            TILE_SIZE_UNPACK_A,
            TILE_SIZE_UNPACK_B,
            false,
            false,
            params->CT_DIM,
            params->RT_DIM,
            params->KT_DIM);
    }

    counters.stop();
}

#endif

#ifdef LLK_TRISC_MATH

#include "counters.h"
#include "llk_math_common.h"
#include "llk_math_matmul.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    llk_perf::PerfCounters counters;
    counters.add(llk_perf::CounterBank::INSTRN_THREAD, llk_perf::CounterId::InstrnThread::INST_MATH);
    counters.add(llk_perf::CounterBank::INSTRN_THREAD, llk_perf::CounterId::InstrnThread::STALLED);
    counters.add(llk_perf::CounterBank::FPU, llk_perf::CounterId::Fpu::FPU_OP_VALID);
    counters.add(llk_perf::CounterBank::FPU, llk_perf::CounterId::Fpu::SFPU_OP_VALID);
    counters.add(llk_perf::CounterBank::TDMA_UNPACK, llk_perf::CounterId::TdmaUnpack::MATH_INSTR_VALID);
    counters.add(llk_perf::CounterBank::TDMA_UNPACK, llk_perf::CounterId::TdmaUnpack::MATH_INSTR_SRC_READY);
    counters.add(llk_perf::CounterBank::TDMA_PACK, llk_perf::CounterId::TdmaPack::PACK_NOT_DEST_STALL);
    counters.add(llk_perf::CounterBank::L1, llk_perf::CounterId::L1::L1_ARB_TDMA_BUNDLE_0, 0);
    counters.start();

    _llk_math_matmul_init_<MATH_FIDELITY>(TILE_R_DIM, TILE_C_DIM, TILE_R_DIM, TILE_C_DIM, false, 0, params->CT_DIM, params->RT_DIM);
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_(formats.math, formats.math);
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (uint32_t j = 0; j < params->KT_DIM; j++)
    {
        _llk_math_matmul_<MATH_FIDELITY>(0, params->CT_DIM, params->RT_DIM);
    }

    counters.stop();

    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "counters.h"
#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    llk_perf::PerfCounters counters;
    counters.add(llk_perf::CounterBank::INSTRN_THREAD, llk_perf::CounterId::InstrnThread::INST_PACK);
    counters.add(llk_perf::CounterBank::INSTRN_THREAD, llk_perf::CounterId::InstrnThread::INST_CFG);
    counters.add(llk_perf::CounterBank::FPU, llk_perf::CounterId::Fpu::SFPU_OP_VALID);
    counters.add(llk_perf::CounterBank::FPU, llk_perf::CounterId::Fpu::FPU_OP_VALID);
    counters.add(llk_perf::CounterBank::TDMA_PACK, llk_perf::CounterId::TdmaPack::PACK_BUSY_10);
    counters.add(llk_perf::CounterBank::TDMA_PACK, llk_perf::CounterId::TdmaPack::PACK_BUSY_11);
    counters.add(llk_perf::CounterBank::TDMA_UNPACK, llk_perf::CounterId::TdmaUnpack::UNPACK_BUSY_0);
    counters.add(llk_perf::CounterBank::L1, llk_perf::CounterId::L1::NOC_RING0_OUTGOING_1, 0);
    counters.start();

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(formats.pack_src, formats.pack_dst, TILE_SIZE_PACK);
    _llk_pack_init_<false, false, false>(formats.pack_dst);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, TILE_SIZE_PACK);
    _llk_pack_init_<false, false>(formats.pack_dst);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>();
#endif
    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < params->TILE_CNT; i++)
    {
        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(i, L1_ADDRESS(buffer_Res[i]));
    }

    counters.stop();

    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif
