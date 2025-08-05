// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_debug.h
 * @brief Debug Infrastructure for Wormhole B0 Tensix Cores
 *
 * This header provides comprehensive debugging and diagnostic capabilities for
 * Wormhole B0 Tensix cores. It includes tools for inspecting hardware state,
 * dumping memory contents, controlling debug buses, and coordinating thread
 * synchronization during debugging operations.
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # Key Components
 *
 * - **Thread Debugging**: Halt, synchronize, and control thread execution
 * - **Memory Inspection**: Read and dump register arrays and memory contents
 * - **Debug Bus Control**: Configure and read debug signals from hardware
 * - **Configuration Access**: Read thread and hardware configuration registers
 * - **State Synchronization**: Coordinate multi-thread debugging operations
 *
 * # Debug Architecture
 *
 * ## Thread Coordination
 * The debug system manages coordination between three processing threads:
 * - **Unpack Thread**: Data input and preprocessing
 * - **Math Thread**: Mathematical operations and control
 * - **Pack Thread**: Data output and postprocessing
 *
 * ## Debug Bus System
 * The debug bus provides real-time access to internal hardware signals:
 * - Signal selection and filtering
 * - Daisy chain configuration for multi-core debugging
 * - Read data path control
 *
 * ## Memory Array Access
 * Direct access to hardware register arrays:
 * - **SrcA/SrcB**: Source register banks
 * - **Dest**: Destination accumulator arrays
 * - Bank and row-level addressing
 *
 * # Usage Patterns
 *
 * ## Thread Debugging
 * ```cpp
 * // Halt threads for debugging
 * dbg_thread_halt<ThreadId::MathThreadId>();
 *
 * // Perform debug operations...
 * dbg_dump_l1_memory(...);
 *
 * // Resume execution
 * dbg_thread_unhalt<ThreadId::MathThreadId>();
 * ```
 *
 * ## Memory Inspection
 * ```cpp
 * // Dump register array contents
 * uint32_t data[8];
 * dbg_read_array_row(dbg_array_id::DEST, 0, 10, false, data);
 *
 * // Read configuration registers
 * uint32_t config = dbg_read_cfgreg(dbg_cfgreg::THREAD_0_CFG, addr);
 * ```
 *
 * ## Debug Bus Configuration
 * ```cpp
 * // Configure debug bus for signal inspection
 * dbg_bus_cntl_u bus_ctrl = {};
 * bus_ctrl.f.sig_sel = signal_id;
 * bus_ctrl.f.en = 1;
 * reg_write(DEBUG_BUS_CONTROL, bus_ctrl.val);
 * ```
 *
 * @warning Debug operations can significantly impact performance and should
 *          only be used during development and troubleshooting.
 *
 * @warning Thread halt/unhalt operations must be properly paired to avoid
 *          deadlocks or inconsistent hardware state.
 *
 * @note Many debug operations require proper thread synchronization to
 *       ensure consistent hardware state during inspection.
 *
 * @see ckernel.h for core synchronization primitives
 * @see ckernel_defs.h for thread ID definitions
 */

#pragma once

#include <cstdint>

#include "ckernel.h"

/**
 * @namespace ckernel
 * @brief Core kernel functionality and hardware abstraction layer
 */
namespace ckernel
{

/**
 * @defgroup DebugInfrastructure Debug Infrastructure Constants and Structures
 * @brief Hardware debugging and diagnostic capabilities
 * @{
 */

/**
 * @struct dbg_cfgreg
 * @brief Debug configuration register identification constants
 *
 * Defines identifiers for different configuration register groups that can
 * be accessed during debugging. Each group contains specific configuration
 * state for threads or hardware components.
 */
struct dbg_cfgreg
{
    constexpr static uint32_t THREAD_0_CFG    = 0;              ///< Thread 0 configuration registers (unpack)
    constexpr static uint32_t THREAD_1_CFG    = 1;              ///< Thread 1 configuration registers (math)
    constexpr static uint32_t THREAD_2_CFG    = 2;              ///< Thread 2 configuration registers (pack)
    constexpr static uint32_t HW_CFG_0        = 3;              ///< Hardware configuration state 0
    constexpr static uint32_t HW_CFG_1        = 4;              ///< Hardware configuration state 1
    constexpr static uint32_t HW_CFG_SIZE     = 187;            ///< Size of hardware configuration space
    constexpr static uint32_t THREAD_CFG_SIZE = THD_STATE_SIZE; ///< Size of thread configuration space
};

/**
 * @struct dbg_array_id
 * @brief Debug array identification for register bank access
 *
 * Identifies different register arrays that can be inspected during debugging.
 * Each array represents a major register bank in the processing pipeline.
 */
struct dbg_array_id
{
    constexpr static uint32_t SRCA = 0; ///< Source A register bank (last used bank)
    constexpr static uint32_t SRCB = 1; ///< Source B register bank (last used bank)
    constexpr static uint32_t DEST = 2; ///< Destination accumulator array
};

/**
 * @struct dbg_daisy_id
 * @brief Debug daisy chain identification for multi-core debugging
 *
 * Identifies different instruction issue points in the daisy chain for
 * multi-core debugging scenarios. Used for coordinated debugging across
 * multiple Tensix cores.
 */
struct dbg_daisy_id
{
    constexpr static uint32_t INSTR_ISSUE_0 = 4; ///< Instruction issue point 0
    constexpr static uint32_t INSTR_ISSUE_1 = 5; ///< Instruction issue point 1
    constexpr static uint32_t INSTR_ISSUE_2 = 6; ///< Instruction issue point 2
};

/**
 * @struct dbg_bus_cntl_t
 * @brief Debug bus control register structure
 *
 * Controls the debug bus configuration for monitoring internal hardware signals.
 * The debug bus provides real-time access to internal state for debugging and
 * performance analysis.
 */
typedef struct
{
    uint32_t sig_sel    : 16; ///< Signal selection (which internal signal to monitor)
    uint32_t daisy_sel  : 8;  ///< Daisy chain selection for multi-core debugging
    uint32_t rd_sel     : 4;  ///< Read path selection
    uint32_t reserved_0 : 1;  ///< Reserved bit
    uint32_t en         : 1;  ///< Enable debug bus
    uint32_t reserved_1 : 2;  ///< Reserved bits
} dbg_bus_cntl_t;

/**
 * @union dbg_bus_cntl_u
 * @brief Debug bus control union for register access
 *
 * Provides both structured field access and raw register value access
 * for debug bus configuration.
 */
typedef union
{
    uint32_t val;     ///< Raw register value
    dbg_bus_cntl_t f; ///< Structured field access
} dbg_bus_cntl_u;

/**
 * @struct dbg_array_rd_en_t
 * @brief Debug array read enable register structure
 *
 * Controls whether array reading is enabled for register bank inspection.
 */
typedef struct
{
    uint32_t en       : 1;  ///< Enable array reading
    uint32_t reserved : 31; ///< Reserved bits
} dbg_array_rd_en_t;

/**
 * @union dbg_array_rd_en_u
 * @brief Debug array read enable union for register access
 */
typedef union
{
    uint32_t val;        ///< Raw register value
    dbg_array_rd_en_t f; ///< Structured field access
} dbg_array_rd_en_u;

/**
 * @struct dbg_array_rd_cmd_t
 * @brief Debug array read command register structure
 *
 * Specifies the exact location and parameters for reading from register arrays.
 * Provides fine-grained control over which array, bank, row, and data selection
 * to read during debugging operations.
 */
typedef struct
{
    uint32_t row_addr    : 12; ///< Row address within the array
    uint32_t row_32b_sel : 4;  ///< 32-bit word selection within row (0-7)
    uint32_t array_id    : 3;  ///< Array identifier (see dbg_array_id)
    uint32_t bank_id     : 1;  ///< Bank identifier within array
    uint32_t reserved    : 12; ///< Reserved bits
} dbg_array_rd_cmd_t;

/**
 * @union dbg_array_rd_cmd_u
 * @brief Debug array read command union for register access
 */
typedef union
{
    uint32_t val;         ///< Raw register value
    dbg_array_rd_cmd_t f; ///< Structured field access
} dbg_array_rd_cmd_u;

/**
 * @struct dbg_soft_reset_t
 * @brief Debug soft reset control register structure
 *
 * Controls selective soft reset of specific hardware units during debugging.
 * Used as a workaround for certain debug scenarios where units need to be
 * reset without affecting the entire core.
 */
typedef struct
{
    uint32_t unp      : 2;  ///< Unpacker reset control (2 bits for 2 unpackers)
    uint32_t pack     : 4;  ///< Packer reset control (4 bits for 4 packers)
    uint32_t reserved : 26; ///< Reserved bits
} dbg_soft_reset_t;

/**
 * @union dbg_soft_reset_u
 * @brief Debug soft reset union for register access
 */
typedef union
{
    uint32_t val;       ///< Raw register value
    dbg_soft_reset_t f; ///< Structured field access
} dbg_soft_reset_u;

/**
 * @defgroup ThreadDebugging Thread Debugging and Control Functions
 * @brief Functions for halting, synchronizing, and controlling thread execution during debugging
 * @{
 */

/**
 * @brief Halt specified thread for debugging operations
 *
 * Safely halts the specified processing thread and coordinates with other threads
 * to ensure a consistent state for debugging operations. Each thread type has
 * specific halt procedures to maintain pipeline integrity.
 *
 * @tparam thread_id Thread to halt (UnpackThreadId, MathThreadId, or PackThreadId)
 *
 * # Thread-Specific Behavior:
 *
 * ## Unpack Thread Halt:
 * 1. Completes all pending instructions
 * 2. Notifies math thread of idle state
 * 3. Waits for math thread to complete debug operations
 *
 * ## Math Thread Halt:
 * 1. Completes all pending instructions
 * 2. Waits for unpack thread completion
 * 3. Waits for pack pipeline to drain
 *
 * @warning Must be paired with dbg_thread_unhalt() to resume execution
 * @warning Improper use can cause deadlocks between threads
 *
 * @note This function uses compile-time thread selection for efficiency
 */
template <ThreadId thread_id>
inline void dbg_thread_halt()
{
    static_assert(
        (thread_id == ThreadId::MathThreadId) || (thread_id == ThreadId::UnpackThreadId) || (thread_id == ThreadId::PackThreadId),
        "Invalid thread id set in dbg_thread_halt(...)");

    if constexpr (thread_id == ThreadId::UnpackThreadId)
    {
        // Wait for all instructions on the running thread to complete
        tensix_sync();
        // Notify math thread that unpack thread is idle
        mailbox_write(ThreadId::MathThreadId, 1);
        // Wait for math thread to complete debug dump
        volatile uint32_t temp = mailbox_read(ThreadId::MathThreadId);
    }
    else if constexpr (thread_id == ThreadId::MathThreadId)
    {
        // Wait for all instructions on the running thread to complete
        tensix_sync();
        // Wait for unpack thread to complete
        volatile uint32_t temp = mailbox_read(ThreadId::UnpackThreadId);
        // Wait for previous packs to finish
        while (semaphore_read(semaphore::MATH_PACK) > 0)
        {
        };
    }
}

/**
 * @brief Resume execution of halted thread after debugging
 *
 * Safely resumes execution of a previously halted thread, performing any
 * necessary cleanup or reset operations. Ensures proper pipeline state
 * restoration after debugging operations.
 *
 * @tparam thread_id Thread to resume (UnpackThreadId, MathThreadId, or PackThreadId)
 *
 * # Thread-Specific Behavior:
 *
 * ## Math Thread Resume:
 * 1. Performs pack unit soft reset (workaround)
 * 2. Waits for instruction completion
 * 3. Signals unpack thread to resume
 *
 * @warning Must follow a corresponding dbg_thread_halt() call
 * @note Math thread requires pack reset as hardware workaround
 */
template <ThreadId thread_id>
inline void dbg_thread_unhalt()
{
    static_assert(
        (thread_id == ThreadId::MathThreadId) || (thread_id == ThreadId::UnpackThreadId) || (thread_id == ThreadId::PackThreadId),
        "Invalid thread id set in dbg_wait_for_thread_idle(...)");

    if constexpr (thread_id == ThreadId::MathThreadId)
    {
        // Reset pack 0 (workaround)
        dbg_soft_reset_u dbg_soft_reset;
        dbg_soft_reset.val    = 0;
        dbg_soft_reset.f.pack = 1;
        reg_write(RISCV_DEBUG_REG_SOFT_RESET_0, dbg_soft_reset.val);
        wait(5);
        dbg_soft_reset.val = 0;
        reg_write(RISCV_DEBUG_REG_SOFT_RESET_0, dbg_soft_reset.val);

        // Wait for all instructions to complete
        tensix_sync();

        // Unhalt unpack thread
        mailbox_write(ThreadId::UnpackThreadId, 1);
    }
}

/** @} */ // end of ThreadDebugging group

/**
 * @defgroup ArrayInspection Array and Memory Inspection Functions
 * @brief Functions for reading register arrays and memory contents during debugging
 * @{
 */

/**
 * @brief Read a single row from specified register array
 *
 * Reads data from register arrays (SrcA, SrcB, or Dest) for debugging and
 * inspection purposes. Different arrays require different access methods
 * due to hardware limitations.
 *
 * @param array_id Array identifier (dbg_array_id::SRCA, SRCB, or DEST)
 * @param row_addr Row address within the array to read
 * @param rd_data Output buffer for read data (8 x 32-bit words)
 *
 * # Array-Specific Behavior:
 *
 * ## SrcA Array:
 * - Requires copying data to destination first (SrcA direct read not supported)
 * - Saves destination state to SFPU registers
 * - Moves SrcA row to destination for reading
 * - Restores original destination state
 *
 * ## SrcB Array:
 * - Uses special debug operations to latch values
 * - Requires bank selection and data valid control
 * - Reads data through debug bus mechanisms
 *
 * ## Dest Array:
 * - Direct read access supported
 * - Accounts for destination offset in dual-buffer mode
 *
 * @warning Modifies hardware state during operation
 * @warning SrcA reading requires careful state save/restore
 *
 * @note This is a low-level function; consider using dbg_read_array_row() instead
 */
inline void dbg_get_array_row(const uint32_t array_id, const uint32_t row_addr, uint32_t *rd_data)
{
    // Dest offset is added to row_addr to dump currently used half of the dest accumulator (SyncHalf dest mode)
    std::uint32_t dest_offset = 0;
    if (array_id == dbg_array_id::DEST)
    {
        dest_offset = (dest_offset_id == 1) ? DEST_REGISTER_HALF_SIZE : 0;
    }

    if (array_id == dbg_array_id::SRCA)
    {
        // Save dest row
        // When SrcA array is selected we need to copy row from src register into dest to be able to dump data
        // Dump from SrcA array is not supported
        // Save dest row to SFPU register
        // Move SrcA into dest row
        // Dump dest row
        // Restore dest row
        addr_mod_t {
            .srca = {.incr = 0, .clr = 0, .cr = 0},
            .srcb = {.incr = 0, .clr = 0, .cr = 0},
            .dest = {.incr = 0, .clr = 0, .cr = 0},
        }
            .set(ADDR_MOD_0);

        // Clear counters
        TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_ABD_F);
        TTI_SFPLOAD(p_sfpu::LREG3, 0, 0, 0); // Save dest addr 0 (even cols) to LREG_3
        TTI_SFPLOAD(p_sfpu::LREG3, 0, 0, 2); // Save dest addr 0 (odd cols)  to LREG_3

        TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::SFPU1);

        // Move to the last used bank
        TTI_CLEARDVALID(0b01, 0);

        // Copy single row from SrcA[row_addr] to dest location 0
        TT_MOVDBGA2D(0, row_addr, 0, p_mova2d::MOV_1_ROW, 0);

        // Wait for TT instructions to complete
        tensix_sync();
    }
    else if (array_id == dbg_array_id::SRCB)
    {
        addr_mod_t {
            .srca = {.incr = 0, .clr = 0, .cr = 0},
            .srcb = {.incr = 0, .clr = 0, .cr = 0},
            .dest = {.incr = 0, .clr = 0, .cr = 0},
        }
            .set(ADDR_MOD_0);

        // Clear counters
        TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_ABD_F);

        // Move to the last used bank
        TTI_SETDVALID(0b10);
        TTI_CLEARDVALID(0b10, 0);

        // Latch debug values
        TTI_SETDVALID(0b10);
        TTI_SHIFTXB(ADDR_MOD_0, 0, row_addr >> 1);
        TTI_CLEARDVALID(0b10, 0);

        // Wait for TT instructions to complete
        tensix_sync();

        /*
        // Get last used bank id via debug bus
        dbg_bus_cntl_u dbg_bus_cntl;
        dbg_bus_cntl.val = 0;
        dbg_bus_cntl.f.rd_sel = 6; // Read bits 111:96, 110 (write_math_id_reg)
        dbg_bus_cntl.f.sig_sel = 0x4<<1;
        dbg_bus_cntl.f.daisy_sel = dbg_daisy_id::INSTR_ISSUE_2;
        dbg_bus_cntl.f.en = 1;
        reg_write(RISCV_DEBUG_REG_DBG_BUS_CNTL_REG, dbg_bus_cntl.val);
        wait (5); // Wait for value to get stable
        srcb_bank_id = ((reg_read(RISCV_DEBUG_REG_DBG_RD_DATA)) & 0x4000) ? 0 : 1;

        // Disable debug bus
        dbg_bus_cntl.val = 0;
        reg_write(RISCV_DEBUG_REG_DBG_BUS_CNTL_REG, dbg_bus_cntl.val);
        */
    }

    // Get actual row address and array id used in hw
    std::uint32_t hw_row_addr = (array_id == dbg_array_id::SRCA) ? 0 : ((array_id == dbg_array_id::DEST) ? dest_offset + row_addr : row_addr & 0x1);

    std::uint32_t hw_array_id = (array_id == dbg_array_id::SRCA) ? dbg_array_id::DEST : array_id;

    std::uint32_t hw_bank_id = 0;

    bool sel_datums_15_8 = hw_array_id != dbg_array_id::DEST;

    dbg_array_rd_en_u dbg_array_rd_en;
    dbg_array_rd_en.val  = 0;
    dbg_array_rd_en.f.en = 0x1;
    reg_write(RISCV_DEBUG_REG_DBG_ARRAY_RD_EN, dbg_array_rd_en.val);

    dbg_array_rd_cmd_u dbg_array_rd_cmd;
    dbg_array_rd_cmd.val        = 0;
    dbg_array_rd_cmd.f.array_id = hw_array_id;
    dbg_array_rd_cmd.f.bank_id  = hw_bank_id;

    for (uint32_t i = 0; i < 8; i++)
    {
        dbg_array_rd_cmd.f.row_addr    = sel_datums_15_8 ? (hw_row_addr | ((i >= 4) ? (1 << 6) : 0)) : hw_row_addr;
        dbg_array_rd_cmd.f.row_32b_sel = i;
        reg_write(RISCV_DEBUG_REG_DBG_ARRAY_RD_CMD, dbg_array_rd_cmd.val);
        wait(5); // Wait for value to get stable
        rd_data[i] = reg_read(RISCV_DEBUG_REG_DBG_ARRAY_RD_DATA);
    }

    // Disable debug control
    dbg_array_rd_cmd.val = 0;
    reg_write(RISCV_DEBUG_REG_DBG_ARRAY_RD_CMD, dbg_array_rd_cmd.val);
    dbg_array_rd_en.val = 0;
    reg_write(RISCV_DEBUG_REG_DBG_ARRAY_RD_EN, dbg_array_rd_en.val);

    // Restore dest row
    if (array_id == dbg_array_id::SRCA)
    {
        TTI_SFPSTORE(p_sfpu::LREG3, 0, 0, 0); // Restore dest addr 0 (even cols) from LREG_3
        TTI_SFPSTORE(p_sfpu::LREG3, 0, 0, 2); // Restore dest addr 0 (odd cols) from LREG_3
        // Move to the current bank
        TTI_CLEARDVALID(1, 0);
    }
}

/**
 * @brief Read configuration register from debug interface
 *
 * Reads configuration registers from hardware or thread-specific configuration
 * spaces for debugging and inspection purposes. Provides access to internal
 * configuration state that is normally not accessible during runtime.
 *
 * @param cfgreg_id Configuration register group ID (see dbg_cfgreg constants)
 * @param addr Address within the configuration group
 * @return 32-bit configuration register value
 *
 * # Configuration Groups:
 *
 * - **HW_CFG_0**: Hardware configuration state 0 (base address 0)
 * - **HW_CFG_1**: Hardware configuration state 1 (base + HW_CFG_SIZE)
 * - **THREAD_0_CFG**: Thread 0 configuration (unpack thread)
 * - **THREAD_1_CFG**: Thread 1 configuration (math thread)
 * - **THREAD_2_CFG**: Thread 2 configuration (pack thread)
 *
 * # Address Calculation:
 * The function calculates hardware addresses by combining the base address
 * for the configuration group with the provided offset address.
 *
 * @note Configuration reading is intended for debugging and diagnostic purposes only
 * @warning Some configuration registers may be sensitive to read operations
 */
inline std::uint32_t dbg_read_cfgreg(const uint32_t cfgreg_id, const uint32_t addr)
{
    uint32_t hw_base_addr = 0;

    switch (cfgreg_id)
    {
        case dbg_cfgreg::HW_CFG_1:
            hw_base_addr = dbg_cfgreg::HW_CFG_SIZE;
            break;
        case dbg_cfgreg::THREAD_0_CFG:
        case dbg_cfgreg::THREAD_1_CFG:
        case dbg_cfgreg::THREAD_2_CFG:
            hw_base_addr = 2 * dbg_cfgreg::HW_CFG_SIZE + cfgreg_id * dbg_cfgreg::THREAD_CFG_SIZE;
            break;
        default:
            break;
    }

    uint32_t hw_addr = hw_base_addr + (addr & 0x7ff); // hw address is 4-byte aligned
    reg_write(RISCV_DEBUG_REG_CFGREG_RD_CNTL, hw_addr);

    wait(1);

    return reg_read(RISCV_DEBUG_REG_CFGREG_RDDATA);
}

/** @} */ // end of ArrayInspection group
/** @} */ // end of DebugInfrastructure group

} // namespace ckernel
