// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel.h"

/**
 * @file perf_counters.h
 * @brief Hardware Performance Counter Control Interface for Tensix
 *
 * This header provides an API for controlling and reading hardware performance counters
 * on Tensix cores. Performance counters are organized into five categories:
 * - FPU: Floating Point Unit counters
 * - L1: L1 memory access counters
 * - INSTRN_THREAD: Instruction thread counters
 * - TDMA_UNPACK: TDMA Unpack counters
 * - TDMA_PACK: TDMA Pack counters
 *
 * Each category has:
 * - Three configuration registers (reference period, mode, start/stop)
 * - Two output registers (cycle count, counter value)
 */

namespace llk_perf
{

// ========================================================================
// Debug Register Addresses for Performance Counters
// ========================================================================

// Note: Many register addresses are already defined in tensix.h
// We only define the missing output registers here

// FPU Performance Counter Registers
// RISCV_DEBUG_REG_PERF_CNT_FPU0, FPU1, FPU2 defined in tensix.h
#ifndef RISCV_DEBUG_REG_PERF_CNT_OUT_L_FPU
constexpr uint32_t RISCV_DEBUG_REG_PERF_CNT_OUT_L_FPU = 0xFFB12120; // Cycle Count Output
#endif
#ifndef RISCV_DEBUG_REG_PERF_CNT_OUT_H_FPU
constexpr uint32_t RISCV_DEBUG_REG_PERF_CNT_OUT_H_FPU = 0xFFB12124; // Counter Value Output
#endif

// L1 Performance Counter Registers
// RISCV_DEBUG_REG_PERF_CNT_L1_0, L1_1, L1_2 defined in tensix.h
#ifndef RISCV_DEBUG_REG_PERF_CNT_OUT_L_DBG_L1
constexpr uint32_t RISCV_DEBUG_REG_PERF_CNT_OUT_L_DBG_L1 = 0xFFB12118; // Cycle Count Output
#endif
#ifndef RISCV_DEBUG_REG_PERF_CNT_OUT_H_DBG_L1
constexpr uint32_t RISCV_DEBUG_REG_PERF_CNT_OUT_H_DBG_L1 = 0xFFB1211C; // Counter Value Output
#endif

// Instruction Thread Performance Counter Registers
// RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0, THREAD1, THREAD2 defined in tensix.h
#ifndef RISCV_DEBUG_REG_PERF_CNT_OUT_L_INSTRN_THREAD
constexpr uint32_t RISCV_DEBUG_REG_PERF_CNT_OUT_L_INSTRN_THREAD = 0xFFB12100; // Cycle Count Output
#endif
#ifndef RISCV_DEBUG_REG_PERF_CNT_OUT_H_INSTRN_THREAD
constexpr uint32_t RISCV_DEBUG_REG_PERF_CNT_OUT_H_INSTRN_THREAD = 0xFFB12104; // Counter Value Output
#endif

// TDMA Unpack Performance Counter Registers
#ifndef RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0
constexpr uint32_t RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0 = 0xFFB1200C; // Reference period
#endif
#ifndef RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK1
constexpr uint32_t RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK1 = 0xFFB12010; // Mode
#endif
#ifndef RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2
constexpr uint32_t RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2 = 0xFFB12014; // Start/Stop
#endif
#ifndef RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_UNPACK
constexpr uint32_t RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_UNPACK = 0xFFB12018; // Cycle Count Output
#endif
#ifndef RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_UNPACK
constexpr uint32_t RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_UNPACK = 0xFFB1201C; // Counter Value Output
#endif

// TDMA Pack Performance Counter Registers
// RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0, PACK1, PACK2 defined in tensix.h
#ifndef RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_PACK
constexpr uint32_t RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_PACK = 0xFFB12110; // Cycle Count Output
#endif
#ifndef RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_PACK
constexpr uint32_t RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_PACK = 0xFFB12114; // Counter Value Output
#endif

// Special Control Registers
// RISCV_DEBUG_REG_PERF_CNT_ALL defined in tensix.h
#ifndef RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL
constexpr uint32_t RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL = 0xFFB12218; // L1 counter mux control
#endif

// ========================================================================
// Counter Categories
// ========================================================================

enum class CounterCategory
{
    FPU,
    L1,
    INSTRN_THREAD,
    TDMA_UNPACK,
    TDMA_PACK
};

// ========================================================================
// Counter Mode Configuration
// ========================================================================

/**
 * @brief Counter operation modes
 *
 * - CONTINUOUS: Counter runs until explicitly stopped
 * - PERIOD_BASED: Counter stops automatically after reference_period cycles
 * - DISABLED: Counter is disabled
 */
enum class CounterMode : uint32_t
{
    CONTINUOUS   = 0x0,
    PERIOD_BASED = 0x1,
    DISABLED     = 0x2
};

// ========================================================================
// Performance Counter Configuration
// ========================================================================

/**
 * @brief Configuration for a performance counter
 */
struct CounterConfig
{
    CounterMode mode;          ///< Counter operation mode
    uint32_t counter_sel;      ///< Counter selection (0-255)
    uint32_t reference_period; ///< Reference period (only used in PERIOD_BASED mode)
    bool track_grants;         ///< false = track requests, true = track grants (bit 16 of mode register)

    CounterConfig() : mode(CounterMode::CONTINUOUS), counter_sel(0), reference_period(0), track_grants(true) // Default to grants (completed work) not requests
    {
    }
};

// ========================================================================
// Performance Counter Results
// ========================================================================

/**
 * @brief Results from reading a performance counter
 */
struct CounterResult
{
    uint32_t cycle_count;   ///< Total cycles counted (reference period)
    uint32_t counter_value; ///< Counter-specific value (event count)
};

// ========================================================================
// Performance Counter Control Class
// ========================================================================

/**
 * @brief Main class for controlling hardware performance counters
 *
 * Usage:
 * 1. Configure counter: set_config()
 * 2. Start counting: start()
 * 3. ... do work ...
 * 4. Stop counting: stop()
 * 5. Read results: read()
 *
 * You can also change counter selection after stopping to read multiple
 * counters from the same group without re-running the measurement.
 */
class PerformanceCounter
{
private:
    CounterCategory category_;
    CounterConfig config_;

    // Register addresses for this counter category
    uint32_t reg_period_addr_;
    uint32_t reg_mode_addr_;
    uint32_t reg_control_addr_;
    uint32_t reg_out_l_addr_;
    uint32_t reg_out_h_addr_;

    void init_addresses()
    {
        switch (category_)
        {
            case CounterCategory::FPU:
                reg_period_addr_  = RISCV_DEBUG_REG_PERF_CNT_FPU0;
                reg_mode_addr_    = RISCV_DEBUG_REG_PERF_CNT_FPU1;
                reg_control_addr_ = RISCV_DEBUG_REG_PERF_CNT_FPU2;
                reg_out_l_addr_   = RISCV_DEBUG_REG_PERF_CNT_OUT_L_FPU;
                reg_out_h_addr_   = RISCV_DEBUG_REG_PERF_CNT_OUT_H_FPU;
                break;

            case CounterCategory::L1:
                reg_period_addr_  = RISCV_DEBUG_REG_PERF_CNT_L1_0;
                reg_mode_addr_    = RISCV_DEBUG_REG_PERF_CNT_L1_1;
                reg_control_addr_ = RISCV_DEBUG_REG_PERF_CNT_L1_2;
                reg_out_l_addr_   = RISCV_DEBUG_REG_PERF_CNT_OUT_L_DBG_L1;
                reg_out_h_addr_   = RISCV_DEBUG_REG_PERF_CNT_OUT_H_DBG_L1;
                break;

            case CounterCategory::INSTRN_THREAD:
                reg_period_addr_  = RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0;
                reg_mode_addr_    = RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD1;
                reg_control_addr_ = RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD2;
                reg_out_l_addr_   = RISCV_DEBUG_REG_PERF_CNT_OUT_L_INSTRN_THREAD;
                reg_out_h_addr_   = RISCV_DEBUG_REG_PERF_CNT_OUT_H_INSTRN_THREAD;
                break;

            case CounterCategory::TDMA_UNPACK:
                reg_period_addr_  = RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0;
                reg_mode_addr_    = RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK1;
                reg_control_addr_ = RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2;
                reg_out_l_addr_   = RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_UNPACK;
                reg_out_h_addr_   = RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_UNPACK;
                break;

            case CounterCategory::TDMA_PACK:
                reg_period_addr_  = RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0;
                reg_mode_addr_    = RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK1;
                reg_control_addr_ = RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK2;
                reg_out_l_addr_   = RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_PACK;
                reg_out_h_addr_   = RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_PACK;
                break;
        }
    }

public:
    /**
     * @brief Construct a performance counter for a specific category
     */
    explicit PerformanceCounter(CounterCategory category) : category_(category)
    {
        init_addresses();
    }

    /**
     * @brief Configure the counter (must be called before start)
     *
     * @param config Counter configuration
     *
     * Note: Changing configuration does NOT reset the counter. Call start() to reset and begin counting.
     */
    void set_config(const CounterConfig& config)
    {
        config_ = config;

        // Write reference period register
        ckernel::reg_write(reg_period_addr_, config.reference_period);

        // Build mode register value
        // Bits [7:0]   = mode
        // Bits [15:8]  = counter_sel (8 bits, 0-255)
        // Bit  [16]    = request/grant selection (0 = requests, 1 = grants)
        uint32_t mode_val = static_cast<uint32_t>(config.mode) & 0xFF;
        mode_val |= (config.counter_sel & 0xFF) << 8;
        if (config.track_grants)
        {
            mode_val |= (1 << 16);
        }

        ckernel::reg_write(reg_mode_addr_, mode_val);
    }

    /**
     * @brief Start counting (resets counter to 0)
     *
     * Issues a 0-to-1 transition on the start bit.
     * All counters in this category group will start.
     */
    void start()
    {
        // Clear both start and stop bits
        ckernel::reg_write(reg_control_addr_, 0x0);

        // Set start bit (bit 0) to 1
        ckernel::reg_write(reg_control_addr_, 0x1);
    }

    /**
     * @brief Stop counting (freezes counter value)
     *
     * Issues a 0-to-1 transition on the stop bit.
     * All counters in this category group will stop.
     */
    void stop()
    {
        // Clear both start and stop bits
        ckernel::reg_write(reg_control_addr_, 0x0);

        // Set stop bit (bit 1) to 1
        ckernel::reg_write(reg_control_addr_, 0x2);
    }

    /**
     * @brief Read counter results
     *
     * Can be called while counter is running (returns value since last start)
     * or after stopping (returns final value).
     *
     * @return Counter results (cycle count and counter value)
     */
    CounterResult read() const
    {
        CounterResult result;
        result.cycle_count   = ckernel::reg_read(reg_out_l_addr_);
        result.counter_value = ckernel::reg_read(reg_out_h_addr_);
        return result;
    }

    /**
     * @brief Change counter selection and read results
     *
     * Useful for reading multiple counters from the same group after stopping.
     *
     * @param counter_sel New counter selection
     * @return Counter results for the selected counter
     */
    CounterResult read_counter(uint32_t counter_sel)
    {
        // Update counter selection in mode register (preserve other fields)
        uint32_t mode_val = static_cast<uint32_t>(config_.mode) & 0xFF;
        mode_val |= (counter_sel & 0xFF) << 8;
        if (config_.track_grants)
        {
            mode_val |= (1 << 16);
        }

        ckernel::reg_write(reg_mode_addr_, mode_val);

        return read();
    }

    /**
     * @brief Get current configuration
     */
    const CounterConfig& get_config() const
    {
        return config_;
    }

    /**
     * @brief Get counter category
     */
    CounterCategory get_category() const
    {
        return category_;
    }
};

// ========================================================================
// Global Start/Stop Functions
// ========================================================================

/**
 * @brief Start all FPU and INSTRN_THREAD counters simultaneously
 *
 * Uses the special PERF_CNT_ALL register to start multiple counter groups.
 */
inline void start_all_counters()
{
    ckernel::reg_write(RISCV_DEBUG_REG_PERF_CNT_ALL, 0x0);
    ckernel::reg_write(RISCV_DEBUG_REG_PERF_CNT_ALL, 0x1);
}

/**
 * @brief Stop all FPU and INSTRN_THREAD counters simultaneously
 *
 * Uses the special PERF_CNT_ALL register to stop multiple counter groups.
 */
inline void stop_all_counters()
{
    ckernel::reg_write(RISCV_DEBUG_REG_PERF_CNT_ALL, 0x0);
    ckernel::reg_write(RISCV_DEBUG_REG_PERF_CNT_ALL, 0x2);
}

// ========================================================================
// L1 Counter Special Configuration
// ========================================================================

/**
 * @brief Set L1 counter mux control
 *
 * The L1 counters are multiplexed. Bit 4 of the MUX_CTRL register
 * selects between two sets of 8 counters each (total 16 counters).
 *
 * @param use_upper_bank true = use counters 8-15, false = use counters 0-7
 */
inline void set_l1_counter_bank(bool use_upper_bank)
{
    uint32_t mux_val = use_upper_bank ? (1 << 4) : 0;
    ckernel::reg_write(RISCV_DEBUG_REG_PERF_CNT_MUX_CTRL, mux_val);
}

// ========================================================================
// Common Counter Definitions (for convenience)
// ========================================================================

namespace counters
{
// FPU Counters
namespace fpu
{
constexpr uint32_t FPU_OP_VALID  = 0; // th_fpu_op_valid
constexpr uint32_t SFPU_OP_VALID = 1; // th_sfpu_op_valid_s1
} // namespace fpu

// Instruction Thread Counters
namespace instrn_thread
{
// Instruction type counters (Mode bits 15:8)
constexpr uint32_t INST_CFG     = 0; // inst_cfg
constexpr uint32_t INST_SYNC    = 1; // inst_sync
constexpr uint32_t INST_THCON   = 2; // inst_thcon
constexpr uint32_t INST_XSEARCH = 3; // inst_xsearch
constexpr uint32_t INST_MOVE    = 4; // inst_move
constexpr uint32_t INST_MATH    = 5; // inst_math
constexpr uint32_t INST_UNPACK  = 6; // inst_unpack
constexpr uint32_t INST_PACK    = 7; // inst_pack
constexpr uint32_t STALLED      = 8; // stalled

// Stall reason counters - SrcA cleared
constexpr uint32_t SRCA_CLEARED_0 = 9;  // stall_rsn_srca_cleared[0] - TRISC0
constexpr uint32_t SRCA_CLEARED_1 = 10; // stall_rsn_srca_cleared[1] - TRISC1
constexpr uint32_t SRCA_CLEARED_2 = 11; // stall_rsn_srca_cleared[2] - TRISC2

// Stall reason counters - SrcB cleared
constexpr uint32_t SRCB_CLEARED_0 = 12; // stall_rsn_srcb_cleared[0] - TRISC0
constexpr uint32_t SRCB_CLEARED_1 = 13; // stall_rsn_srcb_cleared[1] - TRISC1
constexpr uint32_t SRCB_CLEARED_2 = 14; // stall_rsn_srcb_cleared[2] - TRISC2

// Stall reason counters - SrcA valid
constexpr uint32_t SRCA_VALID_0 = 15; // stall_rsn_srca_valid[0] - TRISC0
constexpr uint32_t SRCA_VALID_1 = 16; // stall_rsn_srca_valid[1] - TRISC1
constexpr uint32_t SRCA_VALID_2 = 17; // stall_rsn_srca_valid[2] - TRISC2

// Stall reason counters - SrcB valid
constexpr uint32_t SRCB_VALID_0 = 18; // stall_rsn_srcb_valid[0] - TRISC0
constexpr uint32_t SRCB_VALID_1 = 19; // stall_rsn_srcb_valid[1] - TRISC1
constexpr uint32_t SRCB_VALID_2 = 20; // stall_rsn_srcb_valid[2] - TRISC2

// Other stall reasons
constexpr uint32_t STALL_THCON            = 21; // stall_rsn_thcon
constexpr uint32_t STALL_PACK0            = 22; // stall_rsn_pack0
constexpr uint32_t STALL_MATH             = 23; // stall_rsn_math
constexpr uint32_t STALL_SEM_ZERO         = 24; // rsn_sem_zero
constexpr uint32_t STALL_SEM_MAX          = 25; // rsn_sem_max
constexpr uint32_t STALL_MOVE             = 26; // stall_rsn_move
constexpr uint32_t STALL_TRISC_REG_ACCESS = 27; // stall_rsn_trisc_reg_access
constexpr uint32_t STALL_SFPU             = 28; // stall_rsn_sfpu
} // namespace instrn_thread

// TDMA Unpack Counters
namespace tdma_unpack
{
// Request counters (Mode bit 16 = 0)
constexpr uint32_t MATH_INSTR_SRC_READY = 0;  // math_instrn_valid & dec_instr_alu & src_data_ready
constexpr uint32_t MATH_NOT_D2A_STALL   = 1;  // math_instrn_valid & ~dest2src_post_stall
constexpr uint32_t MATH_FIDELITY_PHASES = 2;  // math_instrn_valid & fidelity_phases_ongoing
constexpr uint32_t MATH_INSTR_BUF_RDEN  = 3;  // o_math_instrnbuf_rden
constexpr uint32_t MATH_INSTR_VALID     = 4;  // math_instrn_valid
constexpr uint32_t TDMA_SRCB_REGIF_WREN = 5;  // tdma_srcb_regif_wren
constexpr uint32_t TDMA_SRCA_REGIF_WREN = 6;  // tdma_srca_regif_wren
constexpr uint32_t UNPACK_BUSY_0        = 7;  // tdma_unpack_busy[0]
constexpr uint32_t UNPACK_BUSY_1        = 8;  // tdma_unpack_busy[1]
constexpr uint32_t UNPACK_BUSY_2        = 9;  // tdma_unpack_busy[2]
constexpr uint32_t UNPACK_BUSY_3        = 10; // tdma_unpack_busy[3]

// Grant counters (Mode bit 16 = 1) - see spec for grant conditions
} // namespace tdma_unpack

// TDMA Pack Counters
namespace tdma_pack
{
// Request counters (Mode bit 16 = 0)
constexpr uint32_t DSTAC_RDEN_RAW_0    = 0; // tdma_dstac_regif_rden_raw[0]
constexpr uint32_t DSTAC_RDEN_RAW_1    = 1; // tdma_dstac_regif_rden_raw[1]
constexpr uint32_t DSTAC_RDEN_RAW_2    = 2; // tdma_dstac_regif_rden_raw[2]
constexpr uint32_t DSTAC_RDEN_RAW_3    = 3; // tdma_dstac_regif_rden_raw[3]
constexpr uint32_t PACK_NOT_DEST_STALL = 4; // Inst issue not stalled by dest write port
constexpr uint32_t PACK_NOT_SB_STALL   = 5; // Inst issue not stalled by scoreboard
constexpr uint32_t PACK_BUSY_10        = 6; // tdma_pack_busy[10]
constexpr uint32_t PACK_BUSY_11        = 7; // tdma_pack_busy[11]

// Grant counters (Mode bit 16 = 1) - see spec for grant conditions
} // namespace tdma_pack

// L1 Counters (require MUX_CTRL configuration)
namespace l1
{
// Lower bank (MUX_CTRL bit 4 = 0)
constexpr uint32_t UNPACK_NO_ARB        = 7; // #0 - L1 no-arbitration unpacker #0
constexpr uint32_t UNPACK_ARB_1         = 6; // #1 - L1 arbitration unpacker #1
constexpr uint32_t TDMA_BUNDLE_0_ARB    = 5; // #2 - L1 arbitration TDMA bundle 0
constexpr uint32_t TDMA_BUNDLE_1_ARB    = 4; // #3 - L1 arbitration TDMA bundle 1
constexpr uint32_t NOC_RING0_OUTGOING_0 = 3; // #4 - NOC ring0 L1 outgoing0
constexpr uint32_t NOC_RING0_OUTGOING_1 = 2; // #5 - NOC ring0 L1 outgoing1
constexpr uint32_t NOC_RING0_INCOMING_0 = 1; // #6 - NOC ring0 L1 incoming0
constexpr uint32_t NOC_RING0_INCOMING_1 = 0; // #7 - NOC ring0 L1 incoming1

// Upper bank (MUX_CTRL bit 4 = 1)
constexpr uint32_t TDMA_PACKER_2_WR     = 7; // #8 - TDMA packer 2 write to L1
constexpr uint32_t TDMA_EXT_UNPACK_9    = 6; // #9 - TDMA extended unpacker
constexpr uint32_t TDMA_EXT_UNPACK_10   = 5; // #10 - TDMA extended unpacker
constexpr uint32_t TDMA_EXT_UNPACK_11   = 4; // #11 - TDMA extended unpacker
constexpr uint32_t NOC_RING1_OUTGOING_0 = 3; // #12 - NOC ring1 L1 outgoing0
constexpr uint32_t NOC_RING1_OUTGOING_1 = 2; // #13 - NOC ring1 L1 outgoing1
constexpr uint32_t NOC_RING1_INCOMING_0 = 1; // #14 - NOC ring1 L1 incoming0
constexpr uint32_t NOC_RING1_INCOMING_1 = 0; // #15 - NOC ring1 L1 incoming1
} // namespace l1

} // namespace counters

// ========================================================================
// Automatic Performance Profiler
// ========================================================================

/**
 * @brief Comprehensive automatic performance profiler
 *
 * Call start_profiling() at the beginning of your kernel and stop_profiling() at the end.
 * All counter data is automatically written to L1 memory for Python analysis.
 *
 * Usage:
 *   llk_perf::start_profiling();
 *   // ... your kernel code ...
 *   llk_perf::stop_profiling();
 */

// BRILLIANT IDEA: Use spare CFG registers instead of L1 memory!
// CFG registers 100-131 are available (32 registers × 4 bytes = 128 bytes for results)
// Benefits:
//   1. Infinite space - CFG has hundreds of unused registers
//   2. No L1 memory needed - saves precious L1 bandwidth
//   3. Directly readable from Python via debug interface
//   4. Writing to CFG is just a single instruction - NO code bloat!
constexpr uint32_t PERF_COUNTER_CFG_BASE = 100;

// Counter result indices in CFG registers (starting at PERF_COUNTER_CFG_BASE)
namespace perf_indices
{
// Instruction counters (6 values × 2 = 12 CFG registers)
constexpr uint32_t INST_UNPACK_CYCLES = PERF_COUNTER_CFG_BASE + 0;
constexpr uint32_t INST_UNPACK_COUNT  = PERF_COUNTER_CFG_BASE + 1;
constexpr uint32_t INST_MATH_CYCLES   = PERF_COUNTER_CFG_BASE + 2;
constexpr uint32_t INST_MATH_COUNT    = PERF_COUNTER_CFG_BASE + 3;
constexpr uint32_t INST_PACK_CYCLES   = PERF_COUNTER_CFG_BASE + 4;
constexpr uint32_t INST_PACK_COUNT    = PERF_COUNTER_CFG_BASE + 5;
constexpr uint32_t INST_SYNC_CYCLES   = PERF_COUNTER_CFG_BASE + 6;
constexpr uint32_t INST_SYNC_COUNT    = PERF_COUNTER_CFG_BASE + 7;
constexpr uint32_t INST_CFG_CYCLES    = PERF_COUNTER_CFG_BASE + 8;
constexpr uint32_t INST_CFG_COUNT     = PERF_COUNTER_CFG_BASE + 9;
constexpr uint32_t INST_MOVE_CYCLES   = PERF_COUNTER_CFG_BASE + 10;
constexpr uint32_t INST_MOVE_COUNT    = PERF_COUNTER_CFG_BASE + 11;

// TDMA counters (4 values × 2 = 8 CFG registers)
constexpr uint32_t UNPACK_DMA_CYCLES       = PERF_COUNTER_CFG_BASE + 12;
constexpr uint32_t UNPACK_DMA_BUSY         = PERF_COUNTER_CFG_BASE + 13;
constexpr uint32_t PACK_DMA_CYCLES         = PERF_COUNTER_CFG_BASE + 14;
constexpr uint32_t PACK_DMA_BUSY           = PERF_COUNTER_CFG_BASE + 15;
constexpr uint32_t MATH_INSTR_VALID_CYCLES = PERF_COUNTER_CFG_BASE + 16;
constexpr uint32_t MATH_INSTR_VALID_COUNT  = PERF_COUNTER_CFG_BASE + 17;
constexpr uint32_t SRCB_WREN_CYCLES        = PERF_COUNTER_CFG_BASE + 18;
constexpr uint32_t SRCB_WREN_COUNT         = PERF_COUNTER_CFG_BASE + 19;

// Stall counters (6 values × 2 = 12 CFG registers)
constexpr uint32_t SRCA_STALL_CYCLES   = PERF_COUNTER_CFG_BASE + 20;
constexpr uint32_t SRCA_STALL_COUNT    = PERF_COUNTER_CFG_BASE + 21;
constexpr uint32_t SRCB_STALL_CYCLES   = PERF_COUNTER_CFG_BASE + 22;
constexpr uint32_t SRCB_STALL_COUNT    = PERF_COUNTER_CFG_BASE + 23;
constexpr uint32_t DEST_STALL_CYCLES   = PERF_COUNTER_CFG_BASE + 24;
constexpr uint32_t DEST_STALL_COUNT    = PERF_COUNTER_CFG_BASE + 25;
constexpr uint32_t MATH_STALL_CYCLES   = PERF_COUNTER_CFG_BASE + 26;
constexpr uint32_t MATH_STALL_COUNT    = PERF_COUNTER_CFG_BASE + 27;
constexpr uint32_t PACK_STALL_CYCLES   = PERF_COUNTER_CFG_BASE + 28;
constexpr uint32_t PACK_STALL_COUNT    = PERF_COUNTER_CFG_BASE + 29;
constexpr uint32_t UNPACK_STALL_CYCLES = PERF_COUNTER_CFG_BASE + 30;
constexpr uint32_t UNPACK_STALL_COUNT  = PERF_COUNTER_CFG_BASE + 31;

constexpr uint32_t TOTAL_CFGS = 32;
} // namespace perf_indices

// No state needed - using direct register access!

/**
 * @brief Start lightweight performance profiling
 *
 * Call this at the start of your kernel. It automatically configures and starts
 * essential counters for the current thread (minimal code footprint).
 */
inline void start_profiling()
{
    // NO C++ OBJECTS - Direct register writes only!
    // Measure EVERYTHING with minimal code footprint
    volatile uint32_t* dbg_regs = reinterpret_cast<volatile uint32_t*>(RISCV_DEBUG_REGS_START_ADDR);

#ifdef LLK_TRISC_UNPACK
    // Use ALL 5 counter banks! FPU, L1, INSTRN_THREAD, TDMA_UNPACK, TDMA_PACK

    // Counter 1: INST_UNPACK (INSTRN_THREAD)
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::instrn_thread::INST_UNPACK << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;

    // Counter 2: INST_SYNC (FPU bank - repurposed for INSTRN_THREAD)
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::instrn_thread::INST_SYNC << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;

    // Counter 3: UNPACK_BUSY (TDMA_UNPACK)
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::tdma_unpack::UNPACK_BUSY_0 << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;

    // Counter 4: SRCA_VALID (stalls waiting for data) (L1 bank)
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::instrn_thread::SRCA_VALID_0 << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;

    // Counter 5: SRCB_VALID (TDMA_PACK bank repurposed)
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::instrn_thread::SRCB_VALID_0 << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;
#endif

#ifdef LLK_TRISC_MATH
    // Counter 1: INST_MATH
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::instrn_thread::INST_MATH << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;

    // Counter 2: INST_CFG
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::instrn_thread::INST_CFG << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;

    // Counter 3: MATH_INSTR_VALID
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::tdma_unpack::MATH_INSTR_VALID << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;

    // Counter 4: DEST_STALL
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::instrn_thread::SRCB_VALID_1 << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;

    // Counter 5: MATH_STALL
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::instrn_thread::SRCA_VALID_1 << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;
#endif

#ifdef LLK_TRISC_PACK
    // Counter 1: INST_PACK
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::instrn_thread::INST_PACK << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;

    // Counter 2: INST_MOVE
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::instrn_thread::INST_MOVE << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;

    // Counter 3: PACK_DMA
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::tdma_pack::DSTAC_RDEN_RAW_0 << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;

    // Counter 4: PACK_STALL
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::instrn_thread::SRCB_VALID_2 << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;

    // Counter 5: SRCB read stalls
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK0 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0xFFFFFFFF;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK1 - RISCV_DEBUG_REGS_START_ADDR) / 4] = (counters::instrn_thread::SRCA_VALID_2 << 8) | (1 << 16);
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 1;
#endif
}

/**
 * @brief Stop comprehensive performance profiling and write results to L1
 *
 * Call this at the end of your kernel. All results are automatically written
 * to L1 memory for Python analysis.
 */
inline void stop_profiling()
{
    // Stop counters, read results, write to CFG registers
    volatile uint32_t* dbg_regs = reinterpret_cast<volatile uint32_t*>(RISCV_DEBUG_REGS_START_ADDR);

#ifdef LLK_TRISC_UNPACK
    // Stop ALL 5 counters
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU2 - RISCV_DEBUG_REGS_START_ADDR) / 4]           = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4]   = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_2 - RISCV_DEBUG_REGS_START_ADDR) / 4]           = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4]     = 0;

    // Read ALL 5 counter results and write to L1
    volatile uint32_t* l1_mem = reinterpret_cast<volatile uint32_t*>(0x2F800);
    uint32_t cycles, count;

    // INST_UNPACK
    cycles    = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_INSTRN_THREAD - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_INSTRN_THREAD - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[0] = cycles;
    l1_mem[1] = count;

    // INST_SYNC
    cycles    = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_FPU - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_FPU - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[6] = cycles;
    l1_mem[7] = count;

    // UNPACK_DMA
    cycles     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_UNPACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count      = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_UNPACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[12] = cycles;
    l1_mem[13] = count;

    // SRCA_VALID (stalls)
    cycles     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_DBG_L1 - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count      = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_DBG_L1 - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[20] = cycles;
    l1_mem[21] = count;

    // SRCB_VALID (stalls)
    cycles     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_PACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count      = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_PACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[22] = cycles;
    l1_mem[23] = count;
#endif

#ifdef LLK_TRISC_MATH
    // Stop ALL 5 counters
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU2 - RISCV_DEBUG_REGS_START_ADDR) / 4]           = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4]   = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_2 - RISCV_DEBUG_REGS_START_ADDR) / 4]           = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4]     = 0;

    // Read ALL 5 counter results and write to L1
    volatile uint32_t* l1_mem = reinterpret_cast<volatile uint32_t*>(0x2F800);
    uint32_t cycles, count;

    // INST_MATH
    cycles    = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_INSTRN_THREAD - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_INSTRN_THREAD - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[2] = cycles;
    l1_mem[3] = count;

    // INST_CFG
    cycles    = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_FPU - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_FPU - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[8] = cycles;
    l1_mem[9] = count;

    // MATH_INSTR_VALID
    cycles     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_UNPACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count      = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_UNPACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[16] = cycles;
    l1_mem[17] = count;

    // DEST_STALL
    cycles     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_DBG_L1 - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count      = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_DBG_L1 - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[24] = cycles;
    l1_mem[25] = count;

    // MATH_STALL
    cycles     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_PACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count      = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_PACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[26] = cycles;
    l1_mem[27] = count;
#endif

#ifdef LLK_TRISC_PACK
    // Stop ALL 5 counters
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_INSTRN_THREAD2 - RISCV_DEBUG_REGS_START_ADDR) / 4] = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_FPU2 - RISCV_DEBUG_REGS_START_ADDR) / 4]           = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_PACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4]     = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_L1_2 - RISCV_DEBUG_REGS_START_ADDR) / 4]           = 0;
    dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_TDMA_UNPACK2 - RISCV_DEBUG_REGS_START_ADDR) / 4]   = 0;

    // Read ALL 5 counter results and write to L1
    volatile uint32_t* l1_mem = reinterpret_cast<volatile uint32_t*>(0x2F800);
    uint32_t cycles, count;

    // INST_PACK
    cycles    = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_INSTRN_THREAD - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_INSTRN_THREAD - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[4] = cycles;
    l1_mem[5] = count;

    // INST_MOVE
    cycles     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_FPU - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count      = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_FPU - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[10] = cycles;
    l1_mem[11] = count;

    // PACK_DMA
    cycles     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_PACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count      = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_PACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[14] = cycles;
    l1_mem[15] = count;

    // PACK_STALL
    cycles     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_DBG_L1 - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count      = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_DBG_L1 - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[28] = cycles;
    l1_mem[29] = count;

    // SRCA_VALID stalls
    cycles     = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_L_TDMA_UNPACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    count      = dbg_regs[(RISCV_DEBUG_REG_PERF_CNT_OUT_H_TDMA_UNPACK - RISCV_DEBUG_REGS_START_ADDR) / 4];
    l1_mem[30] = cycles;
    l1_mem[31] = count;
#endif
}

} // namespace llk_perf
