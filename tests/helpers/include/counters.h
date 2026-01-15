// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstdint>

#include "ckernel.h"

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
// IMPORTANT: The low output (`OUT_L`) returns the reference cycle count for the bank's
// measurement window. This value is independent of the selected event and will be
// identical across selections when scanning multiple counters in the same bank.
// The high output (`OUT_H`) returns the event-specific count for the currently selected
// counter (set via bits [15:8] of the mode register).
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

// L1 Memory addresses - separate per TRISC thread
// Expanded to support up to 66 counters: 66 config words (264 bytes) + 132 data words (528 bytes) = 792 bytes per thread
// Thread blocks:
// UNPACK: CONFIG 0x2F7D0, DATA 0x2F8D8 (0x2F7D0 + 0x108)
// MATH:   CONFIG 0x2FAE8, DATA 0x2FBF0 (UNPACK + 0x318)
// PACK:   CONFIG 0x2FE00, DATA 0x2FF08 (MATH + 0x318)
#define PERF_COUNTER_UNPACK_CONFIG_ADDR 0x2F7D0 // 66 words: UNPACK metadata
#define PERF_COUNTER_UNPACK_DATA_ADDR   0x2F8D8 // 132 words: UNPACK results
#define PERF_COUNTER_MATH_CONFIG_ADDR   0x2FAE8 // 66 words: MATH metadata
#define PERF_COUNTER_MATH_DATA_ADDR     0x2FBF0 // 132 words: MATH results
#define PERF_COUNTER_PACK_CONFIG_ADDR   0x2FE00 // 66 words: PACK metadata
#define PERF_COUNTER_PACK_DATA_ADDR     0x2FF08 // 132 words: PACK results

// Configuration word format: [mode_bit(16), counter_sel(8-15), bank_id(0-7)]
// Use compact underlying type to reduce memory footprint
enum class CounterBank : uint8_t
{
    INSTRN_THREAD = 0,
    FPU           = 1,
    TDMA_UNPACK   = 2,
    L1            = 3,
    TDMA_PACK     = 4,
};

// Number of counter banks represented by CounterBank enum
inline constexpr uint32_t COUNTER_BANK_COUNT = 5;
// Number of counter slots supported per thread (config words and data pairs)
inline constexpr uint32_t COUNTER_SLOT_COUNT = 66;

inline constexpr uint32_t get_counter_base_addr(CounterBank bank)
{
    constexpr uint32_t base_addrs[COUNTER_BANK_COUNT] = {
        RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, // INSTRN_THREAD
        RISCV_DEBUG_REG_PERF_CNT_FPU0,           // FPU
        RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0,   // TDMA_UNPACK
        RISCV_DEBUG_REG_PERF_CNT_L1_0,           // L1
        RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0,     // TDMA_PACK
    };

    const uint32_t idx = static_cast<uint32_t>(bank);
    return idx < COUNTER_BANK_COUNT ? base_addrs[idx] : 0u;
}

inline constexpr uint32_t get_counter_output_low_addr(CounterBank bank)
{
    constexpr uint32_t low_addrs[COUNTER_BANK_COUNT] = {
        RISCV_DEBUG_REG_PERF_CNT_OUT_L_INSTRN_THREAD, // INSTRN_THREAD
        RISCV_DEBUG_REG_PERF_CNT_OUT_L_FPU,           // FPU
        RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_UNPACK,   // TDMA_UNPACK
        RISCV_DEBUG_REG_PERF_CNT_OUT_L_DBG_L1,        // L1
        RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_PACK,     // TDMA_PACK
    };

    const uint32_t idx = static_cast<uint32_t>(bank);
    return idx < COUNTER_BANK_COUNT ? low_addrs[idx] : 0u;
}

inline constexpr uint32_t get_counter_output_high_addr(CounterBank bank)
{
    constexpr uint32_t high_addrs[COUNTER_BANK_COUNT] = {
        RISCV_DEBUG_REG_PERF_CNT_OUT_H_INSTRN_THREAD, // INSTRN_THREAD
        RISCV_DEBUG_REG_PERF_CNT_OUT_H_FPU,           // FPU
        RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_UNPACK,   // TDMA_UNPACK
        RISCV_DEBUG_REG_PERF_CNT_OUT_H_DBG_L1,        // L1
        RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_PACK,     // TDMA_PACK
    };

    const uint32_t idx = static_cast<uint32_t>(bank);
    return idx < COUNTER_BANK_COUNT ? high_addrs[idx] : 0u;
}

// Compact underlying type for mode as well
enum class CounterMode : uint8_t
{
    REQUESTS = 0,
    GRANTS   = 1,
};

struct CounterResult
{
    uint32_t cycles;
    uint32_t count;
    CounterBank bank;
    uint32_t counter_id;
};

namespace counter_id
{

namespace instrn_thread
{
constexpr uint32_t INST_CFG               = 0;
constexpr uint32_t INST_SYNC              = 1;
constexpr uint32_t INST_THCON             = 2;
constexpr uint32_t INST_XSEARCH           = 3;
constexpr uint32_t INST_MOVE              = 4;
constexpr uint32_t INST_MATH              = 5;
constexpr uint32_t INST_UNPACK            = 6;
constexpr uint32_t INST_PACK              = 7;
constexpr uint32_t STALLED                = 8;
constexpr uint32_t SRCA_CLEARED_0         = 9;
constexpr uint32_t SRCA_CLEARED_1         = 10;
constexpr uint32_t SRCA_CLEARED_2         = 11;
constexpr uint32_t SRCB_CLEARED_0         = 12;
constexpr uint32_t SRCB_CLEARED_1         = 13;
constexpr uint32_t SRCB_CLEARED_2         = 14;
constexpr uint32_t SRCA_VALID_0           = 15;
constexpr uint32_t SRCA_VALID_1           = 16;
constexpr uint32_t SRCA_VALID_2           = 17;
constexpr uint32_t SRCB_VALID_0           = 18;
constexpr uint32_t SRCB_VALID_1           = 19;
constexpr uint32_t SRCB_VALID_2           = 20;
constexpr uint32_t STALL_THCON            = 21;
constexpr uint32_t STALL_PACK0            = 22;
constexpr uint32_t STALL_MATH             = 23;
constexpr uint32_t STALL_SEM_ZERO         = 24;
constexpr uint32_t STALL_SEM_MAX          = 25;
constexpr uint32_t STALL_MOVE             = 26;
constexpr uint32_t STALL_TRISC_REG_ACCESS = 27;
constexpr uint32_t STALL_SFPU             = 28;
} // namespace instrn_thread

namespace fpu
{
constexpr uint32_t FPU_OP_VALID  = 0;
constexpr uint32_t SFPU_OP_VALID = 1;
} // namespace fpu

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

namespace l1
{
// mux_ctrl_bit4 = 0
constexpr uint32_t NOC_RING0_INCOMING_1 = 0;
constexpr uint32_t NOC_RING0_INCOMING_0 = 1;
constexpr uint32_t NOC_RING0_OUTGOING_1 = 2;
constexpr uint32_t NOC_RING0_OUTGOING_0 = 3;
constexpr uint32_t L1_ARB_TDMA_BUNDLE_1 = 4;
constexpr uint32_t L1_ARB_TDMA_BUNDLE_0 = 5;
constexpr uint32_t L1_ARB_UNPACKER      = 6;
constexpr uint32_t L1_NO_ARB_UNPACKER   = 7;

// mux_ctrl_bit4 = 1 (same IDs, different mux setting)
constexpr uint32_t NOC_RING1_INCOMING_1 = 0;
constexpr uint32_t NOC_RING1_INCOMING_0 = 1;
constexpr uint32_t NOC_RING1_OUTGOING_1 = 2;
constexpr uint32_t NOC_RING1_OUTGOING_0 = 3;
constexpr uint32_t TDMA_BUNDLE_1_ARB    = 4;
constexpr uint32_t TDMA_BUNDLE_0_ARB    = 5;
constexpr uint32_t TDMA_EXT_UNPACK_9_10 = 6;
constexpr uint32_t TDMA_PACKER_2_WR     = 7;
} // namespace l1

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

} // namespace counter_id

class PerfCounters
{
private:
    struct CounterConfig
    {
        CounterBank bank;   // 1 byte
        uint8_t counter_id; // 1 byte
        uint8_t l1_mux;     // 1 byte (Only used for L1 counters)
    };

    CounterConfig counters[COUNTER_SLOT_COUNT];
    uint32_t num_counters {0};
    CounterMode mode;

    // Helpers to get per-thread config/data memory addresses
    static inline volatile uint32_t* get_config_mem()
    {
#if defined(LLK_TRISC_UNPACK)
        constexpr uint32_t addr = PERF_COUNTER_UNPACK_CONFIG_ADDR;
#elif defined(LLK_TRISC_MATH)
        constexpr uint32_t addr = PERF_COUNTER_MATH_CONFIG_ADDR;
#elif defined(LLK_TRISC_PACK)
        constexpr uint32_t addr = PERF_COUNTER_PACK_CONFIG_ADDR;
#else
        constexpr uint32_t addr = PERF_COUNTER_MATH_CONFIG_ADDR;
#endif
        return reinterpret_cast<volatile uint32_t*>(addr);
    }

    static inline volatile uint32_t* get_data_mem()
    {
#if defined(LLK_TRISC_UNPACK)
        constexpr uint32_t addr = PERF_COUNTER_UNPACK_DATA_ADDR;
#elif defined(LLK_TRISC_MATH)
        constexpr uint32_t addr = PERF_COUNTER_MATH_DATA_ADDR;
#elif defined(LLK_TRISC_PACK)
        constexpr uint32_t addr = PERF_COUNTER_PACK_DATA_ADDR;
#else
        constexpr uint32_t addr = PERF_COUNTER_MATH_DATA_ADDR;
#endif
        return reinterpret_cast<volatile uint32_t*>(addr);
    }

    void write_metadata()
    {
        volatile uint32_t* config_mem = get_config_mem();

        for (uint32_t i = 0; i < num_counters; i++)
        {
            const auto& config = counters[i];

            // Encode: [valid(31), l1_mux(17), mode(16), counter_id(8-15), bank(0-7)]
            uint32_t metadata = (1u << 31) | // Valid bit to distinguish from empty slots
                                (static_cast<uint32_t>(config.l1_mux) << 17) | ((static_cast<uint32_t>(mode) & 0x1u) << 16) |
                                (static_cast<uint32_t>(config.counter_id) << 8) | static_cast<uint32_t>(config.bank);

            config_mem[i] = metadata;
        }

        // Clear remaining slots
        for (uint32_t i = num_counters; i < COUNTER_SLOT_COUNT; i++)
        {
            config_mem[i] = 0;
        }
    }

public:
    PerfCounters() : num_counters(0), mode(CounterMode::GRANTS)
    {
    }

    /**
     * Add a counter to measure
     *
     * @param bank Counter bank (INSTRN_THREAD, FPU, TDMA_UNPACK, L1, TDMA_PACK)
     * @param counter_id Counter ID within the bank (0-28 for INSTRN_THREAD, etc.)
     * @param l1_mux Only for L1 counters: selects counter set (0 or 1)
     * @return true if the counter was added; false if capacity reached
     */
    [[nodiscard]] bool add(CounterBank bank, uint8_t counter_id, uint8_t l1_mux = 0)
    {
        if (num_counters >= COUNTER_SLOT_COUNT)
        {
            return false; // Max 66 counters
        }

        counters[num_counters++] = {bank, counter_id, l1_mux};
        return true;
    }

    /**
     * Write the current in-memory counter configuration to L1
     * Call this after one or more add() calls to program hardware selection.
     */
    void configure()
    {
        if (num_counters == 0)
        {
            return;
        }
        write_metadata();
    }

    /**
     * Start all configured counters
     * Reads configuration from L1 memory (set by Python or C++)
     */
    void start()
    {
        // Read configuration from L1 (may have been set by Python or C++)
        volatile uint32_t* config_mem = get_config_mem();

        // Metadata is written during add(); if Python configured metadata,
        // we will read it directly from L1 below.

        // Count how many valid counters are configured (check bit 31)
        uint32_t active_count = 0;
        for (uint32_t i = 0; i < COUNTER_SLOT_COUNT; i++)
        {
            if ((config_mem[i] & 0x80000000) != 0)
            {
                active_count++;
            }
        }

        // If no counters configured, update counter_count and return
        if (active_count == 0)
        {
            num_counters = 0;
            return;
        }

        // Update counter_count to match what's in L1
        num_counters = active_count;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
        volatile uint32_t* dbg_regs = reinterpret_cast<volatile uint32_t*>(RISCV_DEBUG_REGS_START_ADDR);

        // First decode configs and store locally
        uint32_t counter_idx = 0;     // Separate index for storing configs
        bool mode_adopted    = false; // If Python provided mode in metadata, adopt it globally
        for (uint32_t i = 0; i < COUNTER_SLOT_COUNT && counter_idx < COUNTER_SLOT_COUNT; i++)
        {
            uint32_t metadata = config_mem[i];

            if ((metadata & 0x80000000u) == 0)
            {
                continue;
            }

            uint8_t bank_id        = metadata & 0xFF;
            uint8_t counter_id_val = (metadata >> 8) & 0xFF;
            uint8_t mode_bit       = (metadata >> 16) & 0x1;
            uint8_t mux_ctrl_val   = (metadata >> 17) & 0x1;

            // Adopt mode from metadata if externally configured (Python), overriding current global mode
            if (!mode_adopted)
            {
                mode         = mode_bit ? CounterMode::GRANTS : CounterMode::REQUESTS;
                mode_adopted = true;
            }

            counters[counter_idx].bank       = static_cast<CounterBank>(bank_id);
            counters[counter_idx].counter_id = counter_id_val;
            counters[counter_idx].l1_mux     = mux_ctrl_val;
            counter_idx++;
        }

        // Avoid repeated resets/starts per bank
        bool bank_started[COUNTER_BANK_COUNT] = {false, false, false, false, false};

        // Compute mask of banks present in the configuration
        uint32_t present_mask = 0;
        for (uint32_t i = 0; i < counter_idx; i++)
        {
            present_mask |= 1u << static_cast<uint32_t>(counters[i].bank);
        }

        // Track which banks have been started and break early once all present banks are started
        uint32_t started_mask = 0;
        for (uint32_t i = 0; i < counter_idx; i++)
        {
            const auto& config = counters[i];

            // Configure L1 MUX if needed (doesn't affect counting, but safe)
            if (config.bank == CounterBank::L1)
            {
                uint32_t mux_ctrl_addr  = (RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL - RISCV_DEBUG_REGS_START_ADDR) / 4;
                uint32_t cur            = dbg_regs[mux_ctrl_addr];
                dbg_regs[mux_ctrl_addr] = (cur & ~(1u << 4)) | ((static_cast<uint32_t>(config.l1_mux) & 0x1u) << 4);
            }

            uint32_t bank_index       = static_cast<uint32_t>(config.bank);
            uint32_t counter_base     = get_counter_base_addr(config.bank);
            uint32_t counter_reg_addr = (counter_base - RISCV_DEBUG_REGS_START_ADDR) / 4;

            if (!bank_started[bank_index])
            {
                // Reset
                dbg_regs[counter_reg_addr] = 0xFFFFFFFF;
                // Set counting mode to continuous (bits [7:0] = 0). Selection/mode_bit do not affect counting.
                dbg_regs[counter_reg_addr + 1] = 0;
                // Ensure 0->1 transition on start bit
                dbg_regs[counter_reg_addr + 2] = 0;
                dbg_regs[counter_reg_addr + 2] = 1;
                bank_started[bank_index]       = true;
                started_mask |= 1u << bank_index;

                // If all present banks have been started, exit early
                if (started_mask == present_mask)
                {
                    break;
                }
            }
        }
#pragma GCC diagnostic pop
    }

    /**
     * Stop all counters and return results
     *
     * @return Array of counter results (cycles, count for each counter)
     */
    std::array<CounterResult, COUNTER_SLOT_COUNT> stop()
    {
        std::array<CounterResult, COUNTER_SLOT_COUNT> results;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
        volatile uint32_t* dbg_regs = reinterpret_cast<volatile uint32_t*>(RISCV_DEBUG_REGS_START_ADDR);

        volatile uint32_t* data_mem = get_data_mem();

        // Stop all banks once via 0->1 transition on stop bit
        bool bank_stopped[COUNTER_BANK_COUNT] = {false, false, false, false, false};
        for (uint32_t i = 0; i < num_counters; i++)
        {
            const auto& config = counters[i];
            uint32_t bank_idx  = static_cast<uint32_t>(config.bank);
            uint32_t base      = get_counter_base_addr(config.bank);
            uint32_t reg_addr  = (base - RISCV_DEBUG_REGS_START_ADDR) / 4;
            if (!bank_stopped[bank_idx])
            {
                dbg_regs[reg_addr + 2] = 0; // Clear
                dbg_regs[reg_addr + 2] = 2; // Stop (bit1 0->1)
                bank_stopped[bank_idx] = true;
            }
        }

        // Now scan each configured counter: select via mode register and read outputs
        for (uint32_t i = 0; i < num_counters; i++)
        {
            const auto& config = counters[i];

            // Configure L1 MUX if needed before reading
            if (config.bank == CounterBank::L1)
            {
                uint32_t mux_ctrl_addr  = (RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL - RISCV_DEBUG_REGS_START_ADDR) / 4;
                uint32_t cur            = dbg_regs[mux_ctrl_addr];
                dbg_regs[mux_ctrl_addr] = (cur & ~(1u << 4)) | ((static_cast<uint32_t>(config.l1_mux) & 0x1u) << 4);
            }

            uint32_t counter_base     = get_counter_base_addr(config.bank);
            uint32_t counter_reg_addr = (counter_base - RISCV_DEBUG_REGS_START_ADDR) / 4;

            // Select the desired counter and req/grant output in mode register (global mode)
            dbg_regs[counter_reg_addr + 1] = (static_cast<uint32_t>(config.counter_id) << 8) | ((static_cast<uint32_t>(mode) & 0x1u) << 16);

            // Allow selection/mux to settle: perform a dummy read sequence
            uint32_t output_low_addr       = (get_counter_output_low_addr(config.bank) - RISCV_DEBUG_REGS_START_ADDR) / 4;
            uint32_t output_high_addr      = (get_counter_output_high_addr(config.bank) - RISCV_DEBUG_REGS_START_ADDR) / 4;
            volatile uint32_t dummy_cycles = dbg_regs[output_low_addr];
            volatile uint32_t dummy_count  = dbg_regs[output_high_addr];
            (void)dummy_cycles;
            (void)dummy_count;

            // Read outputs again for the actual value
            results[i].cycles     = dbg_regs[output_low_addr];
            results[i].count      = dbg_regs[output_high_addr];
            results[i].bank       = config.bank;
            results[i].counter_id = config.counter_id;

            // Write to L1 memory for Python to read (2 words per counter: cycles, count)
            data_mem[i * 2]     = results[i].cycles;
            data_mem[i * 2 + 1] = results[i].count;
        }
#pragma GCC diagnostic pop

        return results;
    }

    uint32_t size() const
    {
        return num_counters;
    }

    bool empty() const
    {
        return num_counters == 0;
    }
};

// RAII helper with ownership: starts on construction and stops on destruction.
class ScopedPerfCounters
{
private:
    PerfCounters counters_;
    bool stopped_ = false;

public:
    // Default constructor: own a PerfCounters and start immediately
    ScopedPerfCounters()
    {
        counters_.start();
    }

    ~ScopedPerfCounters()
    {
        if (!stopped_)
        {
            counters_.stop();
        }
    }

    // Access the underlying counters (e.g., to add/configure when kernel drives config)
    PerfCounters& counters()
    {
        return counters_;
    }

    // Manually stop once and return results; destructor will not stop again
    std::array<CounterResult, COUNTER_SLOT_COUNT> stop()
    {
        stopped_ = true;
        return counters_.stop();
    }

    // Non-copyable, non-movable to avoid lifetime confusion
    ScopedPerfCounters(const ScopedPerfCounters&)            = delete;
    ScopedPerfCounters& operator=(const ScopedPerfCounters&) = delete;
    ScopedPerfCounters(ScopedPerfCounters&&)                 = delete;
    ScopedPerfCounters& operator=(ScopedPerfCounters&&)      = delete;
};

} // namespace llk_perf
