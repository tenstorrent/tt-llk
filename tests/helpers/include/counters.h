// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstdint>

#include "ckernel.h"
#include "ckernel_mutex_guard.h"

namespace llk_perf
{

// ============================================================================
// Hardware Register Addresses
// ============================================================================

// TDMA_UNPACK registers (missing from hw headers)
#ifndef RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0
#define RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0 (RISCV_DEBUG_REGS_START_ADDR | 0x00C)
#define RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK1 (RISCV_DEBUG_REGS_START_ADDR | 0x010)
#define RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2 (RISCV_DEBUG_REGS_START_ADDR | 0x014)
#endif

// L1 counter MUX control register
#ifndef RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL
#define RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL (RISCV_DEBUG_REGS_START_ADDR | 0x218)
#endif

// Performance counter output registers
// OUT_L: Reference cycle count (independent of event)
// OUT_H: Event-specific count (depends on selected counter)
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

// ============================================================================
// L1 Memory Layout (Single Shared Buffer)
// ============================================================================

// SOURCE OF TRUTH: tests/python_tests/helpers/test_config.py (TestConfig class)
// Must be below profiler buffers which start at 0x16B000
#define PERF_COUNTERS_BASE_ADDR    0x16A000
#define PERF_COUNTERS_CONFIG_WORDS 86  // Counter configuration slots
#define PERF_COUNTERS_DATA_WORDS   172 // Counter data (cycles + count per slot)
#define PERF_COUNTERS_BUFFER_SIZE  ((PERF_COUNTERS_CONFIG_WORDS + PERF_COUNTERS_DATA_WORDS) * 4)

#define PERF_COUNTERS_CONFIG_ADDR    (PERF_COUNTERS_BASE_ADDR)
#define PERF_COUNTERS_DATA_ADDR      (PERF_COUNTERS_BASE_ADDR + PERF_COUNTERS_CONFIG_WORDS * 4)
#define PERF_COUNTERS_SYNC_CTRL_ADDR (PERF_COUNTERS_BASE_ADDR + PERF_COUNTERS_BUFFER_SIZE)

// ============================================================================
// Sync Control Word Bit Layout
// ============================================================================

// Bit 0: UNPACK thread started flag
// Bit 1: MATH thread started flag
// Bit 2: PACK thread started flag
// Bit 3: UNPACK thread stopped flag
// Bit 4: MATH thread stopped flag
// Bit 5: PACK thread stopped flag
// Bit 6: Global started flag (at least one thread started)
// Bit 7: Global stopped flag (all threads stopped)
// Bits 9-10: Last stopper thread ID (0=UNPACK, 1=MATH, 2=PACK)
// Bits 8,11-31: Reserved

constexpr std::uint32_t SYNC_START_MASK         = (1u << 0) | (1u << 1) | (1u << 2);
constexpr std::uint32_t SYNC_STOP_MASK          = SYNC_START_MASK << 3;
constexpr std::uint32_t SYNC_STARTED_FLAG       = 1u << 6;
constexpr std::uint32_t SYNC_STOPPED_FLAG       = 1u << 7;
constexpr std::uint32_t SYNC_LAST_STOPPER_SHIFT = 9u;
constexpr std::uint32_t SYNC_LAST_STOPPER_MASK  = 0x3u << SYNC_LAST_STOPPER_SHIFT;

// ============================================================================
// Counter Bank Enumeration
// ============================================================================

enum class counter_bank : std::uint8_t
{
    instrn_thread = 0,
    fpu           = 1,
    tdma_unpack   = 2,
    l1            = 3,
    tdma_pack     = 4,
};

constexpr std::uint32_t COUNTER_BANK_COUNT = 5;
constexpr std::uint32_t COUNTER_SLOT_COUNT = 86;

// ============================================================================
// Helper Functions
// ============================================================================

// Hardware register access functions for performance counter control
namespace hw_access
{
// Write a 32-bit value to a hardware register address
inline void write_reg(std::uint32_t addr, std::uint32_t value)
{
    *reinterpret_cast<volatile std::uint32_t*>(addr) = value;
}

// Read a 32-bit value from a hardware register address
inline std::uint32_t read_reg(std::uint32_t addr)
{
    return *reinterpret_cast<volatile std::uint32_t*>(addr);
}

// Get the base configuration register address for a counter bank
// Used to configure and control counter operation (period, mode, start/stop)
inline std::uint32_t get_counter_base_addr(counter_bank bank)
{
    constexpr std::uint32_t base_addrs[COUNTER_BANK_COUNT] = {
        RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0,
        RISCV_DEBUG_REG_PERF_CNT_FPU0,
        RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0,
        RISCV_DEBUG_REG_PERF_CNT_L1_0,
        RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0,
    };
    const std::uint32_t idx = static_cast<std::uint32_t>(bank);
    return idx < COUNTER_BANK_COUNT ? base_addrs[idx] : 0u;
}

// Get the output register address for cycle counts (reference counter)
// This counter runs continuously regardless of events
inline std::uint32_t get_counter_output_low_addr(counter_bank bank)
{
    constexpr std::uint32_t low_addrs[COUNTER_BANK_COUNT] = {
        RISCV_DEBUG_REG_PERF_CNT_OUT_L_INSTRN_THREAD,
        RISCV_DEBUG_REG_PERF_CNT_OUT_L_FPU,
        RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_UNPACK,
        RISCV_DEBUG_REG_PERF_CNT_OUT_L_DBG_L1,
        RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_PACK,
    };
    const std::uint32_t idx = static_cast<std::uint32_t>(bank);
    return idx < COUNTER_BANK_COUNT ? low_addrs[idx] : 0u;
}

// Get the output register address for event counts
// This counter increments based on the selected counter/event type
inline std::uint32_t get_counter_output_high_addr(counter_bank bank)
{
    constexpr std::uint32_t high_addrs[COUNTER_BANK_COUNT] = {
        RISCV_DEBUG_REG_PERF_CNT_OUT_H_INSTRN_THREAD,
        RISCV_DEBUG_REG_PERF_CNT_OUT_H_FPU,
        RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_UNPACK,
        RISCV_DEBUG_REG_PERF_CNT_OUT_H_DBG_L1,
        RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_PACK,
    };
    const std::uint32_t idx = static_cast<std::uint32_t>(bank);
    return idx < COUNTER_BANK_COUNT ? high_addrs[idx] : 0u;
}
} // namespace hw_access

// Thread identification and sync bit helpers
// Each TRISC thread (Unpack/Math/Pack) needs to identify itself for synchronization
namespace thread_info
{
// Get the current thread ID based on compile-time defines
// Returns: 0 (UNPACK), 1 (MATH), 2 (PACK)
constexpr std::uint32_t get_thread_id()
{
#if defined(LLK_TRISC_UNPACK)
    return 0u;
#elif defined(LLK_TRISC_MATH)
    return 1u;
#elif defined(LLK_TRISC_PACK)
    return 2u;
#else
    return 1u;
#endif
}

// Get the bit mask for this thread's start flag in sync control word
// Returns: bit 0 (UNPACK), bit 1 (MATH), or bit 2 (PACK)
constexpr std::uint32_t get_thread_start_bit()
{
    return 1u << get_thread_id();
}

// Get the bit mask for this thread's stop flag in sync control word
// Returns: bit 3 (UNPACK), bit 4 (MATH), or bit 5 (PACK)
constexpr std::uint32_t get_thread_stop_bit()
{
    return get_thread_start_bit() << 3;
}
} // namespace thread_info

// ============================================================================
// Performance Counter Manager (Singleton)
// ============================================================================

class PerfCounterManager
{
private:
    PerfCounterManager() = default;

    // Get pointer to L1 config buffer (86 words of counter metadata)
    volatile std::uint32_t* get_config_mem()
    {
        return reinterpret_cast<volatile std::uint32_t*>(PERF_COUNTERS_CONFIG_ADDR);
    }

    // Get pointer to L1 data buffer (172 words: cycles + count per counter)
    volatile std::uint32_t* get_data_mem()
    {
        return reinterpret_cast<volatile std::uint32_t*>(PERF_COUNTERS_DATA_ADDR);
    }

    // Get pointer to sync control word (thread coordination flags)
    volatile std::uint32_t* get_sync_ctrl_mem()
    {
        return reinterpret_cast<volatile std::uint32_t*>(PERF_COUNTERS_SYNC_CTRL_ADDR);
    }

    // Initialize and start hardware counters (called by first thread only)
    // Reads config from L1, configures L1 MUX if needed, and starts each bank
    void start_hardware()
    {
        volatile std::uint32_t* config_mem = get_config_mem();
        std::uint32_t started_mask         = 0;

        for (std::uint32_t i = 0; i < COUNTER_SLOT_COUNT; i++)
        {
            const std::uint32_t metadata = config_mem[i];
            if ((metadata & 0x80000000u) == 0)
            {
                continue;
            }

            const std::uint8_t bank_id   = metadata & 0xFF;
            const std::uint32_t bank_bit = 1u << bank_id;

            if (started_mask & bank_bit)
            {
                continue;
            }

            counter_bank bank = static_cast<counter_bank>(bank_id);

            // Configure L1 MUX if needed
            if (bank == counter_bank::l1)
            {
                const std::uint8_t l1_mux = (metadata >> 17) & 0x1;
                std::uint32_t cur         = hw_access::read_reg(RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL);
                hw_access::write_reg(RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL, (cur & ~(1u << 4)) | ((l1_mux & 0x1u) << 4));
            }

            // Start the bank
            std::uint32_t counter_base = hw_access::get_counter_base_addr(bank);
            hw_access::write_reg(counter_base, 0xFFFFFFFF); // Reference period
            hw_access::write_reg(counter_base + 4, 0);      // Mode register
            hw_access::write_reg(counter_base + 8, 0);      // Clear
            hw_access::write_reg(counter_base + 8, 1);      // Start

            started_mask |= bank_bit;
        }
    }

    // Stop hardware counters and read all results (called by last thread only)
    // Stops each bank, configures counter selectors, reads cycle/count pairs, writes to L1
    void stop_hardware()
    {
        volatile std::uint32_t* config_mem = get_config_mem();
        volatile std::uint32_t* data_mem   = get_data_mem();

        std::uint32_t stopped_mask = 0;
        std::uint32_t result_idx   = 0;

        for (std::uint32_t i = 0; i < COUNTER_SLOT_COUNT; i++)
        {
            const std::uint32_t metadata = config_mem[i];
            if ((metadata & 0x80000000u) == 0)
            {
                continue;
            }

            const std::uint8_t bank_id     = metadata & 0xFF;
            const std::uint16_t counter_id = (metadata >> 8) & 0x1FF;
            const std::uint8_t l1_mux      = (metadata >> 17) & 0x1;
            const std::uint32_t bank_bit   = 1u << bank_id;

            counter_bank bank = static_cast<counter_bank>(bank_id);

            // Stop bank on first encounter
            if (!(stopped_mask & bank_bit))
            {
                std::uint32_t counter_base = hw_access::get_counter_base_addr(bank);
                hw_access::write_reg(counter_base + 8, 0); // Clear
                hw_access::write_reg(counter_base + 8, 2); // Stop
                stopped_mask |= bank_bit;
            }

            // Configure L1 MUX before reading
            if (bank == counter_bank::l1)
            {
                std::uint32_t cur = hw_access::read_reg(RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL);
                hw_access::write_reg(RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL, (cur & ~(1u << 4)) | ((l1_mux & 0x1u) << 4));
            }

            std::uint32_t counter_base = hw_access::get_counter_base_addr(bank);
            hw_access::write_reg(counter_base + 4, static_cast<std::uint32_t>(counter_id) << 8);

            // Dummy read for settling
            std::uint32_t output_low_addr  = hw_access::get_counter_output_low_addr(bank);
            std::uint32_t output_high_addr = hw_access::get_counter_output_high_addr(bank);
            (void)hw_access::read_reg(output_low_addr);
            (void)hw_access::read_reg(output_high_addr);

            // Actual read and write directly to L1 buffer
            data_mem[result_idx * 2]     = hw_access::read_reg(output_low_addr);
            data_mem[result_idx * 2 + 1] = hw_access::read_reg(output_high_addr);

            result_idx++;
        }
    }

public:
    // Get singleton instance (Meyer's singleton pattern)
    static PerfCounterManager& instance()
    {
        static PerfCounterManager instance;
        return instance;
    }

    // Delete copy/move constructors and assignment operators
    PerfCounterManager(const PerfCounterManager&)            = delete;
    PerfCounterManager& operator=(const PerfCounterManager&) = delete;
    PerfCounterManager(PerfCounterManager&&)                 = delete;
    PerfCounterManager& operator=(PerfCounterManager&&)      = delete;

    // Thread-safe start: each thread sets its start bit, first thread initializes hardware
    // All sync control word updates happen inside mutex-protected critical section
    void start()
    {
        ckernel::T6MutexLockGuard lock(ckernel::mutex::REG_RMW);
        volatile std::uint32_t* sync_ctrl = get_sync_ctrl_mem();

        std::uint32_t state = *sync_ctrl;

        // Set this thread's start bit (bit 0, 1, or 2) to indicate arrival
        state |= thread_info::get_thread_start_bit();

        // First thread to arrive sees GLOBAL_STARTED unset and does hardware init
        if ((state & SYNC_STARTED_FLAG) == 0u)
        {
            start_hardware();
            state |= SYNC_STARTED_FLAG;
        }

        // Write updated state back (with our start bit set)
        *sync_ctrl = state;
    }

    // Thread-safe stop: each thread sets its stop bit, last thread reads hardware
    // Last stopper thread ID is written to sync control word for Python to identify
    void stop()
    {
        bool should_stop_hardware = false;

        {
            // CRITICAL SECTION: Determine if we're the last thread atomically
            ckernel::T6MutexLockGuard lock(ckernel::mutex::REG_RMW);
            volatile std::uint32_t* sync_ctrl = get_sync_ctrl_mem();
            std::uint32_t state               = *sync_ctrl;

            // Set this thread's stop bit (bit 3, 4, or 5) to indicate arrival
            state |= thread_info::get_thread_stop_bit();

            // Check if all 3 threads have set their stop bits
            bool all_threads_stopped      = (state & SYNC_STOP_MASK) == SYNC_STOP_MASK;
            bool already_stopped_globally = (state & SYNC_STOPPED_FLAG) != 0u;

            // Last thread to arrive becomes the "last stopper" and reads hardware
            if (!already_stopped_globally && all_threads_stopped)
            {
                state |= SYNC_STOPPED_FLAG;
                // Write our thread ID (0-2) to bits 9-10 for Python to read
                state                = (state & ~SYNC_LAST_STOPPER_MASK) | (thread_info::get_thread_id() << SYNC_LAST_STOPPER_SHIFT);
                should_stop_hardware = true;
            }

            // Write updated state back (with our stop bit set and possibly last stopper ID)
            *sync_ctrl = state;
        }
        // CRITICAL SECTION ENDS - mutex released

        // Only the last stopper thread executes this (outside mutex to avoid blocking others)
        if (should_stop_hardware)
        {
            stop_hardware();
        }
    }
};

// ============================================================================
// Public API
// ============================================================================

// Start performance counters (call from all threads)
inline void start_perf_counters()
{
    PerfCounterManager::instance().start();
}

// Stop performance counters (call from all threads)
inline void stop_perf_counters()
{
    PerfCounterManager::instance().stop();
}

// ============================================================================
// Counter ID Constants (for reference/documentation)
// ============================================================================

namespace counter_id
{
namespace instrn_thread
{
// Instruction availability counters (per-thread: add thread offset 0, 1, or 2)
constexpr std::uint32_t CFG_INSTRN_AVAILABLE_0     = 0;
constexpr std::uint32_t CFG_INSTRN_AVAILABLE_1     = 1;
constexpr std::uint32_t CFG_INSTRN_AVAILABLE_2     = 2;
constexpr std::uint32_t SYNC_INSTRN_AVAILABLE_0    = 3;
constexpr std::uint32_t SYNC_INSTRN_AVAILABLE_1    = 4;
constexpr std::uint32_t SYNC_INSTRN_AVAILABLE_2    = 5;
constexpr std::uint32_t THCON_INSTRN_AVAILABLE_0   = 6;
constexpr std::uint32_t THCON_INSTRN_AVAILABLE_1   = 7;
constexpr std::uint32_t THCON_INSTRN_AVAILABLE_2   = 8;
constexpr std::uint32_t XSEARCH_INSTRN_AVAILABLE_0 = 9;
constexpr std::uint32_t XSEARCH_INSTRN_AVAILABLE_1 = 10;
constexpr std::uint32_t XSEARCH_INSTRN_AVAILABLE_2 = 11;
constexpr std::uint32_t MOVE_INSTRN_AVAILABLE_0    = 12;
constexpr std::uint32_t MOVE_INSTRN_AVAILABLE_1    = 13;
constexpr std::uint32_t MOVE_INSTRN_AVAILABLE_2    = 14;
constexpr std::uint32_t FPU_INSTRN_AVAILABLE_0     = 15;
constexpr std::uint32_t FPU_INSTRN_AVAILABLE_1     = 16;
constexpr std::uint32_t FPU_INSTRN_AVAILABLE_2     = 17;
constexpr std::uint32_t UNPACK_INSTRN_AVAILABLE_0  = 18;
constexpr std::uint32_t UNPACK_INSTRN_AVAILABLE_1  = 19;
constexpr std::uint32_t UNPACK_INSTRN_AVAILABLE_2  = 20;
constexpr std::uint32_t PACK_INSTRN_AVAILABLE_0    = 21;
constexpr std::uint32_t PACK_INSTRN_AVAILABLE_1    = 22;
constexpr std::uint32_t PACK_INSTRN_AVAILABLE_2    = 23;
// Thread stalls
constexpr std::uint32_t THREAD_STALLS_0 = 24;
constexpr std::uint32_t THREAD_STALLS_1 = 25;
constexpr std::uint32_t THREAD_STALLS_2 = 26;
// Wait counters (shared across threads)
constexpr std::uint32_t WAITING_FOR_SRCA_CLEAR = 27;
constexpr std::uint32_t WAITING_FOR_SRCB_CLEAR = 28;
constexpr std::uint32_t WAITING_FOR_SRCA_VALID = 29;
constexpr std::uint32_t WAITING_FOR_SRCB_VALID = 30;
// Per-thread wait counters
constexpr std::uint32_t WAITING_FOR_THCON_IDLE_0  = 31;
constexpr std::uint32_t WAITING_FOR_THCON_IDLE_1  = 32;
constexpr std::uint32_t WAITING_FOR_THCON_IDLE_2  = 33;
constexpr std::uint32_t WAITING_FOR_UNPACK_IDLE_0 = 34;
constexpr std::uint32_t WAITING_FOR_UNPACK_IDLE_1 = 35;
constexpr std::uint32_t WAITING_FOR_UNPACK_IDLE_2 = 36;
constexpr std::uint32_t WAITING_FOR_PACK_IDLE_0   = 37;
constexpr std::uint32_t WAITING_FOR_PACK_IDLE_1   = 38;
constexpr std::uint32_t WAITING_FOR_PACK_IDLE_2   = 39;
constexpr std::uint32_t WAITING_FOR_MATH_IDLE_0   = 40;
constexpr std::uint32_t WAITING_FOR_MATH_IDLE_1   = 41;
constexpr std::uint32_t WAITING_FOR_MATH_IDLE_2   = 42;
constexpr std::uint32_t WAITING_FOR_NONZERO_SEM_0 = 43;
constexpr std::uint32_t WAITING_FOR_NONZERO_SEM_1 = 44;
constexpr std::uint32_t WAITING_FOR_NONZERO_SEM_2 = 45;
constexpr std::uint32_t WAITING_FOR_NONFULL_SEM_0 = 46;
constexpr std::uint32_t WAITING_FOR_NONFULL_SEM_1 = 47;
constexpr std::uint32_t WAITING_FOR_NONFULL_SEM_2 = 48;
constexpr std::uint32_t WAITING_FOR_MOVE_IDLE_0   = 49;
constexpr std::uint32_t WAITING_FOR_MOVE_IDLE_1   = 50;
constexpr std::uint32_t WAITING_FOR_MOVE_IDLE_2   = 51;
constexpr std::uint32_t WAITING_FOR_MMIO_IDLE_0   = 52;
constexpr std::uint32_t WAITING_FOR_MMIO_IDLE_1   = 53;
constexpr std::uint32_t WAITING_FOR_MMIO_IDLE_2   = 54;
constexpr std::uint32_t WAITING_FOR_SFPU_IDLE_0   = 55;
constexpr std::uint32_t WAITING_FOR_SFPU_IDLE_1   = 56;
constexpr std::uint32_t WAITING_FOR_SFPU_IDLE_2   = 57;
// Thread instruction counts (bit 8 set = ID 256+n)
constexpr std::uint32_t THREAD_INSTRUCTIONS_0 = 256;
constexpr std::uint32_t THREAD_INSTRUCTIONS_1 = 257;
constexpr std::uint32_t THREAD_INSTRUCTIONS_2 = 258;
} // namespace instrn_thread

namespace fpu
{
constexpr std::uint32_t FPU_INSTRUCTION    = 0;
constexpr std::uint32_t SFPU_INSTRUCTION   = 1;
constexpr std::uint32_t FPU_OR_SFPU_INSTRN = 257;
} // namespace fpu

namespace tdma_unpack
{
constexpr std::uint32_t DATA_HAZARD_STALLS_MOVD2A = 1;
constexpr std::uint32_t MATH_INSTRN_STARTED       = 3;
constexpr std::uint32_t MATH_INSTRN_AVAILABLE     = 4;
constexpr std::uint32_t SRCB_WRITE_AVAILABLE      = 5;
constexpr std::uint32_t SRCA_WRITE_AVAILABLE      = 6;
constexpr std::uint32_t UNPACK0_BUSY_THREAD0      = 7;
constexpr std::uint32_t UNPACK1_BUSY_THREAD0      = 8;
constexpr std::uint32_t UNPACK0_BUSY_THREAD1      = 9;
constexpr std::uint32_t UNPACK1_BUSY_THREAD1      = 10;
constexpr std::uint32_t SRCB_WRITE                = 259;
constexpr std::uint32_t SRCA_WRITE                = 261;
} // namespace tdma_unpack

namespace l1
{
// l1_mux = 0
constexpr std::uint32_t NOC_RING0_INCOMING_1 = 0;
constexpr std::uint32_t NOC_RING0_INCOMING_0 = 1;
constexpr std::uint32_t NOC_RING0_OUTGOING_1 = 2;
constexpr std::uint32_t NOC_RING0_OUTGOING_0 = 3;
constexpr std::uint32_t L1_ARB_TDMA_BUNDLE_1 = 4;
constexpr std::uint32_t L1_ARB_TDMA_BUNDLE_0 = 5;
constexpr std::uint32_t L1_ARB_UNPACKER      = 6;
constexpr std::uint32_t L1_NO_ARB_UNPACKER   = 7;

// l1_mux = 1 (same IDs, different mux setting)
constexpr std::uint32_t NOC_RING1_INCOMING_1 = 0;
constexpr std::uint32_t NOC_RING1_INCOMING_0 = 1;
constexpr std::uint32_t NOC_RING1_OUTGOING_1 = 2;
constexpr std::uint32_t NOC_RING1_OUTGOING_0 = 3;
constexpr std::uint32_t TDMA_BUNDLE_1_ARB    = 4;
constexpr std::uint32_t TDMA_BUNDLE_0_ARB    = 5;
constexpr std::uint32_t TDMA_EXT_UNPACK_9_10 = 6;
constexpr std::uint32_t TDMA_PACKER_2_WR     = 7;
} // namespace l1

namespace tdma_pack
{
constexpr std::uint32_t PACKER_DEST_READ_AVAILABLE = 11;
constexpr std::uint32_t PACKER_BUSY                = 18;
constexpr std::uint32_t AVAILABLE_MATH             = 272;
} // namespace tdma_pack
} // namespace counter_id

} // namespace llk_perf
