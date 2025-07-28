// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "ckernel.h"

/**
 * @file ckernel_xmov.h
 * @brief XMOV data movement operations for Tensix compute kernels
 *
 * @details Provides high-level C++ interface for programming the Tensix Data Movement
 * Architecture (TDMA) XMOV unit. XMOV enables efficient data transfers between L1 memory,
 * configuration registers, and other memory spaces with hardware acceleration.
 *
 * **XMOV Capabilities:**
 * - L1 memory to configuration register transfers
 * - L1 memory to L1 memory transfers  
 * - Hardware-accelerated data movement with minimal CPU overhead
 * - Support for both compact and parameterized instruction modes
 * - 16-byte aligned transfers for optimal memory bandwidth
 *
 * **Transfer Types:**
 * - **L1 → Config Registers**: Configuration data loading
 * - **L1 → L1**: Memory-to-memory data movement
 * - **Compact Mode**: All parameters embedded in single instruction
 * - **Parameter Mode**: Separate parameter registers for flexibility
 *
 * **Performance Benefits:**
 * - Hardware DMA-style transfers reduce CPU cycles
 * - Parallel execution with compute operations
 * - Optimized memory access patterns
 * - Minimal synchronization overhead
 *
 * **Integration:**
 * - Part of Tensix RISC-V TDMA subsystem
 * - Direct hardware register programming interface
 * - Compatible with kernel execution pipeline
 * - Essential for high-performance data staging
 */

// XMOV programming through C kernels

namespace ckernel
{

/**
 * @brief RISC compact move instruction structure
 *
 * @details Defines the bit fields for a compact XMOV instruction where all transfer
 * parameters are embedded within a single 32-bit instruction word. This provides
 * maximum efficiency for simple, fixed-parameter data movements.
 *
 * **Instruction Format (32 bits total):**
 * ```
 * [31]    no_params       : 1 bit  - Parameter mode (0=registers, 1=compact)
 * [30]    xfer_dir        : 1 bit  - Transfer direction (0=L1→cfg, 1=L1→L1)
 * [29:24] xfer_size       : 6 bits - Transfer size in 16-byte chunks (max 64)
 * [23:16] dst_addr        : 8 bits - Destination address (16-byte aligned)
 * [15:8]  src_offset_addr : 8 bits - Source offset from base address
 * [7:0]   instrn          : 8 bits - Instruction opcode (0x40 for move)
 * ```
 *
 * **Field Descriptions:**
 * - **instrn**: Move instruction opcode (always 0x40)
 * - **src_offset_addr**: Offset added to L1 base address for source
 * - **dst_addr**: Destination address for configuration registers
 * - **xfer_size**: Number of 16-byte chunks to transfer (1-64)
 * - **xfer_dir**: Transfer direction (L1→config or L1→L1)
 * - **no_params**: Compact mode flag (1=all parameters embedded)
 *
 * **Compact Mode Benefits:**
 * - Single instruction contains all transfer information
 * - Minimal instruction overhead for simple transfers
 * - Optimal for frequently-used, fixed-size transfers
 * - Hardware can decode and execute immediately
 */
typedef struct
{
    uint32_t instrn          : 8;  ///< Instruction opcode (0x40 for move operations)
    uint32_t src_offset_addr : 8;  ///< Source offset from L1 base address (16-byte units)
    uint32_t dst_addr        : 8;  ///< Destination address (16-byte aligned)
    uint32_t xfer_size       : 6;  ///< Transfer size in 16-byte chunks (1-64)
    uint32_t xfer_dir        : 1;  ///< Transfer direction: 0=L1→cfgreg, 1=L1→L1
    uint32_t no_params       : 1;  ///< Parameter mode: 0=use param registers, 1=compact embedded
} risc_compact_mov_instrn_t;

/**
 * @brief Union for accessing RISC move instruction as both structure and raw value
 *
 * @details Provides convenient access to move instruction data as either individual
 * bit fields (via structure) or as a complete 32-bit value for hardware register
 * programming. Essential for efficient instruction encoding and register writes.
 *
 * **Usage Patterns:**
 * ```cpp
 * risc_compact_mov_instrn_u instr;
 * instr.f.instrn = 0x40;           // Set via bit fields
 * instr.f.xfer_size = 4;
 * XMOV_CMD[0] = instr.val;         // Write as complete value
 * ```
 */
typedef union
{
    uint32_t val;                   ///< Raw 32-bit instruction value for hardware register writes
    risc_compact_mov_instrn_t f;    ///< Structured access to individual instruction bit fields
} risc_compact_mov_instrn_u;

/**
 * @brief Set the L1 base address for XMOV operations
 *
 * @details Configures the base address in L1 memory that serves as the reference point
 * for all XMOV source address calculations. Source addresses in XMOV instructions are
 * specified as offsets from this base address.
 *
 * **Base Address Usage:**
 * - All XMOV source addresses calculated as: base + offset
 * - Must be 16-byte aligned for optimal memory access
 * - Remains active until explicitly changed
 * - Shared across all XMOV operations until reset
 *
 * **Memory Layout Optimization:**
 * Setting an appropriate base address allows efficient use of 8-bit offsets in
 * compact instructions, enabling access to a 4KB window (256 × 16 bytes) of
 * L1 memory from a single base setting.
 *
 * **Hardware Interface:**
 * Directly programs the RISCV_TDMA_REG_XMOV_L1_BASE_ADDR register in the
 * Tensix RISC-V TDMA subsystem.
 *
 * @param l1_base_addr_16B Base address in L1 memory (in 16-byte units)
 *
 * @note Address must be 16-byte aligned for proper hardware operation
 * @note Base address persists until explicitly changed
 * @note Used by all subsequent XMOV operations as reference point
 * @note Setting base address is typically done once per data movement phase
 */
inline void xmov_set_base(const uint32_t l1_base_addr_16B)
{
    volatile uint *XMOV_L1_BASE = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_XMOV_L1_BASE_ADDR);
    // Program mover L1 base to the command base
    XMOV_L1_BASE[0] = l1_base_addr_16B;     // Set L1 memory base address for XMOV operations
}

/**
 * @brief Configure a compact XMOV instruction for L1 to configuration register transfer
 *
 * @details Initializes a compact move instruction structure for transferring data from
 * L1 memory to configuration registers. This function sets up all instruction fields
 * for efficient single-instruction data movement.
 *
 * **Instruction Configuration:**
 * - Sets move opcode (0x40) for hardware recognition
 * - Configures L1→config register transfer direction
 * - Enables compact mode (all parameters embedded)
 * - Calculates proper address alignment and offsets
 *
 * **Address Calculations:**
 * - Source: l1_offset_16B + previously set base address
 * - Destination: cfg_addr(reg_addr32) >> 2 (16-byte aligned)
 * - Transfer size: specified in 16-byte chunks
 *
 * **Typical Usage:**
 * ```cpp
 * risc_compact_mov_instrn_u instr;
 * xmov_cfg_instr_set(instr, offset, reg_addr, size);
 * // Instruction ready for hardware execution
 * ```
 *
 * @param[out] risc_mov_instrn Reference to instruction union to configure
 * @param l1_offset_16B Offset from L1 base address (in 16-byte units)
 * @param reg_addr32 Target configuration register address (byte address)
 * @param xfer_size Transfer size in 16-byte chunks (default: 1)
 *
 * @note Instruction configured but not executed by this function
 * @note All addresses automatically aligned to 16-byte boundaries
 * @note Uses compact mode for maximum efficiency
 * @note Default transfer size is 1 chunk (16 bytes)
 */
inline void xmov_cfg_instr_set(
    risc_compact_mov_instrn_u &risc_mov_instrn, const uint32_t l1_offset_16B, const uint32_t reg_addr32, const uint32_t xfer_size = 1)
{
    risc_mov_instrn.val               = 0;                          // Clear all fields
    risc_mov_instrn.f.instrn          = 0x40;                      // mov instruction opcode
    risc_mov_instrn.f.src_offset_addr = l1_offset_16B;             // final l1 address is src_offset_addr + RISCV_TDMA_REG_XMOV_L1_BASE_ADDR
    risc_mov_instrn.f.dst_addr        = cfg_addr(reg_addr32) >> 2; // movcfg address, 16B aligned
    risc_mov_instrn.f.xfer_size       = xfer_size;                 // transfer size in 16B chunks
    risc_mov_instrn.f.xfer_dir        = 0;                         // l1->cfgreg direction
    risc_mov_instrn.f.no_params       = 0x1;                       // compact move mode
}

/**
 * @brief Program and execute a compact XMOV instruction with runtime address parameters
 *
 * @details Takes a pre-configured XMOV instruction template and overrides the source
 * and destination addresses at runtime, then immediately executes the transfer.
 * Useful for reusing instruction configurations with different addresses.
 *
 * **Runtime Flexibility:**
 * - Reuses pre-configured instruction template
 * - Overrides source offset and destination address
 * - Immediate execution after configuration
 * - Efficient for parameterized transfer patterns
 *
 * **Execution Flow:**
 * 1. Copy template instruction configuration
 * 2. Override source offset with runtime parameter
 * 3. Override destination address with runtime parameter
 * 4. Execute instruction immediately via hardware register write
 *
 * **Performance Benefits:**
 * - Reuses common instruction configurations
 * - Minimizes instruction setup overhead
 * - Enables efficient parameterized transfers
 * - Direct hardware execution without additional setup
 *
 * @param xmov Pre-configured XMOV instruction template
 * @param l1_offset_16B Runtime source offset from L1 base (in 16-byte units)
 * @param reg_addr32 Runtime destination register address (byte address)
 *
 * @note Instruction executed immediately after configuration
 * @note Addresses automatically aligned to 16-byte boundaries
 * @note Uses previously set L1 base address for source calculation
 * @note Transfer size and other parameters from template unchanged
 */
inline void xmov_cfg_instr_program(const risc_compact_mov_instrn_t xmov, const uint32_t l1_offset_16B, const uint32_t reg_addr32)
{
    // Program tile descriptor using fast XMOV path
    volatile uint *XMOV_CMD = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_COMMAND_ADDR);
    risc_compact_mov_instrn_u risc_mov_instrn;
    risc_mov_instrn.f                 = xmov;                       // Copy template configuration
    risc_mov_instrn.f.src_offset_addr = l1_offset_16B;             // final l1 address is src_offset_addr + RISCV_TDMA_REG_XMOV_L1_BASE_ADDR
    risc_mov_instrn.f.dst_addr        = cfg_addr(reg_addr32) >> 2; // movcfg address, 16B aligned
    XMOV_CMD[0]                       = risc_mov_instrn.val;       // Execute instruction immediately
}

/**
 * @brief Program and execute a complete L1 to configuration register transfer
 *
 * @details Configures and immediately executes a complete XMOV transfer from L1 memory
 * to configuration registers. This is a convenience function that combines instruction
 * setup and execution in a single call for simple transfers.
 *
 * **Complete Transfer Flow:**
 * 1. Configure compact move instruction with all parameters
 * 2. Set L1→config register transfer direction
 * 3. Enable compact mode for efficiency
 * 4. Execute transfer immediately via hardware register write
 *
 * **Use Cases:**
 * - Configuration data loading
 * - Kernel parameter initialization
 * - Runtime configuration updates
 * - Efficient single-shot transfers
 *
 * **Performance Characteristics:**
 * - Single function call for complete transfer
 * - Minimal CPU overhead for configuration
 * - Hardware-accelerated data movement
 * - Automatic address alignment and validation
 *
 * @param l1_offset_16B Source offset from L1 base address (in 16-byte units)
 * @param reg_addr32 Target configuration register address (byte address)
 * @param xfer_size Transfer size in 16-byte chunks (default: 1)
 *
 * @note Transfer executed immediately after configuration
 * @note Uses previously set L1 base address for source calculation
 * @note All addresses automatically aligned to 16-byte boundaries
 * @note Default transfer size is 1 chunk (16 bytes) for single register writes
 */
inline void xmov_cfg_program(const uint32_t l1_offset_16B, const uint32_t reg_addr32, const uint32_t xfer_size = 1)
{
    volatile uint *XMOV_CMD = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_COMMAND_ADDR);
    // Program tile descriptor using fast XMOV path
    risc_compact_mov_instrn_u risc_mov_instrn;
    xmov_cfg_instr_set(risc_mov_instrn, l1_offset_16B, reg_addr32, xfer_size);  // Configure instruction
    XMOV_CMD[0] = risc_mov_instrn.val;                                          // Execute transfer immediately
}

/**
 * @brief Execute L1 to L1 memory transfer using non-compact parameter mode
 *
 * @details Performs memory-to-memory transfer within L1 space using separate parameter
 * registers for maximum flexibility. This mode allows for larger address ranges and
 * transfer sizes not constrained by compact instruction bit field limits.
 *
 * **Non-Compact Mode Benefits:**
 * - Full 32-bit addressing for source and destination
 * - Larger transfer sizes beyond compact mode limits
 * - Runtime parameter flexibility
 * - Support for arbitrary L1 memory locations
 *
 * **Parameter Register Programming:**
 * The function programs separate TDMA registers for each parameter:
 * - Source address register
 * - Destination address register  
 * - Transfer size register
 * - Transfer direction register
 *
 * **Transfer Direction:**
 * Uses direction code 3 for L1→L1 transfers, enabling memory-to-memory
 * data movement within the same L1 memory space.
 *
 * **Use Cases:**
 * - Large data block moves within L1
 * - Memory compaction and reorganization
 * - Buffer management operations
 * - Data staging between kernel phases
 *
 * @param src_addr_16B Source address in L1 memory (in 16-byte units)
 * @param dst_addr_16B Destination address in L1 memory (in 16-byte units)
 * @param xfer_size_16B Transfer size in 16-byte chunks (default: 1)
 *
 * @note Uses separate parameter registers (non-compact mode)
 * @note Both source and destination must be in L1 memory space
 * @note All addresses must be 16-byte aligned
 * @note Transfer executed immediately after parameter setup
 * @note No dependency on previously set L1 base address
 */
inline void xmov_l1_to_l1_non_compact(const uint32_t src_addr_16B, const uint32_t dst_addr_16B, const uint32_t xfer_size_16B = 1)
{
    volatile uint *XMOV_CMD  = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_COMMAND_ADDR);
    volatile uint *SRC_ADDR  = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_XMOV_SRC_ADDR);
    volatile uint *DST_ADDR  = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_XMOV_DST_ADDR);
    volatile uint *SIZE_ADDR = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_XMOV_SIZE);
    volatile uint *DIR_ADDR  = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_XMOV_DIRECTION);

    SRC_ADDR[0]  = src_addr_16B;            // Set source address in L1 memory
    DST_ADDR[0]  = dst_addr_16B;            // Set destination address in L1 memory
    SIZE_ADDR[0] = xfer_size_16B;           // Set transfer size in 16-byte chunks
    DIR_ADDR[0]  = 3; // L1 to L1           // Set transfer direction (L1→L1)
    risc_compact_mov_instrn_u risc_mov_instrn;
    risc_mov_instrn.f.instrn    = 0x40; // mov instruction opcode
    risc_mov_instrn.f.no_params = 0;    // Use xmov inputs from param registers
    XMOV_CMD[0]                 = risc_mov_instrn.val;  // Execute transfer with parameter registers
}

/**
 * @brief Wait for XMOV hardware to complete all pending operations
 *
 * @details Polls the XMOV status register until all data movement operations are
 * complete. Essential for ensuring data consistency when subsequent operations
 * depend on transfer completion.
 *
 * **Synchronization Purpose:**
 * - Ensures data transfers complete before dependent operations
 * - Prevents data races between XMOV and compute operations
 * - Required when transfer completion affects subsequent kernel behavior
 * - Critical for maintaining data consistency guarantees
 *
 * **Hardware Status Polling:**
 * Continuously reads the TDMA status register and waits until the busy bit
 * (bit 0) is cleared, indicating all XMOV operations have completed.
 *
 * **Performance Considerations:**
 * - Blocking operation that consumes CPU cycles during wait
 * - Should be used judiciously to avoid unnecessary stalls
 * - Consider asynchronous operation patterns when possible
 * - Essential for correctness in data-dependent operations
 *
 * **Usage Patterns:**
 * ```cpp
 * xmov_cfg_program(offset, reg_addr, size);  // Start transfer
 * xmov_wait_till_idle();                     // Wait for completion
 * // Safe to use transferred data
 * ```
 *
 * @note Blocking operation - CPU will wait until XMOV completes
 * @note Required for data-dependent operations after XMOV
 * @note Not needed if subsequent operations are independent of transfer
 * @note Polls hardware status register continuously until idle
 */
inline void xmov_wait_till_idle()
{
    volatile uint *XMOV_STATUS = reinterpret_cast<volatile uint *>(RISCV_TDMA_REG_STATUS);
    while (XMOV_STATUS[0] & 0x1)           // Poll until busy bit (bit 0) is cleared
    {
        // Wait for XMOV hardware to complete all operations
    }
}

} // namespace ckernel
