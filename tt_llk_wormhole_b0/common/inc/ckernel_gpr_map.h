// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_gpr_map.h
 * @brief General Purpose Register (GPR) Mapping for Wormhole B0 Tensix Threads
 *
 * This header defines the allocation and usage of General Purpose Registers (GPRs)
 * across the different processing threads in Wormhole B0 Tensix cores. It provides
 * structured constants for accessing specific registers within each thread context,
 * enabling consistent and organized register usage throughout the kernel.
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # Thread Architecture
 *
 * Wormhole B0 Tensix cores have multiple independent processing threads:
 * - **Unpack Thread**: Handles data input and preprocessing
 * - **Math Thread**: Performs mathematical operations (FPU/SFPU)
 * - **Pack Thread**: Handles data output and postprocessing
 *
 * Each thread has its own GPR space with specific register allocations
 * optimized for that thread's operations.
 *
 * # GPR Organization
 *
 * ## Common Registers (All Threads)
 * - Debug and control registers shared across threads
 * - Reserved registers for system functions
 *
 * ## Thread-Specific Registers
 * Each thread type has registers optimized for its operations:
 * - **Unpack**: Tile dimensions, addresses, performance counters
 * - **Math**: Operation bases, performance monitoring, temporary storage
 * - **Pack**: Output addressing, BFP compression, stream synchronization
 *
 * # Register Allocation Strategy
 *
 * The allocation follows these principles:
 * - **Low numbers (0-31)**: Core operational registers
 * - **Mid numbers (32-47)**: Performance and monitoring registers
 * - **High numbers (48-63)**: Extended functionality and temporary storage
 *
 * # Usage Example
 *
 * ```cpp
 * #include "ckernel_gpr_map.h"
 *
 * // Set tile size for unpacker A
 * TT_SETDMAREG(0, tile_size, 0, p_gpr_unpack::TILE_SIZE_A);
 *
 * // Configure pack output address
 * TT_SETDMAREG(0, output_addr, 0, p_gpr_pack::OUTPUT_ADDR);
 *
 * // Set math destination base
 * TT_SETDMAREG(0, dest_base, 0, p_gpr_math::DEST_OP0_BASE);
 * ```
 *
 * @warning Incorrect GPR usage can cause register conflicts between threads.
 *          Always use the defined constants rather than raw register numbers.
 *
 * @note These mappings are hand-coded and must match the hardware design.
 *       Changes to register allocation require careful coordination.
 *
 * @see ckernel.h for register access functions
 * @see ckernel_defs.h for thread definitions
 */

#pragma once

/**
 * @namespace ckernel
 * @brief Core kernel functionality and hardware abstraction layer
 */
namespace ckernel
{

/**
 * @defgroup GPRMappings General Purpose Register Mappings
 * @brief Thread-specific GPR allocation constants for Tensix cores
 * @{
 */

/**
 * @struct p_gpr
 * @brief Common GPR mapping across all processing threads
 *
 * Defines GPR registers that are shared or have consistent meaning
 * across all thread types (unpack, math, pack). These registers
 * provide fundamental system functionality.
 */
struct p_gpr
{
    constexpr static uint ZERO         = 0; ///< Always stores 0 (constant zero register)
    constexpr static uint DBG_RESERVED = 1; ///< Reserved for future debug functionality
    constexpr static uint DBG_MSG      = 2; ///< Firmware debug message register
    constexpr static uint DBG_CKID     = 3; ///< Ckernel ID for debugging
};

/**
 * @struct p_gpr_unpack
 * @brief GPR mapping for unpack thread operations
 *
 * Defines register allocation for the unpack thread, which handles data input,
 * preprocessing, and tile management. Registers are organized into functional
 * groups: addressing, tile management, performance monitoring, and state saving.
 *
 * # Register Organization:
 * - **4-19**: Core operational registers (addressing, tile info, temporaries)
 * - **32-48**: Performance monitoring and tile dimension registers
 * - **52-59**: State save/restore for tilizer operations
 */
struct p_gpr_unpack
{
    // Core Addressing and Operational Registers (4-19)
    constexpr static uint OPERAND_BASE_ADDR   = 4;  ///< Operand base address used by zero buffer function
    constexpr static uint OPERAND_OFFSET_ADDR = 5;  ///< Operand offset address used by zero buffer function
    constexpr static uint ZERO_0              = 8;  ///< Zero data register 0
    constexpr static uint ZERO_1              = 9;  ///< Zero data register 1
    constexpr static uint ZERO_2              = 10; ///< Zero data register 2
    constexpr static uint ZERO_3              = 11; ///< Zero data register 3
    constexpr static uint TMP0                = 12; ///< Temporary data register 0
    constexpr static uint TMP1                = 13; ///< Temporary data register 1
    constexpr static uint TILE_SIZE           = 14; ///< General tile size register
    constexpr static uint TILE_OFFSET         = 15; ///< Tile offset for addressing
    constexpr static uint L1_BUFFER_ADDR      = 17; ///< Fixed L1 buffer address for reduce input
    constexpr static uint TMP_LO              = 18; ///< Temporary data (upper 16-bits always 0)
    constexpr static uint TMP_HI              = 19; ///< Temporary data (lower 16-bits always 0)

    // Performance Monitoring Registers (32-48)
    constexpr static uint PERF_FIRST_UNP_LO       = 32; ///< First unpack instruction timestamp (low 32 bits)
    constexpr static uint PERF_FIRST_UNP_HI       = 33; ///< First unpack instruction timestamp (high 32 bits)
    constexpr static uint TILE_SIZE_A             = 36; ///< Tile size for unpacker 0 (source A)
    constexpr static uint TILE_SIZE_B             = 37; ///< Tile size for unpacker 1 (source B)
    constexpr static uint KT_DIM                  = 38; ///< Matrix multiplication K dimension
    constexpr static uint FACE_DIM_16x16          = 40; ///< Face dimension constant (16x16)
    constexpr static uint FACE_DIM_8x16           = 41; ///< Face dimension constant (8x16)
    constexpr static uint FACE_DIM_4x16           = 42; ///< Face dimension constant (4x16)
    constexpr static uint FACE_DIM_2x16           = 43; ///< Face dimension constant (2x16)
    constexpr static uint FACE_DIM_1x16           = 44; ///< Face dimension constant (1x16)
    constexpr static uint PERF_UNPACK_NUM_TILES_0 = 45; ///< Tile count for input operands 0-1
    constexpr static uint PERF_UNPACK_NUM_TILES_1 = 46; ///< Tile count for input operands 2-3
    constexpr static uint PERF_UNPACK_NUM_TILES_2 = 47; ///< Tile count for input operands 4-5
    constexpr static uint PERF_UNPACK_NUM_TILES_3 = 48; ///< Tile count for input operands 6-7

    // State Save/Restore Registers (52-59)
    constexpr static uint UNPACK_STRIDE               = 52; ///< Unpack A stride save/restore register
    constexpr static uint SR_UNPACK_TILIZER_STATE_0   = 54; ///< Tilizer state save register 0
    constexpr static uint SR_UNPACK_TILIZER_STATE_1   = 55; ///< Tilizer state save register 1
    constexpr static uint SR_UNPACK_UNTILIZER_STATE_0 = 56; ///< Untilizer state save register 0
    constexpr static uint SR_UNPACK_UNTILIZER_STATE_1 = 57; ///< Untilizer state save register 1
    constexpr static uint SR_UNPACK_UNTILIZER_STATE_2 = 58; ///< Untilizer state save register 2
    constexpr static uint SR_UNPACK_UNTILIZER_STATE_3 = 59; ///< Untilizer state save register 3
};

/**
 * @struct p_gpr_math
 * @brief GPR mapping for math thread operations
 *
 * Defines register allocation for the math thread, which handles mathematical
 * operations including FPU and SFPU operations. Registers are organized for
 * performance monitoring, destination management, and temporary storage.
 *
 * # Register Organization:
 * - **4-10**: Performance monitoring and control registers
 * - **48-53**: SFPU destination base and addressing registers
 * - **60-61**: Temporary storage and DRAM request tracking
 */
struct p_gpr_math
{
    // Performance Monitoring and Control (4-10)
    constexpr static uint PERF_DBUS_CNTL           = 4;  ///< Debug bus performance counter selection control
    constexpr static uint PERF_MEM_DUMP_CNTL_CLEAR = 5;  ///< Clear memory dump write flag
    constexpr static uint PERF_MEM_DUMP_CNTL_SET   = 6;  ///< Set memory dump write flag
    constexpr static uint PERF_CNT_START           = 7;  ///< Start performance counter
    constexpr static uint PERF_CNT_STOP            = 8;  ///< Stop performance counter
    constexpr static uint PERF_EPOCH_BASE_ADDR     = 9;  ///< Performance event/epoch ID base address
    constexpr static uint PERF_EPOCH_OFFSET        = 10; ///< Offset address for epoch variables

    // SFPU Destination Management (48-53)
    constexpr static uint DEST_OP0_BASE     = 48; ///< Destination base address for SFPU operation 0
    constexpr static uint DEST_OP1_BASE     = 49; ///< Destination base address for SFPU operation 1
    constexpr static uint DEST_REGW_OFFSET  = 50; ///< Destination read/write/clear base offset (1st set)
    constexpr static uint DEST_REGW_INCR    = 51; ///< Destination read/write/clear increment (1st set)
    constexpr static uint DEST_REGW_OFFSET2 = 52; ///< Destination read/write/clear base offset (2nd set)
    constexpr static uint DEST_REGW_INCR2   = 53; ///< Destination read/write/clear increment (2nd set)

    // Temporary and Request Tracking (60-61)
    constexpr static uint TMP0          = 60; ///< Temporary storage register
    constexpr static uint NUM_DRAM_REQS = 61; ///< Number of DRAM requests counter
};

/**
 * @struct p_gpr_pack
 * @brief GPR mapping for pack thread operations
 *
 * Defines register allocation for the pack thread, which handles data output,
 * formatting, and L1 memory writes. Registers are organized for destination
 * management, output addressing, BFP compression control, and stream synchronization.
 *
 * # Register Organization:
 * - **4-31**: Core pack operations (addressing, headers, temporaries)
 * - **32-51**: Stream synchronization and performance monitoring
 * - **52-63**: BFP compression section size management
 */
struct p_gpr_pack
{
    // Core Pack Operations (4-31)
    constexpr static uint DEST_OFFSET_LO    = 4;  ///< Destination lower bank offsets
    constexpr static uint DEST_OFFSET_HI    = 8;  ///< Destination upper bank offsets
    constexpr static uint OUTPUT_ADDR       = 12; ///< Output address that packer is writing to
    constexpr static uint TILE_HEADER       = 16; ///< Tile header containing ID and tile size
    constexpr static uint TEMP_TILE_OFFSET  = 20; ///< Temporary variable for tile offset in destination
    constexpr static uint NUM_MSGS_RECEIVED = 24; ///< Tile count and word size storage
    constexpr static uint ONE_MSG_RECEIVED  = 25; ///< Single tile count for streaming operations
    constexpr static uint HEADER_ADDR       = 26; ///< Header address (used by pack shift kernel)
    constexpr static uint TMP0              = 28; ///< Temporary data register 0
    constexpr static uint TMP1              = 29; ///< Temporary data register 1
    constexpr static uint TMP_LO            = 30; ///< Temporary data (upper 16-bit always 0)
    constexpr static uint TMP_HI            = 31; ///< Temporary data (lower 16-bit always 0)

    // Stream Synchronization and Performance (32-51)
    constexpr static uint PACK_STREAM_SYNC    = 32; ///< Synchronization between pack and output stream [32:63]
    constexpr static uint OUTPUT_ADDR_OFFSET  = 50; ///< Output offset address added to OUTPUT_ADDR
    constexpr static uint PERF_PACK_NUM_TILES = 51; ///< Output operand tile count for performance

    // BFP Compression Section Sizes (52-63)
    constexpr static uint EXP0_SEC_SIZE_BFP  = 52; ///< Pack0,1,2,3 exp section size for BFP8/4/2
    constexpr static uint EXP1_SEC_SIZE_BFP8 = 53; ///< Pack1 exp section size for BFP8
    constexpr static uint EXP2_SEC_SIZE_BFP8 = 54; ///< Pack2 exp section size for BFP8
    constexpr static uint EXP3_SEC_SIZE_BFP8 = 55; ///< Pack3 exp section size for BFP8
    constexpr static uint EXP1_SEC_SIZE_BFP4 = 57; ///< Pack1 exp section size for BFP4
    constexpr static uint EXP2_SEC_SIZE_BFP4 = 58; ///< Pack2 exp section size for BFP4
    constexpr static uint EXP3_SEC_SIZE_BFP4 = 59; ///< Pack3 exp section size for BFP4
    constexpr static uint EXP1_SEC_SIZE_BFP2 = 61; ///< Pack1 exp section size for BFP2
    constexpr static uint EXP2_SEC_SIZE_BFP2 = 62; ///< Pack2 exp section size for BFP2
    constexpr static uint EXP3_SEC_SIZE_BFP2 = 63; ///< Pack3 exp section size for BFP2
};

/** @} */ // end of GPRMappings group

} // namespace ckernel
