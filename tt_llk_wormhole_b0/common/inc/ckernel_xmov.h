// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_xmov.h
 * @brief Cross-Memory Data Movement Operations for Wormhole B0 Tensix
 *
 * This header provides XMOV (Cross-Move) functionality for efficient data movement
 * between different memory spaces in Wormhole B0 Tensix cores. XMOV operations are
 * essential for tile-based processing, enabling fast transfer of configuration data
 * and tiles between L1 memory, configuration registers, and other memory regions.
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # XMOV Architecture
 *
 * XMOV provides hardware-accelerated data movement capabilities:
 * - **L1 ↔ Configuration Registers**: Fast configuration loading/saving
 * - **L1 ↔ L1 Memory**: Efficient tile and data redistribution
 * - **Compact Instructions**: Single-instruction moves with embedded parameters
 * - **Non-Compact Instructions**: Multi-register parameterized moves
 *
 * # Data Movement Patterns
 *
 * ## Tile Processing Context
 * XMOV is crucial for tile-based matrix operations:
 * - **Configuration Loading**: Stream tile descriptors to unpacker/packer
 * - **Tile Redistribution**: Move 32×32 tiles between processing stages
 * - **Multi-Core Coordination**: Transfer data between Tensix cores
 * - **Memory Management**: Efficiently manage L1 buffer allocation
 *
 * ## Transfer Granularity
 * All transfers operate on **16-byte boundaries** for optimal memory bandwidth:
 * - Addresses must be 16-byte aligned
 * - Transfer sizes specified in 16-byte chunks
 * - Hardware ensures atomic 16-byte operations
 *
 * # Instruction Modes
 *
 * ## Compact Mode
 * - **Single instruction** with embedded parameters
 * - **Immediate execution** without additional register setup
 * - **Optimal for frequent small transfers**
 *
 * ## Non-Compact Mode
 * - **Multi-register setup** followed by execution command
 * - **Flexible parameter handling** for complex transfers
 * - **Better for large or variable-size transfers**
 *
 * # Usage Patterns
 *
 * ## Fast Configuration Loading
 * ```cpp
 * // Set base address for all subsequent moves
 * xmov_set_base(l1_config_base >> 4);
 *
 * // Fast configuration transfer
 * xmov_cfg_program(config_offset, UNPACKER_CFG_REG, 4);
 * xmov_wait_till_idle();
 * ```
 *
 * ## Tile Data Movement
 * ```cpp
 * // Move tile from source to destination in L1
 * xmov_l1_to_l1_non_compact(src_tile_addr, dst_tile_addr, TILE_SIZE_16B);
 * xmov_wait_till_idle();
 * ```
 *
 * # Performance Considerations
 *
 * - **Always wait for completion** using `xmov_wait_till_idle()`
 * - **Batch operations** when possible to reduce overhead
 * - **Align addresses** to 16-byte boundaries for optimal performance
 * - **Use compact mode** for frequent small configuration updates
 *
 * @note All transfers must complete before accessing moved data
 * @note XMOV operations are asynchronous - always check completion status
 * @note 16-byte alignment is mandatory for all addresses and sizes
 *
 * @see Tiles documentation for context on 32×32 tile organization
 * @see ckernel.h for configuration register address utilities
 * @see Hardware specification for detailed XMOV register layouts
 */

#pragma once
#include "ckernel.h"

/**
 * @namespace ckernel
 * @brief Core kernel functionality and hardware abstraction layer
 */
namespace ckernel
{

/**
 * @defgroup XMOVOperations Cross-Memory Data Movement Operations
 * @brief Hardware-accelerated data movement between memory spaces
 * @{
 */

/**
 * @struct risc_compact_mov_instrn_t
 * @brief Compact XMOV instruction format for single-instruction data moves
 *
 * Defines the bitfield layout for compact XMOV instructions that embed all
 * transfer parameters directly in the instruction word. This format enables
 * efficient single-instruction data moves without requiring separate parameter
 * register setup.
 *
 * # Instruction Format (32-bit):
 * - **Bits 31-24**: `instrn` - Instruction opcode (0x40 for MOV)
 * - **Bits 23-16**: `src_offset_addr` - Source offset from base address
 * - **Bits 15-8**: `dst_addr` - Destination address (16B aligned)
 * - **Bits 7-2**: `xfer_size` - Transfer size in 16B chunks (1-64)
 * - **Bit 1**: `xfer_dir` - Transfer direction (0=L1→CfgReg, 1=L1→L1)
 * - **Bit 0**: `no_params` - Parameter mode (0=use registers, 1=compact)
 */
typedef struct
{
    uint32_t instrn          : 8; ///< Instruction opcode (0x40 for MOV operations)
    uint32_t src_offset_addr : 8; ///< Source address offset from base (16B units)
    uint32_t dst_addr        : 8; ///< Destination address (16B aligned, >>2 for cfg regs)
    uint32_t xfer_size       : 6; ///< Transfer size in 16B chunks (1-64)
    uint32_t xfer_dir        : 1; ///< Transfer direction: 0=L1→CfgReg, 1=L1→L1
    uint32_t no_params       : 1; ///< Parameter mode: 0=use registers, 1=compact
} risc_compact_mov_instrn_t;

/**
 * @union risc_compact_mov_instrn_u
 * @brief Union for easy access to compact XMOV instruction as value or fields
 *
 * Provides convenient access to the compact XMOV instruction format either as
 * a raw 32-bit value (for direct hardware programming) or as structured fields
 * (for parameter setup and manipulation).
 */
typedef union
{
    uint32_t val;                ///< Raw 32-bit instruction value
    risc_compact_mov_instrn_t f; ///< Structured field access
} risc_compact_mov_instrn_u;

/**
 * @defgroup XMOVBaseOps XMOV Base Address Operations
 * @brief Functions for managing XMOV base address configuration
 * @{
 */

/**
 * @brief Set XMOV L1 base address for subsequent operations
 *
 * Configures the base address that will be used for all subsequent compact
 * XMOV operations. Compact moves specify source addresses as offsets from
 * this base, enabling efficient address calculation and instruction encoding.
 *
 * @param l1_base_addr_16B Base address in L1 memory (16-byte aligned)
 *
 * @note Base address must be 16-byte aligned
 * @note All subsequent compact moves use offsets from this base
 * @note Base persists until explicitly changed with another call
 *
 * # Usage Example:
 * ```cpp
 * // Set base to start of tile configuration area
 * xmov_set_base(TILE_CONFIG_BASE >> 4);
 *
 * // Subsequent compact moves use offsets from this base
 * xmov_cfg_program(config_offset, UNPACKER_CFG_REG);
 * ```
 */
inline void xmov_set_base(const uint32_t l1_base_addr_16B)
{
    volatile uint *XMOV_L1_BASE = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_XMOV_L1_BASE_ADDR);
    // Program mover L1 base to the command base
    XMOV_L1_BASE[0] = l1_base_addr_16B;
}

/** @} */ // end of XMOVBaseOps group

/**
 * @defgroup XMOVConfigOps XMOV Configuration Transfer Operations
 * @brief Functions for moving data from L1 memory to configuration registers
 * @{
 */

/**
 * @brief Set up compact configuration transfer instruction
 *
 * Prepares a compact XMOV instruction for transferring data from L1 memory
 * to configuration registers. This function sets up the instruction structure
 * but does not execute it - use with `xmov_cfg_instr_program()` for execution.
 *
 * @param risc_mov_instrn [out] Instruction structure to configure
 * @param l1_offset_16B Source offset from base address (16-byte units)
 * @param reg_addr32 Configuration register address (32-bit aligned)
 * @param xfer_size Transfer size in 16-byte chunks (default: 1)
 *
 * @note Source address = l1_offset_16B + XMOV_L1_BASE
 * @note Configuration registers require specific address translation
 * @note Use `xmov_set_base()` to establish base address first
 */
inline void xmov_cfg_instr_set(
    risc_compact_mov_instrn_u &risc_mov_instrn, const uint32_t l1_offset_16B, const uint32_t reg_addr32, const uint32_t xfer_size = 1)
{
    risc_mov_instrn.val               = 0;
    risc_mov_instrn.f.instrn          = 0x40;                      // mov instruction
    risc_mov_instrn.f.src_offset_addr = l1_offset_16B;             // final l1 address is src_offset_addr + RISCV_TDMA_REG_XMOV_L1_BASE_ADDR
    risc_mov_instrn.f.dst_addr        = cfg_addr(reg_addr32) >> 2; // movcfg address, 16B aligned
    risc_mov_instrn.f.xfer_size       = xfer_size;                 // transfer size in 16B chunks
    risc_mov_instrn.f.xfer_dir        = 0;                         // l1->cfgreg
    risc_mov_instrn.f.no_params       = 0x1;                       // compact move
}

/**
 * @brief Execute pre-configured compact configuration transfer
 *
 * Executes a compact XMOV instruction that was previously configured with
 * `xmov_cfg_instr_set()`. This allows reuse of instruction templates with
 * different source addresses or destination registers.
 *
 * @param xmov Pre-configured instruction template
 * @param l1_offset_16B Source offset from base address (16-byte units)
 * @param reg_addr32 Configuration register address (32-bit aligned)
 *
 * @note Modifies source and destination fields of the template
 * @note Executes immediately - no separate trigger required
 * @note Use `xmov_wait_till_idle()` to ensure completion
 */
inline void xmov_cfg_instr_program(const risc_compact_mov_instrn_t xmov, const uint32_t l1_offset_16B, const uint32_t reg_addr32)
{
    // Program tile descriptor using fast XMOV path
    volatile uint *XMOV_CMD = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_COMMAND_ADDR);
    risc_compact_mov_instrn_u risc_mov_instrn;
    risc_mov_instrn.f                 = xmov;
    risc_mov_instrn.f.src_offset_addr = l1_offset_16B;             // final l1 address is src_offset_addr + RISCV_TDMA_REG_XMOV_L1_BASE_ADDR
    risc_mov_instrn.f.dst_addr        = cfg_addr(reg_addr32) >> 2; // movcfg address, 16B aligned
    XMOV_CMD[0]                       = risc_mov_instrn.val;
}

/**
 * @brief Execute immediate compact configuration transfer
 *
 * Combines instruction setup and execution for immediate configuration
 * transfer from L1 memory to configuration registers. This is the most
 * common interface for configuration loading operations.
 *
 * @param l1_offset_16B Source offset from base address (16-byte units)
 * @param reg_addr32 Configuration register address (32-bit aligned)
 * @param xfer_size Transfer size in 16-byte chunks (default: 1)
 *
 * @note Requires prior call to `xmov_set_base()` to establish base address
 * @note Ideal for single-shot configuration transfers
 * @note Use `xmov_wait_till_idle()` to ensure completion before register access
 *
 * # Usage Example:
 * ```cpp
 * // Load tile descriptor to unpacker
 * xmov_set_base(tile_desc_base >> 4);
 * xmov_cfg_program(tile_offset, UNPACKER_TILE_DESC_REG, 2);
 * xmov_wait_till_idle();
 * ```
 */
inline void xmov_cfg_program(const uint32_t l1_offset_16B, const uint32_t reg_addr32, const uint32_t xfer_size = 1)
{
    volatile uint *XMOV_CMD = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_COMMAND_ADDR);
    // Program tile descriptor using fast XMOV path
    risc_compact_mov_instrn_u risc_mov_instrn;
    xmov_cfg_instr_set(risc_mov_instrn, l1_offset_16B, reg_addr32, xfer_size);
    XMOV_CMD[0] = risc_mov_instrn.val;
}

/** @} */ // end of XMOVConfigOps group

/**
 * @defgroup XMOVL1Ops XMOV L1-to-L1 Transfer Operations
 * @brief Functions for moving data between L1 memory locations
 * @{
 */

/**
 * @brief Execute L1-to-L1 data transfer using non-compact mode
 *
 * Performs data transfer between two L1 memory locations using the non-compact
 * instruction format. This mode uses separate parameter registers for maximum
 * flexibility in addressing and transfer sizes.
 *
 * @param src_addr_16B Source address in L1 memory (16-byte aligned)
 * @param dst_addr_16B Destination address in L1 memory (16-byte aligned)
 * @param xfer_size_16B Transfer size in 16-byte chunks (default: 1)
 *
 * @note Both addresses must be 16-byte aligned
 * @note Transfer size can be larger than compact mode limits
 * @note Ideal for tile redistribution and buffer management
 * @note Use `xmov_wait_till_idle()` to ensure completion
 *
 * # Usage Examples:
 *
 * ## Single Tile Move:
 * ```cpp
 * // Move one 32x32 tile (2KB = 128 chunks of 16B)
 * xmov_l1_to_l1_non_compact(src_tile, dst_tile, 128);
 * xmov_wait_till_idle();
 * ```
 *
 * ## Buffer Reorganization:
 * ```cpp
 * // Move micro-block between L1 buffers
 * xmov_l1_to_l1_non_compact(input_buf, working_buf, ublock_size_16B);
 * xmov_wait_till_idle();
 * ```
 */
inline void xmov_l1_to_l1_non_compact(const uint32_t src_addr_16B, const uint32_t dst_addr_16B, const uint32_t xfer_size_16B = 1)
{
    volatile uint *XMOV_CMD  = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_COMMAND_ADDR);
    volatile uint *SRC_ADDR  = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_XMOV_SRC_ADDR);
    volatile uint *DST_ADDR  = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_XMOV_DST_ADDR);
    volatile uint *SIZE_ADDR = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_XMOV_SIZE);
    volatile uint *DIR_ADDR  = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_XMOV_DIRECTION);

    SRC_ADDR[0]  = src_addr_16B;
    DST_ADDR[0]  = dst_addr_16B;
    SIZE_ADDR[0] = xfer_size_16B;
    DIR_ADDR[0]  = 3; // L1 to L1
    risc_compact_mov_instrn_u risc_mov_instrn;
    risc_mov_instrn.f.instrn    = 0x40; // mov instruction
    risc_mov_instrn.f.no_params = 0;    // Use xmov inputs from param registers
    XMOV_CMD[0]                 = risc_mov_instrn.val;
}

/** @} */ // end of XMOVL1Ops group

/**
 * @defgroup XMOVSyncOps XMOV Synchronization Operations
 * @brief Functions for synchronizing with XMOV hardware completion
 * @{
 */

/**
 * @brief Wait for all XMOV operations to complete
 *
 * Blocks execution until all previously issued XMOV operations have completed.
 * This is essential for ensuring data consistency before accessing moved data
 * or issuing dependent operations.
 *
 * @note Always call this function after XMOV operations before accessing moved data
 * @note XMOV operations are asynchronous and may complete out of order
 * @note This function polls the hardware status register
 *
 * # Critical Usage:
 * ```cpp
 * // Move configuration data
 * xmov_cfg_program(config_offset, UNPACKER_CFG_REG);
 * xmov_wait_till_idle();  // REQUIRED before using configuration
 *
 * // Now safe to use unpacker with new configuration
 * unpack_tiles();
 * ```
 *
 * # Performance Note:
 * While this function polls, XMOV operations typically complete very quickly
 * (few cycles for small transfers). For optimal performance, batch multiple
 * XMOV operations before waiting for completion.
 */
inline void xmov_wait_till_idle()
{
    volatile uint *XMOV_STATUS = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_STATUS);
    while (XMOV_STATUS[0] & 0x1)
    {
    }
}

/** @} */ // end of XMOVSyncOps group
/** @} */ // end of XMOVOperations group

} // namespace ckernel
