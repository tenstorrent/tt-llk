// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel.h"

namespace llk_perf
{

// ============================================================================
// L1 Memory Layout (Single Shared Buffer)
// ============================================================================

// SOURCE OF TRUTH: tests/python_tests/helpers/test_config.py (TestConfig class)
// Must be below profiler buffers which start at 0x16B000
// Memory budget: 0x16A000 to 0x16AFE3 (dump mailbox at 0x16AFE4) = 4068 bytes
#define PERF_COUNTERS_BASE_ADDR    0x16A000
#define PERF_COUNTERS_CONFIG_WORDS 86  // Counter configuration slots
#define PERF_COUNTERS_DATA_WORDS   172 // Counter data (cycles + count per slot)
#define PERF_COUNTERS_BUFFER_SIZE  ((PERF_COUNTERS_CONFIG_WORDS + PERF_COUNTERS_DATA_WORDS) * 4)

constexpr std::uint32_t PERF_COUNTERS_ZONE_SIZE = PERF_COUNTERS_BUFFER_SIZE + 40; // +40 for sync words
constexpr std::uint32_t PERF_COUNTERS_MAX_ZONES = 3;                              // Max zones that fit before dump mailbox at 0x16AFE4

constexpr std::uint32_t perf_counters_config_addr(std::uint32_t zone)
{
    return PERF_COUNTERS_BASE_ADDR + zone * PERF_COUNTERS_ZONE_SIZE;
}

constexpr std::uint32_t perf_counters_data_addr(std::uint32_t zone)
{
    return perf_counters_config_addr(zone) + PERF_COUNTERS_CONFIG_WORDS * 4;
}

constexpr std::uint32_t perf_counters_sync_ctrl_addr(std::uint32_t zone)
{
    return perf_counters_config_addr(zone) + PERF_COUNTERS_BUFFER_SIZE;
}

// Thread count for perf counter synchronization
#define PERF_COUNTERS_THREAD_COUNT 3

// Atomic counters for ATINCGET-based synchronization
// Per-thread stop arrival flags (3 words at sync_ctrl + 4) — used by lightweight stop
constexpr std::uint32_t perf_counters_stop_flags_addr(std::uint32_t zone)
{
    return perf_counters_sync_ctrl_addr(zone) + 4;
}

// Atomic counters for ATINCGET-based synchronization (overlaps stop_flags when using lightweight mode)
constexpr std::uint32_t perf_counters_start_counter_addr(std::uint32_t zone)
{
    return perf_counters_sync_ctrl_addr(zone) + 4;
}

constexpr std::uint32_t perf_counters_stop_counter_addr(std::uint32_t zone)
{
    return perf_counters_start_counter_addr(zone) + (PERF_COUNTERS_THREAD_COUNT * 4);
}

constexpr std::uint32_t perf_counters_stop_elect_addr(std::uint32_t zone)
{
    return perf_counters_stop_counter_addr(zone) + (PERF_COUNTERS_THREAD_COUNT * 4);
}

// Barriers to synchronize all threads after start_hardware / stop_hardware.
// Ensures all threads enter/exit profiler zones at the same time,
// preventing inter-thread timing skew from leaking into profiler measurements.
constexpr std::uint32_t perf_counters_start_barrier_addr(std::uint32_t zone)
{
    return perf_counters_stop_elect_addr(zone) + 4;
}

constexpr std::uint32_t perf_counters_stop_barrier_addr(std::uint32_t zone)
{
    return perf_counters_start_barrier_addr(zone) + 4;
}

// Global enabled flag — set by BRISC, read by TRISCs. Located after all zone data.
constexpr std::uint32_t PERF_COUNTERS_ENABLED_FLAG_ADDR = PERF_COUNTERS_BASE_ADDR + PERF_COUNTERS_MAX_ZONES * PERF_COUNTERS_ZONE_SIZE;

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
// Bits 8-9: Starter thread ID (0=UNPACK, 1=MATH, 2=PACK) - thread that initialized hardware
// Bits 10-11: Stopper thread ID (0=UNPACK, 1=MATH, 2=PACK) - thread that read hardware
// Bits 12-31: Reserved

// Lightweight sync: zone complete marker + stopper thread ID
constexpr std::uint32_t SYNC_ZONE_COMPLETE = 0xFFu;
constexpr std::uint32_t SYNC_STOPPER_SHIFT = 8u;

// Full CAS sync constants (kept for future use)
constexpr std::uint32_t SYNC_START_MASK    = (1u << 0) | (1u << 1) | (1u << 2);
constexpr std::uint32_t SYNC_STOP_MASK     = SYNC_START_MASK << 3;
constexpr std::uint32_t SYNC_STARTED_FLAG  = 1u << 6;
constexpr std::uint32_t SYNC_STOPPED_FLAG  = 1u << 7;
constexpr std::uint32_t SYNC_STARTER_SHIFT = 8u;
constexpr std::uint32_t SYNC_STARTER_MASK  = 0x3u << SYNC_STARTER_SHIFT;
constexpr std::uint32_t SYNC_STOPPER_MASK  = 0x3u << SYNC_STOPPER_SHIFT;

// ============================================================================
// ATINCGET Helpers
// ============================================================================

// Use architecture-specific ATINCGET macro shape
#if defined(ARCH_QUASAR)
#define PERF_COUNTERS_TTI_ATINCGET(WrapVal, Sel32b, DataRegIndex, AddrRegIndex) TTI_ATINCGET(WrapVal, Sel32b, DataRegIndex, AddrRegIndex)
#else
#define PERF_COUNTERS_TTI_ATINCGET(WrapVal, Sel32b, DataRegIndex, AddrRegIndex) TTI_ATINCGET(0, WrapVal, Sel32b, DataRegIndex, AddrRegIndex)
#endif

#ifndef PERF_COUNTERS_USE_ATINCGET
#define PERF_COUNTERS_USE_ATINCGET 0
#endif

constexpr std::uint32_t ATINCGET_WIDTH_32    = 31u; // IntWidth for 32-bit
constexpr std::uint32_t PERF_COUNTER_THREADS = PERF_COUNTERS_THREAD_COUNT;

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
    const volatile std::uint32_t* get_config_mem(std::uint32_t zone)
    {
        return reinterpret_cast<volatile std::uint32_t*>(perf_counters_config_addr(zone));
    }

    // Get pointer to L1 data buffer (172 words: cycles + count per counter)
    volatile std::uint32_t* get_data_mem(std::uint32_t zone)
    {
        return reinterpret_cast<volatile std::uint32_t*>(perf_counters_data_addr(zone));
    }

    volatile std::uint32_t* get_enabled_flag()
    {
        return reinterpret_cast<volatile std::uint32_t*>(PERF_COUNTERS_ENABLED_FLAG_ADDR);
    }

    bool is_enabled()
    {
        return *get_enabled_flag() != 0u;
    }

    // Get pointer to sync control word (thread coordination flags)
    volatile std::uint32_t* get_sync_ctrl_mem(std::uint32_t zone)
    {
        return reinterpret_cast<volatile std::uint32_t*>(perf_counters_sync_ctrl_addr(zone));
    }

    // Read an L1 word with a cache invalidation to improve visibility across threads.
    inline std::uint32_t read_l1_word(volatile std::uint32_t* addr)
    {
        ckernel::invalidate_data_cache();
        return *addr;
    }

    // Issue ATINCGET in L1 and return the original value.
    // Uses regfile indices reserved for perf counters, and restores them afterward.
    inline std::uint32_t atincget_l1(std::uint32_t addr, std::uint32_t increment)
    {
        constexpr std::uint32_t kDataReg = ckernel::p_gpr::DBG_RESERVED;
        constexpr std::uint32_t kAddrReg = ckernel::p_gpr::DBG_MSG;

        const std::uint32_t base16 = addr & ~0xFu;
        const std::uint32_t sel32b = (addr >> 2) & 0x3u;

        const std::uint32_t saved_data = ckernel::regfile[kDataReg];
        const std::uint32_t saved_addr = ckernel::regfile[kAddrReg];

        // Store to GPRs with explicit ordering (sw -> lw -> addi)
        volatile std::uint32_t* data_ptr = &ckernel::regfile[kDataReg];
        volatile std::uint32_t* addr_ptr = &ckernel::regfile[kAddrReg];
        std::uint32_t tmp;
        const std::uint32_t inc_val  = increment;
        const std::uint32_t addr_val = base16 >> 4;
        asm volatile(
            "sw %2, 0(%1)\n"
            "lw %0, 0(%1)\n"
            "addi x0, %0, 0\n"
            : "=&r"(tmp)
            : "r"(data_ptr), "r"(inc_val)
            : "memory");
        asm volatile(
            "sw %2, 0(%1)\n"
            "lw %0, 0(%1)\n"
            "addi x0, %0, 0\n"
            : "=&r"(tmp)
            : "r"(addr_ptr), "r"(addr_val)
            : "memory");

        PERF_COUNTERS_TTI_ATINCGET(ATINCGET_WIDTH_32, sel32b, kDataReg, kAddrReg);

        // Wait for ATINCGET result by spinning on the GPR (RISC-V loads only, no coprocessor FIFO pollution).
        // The ATINCGET overwrites kDataReg with the old L1 value. We detect completion by
        // reading until the value differs from the increment we stored, or a timeout.
        std::uint32_t old_value;
        for (std::uint32_t i = 0; i < 1000; ++i)
        {
            old_value = ckernel::regfile[kDataReg];
            if (old_value != inc_val)
            {
                break;
            }
            // Pure RISC-V delay — no coprocessor instructions
            asm volatile("nop");
        }

        // Restore GPRs using the same ordered store sequence
        asm volatile(
            "sw %2, 0(%1)\n"
            "lw %0, 0(%1)\n"
            "addi x0, %0, 0\n"
            : "=&r"(tmp)
            : "r"(data_ptr), "r"(saved_data)
            : "memory");
        asm volatile(
            "sw %2, 0(%1)\n"
            "lw %0, 0(%1)\n"
            "addi x0, %0, 0\n"
            : "=&r"(tmp)
            : "r"(addr_ptr), "r"(saved_addr)
            : "memory");

        return old_value;
    }

    // Configure banks for a zone: L1 MUX, reference period, mode register.
    void configure_hardware(std::uint32_t zone)
    {
        const volatile std::uint32_t* config_mem = get_config_mem(zone);
        std::uint32_t configured_mask            = 0;

        for (std::uint32_t i = 0; i < COUNTER_SLOT_COUNT; i++)
        {
            const std::uint32_t metadata = config_mem[i];
            if ((metadata & 0x80000000u) == 0)
            {
                continue;
            }

            const std::uint8_t bank_id   = static_cast<std::uint8_t>(metadata);
            const std::uint32_t bank_bit = 1u << bank_id;

            if (configured_mask & bank_bit)
            {
                continue;
            }

            const counter_bank bank = static_cast<counter_bank>(bank_id);

            if (bank == counter_bank::l1)
            {
                const std::uint8_t l1_mux = (metadata >> 17) & 0x1;
                std::uint32_t cur         = hw_access::read_reg(RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL);
                hw_access::write_reg(RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL, (cur & ~(1u << 4)) | ((l1_mux & 0x1u) << 4));
            }

            std::uint32_t counter_base = hw_access::get_counter_base_addr(bank);
            hw_access::write_reg(counter_base, 0xFFFFFFFF); // Reference period
            hw_access::write_reg(counter_base + 4, 0);      // Mode register

            configured_mask |= bank_bit;
        }
    }

    // Arm all 5 counter banks: clear + start. Very cheap (~20 cycles).
    void arm_hardware()
    {
        static constexpr counter_bank ALL_BANKS[] = {
            counter_bank::instrn_thread,
            counter_bank::fpu,
            counter_bank::tdma_unpack,
            counter_bank::l1,
            counter_bank::tdma_pack,
        };

        for (auto bank : ALL_BANKS)
        {
            std::uint32_t counter_base = hw_access::get_counter_base_addr(bank);
            hw_access::write_reg(counter_base + 8, 0); // Clear
            hw_access::write_reg(counter_base + 8, 1); // Start
        }
    }

    // Initialize and start hardware counters (called by first thread only)
    void start_hardware(std::uint32_t zone)
    {
        configure_hardware(zone);
        arm_hardware();
    }

    // Stop hardware counters and read all results (called by last thread only)
    // Stops each bank, configures counter selectors, reads cycle/count pairs, writes to L1
    void stop_hardware(std::uint32_t zone)
    {
        const volatile std::uint32_t* config_mem = get_config_mem(zone);
        volatile std::uint32_t* data_mem         = get_data_mem(zone);

        std::uint32_t stopped_mask = 0;
        std::uint32_t result_idx   = 0;

        for (std::uint32_t i = 0; i < COUNTER_SLOT_COUNT; i++)
        {
            const std::uint32_t metadata = config_mem[i];
            if ((metadata & 0x80000000u) == 0)
            {
                continue;
            }

            const std::uint8_t bank_id     = static_cast<std::uint8_t>(metadata);
            const std::uint16_t counter_id = (metadata >> 8) & 0x1FF;
            const std::uint8_t l1_mux      = (metadata >> 17) & 0x1;
            const std::uint32_t bank_bit   = 1u << bank_id;

            const counter_bank bank = static_cast<counter_bank>(bank_id);

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
    // Pre-configure all zones. Called by BRISC before releasing TRISCs.
    void configure_all_zones()
    {
        bool found_valid = false;
        for (std::uint32_t zone = 0; zone < PERF_COUNTERS_MAX_ZONES; ++zone)
        {
            const volatile std::uint32_t* config_mem = get_config_mem(zone);
            for (std::uint32_t i = 0; i < COUNTER_SLOT_COUNT; i++)
            {
                if (config_mem[i] & 0x80000000u)
                {
                    found_valid = true;
                    break;
                }
            }
            if (found_valid)
            {
                break;
            }
        }

        if (found_valid)
        {
            for (std::uint32_t zone = 0; zone < PERF_COUNTERS_MAX_ZONES; ++zone)
            {
                configure_hardware(zone);
            }
        }

        volatile std::uint32_t* enabled_flag = get_enabled_flag();
        *enabled_flag                        = found_valid ? 1u : 0u;
    }

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

    // Lightweight start: thread 0 arms zone 0. Zone > 0 is a no-op (armed by prev stop).
    void start(std::uint32_t zone)
    {
        if (!is_enabled())
        {
            return;
        }

        if (zone == 0 && thread_info::get_thread_id() == 0)
        {
            arm_hardware();
        }
    }

    // Lightweight stop: per-thread L1 flags for last-thread detection.
    // Early threads: write flag + 2 reads + return (~5 cycles).
    // Last thread: stop_hardware + arm_hardware + bookkeeping.
    void stop(std::uint32_t zone)
    {
        if (!is_enabled())
        {
            return;
        }

        volatile std::uint32_t* stop_flags = reinterpret_cast<volatile std::uint32_t*>(perf_counters_stop_flags_addr(zone));
        const std::uint32_t thread_id      = thread_info::get_thread_id();

        // Signal arrival.
        stop_flags[thread_id] = 1;
        ckernel::invalidate_data_cache();

        // Check if we're the last thread (other two flags already set).
        bool is_last = true;
        for (std::uint32_t t = 0; t < PERF_COUNTER_THREADS; ++t)
        {
            if (t != thread_id && !stop_flags[t])
            {
                is_last = false;
                break;
            }
        }

        if (!is_last)
        {
            return;
        }

        // Last thread: stop hardware immediately (tightest counter window).
        stop_hardware(zone);

        if (zone + 1u < PERF_COUNTERS_MAX_ZONES)
        {
            arm_hardware();
        }

        // Clear flags for next zone.
        stop_flags[0] = 0;
        stop_flags[1] = 0;
        stop_flags[2] = 0;

        volatile std::uint32_t* sync_ctrl = get_sync_ctrl_mem(zone);
        *sync_ctrl                        = SYNC_ZONE_COMPLETE | (thread_id << SYNC_STOPPER_SHIFT);
        ckernel::invalidate_data_cache();
    }
};

// ============================================================================
// Public API
// ============================================================================

// Start performance counters (call from all threads)
// noinline + cold: keep counter code out of the hot instruction cache
__attribute__((noinline, cold)) inline void start_perf_counters(std::uint32_t zone)
{
    PerfCounterManager::instance().start(zone);
}

// Stop performance counters (call from all threads)
__attribute__((noinline, cold)) inline void stop_perf_counters(std::uint32_t zone)
{
    PerfCounterManager::instance().stop(zone);
}

// Pre-configure all counter banks (called by BRISC before TRISCs start)
inline void configure_perf_counters_from_brisc()
{
    PerfCounterManager::instance().configure_all_zones();
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

namespace llk_perf
{
// RAII wrapper for automatic counter start/stop.
// Accepts zone_id at runtime (non-template) to support auto-assignment.
class perf_counter_scoped
{
    std::uint32_t zone_id;

public:
    perf_counter_scoped(const perf_counter_scoped&)            = delete;
    perf_counter_scoped(perf_counter_scoped&&)                 = delete;
    perf_counter_scoped& operator=(const perf_counter_scoped&) = delete;
    perf_counter_scoped& operator=(perf_counter_scoped&&)      = delete;

    inline __attribute__((always_inline)) perf_counter_scoped(std::uint32_t id) : zone_id(id)
    {
        start_perf_counters(zone_id);
    }

    ~perf_counter_scoped()
    {
        stop_perf_counters(zone_id);
    }
};

// ── Runtime zone allocator ──────────────────────────────────────────
// Maps compile-time zone name hashes to sequential zone IDs (0, 1, 2).
namespace detail
{
constexpr std::uint32_t ZONE_UNALLOCATED = 0xFF;
constexpr std::uint32_t ZONE_LOOKUP_SIZE = 32;
static std::uint32_t zone_lookup[ZONE_LOOKUP_SIZE];
static std::uint32_t next_zone_id;
static bool zone_lookup_ready;

constexpr std::uint32_t zone_name_hash(const char* s)
{
    std::uint32_t h = 5381;
    while (*s)
    {
        h = h * 33 + static_cast<std::uint32_t>(*s++);
    }
    return h % ZONE_LOOKUP_SIZE;
}
} // namespace detail

inline std::uint32_t get_zone_id(std::uint32_t hash_val)
{
    if (!detail::zone_lookup_ready)
    {
        for (std::uint32_t i = 0; i < detail::ZONE_LOOKUP_SIZE; ++i)
        {
            detail::zone_lookup[i] = detail::ZONE_UNALLOCATED;
        }
        detail::zone_lookup_ready = true;
    }
    if (hash_val < detail::ZONE_LOOKUP_SIZE && detail::zone_lookup[hash_val] == detail::ZONE_UNALLOCATED)
    {
        detail::zone_lookup[hash_val] = detail::next_zone_id++;
    }
    return (hash_val < detail::ZONE_LOOKUP_SIZE) ? detail::zone_lookup[hash_val] : 0;
}

} // namespace llk_perf

// ── MEASURE_PERF_COUNTERS ───────────────────────────────────────────
// Auto-assigning performance counter zones. No registration needed.
// First unique name gets zone 0, second gets zone 1, etc. (max 3).
// Zone names are stored in the ELF .perf_counter_meta section for host-side parsing.
// Uses __LINE__ for unique variable names and constexpr hashing for zone IDs.

#define MEASURE_PERF_COUNTERS(zone_name)   _PERF_MEASURE_EXPAND(zone_name, __LINE__)
#define _PERF_MEASURE_EXPAND(zone_name, n) _PERF_MEASURE_IMPL(zone_name, n)
#define _PERF_MEASURE_IMPL(zone_name, n)                                                                                                    \
    __attribute__((section(".perf_counter_meta"), used)) static const char _perf_meta_##n[] = #n ":" #zone_name;                            \
    constexpr std::uint32_t _perf_hash_##n                                                  = llk_perf::detail::zone_name_hash(#zone_name); \
    const std::uint32_t _perf_zid_##n                                                       = llk_perf::get_zone_id(_perf_hash_##n);        \
    const auto _perf_scoped_##n                                                             = llk_perf::perf_counter_scoped(_perf_zid_##n);
