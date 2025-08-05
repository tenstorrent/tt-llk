// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file cmath_common.h
 * @brief Math Operation Infrastructure for Wormhole B0 Tensix
 *
 * This header provides the complete infrastructure for mathematical operations
 * on Wormhole B0 Tensix cores. It contains utility functions, constants, and
 * management routines for the math pipeline, including data movement,
 * synchronization, and format handling.
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # Key Components
 *
 * - **Tile Size Management**: Constants and utilities for different tile shapes
 * - **Data Movement**: Functions for moving data between register banks
 * - **Synchronization**: Semaphore and validity management for pipeline coordination
 * - **Counter Management**: Read/Write/Clear counter control for addressing
 * - **Format Support**: Format detection and math fidelity configuration
 * - **Destination Control**: Management of destination register sections
 *
 * # Math Pipeline Architecture
 *
 * The math pipeline processes data through these stages:
 *
 * 1. **Source Preparation**: Wait for valid data in source banks (A/B)
 * 2. **Math Execution**: Perform mathematical operations (FPU/SFPU)
 * 3. **Destination Management**: Control where results are written
 * 4. **Synchronization**: Coordinate with pack pipeline via semaphores
 * 5. **Counter Updates**: Manage addressing for next operations
 *
 * # Tile Shape Support
 *
 * Supports three tile shapes with optimized constants:
 * - **32x32**: 64 face elements (standard)
 * - **32x16/16x32**: 32 face elements (rectangular)
 * - **16x16**: 16 face elements (compact)
 *
 * # Usage Example
 *
 * ```cpp
 * #include "cmath_common.h"
 *
 * // Wait for source data and prepare math operation
 * wait_bank_valid<Srcs::SrcA>();
 * wait_bank_valid<Srcs::SrcB>();
 *
 * // Execute math operation (implemented by higher-level functions)
 * perform_math_operation();
 *
 * // Clean up and signal completion
 * clear_bank_valid<Srcs::SrcA>();
 * set_math_semaphores();
 * ```
 *
 * @warning Math operations directly control hardware pipelines. Incorrect
 *          synchronization can cause data corruption or hardware lockups.
 *
 * @note This header contains low-level math infrastructure. Use high-level
 *       LLK math APIs when possible for better abstraction and safety.
 *
 * @see llk_math_common.h for high-level math operations
 * @see ckernel.h for core infrastructure
 * @see ckernel_sfpu.h for SFPU-specific operations
 */

#pragma once

#include <cstdint>

// #include "kernel_types.h"
#include "ckernel.h"
#include "ckernel_globals.h"
#include "ckernel_sfpu.h"
#include "ckernel_template.h"
#include "llk_defs.h"

#ifndef SFPU_OP_PARAM
#define SFPU_OP_PARAM 0 ///< Default SFPU operation parameter
#endif

#ifndef FUSE_SQRT_RECIP
#define FUSE_SQRT_RECIP 0 ///< Default fused sqrt-reciprocal setting
#endif

using namespace ckernel;

/**
 * @namespace ckernel::math
 * @brief Math operation infrastructure and utilities
 *
 * Contains all math-related functionality including data movement,
 * synchronization primitives, and utility functions for mathematical
 * operations on Tensix hardware.
 */
namespace ckernel::math
{

/**
 * @defgroup MathConstants Math Operation Constants
 * @brief Core constants for math pipeline configuration
 * @{
 */

/**
 * @brief Destination tile sizes for different tile shapes
 *
 * Array indexed by DstTileShape enum values. Each entry contains the
 * number of 16-element "faces" in the corresponding tile shape.
 * Used for loop bounds and memory calculations.
 */
constexpr uint DstTileSize[3] = {
    64, ///< 32x32 tile shape (4 faces × 16 elements = 64)
    32, ///< 32x16 or 16x32 tile shape (2 faces × 16 elements = 32)
    16  ///< 16x16 tile shape (1 face × 16 elements = 16)
};

/**
 * @brief Log2 of destination tile sizes for efficient bit operations
 *
 * Companion to DstTileSize array providing log2 values for efficient
 * multiplication/division operations using bit shifts.
 */
constexpr uint DstTileSizeLog2[3] = {
    6, ///< log2(64) for 32x32 tile shape
    5, ///< log2(32) for 32x16/16x32 tile shape
    4  ///< log2(16) for 16x16 tile shape
};

/**
 * @brief Replay buffer offset for FPU operations
 *
 * The replay buffer is partitioned between SFPU and FPU operations:
 * - Slots 0-15: SFPU operations
 * - Slots 16-31: FPU operations (starting at this offset)
 */
constexpr uint replay_buf_offset = 16;

/** @} */ // end of MathConstants group

/**
 * @defgroup CounterManagement Counter Management Functions
 * @brief Functions for managing read/write/clear counters in math pipeline
 * @{
 */

/**
 * @brief Reset address counters for math operations
 *
 * Resets the specified address counters to their initial values without
 * clearing validity flags. Used to restart addressing sequences for
 * repeated operations.
 *
 * @param setrwc Counter selection mask (see p_setrwc constants)
 *
 * @note This only resets counters, not validity flags
 */
inline void reset_counters(const uint setrwc)
{
    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, setrwc);
}

/**
 * @brief Increment address counters for math operations
 *
 * Increments the specified address counters by the given amounts.
 * Used to advance addressing for the next set of operations.
 *
 * @param incr_a Increment for source A counter
 * @param incr_b Increment for source B counter
 * @param incr_d Increment for destination counter
 * @param incr_cr Increment for carriage return counter
 */
inline void incr_counters(const uint incr_a, const uint incr_b, const uint incr_d, const uint incr_cr)
{
    TT_INCRWC(incr_cr, incr_d, incr_b, incr_a);
}

/** @} */ // end of CounterManagement group

/**
 * @defgroup DataMovement Data Movement Functions
 * @brief Functions for moving data between register banks in math pipeline
 * @{
 */

/**
 * @brief Move destination data to source A for one face (fixed pattern)
 *
 * Moves a complete 16x16 face from destination registers to source A
 * in 4-row chunks. Used for operations that need to feed previous
 * results back as source operands.
 *
 * @param addrmod Address mode for the move operation
 *
 * @note Waits for source A validity signal before proceeding
 * @note Moves 4 rows at a time for optimal performance
 */
inline void move_d2a_fixed_face(const uint8_t addrmod)
{
    TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::SRCA_VLD); // MOVD2A for a whole face assumes unpacker will set a dummy data_valid, so we want to wait on that
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0, addrmod, p_movd2a::MOV_4_ROWS, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4, addrmod, p_movd2a::MOV_4_ROWS, 4);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8, addrmod, p_movd2a::MOV_4_ROWS, 8);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12, addrmod, p_movd2a::MOV_4_ROWS, 12);
}

inline void move_d2b_fixed_face(const uint8_t addrmod)
{
    TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::SRCB_VLD); // MOVD2B for a whole face assumes unpacker will set a dummy data_valid, so we want to wait on that
    TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 0, addrmod, p_movd2b::MOV_4_ROWS, 0);
    TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 4, addrmod, p_movd2b::MOV_4_ROWS, 4);
    TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 8, addrmod, p_movd2b::MOV_4_ROWS, 8);
    TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 12, addrmod, p_movd2b::MOV_4_ROWS, 12);
}

inline void move_d2a_row_broadcast_fixed_face(const uint8_t addrmod)
{
    // // Seems to make things 200 clocks slower. Really shouldn't though.
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 1, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 2, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 3, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 5, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 6, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 7, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 9, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 10, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 11, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 13, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 14, addrmod, p_movd2a::MOV_1_ROW, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 15, addrmod, p_movd2a::MOV_1_ROW, 0);
}

inline void move_a2d_fixed_face(const uint8_t addrmod)
{
    TTI_MOVA2D(0, p_mova2d::MATH_HALO_ROWS, addrmod, p_mova2d::MOV_8_ROWS, 0);
    TTI_MOVA2D(0, p_mova2d::MATH_HALO_ROWS, addrmod, p_mova2d::MOV_8_ROWS, 0);
}

/**
 * @brief Move destination data to source B for one face (fixed pattern)
 *
 * Similar to move_d2a_fixed_face but targets source B registers.
 * Used for operations that need destination feedback to source B.
 *
 * @param addrmod Address mode for the move operation
 */
// [Implementation for move_d2b_fixed_face follows...]

/**
 * @brief Move destination data to source A with row broadcast
 *
 * Broadcasts the first row of destination data to all 16 rows of
 * source A. Used for operations requiring row-wise broadcasting.
 *
 * @param addrmod Address mode for the move operation
 */
// [Implementation for move_d2a_row_broadcast_fixed_face follows...]

/**
 * @brief Move source A data to destination (fixed pattern)
 *
 * Moves data from source A to destination in 8-row chunks.
 * Used for operations that store intermediate results.
 *
 * @param addrmod Address mode for the move operation
 */
// [Implementation for move_a2d_fixed_face follows...]

/** @} */ // end of DataMovement group

/**
 * @defgroup BankValidityControl Bank Validity Control Functions
 * @brief Template functions for managing source bank validity
 * @{
 */

/**
 * @brief Wait for valid data in specified source bank
 *
 * Template function that waits for the specified source register bank
 * to contain valid data before proceeding. Essential for pipeline
 * synchronization.
 *
 * @tparam SrcReg Source register selection (Srcs::SrcA or Srcs::SrcB)
 *
 * @note This is a blocking operation that stalls until data is valid
 */
template <uint SrcReg>
inline void wait_bank_valid()
{
    if constexpr (SrcReg == Srcs::SrcA)
    {
        TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::SRCA_VLD);
    }
    else
    {
        TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::SRCB_VLD);
    }
}

/**
 * @brief Clear validity flag for specified source bank
 *
 * Template function that clears the validity flag for the specified
 * source register bank after consumption. Used to signal that the
 * data has been processed.
 *
 * @tparam SrcReg Source register selection (Srcs::SrcA or Srcs::SrcB)
 *
 * @note Must be called after consuming data to maintain pipeline flow
 */
template <uint SrcReg>
inline void clear_bank_valid()
{
    if constexpr (SrcReg == Srcs::SrcA)
    {
        TTI_SETRWC(p_setrwc::CLR_A, 0, 0, 0, 0, p_setrwc::SET_A);
    }
    else
    {
        TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_B);
    }
}

/** @} */ // end of BankValidityControl group

/**
 * @defgroup SemaphoreManagement Math Pipeline Semaphore Management
 * @brief Functions for coordinating math and pack pipeline operations
 * @{
 */

/**
 * @brief Wait for space in math-to-pack pipeline
 *
 * Waits until the math-pack semaphore is not at maximum, indicating
 * there is space to write new math results. This prevents math
 * operations from overrunning the pack pipeline.
 *
 * @note Blocks math and SFPU threads until space is available
 */
inline void wait_math_semaphores()
{
    // wait while math semaphore is on max, no room to write math results
    TTI_SEMWAIT(p_stall::STALL_MATH | p_stall::STALL_SFPU, semaphore::t6_sem(semaphore::MATH_PACK), p_stall::STALL_ON_MAX);
}

/**
 * @brief Signal math operation completion to pack pipeline
 *
 * Posts to the math-pack semaphore to signal that new math results
 * are available for packing. This coordinates the handoff between
 * math and pack stages of the pipeline.
 *
 * @note Called after math operations complete to maintain pipeline flow
 */
inline void set_math_semaphores()
{
    // Tell packer that it has something to pack
    t6_semaphore_post<p_stall::MATH | p_stall::WAIT_SFPU>(semaphore::MATH_PACK);
}

inline void math_unpack_to_dest_math_ready()
{
    t6_semaphore_wait_on_max<p_stall::STALL_SYNC>(semaphore::MATH_DONE);
    t6_semaphore_post<p_stall::MATH | p_stall::WAIT_SFPU>(semaphore::MATH_DONE);
    while (semaphore_read(semaphore::MATH_DONE) == 0)
    {
    }
    semaphore_get(semaphore::MATH_DONE);
}

inline void math_unpack_to_dest_tile_ready()
{
    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::UNPACK_TO_DEST);
    t6_semaphore_get<p_stall::MATH | p_stall::WAIT_SFPU>(semaphore::UNPACK_TO_DEST);
}

template <DstTileLayout layout, DstTileShape tile_shape, bool unpack_to_dest = false>
inline void set_dst_write_addr(uint32_t tile_index)
{
    if constexpr (layout == DstTileLayout::Default)
    {
        uint dst_index = tile_index << DstTileSizeLog2[tile_shape];
        dst_index      = dst_index + get_dest_buffer_base();
        if constexpr (unpack_to_dest)
        {
            mailbox_write(ThreadId::UnpackThreadId, dst_index); // Send to unpacker
        }
        else
        {
            TT_SETC16(DEST_TARGET_REG_CFG_MATH_Offset_ADDR32, dst_index);
        }
    }
    else
    {
        // FIXME MT: add this mapping for other layout
    }
}

// Programming a dst write addr offset that gets added to base
//
inline void clear_dst_reg_addr()
{
    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);
}

inline void set_addr_mod_base()
{
    TTI_SETC16(ADDR_MOD_SET_Base_ADDR32, 1); // set addr mod base (use addr mods 4..7)
}

inline void clear_addr_mod_base()
{
    TTI_SETC16(ADDR_MOD_SET_Base_ADDR32, 0); // clear addr mod base (use addr mods 0..3)
}

template <uint num_rows = 8>
inline void inc_dst_addr()
{
    static_assert(num_rows <= 15, "num_rows must be <= 15");
    TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, num_rows, 0, 0, p_setrwc::SET_D);
}

inline void math_dest_wait()
{
    TTI_SEMWAIT(p_stall::STALL_MATH | p_stall::STALL_SFPU | p_stall::STALL_SYNC, semaphore::t6_sem(semaphore::MATH_PACK), p_stall::STALL_ON_MAX);
}

inline void dest_section_flip()
{
    update_dest_offset_id();
    uint base_addr = get_dest_buffer_base();
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::MATH | p_stall::SFPU1);
    TT_SETC16(DEST_TARGET_REG_CFG_MATH_Offset_ADDR32, base_addr);
}

template <DstStart Dst>
inline void set_dest_section_base()
{
    uint base_addr;
    if constexpr (Dst == DstStart::StartZero)
    {
        base_addr = 0;
    }
    else
    {
        base_addr = DEST_REGISTER_HALF_SIZE;
    }
    TT_SETC16(DEST_TARGET_REG_CFG_MATH_Offset_ADDR32, base_addr);
}

/** @} */ // end of SemaphoreManagement group

/**
 * @defgroup UtilityFunctions Math Utility Functions
 * @brief Helper functions for format detection and math configuration
 * @{
 */

/**
 * @brief Check if operation uses 32-bit input/output formats
 *
 * Determines whether both source and destination formats are 32-bit
 * (Float32 or Int32). Used to optimize math operations for 32-bit
 * data paths.
 *
 * @param src_format Source data format
 * @param dst_format Destination data format
 * @return true if both formats are 32-bit, false otherwise
 */
inline constexpr bool is_32bit_input(const std::uint32_t src_format, const std::uint32_t dst_format)
{
    const uint input_df  = src_format & 0xF;
    const uint output_df = dst_format & 0xF;

    return ((input_df == (uint)DataFormat::Int32) || (input_df == (uint)DataFormat::Float32)) &&
           ((output_df == (uint)DataFormat::Int32) || (output_df == (uint)DataFormat::Float32));
}

/**
 * @brief Extract number of fidelity phases from math fidelity descriptor
 *
 * Math operations can be performed in multiple phases for improved
 * accuracy. This function extracts the phase count from the fidelity
 * configuration descriptor.
 *
 * @param math_fidelity_desc Math fidelity configuration descriptor
 * @return Number of fidelity phases (0-7)
 */
inline constexpr int get_math_num_fidelity_phases(const int math_fidelity_desc)
{
    return (math_fidelity_desc & 0x7);
}

/**
 * @brief Extract fidelity increment from math fidelity descriptor
 *
 * Determines the increment value used between fidelity phases.
 * This controls how the fidelity phases are stepped through.
 *
 * @param math_fidelity_desc Math fidelity configuration descriptor
 * @return Fidelity increment value (1 or 2)
 */
inline constexpr int get_math_fidelity_increment(const int math_fidelity_desc)
{
    return ((math_fidelity_desc >> 3) & 0x1) + 1;
}

/** @} */ // end of UtilityFunctions group

} // namespace ckernel::math
