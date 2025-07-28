// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

// #include "kernel_types.h"
#include "ckernel.h"
#include "ckernel_globals.h"
#include "ckernel_sfpu.h"
#include "ckernel_template.h"
#include "llk_defs.h"

/**
 * @file cmath_common.h
 * @brief Common mathematical operations and utilities for compute kernel math thread
 *
 * @details This file provides fundamental mathematical operations, data movement utilities,
 * and tile management functions for the math execution thread in the compute kernel framework.
 * It includes low-level operations for register management, synchronization, and mathematical
 * computations optimized for the Tensix architecture.
 *
 * **Key Functionality Categories:**
 * - **Tile Size Constants**: Compile-time constants for different tile shapes
 * - **Data Movement**: Register-to-register data transfer operations
 * - **Synchronization**: Math thread coordination with unpack and pack threads
 * - **Address Management**: Destination register addressing and offset calculation
 * - **Format Utilities**: Data format detection and conversion utilities
 * - **Math Fidelity**: Precision control for mathematical operations
 *
 * **Performance Optimization:**
 * - Template-based optimizations for compile-time tile shape selection
 * - Hardware-specific instruction sequences for optimal data movement
 * - Efficient synchronization primitives for pipeline coordination
 * - Zero-overhead abstractions for common mathematical operations
 *
 * **Hardware Integration:**
 * - Direct integration with Tensix vector units and SFPU
 * - Optimized for Blackhole architecture register organization
 * - Hardware-accelerated data movement and synchronization
 * - Efficient utilization of destination register file organization
 */

#ifndef SFPU_OP_PARAM
#define SFPU_OP_PARAM 0
#endif

#ifndef FUSE_SQRT_RECIP
#define FUSE_SQRT_RECIP 0
#endif

using namespace ckernel;

namespace ckernel::math
{

/**
 * @brief Tile size constants for different tile shapes
 *
 * @details Compile-time constants defining the size of destination tiles in different
 * geometric configurations. These constants are used for efficient address calculation
 * and memory allocation across different tile shapes supported by the hardware.
 *
 * **Tile Shape Organization:**
 * - Index 0: 32x32 tiles (64 face units) - Standard full tiles
 * - Index 1: 32x16 or 16x32 tiles (32 face units) - Half-size tiles
 * - Index 2: 16x16 tiles (16 face units) - Quarter-size tiles
 *
 * **Usage in Address Calculation:**
 * These constants are used with bit shift operations for fast tile address
 * computation, enabling efficient memory layout and access patterns.
 */
constexpr uint DstTileSize[3] = {
    64, // 32x32 tile shape (4 faces × 16 face units = 64 total units)
    32, // 32x16, 16x32 tile shape (2 faces × 16 face units = 32 total units)  
    16  // 16x16 tile shape (1 face × 16 face units = 16 total units)
};

/**
 * @brief Log₂ of tile sizes for efficient bit-shift addressing
 *
 * @details Pre-calculated logarithm base 2 values for tile sizes, enabling
 * efficient address calculation using bit shift operations instead of
 * multiplication. Critical for performance in address generation.
 *
 * **Bit Shift Optimization:**
 * - tile_index << DstTileSizeLog2[shape] == tile_index * DstTileSize[shape]
 * - Hardware bit shifts are significantly faster than multiplication
 * - Enables single-cycle address calculations in critical paths
 */
constexpr uint DstTileSizeLog2[3] = {
    6, // log₂(64) = 6 for 32x32 tile shape
    5, // log₂(32) = 5 for 32x16, 16x32 tile shape
    4  // log₂(16) = 4 for 16x16 tile shape
};

/**
 * @brief Replay buffer offset for SFPU/FPU operation separation
 *
 * @details Defines the boundary in replay buffer allocation between SFPU
 * (Special Function Processing Unit) and FPU (Floating Point Unit) operations.
 * The first 16 entries are reserved for SFPU, the next 16 for FPU.
 *
 * **Buffer Organization:**
 * - Entries 0-15: SFPU operation replay buffer
 * - Entries 16-31: FPU operation replay buffer
 * - Prevents resource conflicts between different processing units
 * - Enables parallel operation of SFPU and FPU subsystems
 */
constexpr uint replay_buf_offset = 16; // split replay buffer usage between fpu/sfpu
                                       // first 16 for sfpu, next 16 for fpu

//
// **Counter and Address Management Functions**
//

/**
 * @brief Reset read/write counters for register addressing
 *
 * @details Resets the hardware read/write counters (RWC) used for register
 * addressing and auto-increment functionality. Essential for establishing
 * known initial state for register access patterns.
 *
 * **Counter Reset:**
 * - Clears all counter values to zero
 * - Establishes deterministic starting state
 * - Critical for repeatable register access patterns
 * - Used at the beginning of major computation phases
 *
 * @param setrwc Counter configuration flags for reset operation
 */
inline void reset_counters(const uint setrwc)
{
    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, setrwc);  // Reset counters with specified configuration
}

/**
 * @brief Increment read/write counters for register addressing
 *
 * @details Increments hardware counters for source A, source B, destination,
 * and configuration registers. Enables efficient register stepping for
 * sequential data processing operations.
 *
 * **Counter Increments:**
 * - incr_a: Source A register counter increment
 * - incr_b: Source B register counter increment  
 * - incr_d: Destination register counter increment
 * - incr_cr: Configuration register counter increment
 *
 * **Use Cases:**
 * - Sequential tile processing
 * - Automatic register advancement
 * - Pipeline register management
 * - Efficient loop-based operations
 *
 * @param incr_a Source A register increment value
 * @param incr_b Source B register increment value
 * @param incr_d Destination register increment value
 * @param incr_cr Configuration register increment value
 */
inline void incr_counters(const uint incr_a, const uint incr_b, const uint incr_d, const uint incr_cr)
{
    TT_INCRWC(incr_cr, incr_d, incr_b, incr_a);  // Increment all specified counters atomically
}

//
// **Data Movement Operations**
//

/**
 * @brief Move complete face from destination to source A registers
 *
 * @details Transfers a complete 16x16 face worth of data from destination
 * registers to source A registers using fixed addressing. Performs the
 * transfer in 4-row chunks for optimal memory bandwidth utilization.
 *
 * **Transfer Characteristics:**
 * - Source: Destination register file
 * - Target: Source A register bank
 * - Size: Complete face (16 rows × 16 elements)
 * - Method: Four 4-row transfer operations
 * - Synchronization: Waits for source A valid signal
 *
 * **Memory Access Pattern:**
 * - Row 0-3: First 4-row transfer
 * - Row 4-7: Second 4-row transfer
 * - Row 8-11: Third 4-row transfer
 * - Row 12-15: Fourth 4-row transfer
 *
 * @param addrmod Address modulation mode for transfer operation
 */
inline void move_d2a_fixed_face(const uint8_t addrmod)
{
    TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::SRCA_VLD); // MOVD2A for a whole face assumes unpacker will set a dummy data_valid, so we want to wait on that
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0, addrmod, p_movd2a::MOV_4_ROWS, 0);   // Move rows 0-3
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4, addrmod, p_movd2a::MOV_4_ROWS, 4);   // Move rows 4-7
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8, addrmod, p_movd2a::MOV_4_ROWS, 8);   // Move rows 8-11
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12, addrmod, p_movd2a::MOV_4_ROWS, 12); // Move rows 12-15
}

/**
 * @brief Move complete face from destination to source B registers
 *
 * @details Transfers a complete 16x16 face worth of data from destination
 * registers to source B registers using fixed addressing. Similar to
 * move_d2a_fixed_face but targets source B register bank.
 *
 * **Transfer Characteristics:**
 * - Source: Destination register file
 * - Target: Source B register bank
 * - Size: Complete face (16 rows × 16 elements)
 * - Method: Four 4-row transfer operations
 * - Synchronization: Waits for source B valid signal
 *
 * @param addrmod Address modulation mode for transfer operation
 */
inline void move_d2b_fixed_face(const uint8_t addrmod)
{
    TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::SRCB_VLD); // MOVD2B for a whole face assumes unpacker will set a dummy data_valid, so we want to wait on that
    TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 0, addrmod, p_movd2b::MOV_4_ROWS, 0);   // Move rows 0-3
    TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 4, addrmod, p_movd2b::MOV_4_ROWS, 4);   // Move rows 4-7
    TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 8, addrmod, p_movd2b::MOV_4_ROWS, 8);   // Move rows 8-11
    TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 12, addrmod, p_movd2b::MOV_4_ROWS, 12); // Move rows 12-15
}

/**
 * @brief Broadcast single row to complete face in source A registers
 *
 * @details Performs row broadcasting by copying a single row of data to all
 * 16 rows of a face in source A registers. Useful for operations requiring
 * scalar-to-vector broadcasting or row-wise uniform operations.
 *
 * **Broadcasting Pattern:**
 * - Source: Single row (row 0) from destination
 * - Target: All 16 rows of source A face
 * - Result: Uniform data across all rows of the face
 * - Applications: Scalar broadcasting, bias addition, normalization
 *
 * **Performance Note:**
 * While this creates uniform data distribution, it may have performance
 * implications compared to native broadcasting operations.
 *
 * @param addrmod Address modulation mode for transfer operation
 */
inline void move_d2a_row_broadcast_fixed_face(const uint8_t addrmod)
{
    // // Seems to make things 200 clocks slower. Really shouldn't though.
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0, addrmod, p_movd2a::MOV_1_ROW, 0);   // Broadcast row 0 to position 0
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 1, addrmod, p_movd2a::MOV_1_ROW, 0);   // Broadcast row 0 to position 1
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 2, addrmod, p_movd2a::MOV_1_ROW, 0);   // Broadcast row 0 to position 2
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 3, addrmod, p_movd2a::MOV_1_ROW, 0);   // Broadcast row 0 to position 3
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4, addrmod, p_movd2a::MOV_1_ROW, 0);   // Broadcast row 0 to position 4
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 5, addrmod, p_movd2a::MOV_1_ROW, 0);   // Broadcast row 0 to position 5
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 6, addrmod, p_movd2a::MOV_1_ROW, 0);   // Broadcast row 0 to position 6
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 7, addrmod, p_movd2a::MOV_1_ROW, 0);   // Broadcast row 0 to position 7
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8, addrmod, p_movd2a::MOV_1_ROW, 0);   // Broadcast row 0 to position 8
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 9, addrmod, p_movd2a::MOV_1_ROW, 0);   // Broadcast row 0 to position 9
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 10, addrmod, p_movd2a::MOV_1_ROW, 0);  // Broadcast row 0 to position 10
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 11, addrmod, p_movd2a::MOV_1_ROW, 0);  // Broadcast row 0 to position 11
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12, addrmod, p_movd2a::MOV_1_ROW, 0);  // Broadcast row 0 to position 12
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 13, addrmod, p_movd2a::MOV_1_ROW, 0);  // Broadcast row 0 to position 13
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 14, addrmod, p_movd2a::MOV_1_ROW, 0);  // Broadcast row 0 to position 14
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 15, addrmod, p_movd2a::MOV_1_ROW, 0);  // Broadcast row 0 to position 15
}

/**
 * @brief Move complete face from source A to destination registers
 *
 * @details Transfers data from source A registers to destination registers
 * using optimized 8-row transfer operations. Efficient for moving computed
 * results back to destination for further processing or output.
 *
 * **Transfer Characteristics:**
 * - Source: Source A register bank
 * - Target: Destination register file
 * - Size: Complete face (16 rows)
 * - Method: Two 8-row transfer operations
 * - Optimization: Larger transfer chunks for better bandwidth utilization
 *
 * @param addrmod Address modulation mode for transfer operation
 */
inline void move_a2d_fixed_face(const uint8_t addrmod)
{
    TTI_MOVA2D(0, p_mova2d::MATH_HALO_ROWS, addrmod, p_mova2d::MOV_8_ROWS, 0);  // Move first 8 rows
    TTI_MOVA2D(0, p_mova2d::MATH_HALO_ROWS, addrmod, p_mova2d::MOV_8_ROWS, 0);  // Move second 8 rows
}

//
// **Bank Validation and Synchronization**
//

/**
 * @brief Wait for source register bank to become valid
 *
 * @details Template function that waits for the specified source register
 * bank (A or B) to contain valid data before proceeding. Essential for
 * maintaining data consistency in pipelined operations.
 *
 * **Synchronization Behavior:**
 * - SrcA: Waits for source A valid signal
 * - SrcB: Waits for source B valid signal
 * - Blocks math thread until data is available
 * - Prevents reading invalid or stale data
 *
 * @tparam SrcReg Source register identifier (Srcs::SrcA or Srcs::SrcB)
 */
template <uint SrcReg>
inline void wait_bank_valid()
{
    if constexpr (SrcReg == Srcs::SrcA)
    {
        TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::SRCA_VLD);  // Wait for source A valid
    }
    else
    {
        TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::SRCB_VLD);  // Wait for source B valid
    }
}

/**
 * @brief Clear source register bank valid flag
 *
 * @details Template function that clears the valid flag for the specified
 * source register bank, indicating that the data has been consumed and
 * the bank is ready for new data.
 *
 * **Flag Management:**
 * - SrcA: Clears source A valid flag
 * - SrcB: Clears source B valid flag
 * - Signals data consumption to unpacker
 * - Enables pipeline flow control
 *
 * @tparam SrcReg Source register identifier (Srcs::SrcA or Srcs::SrcB)
 */
template <uint SrcReg>
inline void clear_bank_valid()
{
    if constexpr (SrcReg == Srcs::SrcA)
    {
        TTI_SETRWC(p_setrwc::CLR_A, 0, 0, 0, 0, p_setrwc::SET_A);  // Clear source A valid
    }
    else
    {
        TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_B);  // Clear source B valid
    }
}

//
// **Semaphore-Based Synchronization**
//

/**
 * @brief Wait for math operation semaphores before proceeding
 *
 * @details Waits for the math-pack semaphore to indicate available space
 * for writing math results. Prevents overflow of the math-to-pack pipeline
 * by ensuring pack thread has capacity for new results.
 *
 * **Synchronization Logic:**
 * - Waits while semaphore is at maximum value
 * - Indicates pack thread is ready to receive results
 * - Prevents math thread from overproducing results
 * - Essential for balanced pipeline operation
 */
inline void wait_math_semaphores()
{
    // wait while math semaphore is on max, no room to write math results
    TTI_SEMWAIT(p_stall::STALL_MATH | p_stall::STALL_SFPU, semaphore::t6_sem(semaphore::MATH_PACK), p_stall::STALL_ON_MAX);
}

/**
 * @brief Signal completion of math operation to pack thread
 *
 * @details Posts to the math-pack semaphore to indicate that new math
 * results are available for the pack thread to process. Enables the
 * pack thread to proceed with output operations.
 *
 * **Signaling Behavior:**
 * - Increments math-pack semaphore
 * - Signals data availability to pack thread
 * - Enables pack thread to proceed
 * - Maintains pipeline flow balance
 */
inline void set_math_semaphores()
{
    // Tell packer that it has something to pack
    t6_semaphore_post<p_stall::MATH | p_stall::WAIT_SFPU>(semaphore::MATH_PACK);
}

/**
 * @brief Coordinate math readiness for unpack-to-destination operations
 *
 * @details Complex synchronization function for scenarios where unpacker
 * writes directly to destination registers. Manages math thread readiness
 * and ensures proper coordination with unpacker operations.
 *
 * **Synchronization Sequence:**
 * 1. Wait for math done semaphore to reach maximum
 * 2. Post to math done semaphore
 * 3. Wait for semaphore to be consumed
 * 4. Get semaphore token to proceed
 *
 * **Use Case:**
 * - Direct unpack-to-destination data flow
 * - Math thread coordination for direct operations
 * - Complex pipeline synchronization scenarios
 */
inline void math_unpack_to_dest_math_ready()
{
    t6_semaphore_wait_on_max<p_stall::STALL_SYNC>(semaphore::MATH_DONE);
    t6_semaphore_post<p_stall::MATH | p_stall::WAIT_SFPU>(semaphore::MATH_DONE);
    while (semaphore_read(semaphore::MATH_DONE) == 0)
    {
        // Wait for semaphore to be consumed
    }
    semaphore_get(semaphore::MATH_DONE);
}

/**
 * @brief Signal tile readiness for unpack-to-destination operations
 *
 * @details Coordinates tile-level readiness for unpack-to-destination
 * operations by managing the unpack-to-destination semaphore. Ensures
 * proper tile-by-tile synchronization.
 *
 * **Synchronization Behavior:**
 * - Waits for unpack-to-destination semaphore to be zero
 * - Gets semaphore token to proceed
 * - Enables tile-level coordination
 * - Supports direct unpack-to-destination data flow
 */
inline void math_unpack_to_dest_tile_ready()
{
    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::UNPACK_TO_DEST);
    t6_semaphore_get<p_stall::MATH | p_stall::WAIT_SFPU>(semaphore::UNPACK_TO_DEST);
}

//
// **Destination Address Management**
//

/**
 * @brief Set destination write address for tile operations
 *
 * @details Template function that sets the destination write address based on
 * tile layout, shape, and operation mode. Supports different addressing modes
 * for various operational scenarios.
 *
 * **Addressing Modes:**
 * - Default layout with automatic address calculation
 * - Unpack-to-destination mode with mailbox communication
 * - Template-based optimization for different tile shapes
 * - Future support for additional layout types
 *
 * **Address Calculation:**
 * - Uses bit shifts for efficient address computation
 * - Incorporates destination buffer base offset
 * - Supports different tile shapes with appropriate scaling
 *
 * @tparam layout Destination tile layout strategy
 * @tparam tile_shape Shape of the destination tiles
 * @tparam unpack_to_dest Whether unpacker writes directly to destination
 * @param tile_index Index of the tile for address calculation
 */
template <DstTileLayout layout, DstTileShape tile_shape, bool unpack_to_dest = false>
inline void set_dst_write_addr(uint32_t tile_index)
{
    if constexpr (layout == DstTileLayout::Default)
    {
        uint dst_index = tile_index << DstTileSizeLog2[tile_shape];  // Calculate tile address using bit shift
        dst_index      = dst_index + get_dest_buffer_base();        // Add destination buffer base offset
        if constexpr (unpack_to_dest)
        {
            mailbox_write(ThreadId::UnpackThreadId, dst_index); // Send to unpacker for direct write
        }
        else
        {
            TT_SETC16(DEST_TARGET_REG_CFG_MATH_Offset_ADDR32, dst_index);  // Set math thread destination address
        }
    }
    else
    {
        // FIXME MT: add this mapping for other layout
    }
}

/**
 * @brief Clear destination register address offset
 *
 * @details Resets the destination register address to the base position
 * by clearing any accumulated offsets. Essential for establishing known
 * starting positions for destination addressing.
 */
inline void clear_dst_reg_addr()
{
    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);  // Reset destination address counter
}

/**
 * @brief Increment destination address by specified number of rows
 *
 * @details Template function that increments the destination address by
 * a specified number of rows. Enables efficient sequential access to
 * destination register rows for streaming operations.
 *
 * **Row Increment:**
 * - Template parameter for compile-time optimization
 * - Maximum 15 rows per increment (hardware limitation)
 * - Automatic address advancement for sequential operations
 * - Optimized for streaming data processing
 *
 * @tparam num_rows Number of rows to increment (max 15)
 */
template <uint num_rows = 8>
inline void inc_dst_addr()
{
    static_assert(num_rows <= 15, "num_rows must be <= 15");
    TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, num_rows, 0, 0, p_setrwc::SET_D);  // Increment destination address
}

/**
 * @brief Wait for destination register availability
 *
 * @details Waits for destination registers to be available for math operations
 * by checking the math-pack semaphore. Prevents overwriting destination
 * data that hasn't been consumed by the pack thread.
 *
 * **Synchronization:**
 * - Stalls math and SFPU operations
 * - Waits for pack thread to indicate readiness
 * - Prevents destination register conflicts
 * - Maintains pipeline data integrity
 */
inline void math_dest_wait()
{
    TTI_SEMWAIT(p_stall::STALL_MATH | p_stall::STALL_SFPU | p_stall::STALL_SYNC, semaphore::t6_sem(semaphore::MATH_PACK), p_stall::STALL_ON_MAX);
}

/**
 * @brief Flip destination section for double-buffering
 *
 * @details Switches between destination register sections for double-buffering
 * operations. Updates the destination offset and configures the new base
 * address for continuous operation.
 *
 * **Double-Buffering:**
 * - Alternates between destination register sections
 * - Enables continuous operation while previous results are packed
 * - Updates destination base address configuration
 * - Waits for configuration to complete before proceeding
 */
inline void dest_section_flip()
{
    update_dest_offset_id();                                          // Update destination offset identifier
    uint base_addr = get_dest_buffer_base();                         // Get new destination buffer base
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::MATH | p_stall::SFPU1); // Wait for configuration completion
    TT_SETC16(DEST_TARGET_REG_CFG_MATH_Offset_ADDR32, base_addr);     // Set new destination base address
}

/**
 * @brief Set destination section base address
 *
 * @details Template function that sets the destination section base address
 * to either the beginning or middle of the destination register space.
 * Used for establishing initial positioning in double-buffering scenarios.
 *
 * **Section Selection:**
 * - StartZero: Begin at register offset 0
 * - StartHalf: Begin at register offset DEST_REGISTER_HALF_SIZE
 * - Template-based compile-time optimization
 * - Supports double-buffering strategies
 *
 * @tparam Dst Starting position (DstStart::StartZero or DstStart::StartHalf)
 */
template <DstStart Dst>
inline void set_dest_section_base()
{
    uint base_addr;
    if constexpr (Dst == DstStart::StartZero)
    {
        base_addr = 0;                            // Start at beginning of destination space
    }
    else
    {
        base_addr = DEST_REGISTER_HALF_SIZE;      // Start at middle of destination space
    }
    TT_SETC16(DEST_TARGET_REG_CFG_MATH_Offset_ADDR32, base_addr);  // Configure destination base address
}

//
// **Format and Precision Utilities**
//

/**
 * @brief Check if operation uses 32-bit input/output formats
 *
 * @details Compile-time function that determines whether both source and
 * destination formats are 32-bit. Used for optimization decisions and
 * format-specific code paths.
 *
 * **32-bit Format Detection:**
 * - Checks both input and output formats
 * - Supports Int32 and Float32 formats
 * - Returns true only if both are 32-bit
 * - Used for format-specific optimizations
 *
 * @param src_format Source data format identifier
 * @param dst_format Destination data format identifier
 * @return true if both formats are 32-bit, false otherwise
 */
inline constexpr bool is_32bit_input(const std::uint32_t src_format, const std::uint32_t dst_format)
{
    const uint input_df  = src_format & 0xF;   // Extract input format bits
    const uint output_df = dst_format & 0xF;   // Extract output format bits
    return ((input_df == (uint)DataFormat::Int32) || (input_df == (uint)DataFormat::Float32)) &&
           ((output_df == (uint)DataFormat::Int32) || (output_df == (uint)DataFormat::Float32));
}

/**
 * @brief Extract number of math fidelity phases from fidelity descriptor
 *
 * @details Extracts the number of computational phases from a math fidelity
 * descriptor. Used for controlling the precision vs. performance trade-off
 * in mathematical operations.
 *
 * **Fidelity Control:**
 * - Lower 3 bits encode number of phases
 * - More phases = higher precision
 * - Fewer phases = faster computation
 * - Range: 0-7 phases
 *
 * @param math_fidelity_desc Math fidelity descriptor value
 * @return Number of fidelity phases (0-7)
 */
inline constexpr int get_math_num_fidelity_phases(const int math_fidelity_desc)
{
    return (math_fidelity_desc & 0x7);  // Extract lower 3 bits
}

/**
 * @brief Extract math fidelity increment from fidelity descriptor
 *
 * @details Extracts the increment value for math fidelity processing.
 * Controls the step size between fidelity phases for gradual precision
 * improvement.
 *
 * **Increment Control:**
 * - Bit 3 encodes increment flag
 * - Returns 1 or 2 based on flag value
 * - Controls precision stepping
 * - Affects convergence rate
 *
 * @param math_fidelity_desc Math fidelity descriptor value
 * @return Fidelity increment value (1 or 2)
 */
inline constexpr int get_math_fidelity_increment(const int math_fidelity_desc)
{
    return ((math_fidelity_desc >> 3) & 0x1) + 1;  // Extract bit 3 and add 1
}

//
// **Destination Buffer Utilities**
//

/**
 * @brief Get destination buffer base address for 16-bit mode
 *
 * @details Returns the destination buffer base address scaled for 16-bit
 * destination mode. In 16-bit mode, half the destination can store 32 faces,
 * requiring division by 16 for proper address scaling.
 *
 * **16-bit Address Scaling:**
 * - Half destination = 32 faces in 16-bit mode
 * - Base address divided by 16 for face addressing
 * - Optimized for 16-bit data type operations
 * - Maintains proper address alignment
 *
 * @return Destination base address in face units for 16-bit mode
 */
inline std::uint32_t get_dest_buffer_base_16b()
{
    return (get_dest_buffer_base() >> 4);  // Divide by 16 for 16-bit face addressing
}

/**
 * @brief Get destination buffer base address for 32-bit mode
 *
 * @details Returns the destination buffer base address scaled for 32-bit
 * destination mode. In 32-bit mode, half the destination can store 16 faces,
 * requiring division by 32 for proper address scaling.
 *
 * **32-bit Address Scaling:**
 * - Half destination = 16 faces in 32-bit mode
 * - Base address divided by 32 for face addressing
 * - Optimized for 32-bit data type operations
 * - Maintains proper address alignment
 *
 * @return Destination base address in face units for 32-bit mode
 */
inline std::uint32_t get_dest_buffer_base_32b()
{
    return (get_dest_buffer_base() >> 5);  // Divide by 32 for 32-bit face addressing
}

/**
 * @brief Calculate destination index in faces for tile/face addressing
 *
 * @details Converts tile index and face index into destination row offset
 * for precise face-level addressing within tiles. Essential for operations
 * that need sub-tile precision.
 *
 * **Address Calculation:**
 * - Tile index shifted left by 2 (4 faces per tile)
 * - Face index added for specific face within tile
 * - Supports accessing faces beyond current tile
 * - Enables precise face-level operations
 *
 * **Face Indexing:**
 * - Normal range: 0-3 (faces within current tile)
 * - Extended range: >3 (faces from subsequent tiles)
 * - Flexible addressing for complex operations
 *
 * @param dst_index Destination tile index
 * @param face_index Face index within (or beyond) the tile (0-3 normal, >3 extended)
 * @return Destination offset in face units
 */
inline std::uint32_t get_dest_index_in_faces(const std::uint32_t dst_index, const std::uint32_t face_index)
{
    // dst_index << 2 gives a tile index in faces, because there are 4 faces in a tile.
    // face_index should normally take values from {0, 1, 2, 3}, although if it's greater
    // than 3 faces from next tiles can be accessed.
    return (dst_index << 2) + face_index;  // Convert tile index to face index and add face offset
}

} // namespace ckernel::math
