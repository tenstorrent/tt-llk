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

#include "counters.h"
#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel()
{
    _llk_unpack_AB_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst);
    _llk_unpack_AB_init_<>();
    llk_perf::PerfCounters counters;
    counters.add(llk_perf::CounterBank::INSTRN_THREAD, llk_perf::CounterId::InstrnThread::INST_UNPACK);
    counters.add(llk_perf::CounterBank::FPU, llk_perf::CounterId::Fpu::FPU_OP_VALID);
    counters.add(llk_perf::CounterBank::TDMA_UNPACK, llk_perf::CounterId::TdmaUnpack::UNPACK_BUSY_0);
    counters.add(llk_perf::CounterBank::L1, llk_perf::CounterId::L1::L1_ARB_TDMA_BUNDLE_0, 0);
    counters.add(llk_perf::CounterBank::TDMA_PACK, llk_perf::CounterId::TdmaPack::PACK_BUSY_10);
    counters.set_mode(llk_perf::CounterMode::GRANTS);
    counters.start();
    for (int i = 0; i < TILE_CNT; i++)
    {
        _llk_unpack_AB_<>(L1_ADDRESS(buffer_A[i]), L1_ADDRESS(buffer_B[i]));
    }
    counters.stop();
}

#endif

#ifdef LLK_TRISC_MATH

#include "counters.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "params.h"

void run_kernel()
{
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false, false>(formats.math, formats.math);
    _llk_math_eltwise_binary_init_<ELTWISE_BINARY_OP, BroadcastType::NONE, MATH_FIDELITY>(4, 0, 0);

    llk_perf::PerfCounters counters;
    counters.add(llk_perf::CounterBank::INSTRN_THREAD, llk_perf::CounterId::InstrnThread::INST_MATH);
    counters.add(llk_perf::CounterBank::FPU, llk_perf::CounterId::Fpu::FPU_OP_VALID);
    counters.add(llk_perf::CounterBank::TDMA_UNPACK, llk_perf::CounterId::TdmaUnpack::MATH_INSTR_VALID);
    counters.add(llk_perf::CounterBank::L1, llk_perf::CounterId::L1::NOC_RING0_OUTGOING_0, 0);
    counters.add(llk_perf::CounterBank::TDMA_PACK, llk_perf::CounterId::TdmaPack::PACK_BUSY_10);
    counters.set_mode(llk_perf::CounterMode::GRANTS);
    counters.start();
    for (int i = 0; i < TILE_CNT; i++)
    {
        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
        _llk_math_eltwise_binary_<
            ELTWISE_BINARY_OP,
            BroadcastType::NONE,
            DstSync::SyncHalf,
            is_fp32_dest_acc_en,
            MATH_FIDELITY,
            EltwiseBinaryReuseDestType::NONE>(4, 0, false);
        _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }
    counters.stop();
}

#endif

#ifdef LLK_TRISC_PACK

#include "counters.h"
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

    llk_perf::PerfCounters counters;
    counters.add(llk_perf::CounterBank::INSTRN_THREAD, llk_perf::CounterId::InstrnThread::INST_PACK);
    counters.add(llk_perf::CounterBank::FPU, llk_perf::CounterId::Fpu::FPU_OP_VALID);
    counters.add(llk_perf::CounterBank::TDMA_UNPACK, llk_perf::CounterId::TdmaUnpack::MATH_INSTR_VALID);
    counters.add(llk_perf::CounterBank::L1, llk_perf::CounterId::L1::NOC_RING0_OUTGOING_0, 0);
    counters.add(llk_perf::CounterBank::TDMA_PACK, llk_perf::CounterId::TdmaPack::PACK_NOT_DEST_STALL);
    counters.set_mode(llk_perf::CounterMode::GRANTS);
    counters.start();
    for (int i = 0; i < TILE_CNT; i++)
    {
        _llk_packer_wait_for_math_done_();
        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(0, L1_ADDRESS(buffer_Res[i]));
        _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }
    counters.stop();
}

#endif
