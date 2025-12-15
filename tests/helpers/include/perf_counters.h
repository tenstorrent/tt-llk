// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel.h"

/**
 * Performance Counter API
 *
 * Provides a simple interface for measuring up to 5 hardware performance counters.
 * Configuration is written by Python, and C++ code starts/stops counting.
 *
 * Usage from Python:
 *   from helpers.perf_counters import PerfCounterConfig, CounterBank
 *
 *   config = PerfCounterConfig()
 *   config.add_counter(CounterBank.FPU, "FPU_OP_VALID")
 *   config.add_counter(CounterBank.INSTRN_THREAD, "INST_UNPACK")
 *   config.set_mode("grants")  # or "requests"
 *
 *   write_perf_config(config)
 *
 * Usage from C++:
 *   #include "perf_counters.h"
 *
 *   llk_perf::start_perf_counters();
 *   // ... run workload ...
 *   llk_perf::stop_perf_counters();
 */

namespace llk_perf
{

// Note: Most counter base register addresses are defined in hw_specific/<arch>/inc/tensix.h
// TDMA_UNPACK registers are missing from hardware headers, so we define them here
#ifndef RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0
#define RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0 (RISCV_DEBUG_REGS_START_ADDR | 0x00C)
#define RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK1 (RISCV_DEBUG_REGS_START_ADDR | 0x010)
#define RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2 (RISCV_DEBUG_REGS_START_ADDR | 0x014)
#endif

// L1 counter MUX control register - bit 4 selects which L1 counter set is active
#ifndef RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL
#define RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL (RISCV_DEBUG_REGS_START_ADDR | 0x218)
#endif

// Performance counter output registers (for reading cycle/count results)
// NOTE: Table shows TDMA_UNPACK outputs at 0x018/0x01C but empirically 0x108/0x10C works
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

// L1 Memory layout
#define PERF_COUNTER_CONFIG_ADDR 0x2F7F0 // 5 words: configuration
#define PERF_COUNTER_DATA_ADDR   0x2F800 // 10 words: output (5 counters × 2 words)

// Configuration word format: [mode_bit(16), counter_sel(8-15), bank_id(0-7)]
enum class CounterBank : uint32_t
{
    INSTRN_THREAD = 0,
    FPU           = 1,
    TDMA_UNPACK   = 2,
    L1            = 3,
    TDMA_PACK     = 4,
};

// Map bank enum to hardware register base addresses
inline constexpr uint32_t get_counter_base_addr(CounterBank bank)
{
    switch (bank)
    {
        case CounterBank::INSTRN_THREAD:
            return RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0;
        case CounterBank::FPU:
            return RISCV_DEBUG_REG_PERF_CNT_FPU0;
        case CounterBank::TDMA_UNPACK:
            return RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0;
        case CounterBank::L1:
            return RISCV_DEBUG_REG_PERF_CNT_L1_0;
        case CounterBank::TDMA_PACK:
            return RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0;
        default:
            return 0;
    }
}

// Map bank enum to output register addresses (for reading cycle/count)
inline constexpr uint32_t get_counter_output_low_addr(CounterBank bank)
{
    switch (bank)
    {
        case CounterBank::INSTRN_THREAD:
            return RISCV_DEBUG_REG_PERF_CNT_OUT_L_INSTRN_THREAD;
        case CounterBank::FPU:
            return RISCV_DEBUG_REG_PERF_CNT_OUT_L_FPU;
        case CounterBank::TDMA_UNPACK:
            return RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_UNPACK;
        case CounterBank::L1:
            return RISCV_DEBUG_REG_PERF_CNT_OUT_L_DBG_L1;
        case CounterBank::TDMA_PACK:
            return RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_PACK;
        default:
            return 0;
    }
}

inline constexpr uint32_t get_counter_output_high_addr(CounterBank bank)
{
    switch (bank)
    {
        case CounterBank::INSTRN_THREAD:
            return RISCV_DEBUG_REG_PERF_CNT_OUT_H_INSTRN_THREAD;
        case CounterBank::FPU:
            return RISCV_DEBUG_REG_PERF_CNT_OUT_H_FPU;
        case CounterBank::TDMA_UNPACK:
            return RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_UNPACK;
        case CounterBank::L1:
            return RISCV_DEBUG_REG_PERF_CNT_OUT_H_DBG_L1;
        case CounterBank::TDMA_PACK:
            return RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_PACK;
        default:
            return 0;
    }
}

/**
 * Initialize performance counters - clear all configuration and data
 */
inline void init_perf_counters()
{
    // Clear configuration
    volatile uint32_t* config_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_CONFIG_ADDR);
    for (uint32_t i = 0; i < 5; i++)
    {
        config_mem[i] = 0;
    }

    // Clear output data
    volatile uint32_t* data_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_DATA_ADDR);
    for (uint32_t i = 0; i < 10; i++)
    {
        data_mem[i] = 0;
    }
}

/**
 * Start performance counters based on configuration written by Python.
 *
 * Reads configuration from L1 memory and programs hardware registers accordingly.
 * Each of the 3 TRISC cores (UNPACK, MATH, PACK) calls this independently.
 */
inline void start_perf_counters()
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
    volatile uint32_t* dbg_regs   = reinterpret_cast<volatile uint32_t*>(RISCV_DEBUG_REGS_START_ADDR);
    volatile uint32_t* config_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_CONFIG_ADDR);

    // Program each counter
    for (uint32_t i = 0; i < 5; i++)
    {
        uint32_t config = config_mem[i];
        if (config == 0)
        {
            continue; // Unused counter slot
        }

        // Decode configuration: [mux_ctrl_bit4(17), mode_bit(16), counter_sel(8-15), bank_id(0-7)]
        uint32_t bank_id       = config & 0xFF;
        uint32_t counter_sel   = (config >> 8) & 0xFF;
        uint32_t mode_bit      = (config >> 16) & 0x1;
        uint32_t mux_ctrl_bit4 = (config >> 17) & 0x1;

        CounterBank bank = static_cast<CounterBank>(bank_id);

        // If L1 counter, configure MUX_CTRL register bit 4
        if (bank == CounterBank::L1)
        {
            uint32_t mux_ctrl_addr  = (RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL - RISCV_DEBUG_REGS_START_ADDR) / 4;
            dbg_regs[mux_ctrl_addr] = (mux_ctrl_bit4 << 4);
        }

        uint32_t counter_base     = get_counter_base_addr(bank);
        uint32_t counter_reg_addr = (counter_base - RISCV_DEBUG_REGS_START_ADDR) / 4;

        // Program counter: reset, configure mode/sel, start
        dbg_regs[counter_reg_addr]     = 0xFFFFFFFF;                            // Reset counter
        dbg_regs[counter_reg_addr + 1] = (counter_sel << 8) | (mode_bit << 16); // Mode register
        dbg_regs[counter_reg_addr + 2] = 1;                                     // Start counting
    }
#pragma GCC diagnostic pop
}

/**
 * Stop performance counters and save results to L1 memory.
 *
 * Reads cycle/count outputs from hardware registers and writes to L1
 * for Python to collect later.
 */
inline void stop_perf_counters()
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
    volatile uint32_t* dbg_regs   = reinterpret_cast<volatile uint32_t*>(RISCV_DEBUG_REGS_START_ADDR);
    volatile uint32_t* config_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_CONFIG_ADDR);
    volatile uint32_t* data_mem   = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_DATA_ADDR);

    // Stop all configured counters and read their outputs
    for (uint32_t i = 0; i < 5; i++)
    {
        uint32_t config = config_mem[i];
        if (config == 0)
        {
            continue; // Unused counter slot
        }

        // Decode bank
        uint32_t bank_id = config & 0xFF;
        CounterBank bank = static_cast<CounterBank>(bank_id);

        // Stop counter
        uint32_t counter_base          = get_counter_base_addr(bank);
        uint32_t counter_reg_addr      = (counter_base - RISCV_DEBUG_REGS_START_ADDR) / 4;
        dbg_regs[counter_reg_addr + 2] = 0; // Stop bit

        // Read outputs
        uint32_t output_low_addr  = (get_counter_output_low_addr(bank) - RISCV_DEBUG_REGS_START_ADDR) / 4;
        uint32_t output_high_addr = (get_counter_output_high_addr(bank) - RISCV_DEBUG_REGS_START_ADDR) / 4;

        uint32_t cycles = dbg_regs[output_low_addr];
        uint32_t count  = dbg_regs[output_high_addr];

        // Write to L1 memory (2 words per counter: cycles, count)
        data_mem[i * 2]     = cycles;
        data_mem[i * 2 + 1] = count;
    }
#pragma GCC diagnostic pop
}

} // namespace llk_perf
