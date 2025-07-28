// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

/**
 * @file ckernel_gpr_map.h
 * @brief General Purpose Register (GPR) mapping and allocation for kernel threads
 *
 * @details This file defines the allocation and usage of General Purpose Registers
 * across different execution threads in the compute kernel framework. GPRs provide
 * fast local storage for frequently accessed values, intermediate calculations,
 * and thread-specific state management.
 *
 * **Thread-Specific Register Allocation:**
 * The kernel framework operates with three specialized execution threads, each
 * with dedicated GPR allocations optimized for their specific responsibilities:
 * - **Common GPRs**: Shared across all threads for system-wide values
 * - **Unpack Thread GPRs**: Data input processing and format conversion
 * - **Math Thread GPRs**: Mathematical computations and SFPU operations
 * - **Pack Thread GPRs**: Data output processing and memory management
 *
 * **Performance Benefits:**
 * - Zero-latency access to frequently used values
 * - Thread-specific optimization for common operations
 * - Reduced memory bandwidth for temporary storage
 * - Hardware-optimized register allocation for peak performance
 *
 * **Usage Patterns:**
 * - Temporary calculations and intermediate results
 * - Frequently accessed configuration parameters
 * - Performance monitoring and debugging data
 * - Thread coordination and synchronization state
 */

// Hand-coded parameter encoding for various GPR mappings
namespace ckernel
{

/**
 * @brief Common GPR allocation shared across all kernel execution threads
 *
 * @details Defines General Purpose Registers that are accessible and meaningful
 * across all kernel execution threads. These registers store system-wide values,
 * debugging information, and fundamental constants that are needed by multiple
 * threads in the kernel pipeline.
 *
 * **Shared Register Categories:**
 * - **System Constants**: Always-available constant values (zero)
 * - **Debug Infrastructure**: Debugging and diagnostic information
 * - **Thread Coordination**: Cross-thread communication values
 *
 * **Hardware Integration:**
 * - Registers accessible from all thread contexts
 * - Maintained across thread switches and context changes
 * - Used for fundamental system operations and debugging
 * - Critical for consistent behavior across execution contexts
 */
// Common GPR mapping across all threads
struct p_gpr
{
    /**
     * @brief Zero constant register
     * @details Always contains value 0, used for comparisons, initialization, and as null source
     */
    constexpr static uint ZERO         = 0; // Always stores 0
    
    /**
     * @brief Reserved register for future debug functionality
     * @details Reserved for potential future debugging features and system extensions
     */
    constexpr static uint DBG_RESERVED = 1; // Reserved for future use
    
    /**
     * @brief Firmware debug message register
     * @details Stores debug messages and diagnostic information from firmware
     */
    constexpr static uint DBG_MSG      = 2; // Firmware debug message
    
    /**
     * @brief Compute kernel ID register
     * @details Contains the unique identifier for the currently executing compute kernel
     */
    constexpr static uint DBG_CKID     = 3; // Ckernel ID
};

/**
 * @brief Unpack thread GPR allocation for data input processing
 *
 * @details Defines General Purpose Registers specifically allocated for the unpack
 * thread, which handles data input, format conversion, and memory staging operations.
 * These registers are optimized for data flow management and input processing efficiency.
 *
 * **Register Categories:**
 * - **Memory Management**: Base addresses, offsets, and buffer pointers
 * - **Data Dimensions**: Tile sizes, face dimensions, and layout parameters
 * - **Temporary Storage**: Intermediate calculations and data staging
 * - **Performance Monitoring**: Timing and throughput measurement
 * - **State Management**: Configuration state save/restore operations
 *
 * **Optimization Focus:**
 * - Fast access to frequently used memory addresses
 * - Efficient tile and face dimension calculations
 * - Performance monitoring for input bottleneck analysis
 * - State preservation for complex unpacking sequences
 */
// Unpack GPR thread
struct p_gpr_unpack
{
    /**
     * @brief Operand base address for zero buffer operations
     * @details Base memory address used by zero buffer function for operand initialization
     */
    constexpr static uint OPERAND_BASE_ADDR       = 4;      // Operand base address used by zero buffer function
    
    /**
     * @brief Operand offset address for zero buffer operations  
     * @details Offset from base address used by zero buffer function for precise addressing
     */
    constexpr static uint OPERAND_OFFSET_ADDR     = 5;      // Operand offset address used by zero buffer function
    
    /**
     * @brief Zero data storage registers
     * @details Pre-loaded zero values for fast zero initialization and clearing operations
     */
    constexpr static uint ZERO_0                  = 8;      // Zero data
    constexpr static uint ZERO_1                  = 9;      // Zero data
    constexpr static uint ZERO_2                  = 10;     // Zero data
    constexpr static uint ZERO_3                  = 11;     // Zero data
    
    /**
     * @brief Temporary data storage registers
     * @details General-purpose temporary storage for intermediate calculations
     */
    constexpr static uint TMP0                    = 12;     // Temp data
    constexpr static uint TMP1                    = 13;     // Temp data
    
    /**
     * @brief Tile size and offset management
     * @details Storage for tile dimension and positioning calculations
     */
    constexpr static uint TILE_SIZE               = 14;     // Tile size
    constexpr static uint TILE_OFFSET             = 15;     // Tile offset
    
    /**
     * @brief L1 buffer address for reduction operations
     * @details Fixed L1 buffer address used specifically for reduce input operations
     */
    constexpr static uint L1_BUFFER_ADDR          = 17;     // Holds address of fixed l1 buffer used for reduce in1
    
    /**
     * @brief Temporary storage with guaranteed bit patterns
     * @details Specialized temporary registers with guaranteed upper/lower bit constraints
     */
    constexpr static uint TMP_LO                  = 18;     // Temp data. Upper 16-bits always 0
    constexpr static uint TMP_HI                  = 19;     // Temp data. Lower 16-bits always 0
    
    /**
     * @brief Performance monitoring: first unpack instruction timestamp
     * @details 64-bit timestamp for measuring unpack operation latency (split across two registers)
     */
    constexpr static uint PERF_FIRST_UNP_LO       = 32;     // timestamp for first-unpack-instruction (low 32b)
    constexpr static uint PERF_FIRST_UNP_HI       = 33;     // timestamp for first-unpack-instruction (high 32b)
    
    /**
     * @brief Unpacker-specific tile size storage
     * @details Separate tile size storage for different unpacker units
     */
    constexpr static uint TILE_SIZE_A             = 36;     // Holds tile size for unpacker 0
    constexpr static uint TILE_SIZE_B             = 37;     // Holds tile size for unpacker 1
    
    /**
     * @brief Matrix multiplication K-dimension storage
     * @details Stores the K dimension for matrix multiplication operations (inner dimension)
     */
    constexpr static uint KT_DIM                  = 38;     // Holds matmul kt_dim
    
    /**
     * @brief Face dimension storage for different shapes
     * @details Pre-calculated face dimensions for efficient shape-specific processing
     */
    constexpr static uint FACE_DIM_16x16          = 40;     // Holds face dimension (16x16)
    constexpr static uint FACE_DIM_8x16           = 41;     // Holds face dimension (8x16)
    constexpr static uint FACE_DIM_4x16           = 42;     // Holds face dimension (4x16)
    constexpr static uint FACE_DIM_2x16           = 43;     // Holds face dimension (2x16)
    constexpr static uint FACE_DIM_1x16           = 44;     // Holds face dimension (1x16)
    
    /**
     * @brief Performance monitoring: tile count per operand group
     * @details Tracks number of tiles processed for different operand pairs (for performance analysis)
     */
    constexpr static uint PERF_UNPACK_NUM_TILES_0 = 45;     // num tiles for input operands 0-1
    constexpr static uint PERF_UNPACK_NUM_TILES_1 = 46;     // num tiles for input operands 2-3
    constexpr static uint PERF_UNPACK_NUM_TILES_2 = 47;     // num tiles for input operands 4-5
    constexpr static uint PERF_UNPACK_NUM_TILES_3 = 48;     // num tiles for input operands 6-7
    
    /**
     * @brief Unpack stride configuration save/restore
     * @details Saves and restores unpack A stride register for direct-to-destination unpacking
     */
    constexpr static uint UNPACK_STRIDE           = 52;     // Used to save/restore unpack A stride (UNP0_ADDR_CTRL_ZW_REG_1_Zstride register)
                                                            // before/after unpacking directly to dest
    
    /**
     * @brief Tilizer state save/restore registers
     * @details Saves unpack state before tilizer operations for quick restoration
     */
    constexpr static uint SR_UNPACK_TILIZER_STATE_0   = 54; // Save unpack state before tilizer is enabled for quick restore
    constexpr static uint SR_UNPACK_TILIZER_STATE_1   = 55;
    
    /**
     * @brief Untilizer state save/restore registers
     * @details Saves unpack state before untilizer operations for quick restoration
     */
    constexpr static uint SR_UNPACK_UNTILIZER_STATE_0 = 56; // Save unpack state before untilizer is enabled for quick restore
    constexpr static uint SR_UNPACK_UNTILIZER_STATE_1 = 57;
    constexpr static uint SR_UNPACK_UNTILIZER_STATE_2 = 58;
    constexpr static uint SR_UNPACK_UNTILIZER_STATE_3 = 59;
};

/**
 * @brief Math thread GPR allocation for computational operations
 *
 * @details Defines General Purpose Registers specifically allocated for the math
 * thread, which handles mathematical computations, SFPU operations, and numerical
 * processing. These registers are optimized for computation efficiency and result management.
 *
 * **Register Categories:**
 * - **Performance Monitoring**: Debug bus control, counters, and profiling
 * - **Memory Management**: Memory dump control and coordination
 * - **Destination Management**: SFPU operation base addresses and offsets
 * - **Register Window Control**: Register file addressing and increment management
 * - **Temporary Storage**: Intermediate calculation results
 *
 * **Optimization Focus:**
 * - Fast access to SFPU operation parameters
 * - Efficient destination register management
 * - Performance monitoring for computational bottlenecks
 * - Memory coordination for result storage
 */
// Math GPR thread
struct p_gpr_math
{
    /**
     * @brief Performance debug bus control
     * @details Controls debug bus performance counter selection for monitoring math operations
     */
    constexpr static uint PERF_DBUS_CNTL           = 4;  // Control debug bus perf counter selection
    
    /**
     * @brief Memory dump control flags
     * @details Controls when performance data should be written to memory
     */
    constexpr static uint PERF_MEM_DUMP_CNTL_CLEAR = 5;  // Clear write to memory flag
    constexpr static uint PERF_MEM_DUMP_CNTL_SET   = 6;  // Set write to memory flag
    
    /**
     * @brief Performance counter control
     * @details Start and stop controls for performance measurement timing
     */
    constexpr static uint PERF_CNT_START           = 7;  // Start perf counter
    constexpr static uint PERF_CNT_STOP            = 8;  // Stop perf counter
    
    /**
     * @brief Performance epoch management
     * @details Base address and offset for performance epoch variable storage
     */
    constexpr static uint PERF_EPOCH_BASE_ADDR     = 9;  // Perf event ID
    constexpr static uint PERF_EPOCH_OFFSET        = 10; // The offset address for epoch variables
    
    /**
     * @brief SFPU operation destination base addresses
     * @details Base addresses for SFPU operation 0 and 1 destination management
     */
    constexpr static uint DEST_OP0_BASE            = 48; // dest base for sfpu op0
    constexpr static uint DEST_OP1_BASE            = 49; // dest base for sfpu op1
    
    /**
     * @brief Destination register window control (first set)
     * @details Register window offset and increment for destination addressing
     */
    constexpr static uint DEST_REGW_OFFSET         = 50; // dest rwc base (1st set)
    constexpr static uint DEST_REGW_INCR           = 51; // dest rwc incr (1st set)
    
    /**
     * @brief Destination register window control (second set)
     * @details Second set of register window controls for complex addressing patterns
     */
    constexpr static uint DEST_REGW_OFFSET2        = 52; // dest rwc base (2nd set)
    constexpr static uint DEST_REGW_INCR2          = 53; // dest rwc incr (2nd set)
    
    /**
     * @brief General temporary storage and DRAM request tracking
     * @details Temporary storage and DRAM request counter for memory coordination
     */
    constexpr static uint TMP0                     = 60;
    constexpr static uint NUM_DRAM_REQS            = 61;
};

/**
 * @brief Pack thread GPR allocation for data output processing
 *
 * @details Defines General Purpose Registers specifically allocated for the pack
 * thread, which handles data output, format conversion, and memory writeback operations.
 * These registers are optimized for output efficiency and memory coordination.
 *
 * **Register Categories:**
 * - **Destination Management**: Offset calculation and address management
 * - **Output Coordination**: Address tracking and header management
 * - **Message Handling**: Tile count and synchronization management
 * - **Temporary Storage**: Intermediate calculations and staging
 * - **Stream Synchronization**: Multi-stream coordination and control
 * - **Format-Specific Storage**: BFP exponent section size management
 *
 * **Optimization Focus:**
 * - Fast access to output addresses and offsets
 * - Efficient tile header and message management
 * - Stream synchronization for parallel output operations
 * - Format-specific optimization for BFP data types
 */
// Pack GPR thread
struct p_gpr_pack
{
    /**
     * @brief Destination offset management for memory banks
     * @details Separate offset storage for lower and upper destination memory banks
     */
    constexpr static uint DEST_OFFSET_LO = 4;  // dest lower bank offsets
    constexpr static uint DEST_OFFSET_HI = 8;  // dest upper bank offsets
    
    /**
     * @brief Output address tracking
     * @details Current output address that the packer is writing to
     */
    constexpr static uint OUTPUT_ADDR    = 12; // output address that packer is writing to
    
    /**
     * @brief Tile header information
     * @details Complete tile header including ID and size information
     */
    constexpr static uint TILE_HEADER    = 16; // tile header - ID + tile size

    /**
     * @brief Temporary variables for pack operations
     * @details Intermediate storage for pack calculations and temporary tile positioning
     */
    constexpr static uint TEMP_TILE_OFFSET    = 20; // Temp var which holds tile offset in dest
    
    /**
     * @brief Message count management for streaming operations
     * @details Tile count and word size tracking for streaming and messaging
     */
    constexpr static uint NUM_MSGS_RECEIVED   = 24; // holds tile count and word size
    constexpr static uint ONE_MSG_RECEIVED    = 25; // by default holds 1 tile count and word size for streaming per tile
    
    /**
     * @brief Header address storage for specialized kernels
     * @details Address of tile header (used specifically by pack shift kernel)
     */
    constexpr static uint HEADER_ADDR         = 26; // Holds the address of the header (used by pack shift kernel only)
    
    /**
     * @brief General and specialized temporary storage
     * @details General temporary data and constrained bit-pattern temporary storage
     */
    constexpr static uint TMP0                = 28; // Temp data
    constexpr static uint TMP1                = 29; // Temp data
    constexpr static uint TMP_LO              = 30; // Temp data, upper 16-bit always 0
    constexpr static uint TMP_HI              = 31; // Temp data, lower 16-bit always 0
    
    /**
     * @brief Pack stream synchronization
     * @details Synchronization registers between pack operations and output streams [32:63]
     */
    constexpr static uint PACK_STREAM_SYNC    = 32; // sync between pack and output stream [32:63]
    
    /**
     * @brief Output address offset and performance monitoring
     * @details Address offset for output operations and tile count for performance analysis
     */
    constexpr static uint OUTPUT_ADDR_OFFSET  = 50; // output offset address that's added to OUTPUT_ADDR
    constexpr static uint PERF_PACK_NUM_TILES = 51; // output operand num tiles
    
    /**
     * @brief BFP format exponent section size management
     * @details Storage for exponent section sizes across different BFP formats and pack units
     * 
     * **BFP8 Format Exponent Sizes:**
     */
    constexpr static uint EXP0_SEC_SIZE_BFP   = 52; // pack0,1,2,3 exp section size for bfp8,4,2
    constexpr static uint EXP1_SEC_SIZE_BFP8  = 53; // pack1 exp section size for bfp8
    constexpr static uint EXP2_SEC_SIZE_BFP8  = 54; // pack2 exp section size for bfp8
    constexpr static uint EXP3_SEC_SIZE_BFP8  = 55; // pack3 exp section size for bfp8
    
    /**
     * @brief BFP4 Format Exponent Sizes:
     */
    constexpr static uint EXP1_SEC_SIZE_BFP4  = 57; // pack1 exp section size for bfp4
    constexpr static uint EXP2_SEC_SIZE_BFP4  = 58; // pack2 exp section size for bfp4
    constexpr static uint EXP3_SEC_SIZE_BFP4  = 59; // pack3 exp section size for bfp4
    
    /**
     * @brief BFP2 Format Exponent Sizes:
     */
    constexpr static uint EXP1_SEC_SIZE_BFP2  = 61; // pack1 exp section size for bfp2
    constexpr static uint EXP2_SEC_SIZE_BFP2  = 62; // pack2 exp section size for bfp2
    constexpr static uint EXP3_SEC_SIZE_BFP2  = 63; // pack3 exp section size for bfp2
};

} // namespace ckernel
