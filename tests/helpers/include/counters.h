// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

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
enum class CounterBank : uint32_t
{
    INSTRN_THREAD = 0,
    FPU           = 1,
    TDMA_UNPACK   = 2,
    L1            = 3,
    TDMA_PACK     = 4,
};

// Number of counter banks represented by CounterBank enum
inline constexpr uint32_t COUNTER_BANK_COUNT = 5;

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

enum class CounterMode : uint32_t
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

namespace CounterId
{

namespace InstrnThread
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
} // namespace InstrnThread

namespace Fpu
{
constexpr uint32_t FPU_OP_VALID  = 0;
constexpr uint32_t SFPU_OP_VALID = 1;
} // namespace Fpu

namespace TdmaUnpack
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
} // namespace TdmaUnpack

namespace L1
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
} // namespace L1

namespace TdmaPack
{
constexpr uint32_t DSTAC_RDEN_RAW_0    = 0;
constexpr uint32_t DSTAC_RDEN_RAW_1    = 1;
constexpr uint32_t DSTAC_RDEN_RAW_2    = 2;
constexpr uint32_t DSTAC_RDEN_RAW_3    = 3;
constexpr uint32_t PACK_NOT_DEST_STALL = 4;
constexpr uint32_t PACK_NOT_SB_STALL   = 5;
constexpr uint32_t PACK_BUSY_10        = 6;
constexpr uint32_t PACK_BUSY_11        = 7;
} // namespace TdmaPack

} // namespace CounterId

class PerfCounters
{
private:
    struct CounterConfig
    {
        CounterBank bank;
        uint32_t counter_id;
        uint32_t mux_ctrl_bit4; // Only used for L1 counters
        uint32_t mode_bit;      // 0 = REQUESTS, 1 = GRANTS
    };

    CounterConfig counters[66];
    uint32_t counter_count;
    CounterMode mode;

    void write_metadata()
    {
#if defined(LLK_TRISC_UNPACK)
        volatile uint32_t* config_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_UNPACK_CONFIG_ADDR);
#elif defined(LLK_TRISC_MATH)
        volatile uint32_t* config_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_MATH_CONFIG_ADDR);
#elif defined(LLK_TRISC_PACK)
        volatile uint32_t* config_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_PACK_CONFIG_ADDR);
#else
        volatile uint32_t* config_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_MATH_CONFIG_ADDR);
#endif

        for (uint32_t i = 0; i < counter_count; i++)
        {
            const auto& config = counters[i];

            // Encode: [valid(31), mux_ctrl_bit4(17), mode(16), counter_id(8-15), bank(0-7)]
            uint32_t metadata = (1u << 31) | // Valid bit to distinguish from empty slots
                                (config.mux_ctrl_bit4 << 17) | (static_cast<uint32_t>(mode) << 16) | (config.counter_id << 8) |
                                static_cast<uint32_t>(config.bank);

            config_mem[i] = metadata;
        }

        // Clear remaining slots
        for (uint32_t i = counter_count; i < 66; i++)
        {
            config_mem[i] = 0;
        }
    }

public:
    PerfCounters() : counter_count(0), mode(CounterMode::GRANTS)
    {
        for (uint32_t i = 0; i < 30; i++)
        {
            // Initialize with a default mode_bit aligned to current mode (GRANTS by default)
            counters[i] = {CounterBank::INSTRN_THREAD, 0, 0, static_cast<uint32_t>(mode)};
        }
    }

    /**
     * Add a counter to measure
     *
     * @param bank Counter bank (INSTRN_THREAD, FPU, TDMA_UNPACK, L1, TDMA_PACK)
     * @param counter_id Counter ID within the bank (0-28 for INSTRN_THREAD, etc.)
     * @param mux_ctrl_bit4 Only for L1 counters: selects counter set (0 or 1)
     */
    void add(CounterBank bank, uint32_t counter_id, uint32_t mux_ctrl_bit4 = 0)
    {
        if (counter_count >= 30)
        {
            return; // Max 10 counters
        }

        // Initialize mode_bit to current mode (0=REQUESTS, 1=GRANTS)
        counters[counter_count++] = {bank, counter_id, mux_ctrl_bit4, static_cast<uint32_t>(mode)};
    }

    /**
     * Set counter mode (GRANTS or REQUESTS)
     */
    void set_mode(CounterMode m)
    {
        mode = m;
        // Keep existing counter configs in sync with the selected mode
        for (uint32_t i = 0; i < counter_count; i++)
        {
            counters[i].mode_bit = static_cast<uint32_t>(mode);
        }
    }

    /**
     * Start all configured counters
     * Reads configuration from L1 memory (set by Python or C++)
     */
    void start()
    {
        // Read configuration from L1 (may have been set by Python or C++)
#if defined(LLK_TRISC_UNPACK)
        volatile uint32_t* config_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_UNPACK_CONFIG_ADDR);
#elif defined(LLK_TRISC_MATH)
        volatile uint32_t* config_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_MATH_CONFIG_ADDR);
#elif defined(LLK_TRISC_PACK)
        volatile uint32_t* config_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_PACK_CONFIG_ADDR);
#else
        volatile uint32_t* config_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_MATH_CONFIG_ADDR);
#endif

        // If counters were added via C++ add(), write metadata to L1
        if (counter_count > 0)
        {
            write_metadata();
        }

        // Count how many valid counters are configured (check bit 31)
        uint32_t active_count = 0;
        for (uint32_t i = 0; i < 66; i++)
        {
            if ((config_mem[i] & 0x80000000) != 0)
            {
                active_count++;
            }
        }

        // If no counters configured, update counter_count and return
        if (active_count == 0)
        {
            counter_count = 0;
            return;
        }

        // Update counter_count to match what's in L1
        counter_count = active_count;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
        volatile uint32_t* dbg_regs = reinterpret_cast<volatile uint32_t*>(RISCV_DEBUG_REGS_START_ADDR);

        // First decode configs and store locally
        uint32_t counter_idx = 0; // Separate index for storing configs
        for (uint32_t i = 0; i < 66 && counter_idx < 66; i++)
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

            counters[counter_idx].bank          = static_cast<CounterBank>(bank_id);
            counters[counter_idx].counter_id    = counter_id_val;
            counters[counter_idx].mux_ctrl_bit4 = mux_ctrl_val;
            counters[counter_idx].mode_bit      = mode_bit;
            counter_idx++;
        }

        // Avoid repeated resets/starts per bank
        bool bank_started[COUNTER_BANK_COUNT] = {false, false, false, false, false};
        for (uint32_t i = 0; i < counter_idx; i++)
        {
            const auto& config = counters[i];

            // Configure L1 MUX if needed (doesn't affect counting, but safe)
            if (config.bank == CounterBank::L1)
            {
                uint32_t mux_ctrl_addr  = (RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL - RISCV_DEBUG_REGS_START_ADDR) / 4;
                uint32_t cur            = dbg_regs[mux_ctrl_addr];
                dbg_regs[mux_ctrl_addr] = (cur & ~(1u << 4)) | ((config.mux_ctrl_bit4 & 0x1u) << 4);
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
            }
        }
#pragma GCC diagnostic pop
    }

    /**
     * Stop all counters and return results
     *
     * @return Array of counter results (cycles, count for each counter)
     */
    CounterResult* stop()
    {
        static CounterResult results[66];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
        volatile uint32_t* dbg_regs = reinterpret_cast<volatile uint32_t*>(RISCV_DEBUG_REGS_START_ADDR);

#if defined(LLK_TRISC_UNPACK)
        volatile uint32_t* data_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_UNPACK_DATA_ADDR);
#elif defined(LLK_TRISC_MATH)
        volatile uint32_t* data_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_MATH_DATA_ADDR);
#elif defined(LLK_TRISC_PACK)
        volatile uint32_t* data_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_PACK_DATA_ADDR);
#else
        volatile uint32_t* data_mem = reinterpret_cast<volatile uint32_t*>(PERF_COUNTER_MATH_DATA_ADDR);
#endif

        // Stop all banks once via 0->1 transition on stop bit
        bool bank_stopped[COUNTER_BANK_COUNT] = {false, false, false, false, false};
        for (uint32_t i = 0; i < counter_count; i++)
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
        for (uint32_t i = 0; i < counter_count; i++)
        {
            const auto& config = counters[i];

            // Configure L1 MUX if needed before reading
            if (config.bank == CounterBank::L1)
            {
                uint32_t mux_ctrl_addr  = (RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL - RISCV_DEBUG_REGS_START_ADDR) / 4;
                uint32_t cur            = dbg_regs[mux_ctrl_addr];
                dbg_regs[mux_ctrl_addr] = (cur & ~(1u << 4)) | ((config.mux_ctrl_bit4 & 0x1u) << 4);
            }

            uint32_t counter_base     = get_counter_base_addr(config.bank);
            uint32_t counter_reg_addr = (counter_base - RISCV_DEBUG_REGS_START_ADDR) / 4;

            // Select the desired counter and req/grant output in mode register
            dbg_regs[counter_reg_addr + 1] = (config.counter_id << 8) | ((config.mode_bit & 0x1u) << 16);

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

    uint32_t get_count() const
    {
        return counter_count;
    }
};

} // namespace llk_perf
