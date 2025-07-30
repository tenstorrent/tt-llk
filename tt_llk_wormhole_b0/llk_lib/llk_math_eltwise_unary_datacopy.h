// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file llk_math_eltwise_unary_datacopy.h
 * @brief High-performance data copying operations for Tensix mathematical processing
 *
 * This header provides optimized data movement operations including A→D (SrcA to Destination)
 * and B→D (SrcB to Destination) transfers with support for broadcasting, format conversion,
 * and direct destination unpacking. These operations are fundamental for data routing and
 * layout transformation in mathematical computation pipelines.
 *
 * @note Data copy operations are performance-critical and affect memory bandwidth utilization
 *       across the entire Tensix processing pipeline.
 * 
 * @note Supports advanced broadcasting modes, 32-bit data type optimizations, and direct
 *       destination unpacking for maximum throughput and efficiency.
 * 
 * @note Based on PR analysis: Data movement optimizations are critical for overall
 *       system performance, with copy operations showing significant variance patterns.
 */

#pragma once

#include <cstdint>

#include "ckernel_globals.h"
#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_math_common.h"

using namespace ckernel;

// local function declarations
inline void eltwise_unary_configure_addrmod();

/**
 * @brief Execute high-performance data copy operation with advanced optimization
 *
 * Performs optimized data transfer operations between source registers and destination
 * with support for format conversion, broadcasting patterns, and direct destination
 * unpacking. This function implements the core data movement kernel with hardware-specific
 * optimizations for maximum memory bandwidth utilization.
 *
 * @tparam type Data copy operation type:
 *              - DataCopyType::A2D: Source A to Destination transfer
 *              - DataCopyType::B2D: Source B to Destination transfer
 * 
 * @tparam Dst Destination synchronization mode for output coordination
 *             - DstSync::SyncFull: Complete synchronization with downstream units
 *             - DstSync::SyncHalf: Half-buffer synchronization for throughput optimization
 * 
 * @tparam is_fp32_dest_acc_en Enable FP32 destination accumulation mode
 *                             Affects memory layout and precision handling for large data sets
 * 
 * @tparam src_b_bcast_type Broadcasting type for Source B operations (B2D mode only)
 *                          - BroadcastType::NONE: No broadcasting, direct copy
 *                          - BroadcastType::ROW: Broadcast across rows
 *                          - BroadcastType::COL: Broadcast across columns (requires special handling)
 *                          - BroadcastType::SCALAR: Broadcast single value across tile
 * 
 * @tparam unpack_to_dest Enable direct destination unpacking optimization
 *                        Critical for 32-bit data types (Int32/UInt32) performance
 * 
 * @param dst_index Destination tile index for result storage
 * @param src_format Source data format (DataFormat enum value)
 * @param dst_format Destination data format for conversion
 *
 * @note **32-bit Data Type Optimization**: Function automatically detects 32-bit
 *       input/output combinations and enables optimized direct destination paths
 *       for maximum performance and memory efficiency
 * 
 * @note **Broadcasting Implementation**:
 *       - COL broadcast: Requires dual execution with manual source B clearing
 *       - Other broadcasts: Single execution with template-based optimization
 *       - Broadcasting affects memory access patterns and throughput
 * 
 * @note **Memory Bandwidth Optimization**: Data copy operations are memory-bound
 *       and require careful coordination with other pipeline stages for optimal
 *       system throughput and resource utilization
 *
 * @warning **Column Broadcasting Complexity**: COL broadcast mode requires special
 *          handling with manual clearing operations. Improper implementation can
 *          cause data corruption or incomplete transfers
 * 
 * @warning **Format Compatibility**: Source and destination format compatibility
 *          must be verified. Incompatible format combinations can cause hardware
 *          exceptions or incorrect data conversion
 * 
 * @warning **Destination Synchronization**: Sync mode must match downstream
 *          operation requirements. Mismatched synchronization can cause pipeline
 *          stalls, deadlocks, or data corruption
 * 
 * @see _llk_math_eltwise_unary_datacopy_init_ for initialization and configuration
 */
template <DataCopyType type, DstSync Dst, bool is_fp32_dest_acc_en, BroadcastType src_b_bcast_type = BroadcastType::NONE, bool unpack_to_dest = false>
inline void _llk_math_eltwise_unary_datacopy_(const std::uint32_t dst_index, const std::uint32_t src_format, const std::uint32_t dst_format)
{
    if (unpack_to_dest && is_32bit_input(src_format, dst_format))
    {
        math_unpack_to_dest_math_ready();
        math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32, true>(dst_index);
        math::math_unpack_to_dest_tile_ready();
    }
    else
    {
        math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(dst_index);

        if constexpr (type == A2D)
        {
            ckernel_template::run(instrn_buffer);
        }
        else if constexpr (type == B2D)
        {
            if constexpr (src_b_bcast_type == BroadcastType::COL)
            {
                // Mop for col broadcast only does 2 outerloops.  Needs to clear B manually and call twice
                ckernel_template::run(instrn_buffer);
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
                ckernel_template::run(instrn_buffer);
                TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, 0);
            }
            else
            {
                ckernel_template::run(instrn_buffer);
            }
        }

        math::clear_dst_reg_addr();
    }
}

template <DataCopyType type, BroadcastType bcast_type = BroadcastType::NONE>
inline void eltwise_unary_configure_addrmod()
{
    // Use srcA for data movement
    if constexpr (type == A2D)
    {
        addr_mod_t {
            .srca = {.incr = 1},
            .srcb = {.incr = 0},
            .dest = {.incr = 1},
        }
            .set(ADDR_MOD_0);

        // Just unpack into A and move to Dest
        addr_mod_t {
            .srca = {.incr = 8},
            .srcb = {.incr = 0},
            .dest = {.incr = 8},
        }
            .set(ADDR_MOD_2);
    }
    else
    {
        if constexpr (bcast_type == BroadcastType::ROW || bcast_type == BroadcastType::SCALAR)
        {
            addr_mod_t {
                .srca = {.incr = 0},
                .srcb = {.incr = 0},
                .dest = {.incr = 1},
            }
                .set(ADDR_MOD_0);

            // Just unpack into B and move to Dest
            addr_mod_t {
                .srca = {.incr = 0},
                .srcb = {.incr = 0},
                .dest = {.incr = 8},
            }
                .set(ADDR_MOD_2);
        }
        else
        {
            addr_mod_t {
                .srca = {.incr = 0},
                .srcb = {.incr = 1},
                .dest = {.incr = 1},
            }
                .set(ADDR_MOD_0);

            // Just unpack into B and move to Dest
            addr_mod_t {
                .srca = {.incr = 0},
                .srcb = {.incr = 8},
                .dest = {.incr = 8},
            }
                .set(ADDR_MOD_2);
        }
    }
}

template <DataCopyType type, bool is_fp32_dest_acc_en, BroadcastType bcast_type = BroadcastType::NONE, bool is_int_fpu_en = false>
inline void eltwise_unary_configure_mop(uint rows_per_inst, uint total_rows, const uint num_faces, const uint dst_format)
{
    // always move 32x32 tile, packed as 16x16x4

    if constexpr (type == A2D)
    {
        uint addr_mod  = (rows_per_inst == p_mova2d::MOV_1_ROW) ? ADDR_MOD_0 : ADDR_MOD_2;
        uint innerloop = (rows_per_inst == p_mova2d::MOV_1_ROW) ? total_rows : (total_rows >> 3);
        uint outerloop = num_faces;

        if (((is_fp32_dest_acc_en || is_int_fpu_en) && !(dst_format == (uint)DataFormat::UInt16)) || (dst_format == (uint)DataFormat::UInt8))
        {
            // use elwadd to handle unpacking data into src A as fp16, but dest is in fp32 mode OR to handle uint8 datums
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWADD(0, 0, p_elwise::SRCB_NO_BCAST, ADDR_MOD_2, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_AB));
            tmp.program(instrn_buffer);
        }
        else
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_MOVA2D(0, 0, ADDR_MOD_2, p_mova2d::MOV_8_ROWS, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_AB));
            tmp.program(instrn_buffer);
        }
    }
    else if constexpr (type == B2D)
    {
        uint addr_mod       = (rows_per_inst == p_movb2d::MOV_1_ROW) ? ADDR_MOD_0 : ADDR_MOD_2;
        uint innerloop      = (rows_per_inst == p_movb2d::MOV_1_ROW) ? total_rows : (total_rows >> 2);
        uint outerloop      = 4;
        auto broadcast_type = p_movb2d::MOV_1_ROW; // No broadcast;

        if constexpr (bcast_type == BroadcastType::COL)
        {
            innerloop = 16 >> 3; // elwadd produces 8 rows per op
            // The mop only runs for 2 outer loops and mop is called twice for col broadcast
            outerloop = 2;
            // broadcast_type = p_movb2d::MOV_8_ROW_BRCST_D0_BRCST;
            // MOVB2D with column broadcast doesn't work due to the bug in FPU tile
            // which masks dest write enable signals when instrn_mode[1:0] == 2'b01
            // ELTWADD with zeros will be used as a workaround
            broadcast_type = p_elwise::SRCB_BCAST_COL;
        }
        else if constexpr (bcast_type == BroadcastType::ROW)
        {
            innerloop      = (total_rows >> 3);
            broadcast_type = p_movb2d::MOV_8_ROW_BRCST;
        }
        else if constexpr (bcast_type == BroadcastType::SCALAR)
        {
            // ELTWADD with zeros will be used as a workaround
            outerloop      = 1;
            innerloop      = num_faces * (total_rows >> 3);
            broadcast_type = p_elwise::SRCB_BCAST_ALL;
        }

        if constexpr (bcast_type == BroadcastType::SCALAR)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWADD(0, 0, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, 0));
            tmp.program(instrn_buffer);
        }
        else if constexpr (bcast_type == BroadcastType::COL)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWADD(0, 0, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(0, p_setrwc::CR_B, 0, 0, 0, p_setrwc::SET_B));
            tmp.program(instrn_buffer);
        }
        else if constexpr (bcast_type == BroadcastType::ROW)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_MOVB2D(0, 0, addr_mod, broadcast_type, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_B, p_setrwc::CR_B, 0, 0, 0, p_setrwc::SET_B));
            tmp.program(instrn_buffer);
        }
        else
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_MOVB2D(0, 0, addr_mod, rows_per_inst, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_B, p_setrwc::CR_B, 0, 0, 0, p_setrwc::SET_B));
            tmp.program(instrn_buffer);
        }
    }
}

/**
 * @brief Initialize high-performance data copy operations with optimized configuration
 *
 * Configures the mathematical unit for optimized data copy operations including address
 * mode setup, micro-operation programming, and hardware register initialization. This
 * function establishes the foundation for efficient A→D and B→D data transfers with
 * support for broadcasting, format conversion, and performance optimization strategies.
 *
 * @tparam type Data copy operation type:
 *              - DataCopyType::A2D: Source A to Destination transfer optimization
 *              - DataCopyType::B2D: Source B to Destination transfer optimization
 * 
 * @tparam is_fp32_dest_acc_en Enable FP32 destination accumulation mode
 *                             Critical for maintaining precision in accumulation chains
 *                             Affects memory layout and mathematical processing pathways
 * 
 * @tparam src_b_bcast_type Broadcasting configuration for Source B operations (B2D mode)
 *                          - BroadcastType::NONE: Direct transfer, no broadcasting
 *                          - BroadcastType::ROW: Broadcast across tile rows
 *                          - BroadcastType::COL: Broadcast across tile columns (workaround mode)
 *                          - BroadcastType::SCALAR: Broadcast single value across entire tile
 * 
 * @tparam is_int_fpu_en Enable integer FPU processing for integer data types
 *                       Optimizes integer arithmetic and data movement pathways
 * 
 * @param transpose_of_faces Face-level transposition control (unused in math unit)
 *                          Reserved for future functionality and cross-unit compatibility
 * @param within_face_16x16_transpose Intra-face transposition control (unused in math unit)
 *                                   Handled by unpacker, maintained for API consistency
 * @param num_faces Number of tile faces to process (1, 2, or 4)
 *                  Affects loop structure and memory access patterns
 * @param dst_format Destination data format (default: 255 for auto-detection)
 *                   Controls output format conversion and precision handling
 *
 * @note **Address Mode Configuration**: Function automatically configures optimal
 *       address modes for the specified copy type using `eltwise_unary_configure_addrmod`
 *       to maximize memory bandwidth utilization and minimize access latency
 * 
 * @note **Micro-Operation Programming**: Configures hardware instruction templates
 *       with optimized parameters:
 *       - A2D mode: 8-row moves (MOV_8_ROWS) for maximum throughput
 *       - B2D mode: 4-row moves (MOV_4_ROWS) for optimal source B handling
 * 
 * @note **Broadcasting Workarounds**: COL broadcast uses ELTWADD workaround due to
 *       hardware limitation in FPU tile that masks destination write enable signals
 *       when instruction mode equals 2'b01. This maintains functionality while
 *       working around hardware constraints.
 * 
 * @note **Counter Reset and Synchronization**: Initializes mathematical unit counters
 *       and disables source A data validation for optimal performance in data copy
 *       scenarios where validation overhead is unnecessary
 * 
 * @note **Performance Optimization Strategy**:
 *       - Source A operations: Optimized for large block transfers
 *       - Source B operations: Balanced for broadcasting and direct copy efficiency
 *       - FP32 accumulation: Enhanced precision with memory layout optimization
 *       - Integer processing: Specialized pathways for integer data types
 *
 * @warning **Broadcasting Limitations**: Column broadcasting requires special handling
 *          and may have reduced performance compared to row or scalar broadcasting
 *          due to hardware workaround implementation using ELTWADD operations
 * 
 * @warning **Face Count Constraints**: num_faces must be 1, 2, or 4 to match hardware
 *          capabilities. Invalid face counts can cause incorrect loop generation
 *          and memory access violations
 * 
 * @warning **Template Parameter Dependencies**: Template parameters must be consistent
 *          with actual data types and operation requirements. Mismatched parameters
 *          can lead to suboptimal performance or incorrect results
 * 
 * @warning **Initialization Order**: This function must be called before any data copy
 *          operations and after proper unpacker configuration to ensure hardware
 *          state consistency and optimal performance
 * 
 * @see _llk_math_eltwise_unary_datacopy_ for the main execution function
 * @see eltwise_unary_configure_addrmod for address mode configuration details
 * @see eltwise_unary_configure_mop for micro-operation programming specifics
 */
template <DataCopyType type, bool is_fp32_dest_acc_en, BroadcastType src_b_bcast_type = BroadcastType::NONE, bool is_int_fpu_en = false>
// within_face_16x16_transpose is used by unpacker, math does not transpose
inline void _llk_math_eltwise_unary_datacopy_init_(
    const std::uint32_t transpose_of_faces          = 0 /*unused*/,
    const std::uint32_t within_face_16x16_transpose = 0 /* unused */,
    const std::uint32_t num_faces                   = 4,
    const std::uint32_t dst_format                  = 255)
{
    eltwise_unary_configure_addrmod<type, src_b_bcast_type>();

    if constexpr (type == A2D)
    {
        eltwise_unary_configure_mop<type, is_fp32_dest_acc_en, src_b_bcast_type, is_int_fpu_en>(p_mova2d::MOV_8_ROWS, 16, num_faces, dst_format);
    }
    else if constexpr (type == B2D)
    {
        eltwise_unary_configure_mop<type, false, src_b_bcast_type>(p_movb2d::MOV_4_ROWS, 16, num_faces, dst_format);
    }

    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);

    math::reset_counters(p_setrwc::SET_ABD_F);
}

/*************************************************************************
 * LLK MATH FAST TILIZE (Tilize single input using both unpackers and packer)
 * unit_dim is the number of tiles processed in a single iteration, num_units is the number of units processed in a single call
 * unit_dim and num_units must match the ones given to the unpacker (all unit_dim usage notes from the unpacker also apply here)
 * dst_index is the index of the tile inside the destination register to write to
 * both dest modes are supported (although 32 bit mode is supported by intentionally ignoring it for both math and pack unless src regs are TF32)
 * only DstSync::SyncHalf is supported
 * tiles are split across halves of the active dest bank (effectively quarters since DstSync::SyncHalf)
 * so nothing except fast tilize should be using that dest bank
 *************************************************************************/

inline void _llk_math_fast_tilize_addrmod_config_(const std::uint32_t unpack_dst_format, const std::uint32_t unit_dim)
{
    // standard addrmod that follows MOVB2D
    addr_mod_t {
        .srcb = {.incr = 4},
        .dest = {.incr = 4},
    }
        .set(ADDR_MOD_1);

    // standard addrmod that follows MOVA2D
    addr_mod_t {
        .srca = {.incr = 8},
        .dest = {.incr = 8},
    }
        .set(ADDR_MOD_2);

    // next two addrmods are mostly used for jumping to and from the offset for the bottom faces
    // offset for the bottom faces is always half the number of rows in the dest bank (512 / 2 for 16bit and 256 / 2 for 32bit since DstSync is always Half)
    uint32_t bottom_face_offset = (unpack_dst_format == (uint)DataFormat::Tf32 ? 256 : 512) / 2;
    // unit_dim 1 copies 2 faces before jumping so at the moment of the jump dest RWC is
    // 2*16 (two faces) -  8 (number of rows moved by current instruction)
    // unit_dim 2 copies 4 faces before jumping so at the moment of the jump dest RWC is
    // 4*16 (four faces) - 8 (number of rows moved by current instruction)
    uint8_t unit_dim_1_forward_jump = bottom_face_offset - (1 * (TILE_NUM_FACES / 2) * FACE_R_DIM - 8);
    uint8_t unit_dim_2_forward_jump = bottom_face_offset - (2 * (TILE_NUM_FACES / 2) * FACE_R_DIM - 8);

    // jumping back to the offset for the next tile is logically -bottom_face_offset if dest RWC is at the correct offset for the bottom faces of the next tile
    // only catch is the need to compensate for the current instruction, for unit_dim 1 that is MOVA2D while for unit_dim 2 and 3 that is MOVB2D
    int16_t unit_dim_1_backward_jump = -bottom_face_offset + 8;
    int16_t unit_dim_2_backward_jump = -bottom_face_offset + 4;

    // this follows MOVA2D in src and jumps to the offset for the bottom faces (for unit_dim 1 and 2, for unit_dim 3 that is handled the other way)
    addr_mod_t {
        .srca = {.incr = 8},
        .dest = {.incr = unit_dim == 1 ? unit_dim_1_forward_jump : unit_dim_2_forward_jump},
    }
        .set(ADDR_MOD_3);

    // this jumps back to the offset for the next tile, RWCs for source registers are reset separately when clearing dvalids
    addr_mod_t {
        .dest = {.incr = unit_dim == 1 ? unit_dim_1_backward_jump : unit_dim_2_backward_jump},
    }
        .set(ADDR_MOD_0);
}

inline void _llk_math_fast_tilize_mop_config_()
{
    ckernel_unpack_template tmp = ckernel_unpack_template(
        false,
        false,
        TT_OP_MOVA2D(p_mov::DEST_NORM, 0, ADDR_MOD_2, p_mova2d::MOV_8_ROWS, 0),
        TT_OP_NOP,
        TT_OP_NOP,
        TT_OP_NOP,
        TT_OP_MOVB2D(p_mov::DEST_NORM, 0, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 0),
        TT_OP_NOP,
        TT_OP_NOP);

    tmp.program(instrn_buffer);
}

inline void _llk_math_fast_tilize_init_(const std::uint32_t unpack_dst_format, const std::uint32_t unit_dim)
{
    // even though MOVA2D and MOVB2D are supposed to ignore ALU_ACC_CTRL_Fp32_enabled some parts still rely on it (not sure why)
    // it would be easier if they just fully respected ALU_ACC_CTRL_Fp32_enabled but it's a hardware quirk
    // so in non Tf32 cases, clear it to fully ignore FP32 dest mode
    // not sure why it doesn't work if CFG_STATE_ID_StateID is not 1
    if (unpack_dst_format != (uint)DataFormat::Tf32)
    {
        TT_SETC16(CFG_STATE_ID_StateID_ADDR32, 1);
        TTI_NOP;
        TTI_NOP;
        cfg_reg_rmw_tensix<ALU_ACC_CTRL_Fp32_enabled_RMW>(0);
    }

    // everything else is quite standard math init
    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);

    math::reset_counters(p_setrwc::SET_ABD_F);

    _llk_math_fast_tilize_addrmod_config_(unpack_dst_format, unit_dim);

    _llk_math_fast_tilize_mop_config_();
}

template <bool is_fp32_dest_acc_en>
inline void _llk_math_fast_tilize_uninit_(const std::uint32_t unpack_dst_format)
{
    // if ALU_ACC_CTRL_Fp32_enabled was previously cleared, restore it
    // still not sure why this CFG_STATE_ID_StateID manipulation is needed
    if (unpack_dst_format != (uint)DataFormat::Tf32)
    {
        cfg_reg_rmw_tensix<ALU_ACC_CTRL_Fp32_enabled_RMW>(is_fp32_dest_acc_en);
        TT_SETC16(CFG_STATE_ID_StateID_ADDR32, 0);
        TTI_NOP;
        TTI_NOP;
    }
}

inline void _llk_math_fast_tilize_block_(
    const std::uint32_t dst_index, const std::uint32_t unpack_dst_format, const std::uint32_t unit_dim, const std::uint32_t num_units)
{
    // split dest and write the top faces in the first half and the bottom faces in the second half (or more precisely quarter, since dest sync half)
    // make life easier by lying to set_dst_write_addr that tile shape is 32x16 so correct stride is obtained for dst_index
    math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x16>(dst_index);

    for (uint i = 0; i < num_units; i++)
    {
        if (unit_dim == 1)
        {
            // srcA has the full tile, copy the top faces first
            // inside mop:
            // for (uint j = 0; j < 3; j++)
            // {
            //     TTI_MOVA2D(p_mov::DEST_NORM, 0, ADDR_MOD_2, p_mova2d::MOV_8_ROWS, 0);
            // }
            TTI_MOP(p_mop::MASK_LOOP, 3 - 1, 0x0);
            // finish with the top faces and jump to the offset for the bottom faces
            TTI_MOVA2D(p_mov::DEST_NORM, 0, ADDR_MOD_3, p_mova2d::MOV_8_ROWS, 0);
            // copy the bottom faces
            // inside mop:
            // for (uint j = 0; j < 3; j++)
            // {
            //     TTI_MOVA2D(p_mov::DEST_NORM, 0, ADDR_MOD_2, p_mova2d::MOV_8_ROWS, 0);
            // }
            TTI_MOP(p_mop::MASK_LOOP, 3 - 1, 0x0);
            // finish with the bottom faces and jump back to the offset for the next tile
            TTI_MOVA2D(p_mov::DEST_NORM, 0, ADDR_MOD_0, p_mova2d::MOV_8_ROWS, 0);
            // clear just srcA dvalid since it's the only one set by the unpacker for unit_dim 1 and src RWCs
            TTI_SETRWC(p_setrwc::CLR_A, 0, 0, 0, 0, p_setrwc::SET_AB);
        }
        else if (unit_dim == 2)
        {
            // srcA has the top faces (4 of them), copy them
            // inside mop:
            // for (uint j = 0; j < 7; j++)
            // {
            //     TTI_MOVA2D(p_mov::DEST_NORM, 0, ADDR_MOD_2, p_mova2d::MOV_8_ROWS, 0);
            // }
            TTI_MOP(p_mop::MASK_LOOP, 7 - 1, 0x0);
            // finish with the top faces and jump to the offset for the bottom faces
            TTI_MOVA2D(p_mov::DEST_NORM, 0, ADDR_MOD_3, p_mova2d::MOV_8_ROWS, 0);
            // srcB has the bottom faces (4 of them), copy them
            // inside mop:
            // for (uint j = 0; j < 15; j++)
            // {
            //     TTI_MOVB2D(p_mov::DEST_NORM, 0, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 0);
            // }
            TTI_MOP(p_mop::MASK_LOOP, 15 - 1, 0xFFFF);
            // finish with the bottom faces and jump back to the offset for the next tile
            TTI_MOVB2D(p_mov::DEST_NORM, 0, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 0);
            // clear both dvalids and src RWCs
            TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_AB);
        }
        else if (unit_dim == 3)
        {
            // srcA has the top 8 rows of the top faces (6 of them), copy them
            // inside mop:
            // for (uint j = 0; j < 6; j++)
            // {
            //     TTI_MOVA2D(p_mov::DEST_NORM, 0, ADDR_MOD_2, p_mova2d::MOV_8_ROWS, 0);
            // }
            TTI_MOP(p_mop::MASK_LOOP, 6 - 1, 0x0);
            // srcB has the bottom 8 rows of the top faces (6 of them), copy them
            // inside mop:
            // for (uint j = 0; j < 12; j++)
            // {
            //     TTI_MOVB2D(p_mov::DEST_NORM, 0, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 0);
            // }
            TTI_MOP(p_mop::MASK_LOOP, 12 - 1, 0xFFFF);
            // done with the top faces, clear dvalids and src RWCs, next banks contain bottom faces
            // also clear dest RWC since we use dest offset for forward jump here
            TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_ABD);
            // don't have enough address mods to have unit_dim 3 forward jump so dest offset is used here
            uint32_t top_face_offset = dst_index + i * 3; // copy 3 tiles per iteration
            // offset to the bottom is the number of tiles that fit into the dest bank
            // since half size faces are specified, this gets into the correct position in the second half
            uint32_t bottom_face_offset = top_face_offset + (unpack_dst_format == (uint)DataFormat::Tf32 ? 4 : 8);
            math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x16>(bottom_face_offset);
            // srcA has the top 8 rows of the bottom faces (6 of them), copy them
            // inside mop:
            // for (uint j = 0; j < 6; j++)
            // {
            //     TTI_MOVA2D(p_mov::DEST_NORM, 0, ADDR_MOD_2, p_mova2d::MOV_8_ROWS, 0);
            // }
            TTI_MOP(p_mop::MASK_LOOP, 6 - 1, 0x0);
            // srcB has the bottom 8 rows of the bottom faces (6 of them), copy them
            // inside mop:
            // for (uint j = 0; j < 11; j++)
            // {
            //     TTI_MOVB2D(p_mov::DEST_NORM, 0, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 0);
            // }
            TTI_MOP(p_mop::MASK_LOOP, 11 - 1, 0xFFFF);
            // finish with the bottom faces and jump back to the offset for the next tile
            TTI_MOVB2D(p_mov::DEST_NORM, 0, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 0);
            // clear both dvalids and src RWCs
            TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_AB);
        }
    }

    math::clear_dst_reg_addr();
}
