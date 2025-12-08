// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel.h"

namespace llk_perf
{
#define RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0 (RISCV_DEBUG_REGS_START_ADDR | 0xC)
#define RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK1 (RISCV_DEBUG_REGS_START_ADDR | 0x10)
#define RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2 (RISCV_DEBUG_REGS_START_ADDR | 0x14)

#define RISCV_DEBUG_REG_PERF_CNT_OUT_L_INSTRN_THREAD (RISCV_DEBUG_REGS_START_ADDR | 0x100)
#define RISCV_DEBUG_REG_PERF_CNT_OUT_H_INSTRN_THREAD (RISCV_DEBUG_REGS_START_ADDR | 0x104)
#define RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_UNPACK   (RISCV_DEBUG_REGS_START_ADDR | 0x108)
#define RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_UNPACK   (RISCV_DEBUG_REGS_START_ADDR | 0x10C)
#define RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_PACK     (RISCV_DEBUG_REGS_START_ADDR | 0x110)
#define RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_PACK     (RISCV_DEBUG_REGS_START_ADDR | 0x114)
#define RISCV_DEBUG_REG_PERF_CNT_OUT_L_DBG_L1        (RISCV_DEBUG_REGS_START_ADDR | 0x118)
#define RISCV_DEBUG_REG_PERF_CNT_OUT_H_DBG_L1        (RISCV_DEBUG_REGS_START_ADDR | 0x11C)
#define RISCV_DEBUG_REG_PERF_CNT_OUT_L_FPU           (RISCV_DEBUG_REGS_START_ADDR | 0x120)
#define RISCV_DEBUG_REG_PERF_CNT_OUT_H_FPU           (RISCV_DEBUG_REGS_START_ADDR | 0x124)

namespace counters
{
namespace fpu
{
constexpr uint32_t FPU_OP_VALID  = 0;
constexpr uint32_t SFPU_OP_VALID = 1;
} // namespace fpu

namespace instrn_thread
{

constexpr uint32_t INST_CFG     = 0;
constexpr uint32_t INST_SYNC    = 1;
constexpr uint32_t INST_THCON   = 2;
constexpr uint32_t INST_XSEARCH = 3;
constexpr uint32_t INST_MOVE    = 4;
constexpr uint32_t INST_MATH    = 5;
constexpr uint32_t INST_UNPACK  = 6;
constexpr uint32_t INST_PACK    = 7;
constexpr uint32_t STALLED      = 8;

constexpr uint32_t SRCA_CLEARED_0 = 9;
constexpr uint32_t SRCA_CLEARED_1 = 10;
constexpr uint32_t SRCA_CLEARED_2 = 11;

constexpr uint32_t SRCB_CLEARED_0 = 12;
constexpr uint32_t SRCB_CLEARED_1 = 13;
constexpr uint32_t SRCB_CLEARED_2 = 14;

constexpr uint32_t SRCA_VALID_0 = 15;
constexpr uint32_t SRCA_VALID_1 = 16;
constexpr uint32_t SRCA_VALID_2 = 17;

constexpr uint32_t SRCB_VALID_0 = 18;
constexpr uint32_t SRCB_VALID_1 = 19;
constexpr uint32_t SRCB_VALID_2 = 20;

constexpr uint32_t STALL_THCON            = 21;
constexpr uint32_t STALL_PACK0            = 22;
constexpr uint32_t STALL_MATH             = 23;
constexpr uint32_t STALL_SEM_ZERO         = 24;
constexpr uint32_t STALL_SEM_MAX          = 25;
constexpr uint32_t STALL_MOVE             = 26;
constexpr uint32_t STALL_TRISC_REG_ACCESS = 27;
constexpr uint32_t STALL_SFPU             = 28;
} // namespace instrn_thread

namespace tdma_unpack
{

constexpr uint32_t MATH_INSTR_SRC_READY = 0;
constexpr uint32_t MATH_NOT_D2A_STALL   = 1;
constexpr uint32_t MATH_FIDELITY_PHASES = 2;
constexpr uint32_t MATH_INSTR_BUF_RDEN  = 3;
constexpr uint32_t MATH_INSTR_VALID     = 4;
constexpr uint32_t TDMA_SRCB_REGIF_WREN = 5;
constexpr uint32_t TDMA_SRCA_REGIF_WREN = 6;
constexpr uint32_t UNPACK_BUSY_0        = 7;
constexpr uint32_t UNPACK_BUSY_1        = 8;
constexpr uint32_t UNPACK_BUSY_2        = 9;
constexpr uint32_t UNPACK_BUSY_3        = 10;

} // namespace tdma_unpack

namespace tdma_pack
{

constexpr uint32_t DSTAC_RDEN_RAW_0    = 0;
constexpr uint32_t DSTAC_RDEN_RAW_1    = 1;
constexpr uint32_t DSTAC_RDEN_RAW_2    = 2;
constexpr uint32_t DSTAC_RDEN_RAW_3    = 3;
constexpr uint32_t PACK_NOT_DEST_STALL = 4;
constexpr uint32_t PACK_NOT_SB_STALL   = 5;
constexpr uint32_t PACK_BUSY_10        = 6;
constexpr uint32_t PACK_BUSY_11        = 7;

} // namespace tdma_pack

namespace l1
{

constexpr uint32_t UNPACK_NO_ARB        = 7;
constexpr uint32_t UNPACK_ARB_1         = 6;
constexpr uint32_t TDMA_BUNDLE_0_ARB    = 5;
constexpr uint32_t TDMA_BUNDLE_1_ARB    = 4;
constexpr uint32_t NOC_RING0_OUTGOING_0 = 3;
constexpr uint32_t NOC_RING0_OUTGOING_1 = 2;
constexpr uint32_t NOC_RING0_INCOMING_0 = 1;
constexpr uint32_t NOC_RING0_INCOMING_1 = 0;

constexpr uint32_t TDMA_PACKER_2_WR     = 7;
constexpr uint32_t TDMA_EXT_UNPACK_9    = 6;
constexpr uint32_t TDMA_EXT_UNPACK_10   = 5;
constexpr uint32_t TDMA_EXT_UNPACK_11   = 4;
constexpr uint32_t NOC_RING1_OUTGOING_0 = 3;
constexpr uint32_t NOC_RING1_OUTGOING_1 = 2;
constexpr uint32_t NOC_RING1_INCOMING_0 = 1;
constexpr uint32_t NOC_RING1_INCOMING_1 = 0;
} // namespace l1

} // namespace counters

#define PERF_ITERATION_ADDR      0x2F7FC
#define PERF_COUNTER_DATA_ADDR   0x2F800
#define PERF_COUNTER_TOTAL_WORDS 540

inline void set_profiling_iteration(uint32_t iteration)
{
    volatile uint32_t* iter_ptr = reinterpret_cast<volatile uint32_t*>(PERF_ITERATION_ADDR);
    *iter_ptr                   = iteration;
}

inline uint32_t get_profiling_iteration()
{
    volatile uint32_t* iter_ptr = reinterpret_cast<volatile uint32_t*>(PERF_ITERATION_ADDR);
    return *iter_ptr;
}

inline void init_profiling()
{
    set_profiling_iteration(0);

    volatile uint32_t* l1_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_DATA_ADDR);
    for (uint32_t i = 0; i < PERF_COUNTER_TOTAL_WORDS; i++)
    {
        l1_mem[i] = 0;
    }
}

struct PerfCounterDef
{
    uint32_t reg_base;
    uint32_t counter_sel;
};

inline void start_profiling()
{
    volatile uint32_t* dbg_regs = reinterpret_cast<volatile uint32_t*>(RISCV_DEBUG_REGS_START_ADDR);
    uint32_t iteration          = get_profiling_iteration();

#ifdef LLK_TRISC_UNPACK

    const uint32_t counters_per_iteration = 5;
    uint32_t base_counter                 = iteration * counters_per_iteration;

    const PerfCounterDef unpack_counters[] = {

        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_CFG},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_INSTR_SRC_READY},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::UNPACK_NO_ARB},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_0},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_SYNC},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_NOT_D2A_STALL},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::UNPACK_ARB_1},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_1},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_THCON},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_FIDELITY_PHASES},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_BUNDLE_0_ARB},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_2},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_XSEARCH},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_INSTR_BUF_RDEN},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_BUNDLE_1_ARB},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_3},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_MOVE},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_INSTR_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING0_OUTGOING_0},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_NOT_DEST_STALL},

        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_MATH},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::TDMA_SRCB_REGIF_WREN},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING0_OUTGOING_1},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_NOT_SB_STALL},

        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_UNPACK},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::TDMA_SRCA_REGIF_WREN},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING0_INCOMING_0},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_BUSY_10},

        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_PACK},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::UNPACK_BUSY_0},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING0_INCOMING_1},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_BUSY_11},

        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::STALLED},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::UNPACK_BUSY_1},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_PACKER_2_WR},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_0},

        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCA_CLEARED_0},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::UNPACK_BUSY_2},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_EXT_UNPACK_9},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_1},

        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCA_CLEARED_1},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::UNPACK_BUSY_3},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_EXT_UNPACK_10},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_2},

        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCA_CLEARED_2},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_INSTR_SRC_READY},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_EXT_UNPACK_11},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_3},

        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCB_CLEARED_0},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_NOT_D2A_STALL},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING1_OUTGOING_0},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_NOT_DEST_STALL},

        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCB_CLEARED_1},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_FIDELITY_PHASES},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING1_OUTGOING_1},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_NOT_SB_STALL},
    };
    const uint32_t num_unpack_counters = 70;

    for (uint32_t i = 0; i < 5; i++)
    {
        uint32_t counter_idx          = (base_counter + i) % num_unpack_counters;
        const PerfCounterDef& counter = unpack_counters[counter_idx];

        dbg_regs[(counter.reg_base - RISCV_DEBUG_REGS_START_ADDR) / 4]     = 0xFFFFFFFF;
        dbg_regs[(counter.reg_base + 4 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counter.counter_sel << 8) | (1 << 16);
        dbg_regs[(counter.reg_base + 8 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;
    }
#endif

#ifdef LLK_TRISC_MATH

    const uint32_t counters_per_iteration = 5;
    uint32_t base_counter                 = iteration * counters_per_iteration;

    const PerfCounterDef math_counters[] = {

        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_CFG},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_INSTR_SRC_READY},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::UNPACK_NO_ARB},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_0},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_SYNC},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_NOT_D2A_STALL},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::UNPACK_ARB_1},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_1},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_THCON},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_FIDELITY_PHASES},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_BUNDLE_0_ARB},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_2},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_XSEARCH},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_INSTR_BUF_RDEN},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_BUNDLE_1_ARB},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_3},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_MOVE},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_INSTR_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING0_OUTGOING_0},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_NOT_DEST_STALL},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_MATH},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::TDMA_SRCB_REGIF_WREN},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING0_OUTGOING_1},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_NOT_SB_STALL},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_UNPACK},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::TDMA_SRCA_REGIF_WREN},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING0_INCOMING_0},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_BUSY_10},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_PACK},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::UNPACK_BUSY_0},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING0_INCOMING_1},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_BUSY_11},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::STALLED},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::UNPACK_BUSY_1},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_PACKER_2_WR},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_0},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCA_CLEARED_0},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::UNPACK_BUSY_2},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_EXT_UNPACK_9},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_1},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCA_CLEARED_1},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::UNPACK_BUSY_3},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_EXT_UNPACK_10},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_2},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCA_CLEARED_2},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_INSTR_SRC_READY},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_EXT_UNPACK_11},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_3},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCB_CLEARED_0},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_NOT_D2A_STALL},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING1_OUTGOING_0},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_NOT_DEST_STALL},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCB_CLEARED_1},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_FIDELITY_PHASES},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING1_OUTGOING_1},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_NOT_SB_STALL},
    };
    const uint32_t num_math_counters = 70;

    for (uint32_t i = 0; i < 5; i++)
    {
        uint32_t counter_idx          = (base_counter + i) % num_math_counters;
        const PerfCounterDef& counter = math_counters[counter_idx];

        dbg_regs[(counter.reg_base - RISCV_DEBUG_REGS_START_ADDR) / 4]     = 0xFFFFFFFF;
        dbg_regs[(counter.reg_base + 4 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counter.counter_sel << 8) | (1 << 16);
        dbg_regs[(counter.reg_base + 8 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;
    }
#endif

#ifdef LLK_TRISC_PACK

    const uint32_t counters_per_iteration = 5;
    uint32_t base_counter                 = iteration * counters_per_iteration;

    const PerfCounterDef pack_counters[] = {

        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_CFG},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_INSTR_SRC_READY},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::UNPACK_NO_ARB},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_0},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_SYNC},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_NOT_D2A_STALL},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::UNPACK_ARB_1},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_1},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_THCON},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_FIDELITY_PHASES},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_BUNDLE_0_ARB},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_2},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_XSEARCH},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_INSTR_BUF_RDEN},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_BUNDLE_1_ARB},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_3},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_MOVE},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_INSTR_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING0_OUTGOING_0},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_NOT_DEST_STALL},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_MATH},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::TDMA_SRCB_REGIF_WREN},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING0_OUTGOING_1},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_NOT_SB_STALL},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_UNPACK},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::TDMA_SRCA_REGIF_WREN},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING0_INCOMING_0},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_BUSY_10},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::INST_PACK},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::UNPACK_BUSY_0},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING0_INCOMING_1},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_BUSY_11},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::STALLED},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::UNPACK_BUSY_1},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_PACKER_2_WR},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_0},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCA_CLEARED_0},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::UNPACK_BUSY_2},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_EXT_UNPACK_9},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_1},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCA_CLEARED_1},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::UNPACK_BUSY_3},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_EXT_UNPACK_10},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_2},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCA_CLEARED_2},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_INSTR_SRC_READY},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::TDMA_EXT_UNPACK_11},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::DSTAC_RDEN_RAW_3},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCB_CLEARED_0},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::FPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_NOT_D2A_STALL},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING1_OUTGOING_0},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_NOT_DEST_STALL},
        {RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, counters::instrn_thread::SRCB_CLEARED_1},
        {RISCV_DEBUG_REG_PERF_CNT_FPU0, counters::fpu::SFPU_OP_VALID},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0, counters::tdma_unpack::MATH_FIDELITY_PHASES},
        {RISCV_DEBUG_REG_PERF_CNT_L1_0, counters::l1::NOC_RING1_OUTGOING_1},
        {RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, counters::tdma_pack::PACK_NOT_SB_STALL},
    };
    const uint32_t num_pack_counters = 70;

    for (uint32_t i = 0; i < 5; i++)
    {
        uint32_t counter_idx          = (base_counter + i) % num_pack_counters;
        const PerfCounterDef& counter = pack_counters[counter_idx];

        dbg_regs[(counter.reg_base - RISCV_DEBUG_REGS_START_ADDR) / 4]     = 0xFFFFFFFF;
        dbg_regs[(counter.reg_base + 4 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counter.counter_sel << 8) | (1 << 16);
        dbg_regs[(counter.reg_base + 8 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;
    }
#endif
}

inline void stop_profiling()
{
    volatile uint32_t* dbg_regs = reinterpret_cast<volatile uint32_t*>(RISCV_DEBUG_REGS_START_ADDR);

#ifdef LLK_TRISC_UNPACK

    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU2 - RISCV_DEBUG_REGS_START_ADDR) / 4]           = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4]   = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_2 - RISCV_DEBUG_REGS_START_ADDR) / 4]           = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4]     = 0;

    volatile uint32_t* l1_control = reinterpret_cast<volatile uint32_t*>(0x2F7FC);
    uint32_t iteration            = l1_control[0];
    uint32_t base_offset          = 0 + (iteration * 10);

    volatile uint32_t* l1_mem = reinterpret_cast<volatile uint32_t*>(0x2F800);
    uint32_t cycles, count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_INSTRN_THREAD - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_INSTRN_THREAD - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 0] = cycles;
    l1_mem[base_offset + 1] = count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_FPU - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_FPU - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 2] = cycles;
    l1_mem[base_offset + 3] = count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_UNPACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_UNPACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 4] = cycles;
    l1_mem[base_offset + 5] = count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_DBG_L1 - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_DBG_L1 - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 6] = cycles;
    l1_mem[base_offset + 7] = count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_PACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_PACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 8] = cycles;
    l1_mem[base_offset + 9] = count;
#endif

#ifdef LLK_TRISC_MATH

    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU2 - RISCV_DEBUG_REGS_START_ADDR) / 4]           = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4]   = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_2 - RISCV_DEBUG_REGS_START_ADDR) / 4]           = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4]     = 0;

    volatile uint32_t* l1_control = reinterpret_cast<volatile uint32_t*>(0x2F7FC);
    uint32_t iteration            = l1_control[0];
    uint32_t base_offset          = 200 + (iteration * 10);

    volatile uint32_t* l1_mem = reinterpret_cast<volatile uint32_t*>(0x2F800);
    uint32_t cycles, count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_INSTRN_THREAD - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_INSTRN_THREAD - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 0] = cycles;
    l1_mem[base_offset + 1] = count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_FPU - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_FPU - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 2] = cycles;
    l1_mem[base_offset + 3] = count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_UNPACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_UNPACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 4] = cycles;
    l1_mem[base_offset + 5] = count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_DBG_L1 - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_DBG_L1 - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 6] = cycles;
    l1_mem[base_offset + 7] = count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_PACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_PACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 8] = cycles;
    l1_mem[base_offset + 9] = count;
#endif

#ifdef LLK_TRISC_PACK

    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU2 - RISCV_DEBUG_REGS_START_ADDR) / 4]           = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4]     = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_2 - RISCV_DEBUG_REGS_START_ADDR) / 4]           = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4]   = 0;

    volatile uint32_t* l1_control = reinterpret_cast<volatile uint32_t*>(0x2F7FC);
    uint32_t iteration            = l1_control[0];
    uint32_t base_offset          = 400 + (iteration * 10);

    volatile uint32_t* l1_mem = reinterpret_cast<volatile uint32_t*>(0x2F800);
    uint32_t cycles, count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_INSTRN_THREAD - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_INSTRN_THREAD - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 0] = cycles;
    l1_mem[base_offset + 1] = count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_FPU - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_FPU - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 2] = cycles;
    l1_mem[base_offset + 3] = count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_PACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_PACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 4] = cycles;
    l1_mem[base_offset + 5] = count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_DBG_L1 - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_DBG_L1 - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 6] = cycles;
    l1_mem[base_offset + 7] = count;

    cycles                  = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_UNPACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count                   = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_UNPACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[base_offset + 8] = cycles;
    l1_mem[base_offset + 9] = count;
#endif
}

} // namespace llk_perf
