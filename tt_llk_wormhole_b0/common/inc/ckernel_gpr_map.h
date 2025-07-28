// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_gpr_map.h
 * @brief General Purpose Register allocation for Wormhole B0 Tensix 3-thread architecture
 *
 * @details This header defines the allocation and usage of General Purpose Registers (GPRs)
 * across the Wormhole B0 Tensix engine's 3-thread execution model. GPRs serve as fast local
 * storage for frequently accessed values, configuration parameters, and intermediate calculations
 * specific to each thread's specialized role in the compute pipeline.
 *
 * **Wormhole B0 GPR Architecture:**
 * Each Tensix thread has access to its own set of GPRs for thread-specific operations:
 * - **Thread 0 (Unpack)**: GPRs for unpacker configuration, L1→register data movement
 * - **Thread 1 (Math)**: GPRs for compute configuration, SFPU parameters, math operations
 * - **Thread 2 (Pack)**: GPRs for packer configuration, register→L1 data movement
 * - **Common GPRs**: Shared registers for debug, synchronization, and global state
 *
 * **GPR Access Characteristics:**
 * - **Multi-Client Access**: Accessible by TRISC processors, THCON EXU, and CFG EXU
 * - **Arbitration**: Hardware arbitration ensures conflict-free access across clients
 * - **Variable Latency**: Port conflicts may cause variable instruction completion times
 * - **Shared Resource**: Coordinated allocation prevents register conflicts between uses
 *
 * **GPR Usage Patterns:**
 * 1. **Configuration Passing**: Transfer settings from software to hardware units
 * 2. **Instruction Arguments**: Parameters for Tensix arithmetic and data movement instructions
 * 3. **L1 Access Arguments**: Address and control parameters for LOADIND/STOREIND operations
 * 4. **MMIO Interface**: Arguments for accessing RISC-V MMIO registers via THCON
 * 5. **Intermediate Storage**: Temporary values during complex computation sequences
 *
 * **Performance Considerations:**
 * - **Access Latency**: ~1-2 cycles depending on arbitration and port availability
 * - **Thread Coordination**: Explicit barriers required for dependencies between threads
 * - **Resource Management**: Careful allocation prevents conflicts in multi-threaded execution
 * - **Local Memory Alternative**: TRISC local memory provides lower latency for frequent access
 */

#pragma once

// Hand-coded parameter encoding for various GPR mappings
namespace ckernel
{

/**
 * @brief Common GPR allocation shared across all Tensix threads
 * @details Defines standard GPR usage patterns that are consistent across
 * Unpack, Math, and Pack threads for debugging, synchronization, and global state management.
 */
struct p_gpr
{
    /**
     * @brief Constant zero register
     * @details Always contains value 0, used for initializations and as a
     * source operand when zero values are needed in computations.
     */
    constexpr static uint ZERO         = 0;
    
    /**
     * @brief Reserved register for future debug functionality
     * @details Reserved for potential future debugging features and
     * development tools integration.
     */
    constexpr static uint DBG_RESERVED = 1;
    
    /**
     * @brief Firmware debug message register
     * @details Stores debug message identifiers or pointers for firmware
     * debugging and diagnostic information exchange.
     */
    constexpr static uint DBG_MSG      = 2;
    
    /**
     * @brief Compute kernel identifier register
     * @details Contains the current compute kernel ID for tracking and
     * debugging purposes across the 3-thread execution pipeline.
     */
    constexpr static uint DBG_CKID     = 3;
};

/**
 * @brief Unpack thread (Thread 0) specific GPR allocation
 * @details Defines GPR usage for the Unpack thread, which handles L1→register
 * data movement and preparation for Math thread consumption.
 */
struct p_gpr_unpack
{
    constexpr static uint OPERAND_BASE_ADDR       = 4;      // Operand base address used by zero buffer function
    constexpr static uint OPERAND_OFFSET_ADDR     = 5;      // Operand offset address used by zero buffer function
    constexpr static uint ZERO_0                  = 8;      // Zero data
    constexpr static uint ZERO_1                  = 9;      // Zero data
    constexpr static uint ZERO_2                  = 10;     // Zero data
    constexpr static uint ZERO_3                  = 11;     // Zero data
    constexpr static uint TMP0                    = 12;     // Temp data
    constexpr static uint TMP1                    = 13;     // Temp data
    constexpr static uint TILE_SIZE               = 14;     // Tile size
    constexpr static uint TILE_OFFSET             = 15;     // Tile offset
    constexpr static uint L1_BUFFER_ADDR          = 17;     // Holds address of fixed l1 buffer used for reduce in1
    constexpr static uint TMP_LO                  = 18;     // Temp data. Upper 16-bits always 0
    constexpr static uint TMP_HI                  = 19;     // Temp data. Lower 16-bits always 0
    constexpr static uint PERF_FIRST_UNP_LO       = 32;     // timestamp for first-unpack-instruction (low 32b)
    constexpr static uint PERF_FIRST_UNP_HI       = 33;     // timestamp for first-unpack-instruction (high 32b)
    constexpr static uint TILE_SIZE_A             = 36;     // Holds tile size for unpacker 0
    constexpr static uint TILE_SIZE_B             = 37;     // Holds tile size for unpacker 1
    constexpr static uint KT_DIM                  = 38;     // Holds matmul kt_dim
    constexpr static uint FACE_DIM_16x16          = 40;     // Holds face dimension (16x16)
    constexpr static uint FACE_DIM_8x16           = 41;     // Holds face dimension (8x16)
    constexpr static uint FACE_DIM_4x16           = 42;     // Holds face dimension (4x16)
    constexpr static uint FACE_DIM_2x16           = 43;     // Holds face dimension (2x16)
    constexpr static uint FACE_DIM_1x16           = 44;     // Holds face dimension (1x16)
    constexpr static uint PERF_UNPACK_NUM_TILES_0 = 45;     // num tiles for input operands 0-1
    constexpr static uint PERF_UNPACK_NUM_TILES_1 = 46;     // num tiles for input operands 2-3
    constexpr static uint PERF_UNPACK_NUM_TILES_2 = 47;     // num tiles for input operands 4-5
    constexpr static uint PERF_UNPACK_NUM_TILES_3 = 48;     // num tiles for input operands 6-7
    constexpr static uint UNPACK_STRIDE           = 52;     // Used to save/restore unpack A stride (UNP0_ADDR_CTRL_ZW_REG_1_Zstride register)
                                                            // before/after unpacking directly to dest
    constexpr static uint SR_UNPACK_TILIZER_STATE_0   = 54; // Save unpack state before tilizer is enabled for quick restore
    constexpr static uint SR_UNPACK_TILIZER_STATE_1   = 55;
    constexpr static uint SR_UNPACK_UNTILIZER_STATE_0 = 56; // Save unpack state before tilizer is enabled for quick restore
    constexpr static uint SR_UNPACK_UNTILIZER_STATE_1 = 57;
    constexpr static uint SR_UNPACK_UNTILIZER_STATE_2 = 58;
    constexpr static uint SR_UNPACK_UNTILIZER_STATE_3 = 59;
};

// Math GPR thread
struct p_gpr_math
{
    constexpr static uint PERF_DBUS_CNTL           = 4;  // Control debug bus perf counter selection
    constexpr static uint PERF_MEM_DUMP_CNTL_CLEAR = 5;  // Clear write to memory flag
    constexpr static uint PERF_MEM_DUMP_CNTL_SET   = 6;  // Set write to memory flag
    constexpr static uint PERF_CNT_START           = 7;  // Start perf counter
    constexpr static uint PERF_CNT_STOP            = 8;  // Stop perf counter
    constexpr static uint PERF_EPOCH_BASE_ADDR     = 9;  // Perf event ID
    constexpr static uint PERF_EPOCH_OFFSET        = 10; // The offset address for epoch variables
    constexpr static uint DEST_OP0_BASE            = 48; // dest base for sfpu op0
    constexpr static uint DEST_OP1_BASE            = 49; // dest base for sfpu op1
    constexpr static uint DEST_REGW_OFFSET         = 50; // dest rwc base (1st set)
    constexpr static uint DEST_REGW_INCR           = 51; // dest rwc incr (1st set)
    constexpr static uint DEST_REGW_OFFSET2        = 52; // dest rwc base (2nd set)
    constexpr static uint DEST_REGW_INCR2          = 53; // dest rwc incr (2nd set)
    constexpr static uint TMP0                     = 60;
    constexpr static uint NUM_DRAM_REQS            = 61;
};

// Pack GPR thread
struct p_gpr_pack
{
    constexpr static uint DEST_OFFSET_LO = 4;  // dest lower bank offsets
    constexpr static uint DEST_OFFSET_HI = 8;  // dest upper bank offsets
    constexpr static uint OUTPUT_ADDR    = 12; // output address that packer is writing to
    constexpr static uint TILE_HEADER    = 16; // tile header - ID + tile size

    constexpr static uint TEMP_TILE_OFFSET    = 20; // Temp var which holds tile offset in dest
    constexpr static uint NUM_MSGS_RECEIVED   = 24; // holds tile count and word size
    constexpr static uint ONE_MSG_RECEIVED    = 25; // by default holds 1 tile count and word size for streaming per tile
    constexpr static uint HEADER_ADDR         = 26; // Holds the address of the header (used by pack shift kernel only)
    constexpr static uint TMP0                = 28; // Temp data
    constexpr static uint TMP1                = 29; // Temp data
    constexpr static uint TMP_LO              = 30; // Temp data, upper 16-bit always 0
    constexpr static uint TMP_HI              = 31; // Temp data, lower 16-bit always 0
    constexpr static uint PACK_STREAM_SYNC    = 32; // sync between pack and output stream [32:63]
    constexpr static uint OUTPUT_ADDR_OFFSET  = 50; // output offset address that's added to OUTPUT_ADDR
    constexpr static uint PERF_PACK_NUM_TILES = 51; // output operand num tiles
    constexpr static uint EXP0_SEC_SIZE_BFP   = 52; // pack0,1,2,3 exp section size for bfp8,4,2
    constexpr static uint EXP1_SEC_SIZE_BFP8  = 53; // pack1 exp section size for bfp8
    constexpr static uint EXP2_SEC_SIZE_BFP8  = 54; // pack2 exp section size for bfp8
    constexpr static uint EXP3_SEC_SIZE_BFP8  = 55; // pack2 exp section size for bfp8
    constexpr static uint EXP1_SEC_SIZE_BFP4  = 57; // pack1 exp section size for bfp4
    constexpr static uint EXP2_SEC_SIZE_BFP4  = 58; // pack2 exp section size for bfp4
    constexpr static uint EXP3_SEC_SIZE_BFP4  = 59; // pack3 exp section size for bfp4
    constexpr static uint EXP1_SEC_SIZE_BFP2  = 61; // pack1 exp section size for bfp2
    constexpr static uint EXP2_SEC_SIZE_BFP2  = 62; // pack2 exp section size for bfp2
    constexpr static uint EXP3_SEC_SIZE_BFP2  = 63; // pack3 exp section size for bfp2
};

} // namespace ckernel
