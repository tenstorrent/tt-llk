// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file cmath_common.h
 * @brief Thread 1 (Math) execution utilities for Wormhole B0 Tensix compute operations
 *
 * @details This header provides fundamental mathematical operations, data movement utilities,
 * and computation management functions for Thread 1 (Math) in the Wormhole B0 Tensix engine.
 * Thread 1 is the central compute thread that consumes data from SRCA/SRCB registers,
 * performs high-throughput tensor operations using 2048 hardware multipliers, and produces
 * results in DEST registers for consumption by Thread 2 (Pack).
 *
 * **Wormhole B0 Math Thread Architecture:**
 * Thread 1 operates at the heart of the Tensix compute pipeline:
 * - **Input Sources**: SRCA (64×16 datums) and SRCB (64×16 datums) register files
 * - **Compute Engine**: 2048 hardware multipliers (5×7 bit) with fidelity phase control
 * - **Output Destination**: DEST register file (1024×16 or 512×16 datums)
 * - **SFPU Integration**: 8 SFPU instances × 4 lanes = 32-lane SIMD special functions
 * - **Thread Coordination**: Hardware sync with Thread 0 (Unpack) and Thread 2 (Pack)
 *
 * **Mathematical Operations Framework:**
 * - **Matrix Multiplication**: Optimized for AI workloads with high-throughput multiply-accumulate
 * - **Elementwise Operations**: SIMD operations across register file data
 * - **Fidelity Control**: 4 fidelity phases for precision vs. performance optimization
 * - **Data Format Support**: FP32, FP16A/B, BF16, TF32, INT8/16/32, BFP2/4/8
 * - **Special Functions**: Complex mathematical functions via integrated SFPU
 *
 * **Register File Management:**
 * - **SRCA/SRCB Access**: 64×16 datum register files with 19-bit datum containers
 * - **DEST Management**: Dual-mode operation (16-bit or 32-bit datums) with double buffering
 * - **Register Window Addressing**: Hardware register window counters (RWC) for efficient access
 * - **Hardware Synchronization**: Embedded sync with unpacker data flow
 *
 * **Performance Optimization Features:**
 * - **Pipeline Efficiency**: Overlapped execution with Unpack and Pack threads
 * - **Double Buffering**: DEST register dual-bank design enables concurrent math/pack
 * - **Fidelity Phases**: Configurable precision for optimal performance/accuracy balance
 * - **SFPU Acceleration**: Hardware-accelerated special functions for complex operations
 * - **MOPs Integration**: Hardware macro-operations for instruction sequence optimization
 *
 * **Thread Synchronization:**
 * - **Semaphore Coordination**: Hardware semaphores for resource management with Pack thread
 * - **DEST Register Sync**: Coordination between math computation and pack operations
 * - **Pipeline Flow**: Maintains balanced throughput through Unpack→Math→Pack pipeline
 *
 * **Integration with Compute FPU:**
 * Thread 1 directly controls the Wormhole B0 Compute FPU components:
 * - **Math Engine**: 2048 multipliers organized for optimal matrix operations
 * - **SFPU Control**: Direct access to special function processing units
 * - **Configuration Management**: Runtime control of compute modes and precision
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
#define SFPU_OP_PARAM 0
#endif

#ifndef FUSE_SQRT_RECIP
#define FUSE_SQRT_RECIP 0
#endif

using namespace ckernel;

namespace ckernel::math
{

/**
 * @brief DEST register tile size mapping for different tensor dimensions
 * @details Maps tile shape indices to the number of register entries required
 * in the DEST register file. Wormhole B0 supports flexible tile shapes to
 * optimize memory usage and computational patterns for different AI workloads.
 * 
 * **Wormhole B0 Tile Shape Support:**
 * - Index 0: 32×32 tiles (64 register entries) - Large matrix operations
 * - Index 1: 32×16 or 16×32 tiles (32 entries) - Rectangular tensor operations  
 * - Index 2: 16×16 tiles (16 entries) - Small tensor operations, memory efficiency
 */
constexpr uint DstTileSize[3] = {
    64, // 32x32 tile shape
    32, // 32x16, 16x32 tile shape
    16  // 16x16 tile shape
};

/**
 * @brief Log2 of DEST register tile sizes for efficient address calculations
 * @details Provides log2 values for tile sizes, enabling efficient bit-shift
 * address calculations and memory stride computations in the Wormhole B0
 * address generation hardware without requiring expensive division operations.
 */
constexpr uint DstTileSizeLog2[3] = {
    6, // 32x32 tile shape (log2(64) = 6)
    5, // 32x16, 16x32 tile shape (log2(32) = 5)
    4  // 16x16 tile shape (log2(16) = 4)
};

/**
 * @brief REPLAY buffer partitioning offset between FPU and SFPU usage
 * @details Defines the split point in the 32-instruction REPLAY buffer between
 * SFPU (instructions 0-15) and FPU (instructions 16-31) usage. This partitioning
 * enables concurrent SFPU and FPU instruction sequences for optimal pipeline
 * utilization in Thread 1 (Math) operations.
 * 
 * **Wormhole B0 REPLAY Buffer Architecture:**
 * - **Total Capacity**: 32 instructions maximum
 * - **SFPU Allocation**: Instructions 0-15 (first 16)
 * - **FPU Allocation**: Instructions 16-31 (next 16)
 * - **Concurrent Execution**: Enables overlapped FPU and SFPU operations
 */
constexpr uint replay_buf_offset = 16;

/**
 * @brief Reset register window counters for SRCA, SRCB, and DEST register files
 * @param setrwc Register window counter reset configuration mask
 * 
 * @details Resets the register window counters (RWC) that control addressing
 * into the SRCA, SRCB, and DEST register files. This function initializes
 * the addressing state for fresh computation sequences in Thread 1 (Math).
 * 
 * **Wormhole B0 Register Window Counter System:**
 * - **SRCA RWC**: Controls addressing into 64×16 SRCA register file
 * - **SRCB RWC**: Controls addressing into 64×16 SRCB register file  
 * - **DEST RWC**: Controls addressing into 1024×16 or 512×16 DEST register file
 * - **Carriage Return**: Each counter includes CR functionality for reuse patterns
 * - **Hardware Integration**: Counters integrate with address generation logic
 */
inline void reset_counters(const uint setrwc)
{
    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, setrwc);
}

/**
 * @brief Increment register window counters for coordinated register access
 * @param incr_a SRCA register window counter increment value
 * @param incr_b SRCB register window counter increment value 
 * @param incr_d DEST register window counter increment value
 * @param incr_cr Carriage return counter increment value
 * 
 * @details Increments the register window counters for SRCA, SRCB, and DEST
 * register files, enabling coordinated advancement through tensor data during
 * mathematical operations. The carriage return functionality supports matrix
 * multiplication and convolution access patterns.
 * 
 * **Wormhole B0 Address Pattern Optimization:**
 * - **Coordinated Increment**: Maintains alignment between operand and result registers
 * - **Matrix Operations**: CR functionality enables efficient operand reuse
 * - **Convolution Support**: Address patterns optimized for sliding window operations
 * - **Pipeline Efficiency**: Counter updates don't stall 2048-multiplier computation
 */
inline void incr_counters(const uint incr_a, const uint incr_b, const uint incr_d, const uint incr_cr)
{
    TT_INCRWC(incr_cr, incr_d, incr_b, incr_a);
}

/**
 * @brief Move complete data face from DEST to SRCA register file
 * @param addrmod Address modification mode for register window counter updates
 * 
 * @details Transfers a complete 16×16 data face from DEST registers to SRCA
 * register file in 4-row chunks. This function is essential for recirculation
 * patterns where computed results need to become input operands for subsequent
 * operations, common in iterative algorithms and accumulation patterns.
 * 
 * **Wormhole B0 Data Recirculation Architecture:**
 * - **DEST→SRCA Path**: Direct data path for result recirculation
 * - **Chunk Transfer**: 4-row transfers optimize register file bandwidth
 * - **Synchronization**: Waits for SRCA validity to ensure data consistency
 * - **Halo Row Support**: Includes mathematical halo row addressing for convolution
 * - **Address Modification**: Automatic counter updates based on addressing mode
 * 
 * **Pipeline Integration:**
 * - **Thread 1 Control**: Executed by Math thread for operand preparation
 * - **Register Reuse**: Enables efficient accumulation and iterative algorithms
 * - **Memory Efficiency**: Avoids L1 memory round-trips for recirculated data
 */
inline void move_d2a_fixed_face(const uint8_t addrmod)
{
    TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::SRCA_VLD); // MOVD2A for a whole face assumes unpacker will set a dummy data_valid, so we want to wait on that
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0, addrmod, p_movd2a::MOV_4_ROWS, 0);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4, addrmod, p_movd2a::MOV_4_ROWS, 4);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8, addrmod, p_movd2a::MOV_4_ROWS, 8);
    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12, addrmod, p_movd2a::MOV_4_ROWS, 12);
}

/**
 * @brief Move complete data face from DEST to SRCB register file  
 * @param addrmod Address modification mode for register window counter updates
 * 
 * @details Transfers a complete 16×16 data face from DEST registers to SRCB
 * register file in 4-row chunks. This enables computed results to become
 * the second operand (B) for subsequent mathematical operations, supporting
 * complex algorithmic patterns requiring result feedback.
 * 
 * **Wormhole B0 Operand B Recirculation:**
 * - **DEST→SRCB Path**: Direct data path for result-to-operand-B flow
 * - **4-Row Optimization**: Transfers aligned with register file architecture
 * - **Synchronization**: SRCB validity ensures proper data flow coordination
 * - **Zero Offset Addressing**: Standard SRCB addressing without halo rows
 * - **Address Management**: Automatic counter updates for sequential access
 * 
 * **Mathematical Operation Support:**
 * - **Iterative Algorithms**: Enables feedback loops and accumulation patterns
 * - **Matrix Operations**: Supports complex matrix algorithmic sequences
 * - **2048 Multiplier Utilization**: Maintains high multiplier utilization efficiency
 * - **Pipeline Flow**: Integrates with Unpack→Math→Pack data flow
 */
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

inline void wait_math_semaphores()
{
    // wait while math semaphore is on max, no room to write math results
    TTI_SEMWAIT(p_stall::STALL_MATH | p_stall::STALL_SFPU, semaphore::t6_sem(semaphore::MATH_PACK), p_stall::STALL_ON_MAX);
}

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

inline constexpr bool is_32bit_input(const std::uint32_t src_format, const std::uint32_t dst_format)
{
    const uint input_df  = src_format & 0xF;
    const uint output_df = dst_format & 0xF;

    return ((input_df == (uint)DataFormat::Int32) || (input_df == (uint)DataFormat::Float32)) &&
           ((output_df == (uint)DataFormat::Int32) || (output_df == (uint)DataFormat::Float32));
}

inline constexpr int get_math_num_fidelity_phases(const int math_fidelity_desc)
{
    return (math_fidelity_desc & 0x7);
}

inline constexpr int get_math_fidelity_increment(const int math_fidelity_desc)
{
    return ((math_fidelity_desc >> 3) & 0x1) + 1;
}

} // namespace ckernel::math
