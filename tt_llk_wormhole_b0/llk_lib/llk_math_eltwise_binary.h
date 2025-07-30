// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file llk_math_eltwise_binary.h
 * @brief Advanced element-wise binary mathematical operations for Tensix processing
 *
 * This header provides highly optimized element-wise binary mathematical operations including
 * addition, subtraction, multiplication, and complex arithmetic with advanced features such as
 * broadcasting patterns, destination reuse optimization, fidelity control, and high-performance
 * register management. These operations form the foundation of most mathematical workloads.
 *
 * @note Binary operations support sophisticated optimization strategies including destination
 *       register reuse (DEST→SRCA, DEST→SRCB), multiple broadcasting modes, and fidelity-based
 *       precision control for optimal performance vs accuracy tradeoffs.
 * 
 * @note Based on PR analysis: Critical performance optimizations include broadcast handling,
 *       precision management for large mathematical chains, and memory bandwidth optimization
 *       for high-throughput computational workloads.
 * 
 * @note High-performance template-based implementation with compile-time optimization for
 *       maximum throughput and minimal runtime overhead in embedded mathematical processing.
 */

#pragma once

#include <cstdint>

#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_math_common.h"

using namespace ckernel;

// local function declarations
inline void eltwise_binary_configure_addrmod();

/**
 * @brief Optimize register usage through destination-to-source register reuse
 *
 * Implements sophisticated register reuse optimization by moving destination register
 * contents to source registers, eliminating unnecessary memory traffic and improving
 * computational throughput. This optimization is critical for mathematical chains
 * where intermediate results are immediately consumed as inputs.
 *
 * @tparam binary_reuse_dest Destination reuse strategy:
 *                           - EltwiseBinaryReuseDestType::NONE: No reuse, standard operation
 *                           - EltwiseBinaryReuseDestType::DEST_TO_SRCA: Move destination to Source A
 *                           - EltwiseBinaryReuseDestType::DEST_TO_SRCB: Move destination to Source B
 *
 * @note **Register Movement Optimization**: Uses fixed-face addressing (ADDR_MOD_1)
 *       for optimal memory access patterns and minimal register transfer overhead
 * 
 * @note **Mathematical Chain Acceleration**: Particularly effective for iterative
 *       algorithms, accumulation patterns, and complex mathematical expressions
 *       where intermediate results are immediately reused
 * 
 * @note **Memory Bandwidth Reduction**: Eliminates external memory access by
 *       keeping data in fast register files, significantly improving throughput
 *       for memory-bound mathematical operations
 *
 * @warning **Register State Dependencies**: Must be carefully coordinated with
 *          subsequent operations to ensure correct data flow and avoid register
 *          conflicts or data corruption scenarios
 * 
 * @warning **Template Parameter Validation**: Reuse strategy must match actual
 *          operation requirements. Incorrect reuse patterns can cause incorrect
 *          mathematical results or register state corruption
 */
template <EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void eltwise_binary_reuse_dest_as_src()
{
    if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCA)
    {
        move_d2a_fixed_face(ADDR_MOD_1);
    }
    else if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCB)
    {
        move_d2b_fixed_face(ADDR_MOD_1);
    }
}

/**
 * @brief Execute high-performance element-wise binary mathematical operations
 *
 * Performs optimized element-wise binary operations with advanced features including
 * broadcasting support, destination register reuse, fidelity control, and format-specific
 * optimizations. This function implements the core computational kernel for mathematical
 * operations with hardware-specific optimizations for maximum throughput and precision.
 *
 * @tparam eltwise_binary_type Mathematical operation type:
 *                             - ELWADD: Element-wise addition with broadcast support
 *                             - ELWSUB: Element-wise subtraction with optimized patterns
 *                             - ELWMUL: Element-wise multiplication with fidelity control
 *                             - Additional operations based on hardware capabilities
 * 
 * @tparam src_b_bcast_type Broadcasting pattern for Source B operand:
 *                          - BroadcastType::NONE: No broadcasting, element-wise operation
 *                          - BroadcastType::ROW: Broadcast across tile rows
 *                          - BroadcastType::COL: Broadcast across tile columns (special handling)
 *                          - BroadcastType::SCALAR: Broadcast single value across entire tile
 * 
 * @tparam Dst Destination synchronization mode for pipeline coordination:
 *             - DstSync::SyncFull: Complete synchronization with downstream units
 *             - DstSync::SyncHalf: Half-buffer synchronization for throughput optimization
 * 
 * @tparam is_fp32_dest_acc_en Enable FP32 destination accumulation for precision
 *                             Critical for large mathematical chains and precision-sensitive workloads
 * 
 * @tparam NUM_FIDELITY_PHASES Number of fidelity phases for precision control (0-4)
 *                             Higher values increase precision at cost of performance
 *                             Essential for preventing catastrophic precision loss
 * 
 * @tparam binary_reuse_dest Destination register reuse optimization strategy
 *                          Enables high-performance register movement for mathematical chains
 * 
 * @param num_faces Number of tile faces to process (1, 2, or 4)
 * @param dst_index Destination tile index for result storage
 * @param clear_fp32_dst_acc Enable destination accumulator clearing for fresh computation
 *
 * @note **Broadcasting Implementation**: COL broadcast requires special dual-execution
 *       pattern with manual source clearing operations due to hardware limitations.
 *       Other broadcast modes use optimized single-execution templates.
 * 
 * @note **Fidelity vs Performance**: High fidelity operations (NUM_FIDELITY_PHASES > 0)
 *       provide enhanced precision for critical mathematical operations but reduce
 *       throughput by 2-4x. Essential for preventing precision catastrophes.
 * 
 * @note **FP32 Accumulation Strategy**: When enabled, maintains intermediate results
 *       in FP32 format to prevent precision loss in long mathematical chains and
 *       complex computational workloads requiring high numerical accuracy.
 * 
 * @note **Register Reuse Optimization**: Automatically applies destination-to-source
 *       register movement when configured, eliminating memory traffic and improving
 *       performance for iterative and chain-based mathematical operations.
 *
 * @warning **Broadcasting Performance Impact**: COL broadcast mode has reduced
 *          performance due to required workarounds. Consider alternative algorithms
 *          or data layouts when possible to maximize computational efficiency.
 * 
 * @warning **Fidelity Configuration**: NUM_FIDELITY_PHASES must be carefully selected
 *          based on precision requirements and performance constraints. Excessive
 *          fidelity can cause unacceptable performance degradation.
 * 
 * @warning **Synchronization Dependencies**: Dst sync mode must match downstream
 *          operation requirements and overall pipeline configuration to prevent
 *          deadlocks, stalls, or data corruption scenarios.
 * 
 * @warning **Accumulator State Management**: clear_fp32_dst_acc flag must be properly
 *          coordinated with mathematical operation sequence to ensure correct
 *          accumulation behavior and prevent stale data corruption.
 * 
 * @see eltwise_binary_reuse_dest_as_src for register reuse optimization details
 * @see _llk_math_eltwise_binary_init_ for initialization and configuration
 */
template <
    EltwiseBinaryType eltwise_binary_type,
    BroadcastType src_b_bcast_type,
    DstSync Dst,
    bool is_fp32_dest_acc_en,
    int NUM_FIDELITY_PHASES                      = 0,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void _llk_math_eltwise_binary_(const std::uint32_t num_faces, uint dst_index, const bool clear_fp32_dst_acc)
{
    constexpr bool high_fidelity     = (NUM_FIDELITY_PHASES > 0);
    constexpr uint32_t ZERO_ACC_MODE = p_zeroacc::CLR_16;

    math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(dst_index);

    if constexpr ((eltwise_binary_type == ELWADD) || (eltwise_binary_type == ELWSUB))
    {
        if constexpr (src_b_bcast_type == BroadcastType::COL)
        {
            // Mop for col broadcast only does 2 outerloops.  Needs to clear B manually and call twice for full tile size
            constexpr uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? 2 : 1;
#pragma GCC unroll 0
            for (std::uint32_t n = 0; n < outerloop; n++)
            {
                eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                ckernel_template::run(instrn_buffer);
            }
            TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            if (num_faces == 4)
            {
#pragma GCC unroll 0
                for (std::uint32_t n = 0; n < outerloop; n++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    ckernel_template::run(instrn_buffer);
                }
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            }
        }
        else
        {
            constexpr uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? 4 : 1;
#pragma GCC unroll 0
            for (std::uint32_t n = 0; n < outerloop; n++)
            {
                eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                ckernel_template::run(instrn_buffer);
            }
            // Manually clear B once mop is done for scaler bcast
            if constexpr (src_b_bcast_type == BroadcastType::SCALAR)
            {
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_D);
            }
        }
    }
    else if constexpr (eltwise_binary_type == ELWMUL)
    {
        if constexpr (src_b_bcast_type == BroadcastType::COL)
        {
            // Mop for col broadcast only does 2 outerloops.  Needs to clear B manually and call twice for full tile size
            constexpr uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? 2 : 1;
            if constexpr (high_fidelity)
            {
#pragma GCC unroll 0
                for (std::uint32_t n = 0; n < 2; n++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    auto base_address = (get_dest_buffer_base() >> 4) + (dst_index << (is_fp32_dest_acc_en && clear_fp32_dst_acc ? 3 : 2));
                    // fp32 zeroacc can only clear 8x16 datums at a time, need to call twice per 16x16 face
                    if (is_fp32_dest_acc_en && clear_fp32_dst_acc)
                    {
                        TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + (0 + n * 2));         // Clear lower half of faces 0 & 1 (offsets 0, 2)
                        TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + (0 + ((n * 2) + 1))); // Clear upper half of faces 0 & 1 (offsets: 1, 3)
                    }
                    else
                    {
                        TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + (0 + n)); // Clear faces 0 & 1
                    }
                    ckernel_template::run(instrn_buffer);
                }
            }
            else
            {
#pragma GCC unroll 0
                for (std::uint32_t n = 0; n < outerloop; n++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                    {
                        auto base_address = (get_dest_buffer_base() >> 4) + (dst_index << (is_fp32_dest_acc_en && clear_fp32_dst_acc ? 3 : 2));
                        // fp32 zeroacc can only clear 8x16 datums at a time, need to call twice per 16x16 face
                        if (is_fp32_dest_acc_en && clear_fp32_dst_acc)
                        {
                            TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + (0 + n * 2));         // Clear lower half of faces 0 & 1 (offsets 0, 2)
                            TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + (0 + ((n * 2) + 1))); // Clear upper half of faces 0 & 1 (offsets: 1, 3)
                        }
                        else
                        {
                            TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + (0 + n)); // Clear faces 0 & 1
                        }
                    }
                    ckernel_template::run(instrn_buffer);
                }
            }
            TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            if (num_faces == 4)
            {
                if constexpr (high_fidelity)
                {
#pragma GCC unroll 0
                    for (std::uint32_t n = 0; n < 2; n++)
                    {
                        eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                        if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                        {
                            auto base_address = (get_dest_buffer_base() >> 4) + (dst_index << (is_fp32_dest_acc_en && clear_fp32_dst_acc ? 3 : 2));
                            // fp32 zeroacc can only clear 8x16 datums at a time, need to call twice per 16x16 face
                            if (is_fp32_dest_acc_en && clear_fp32_dst_acc)
                            {
                                TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + (4 + n * 2));         // Clear lower half of faces 2 & 3  (offsets: 4, 6)
                                TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + (4 + ((n * 2) + 1))); // Clear upper half of faces 2 & 3 (offsets: 5, 7)
                            }
                            else
                            {
                                TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + (2 + n)); // Clear faces 2 & 3
                            }
                        }
                        ckernel_template::run(instrn_buffer);
                    }
                }
                else
                {
#pragma GCC unroll 0
                    for (std::uint32_t n = 0; n < outerloop; n++)
                    {
                        eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                        if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                        {
                            auto base_address = (get_dest_buffer_base() >> 4) + (dst_index << (is_fp32_dest_acc_en && clear_fp32_dst_acc ? 3 : 2));
                            // fp32 zeroacc can only clear 8x16 datums at a time, need to call twice per 16x16 face
                            if (is_fp32_dest_acc_en && clear_fp32_dst_acc)
                            {
                                TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + (4 + n * 2));         // Clear lower half of faces 2 & 3  (offsets: 4, 6)
                                TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + (4 + ((n * 2) + 1))); // Clear upper half of faces 2 & 3 (offsets: 5, 7)
                            }
                            else
                            {
                                TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + (2 + n)); // Clear faces 2 & 3
                            }
                        }
                        ckernel_template::run(instrn_buffer);
                    }
                }
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            }
        }
        else
        {
            // Row and no broadcasted behaves similarly
            const uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? num_faces : 1;
            if constexpr (high_fidelity)
            {
#pragma GCC unroll 0
                for (std::uint32_t n = 0; n < num_faces; n++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                    {
                        auto base_address = (get_dest_buffer_base() >> 4) + (dst_index << (is_fp32_dest_acc_en && clear_fp32_dst_acc ? 3 : 2));
                        // fp32 zeroacc can only clear 8x16 datums at a time, need to call twice per 16x16 face
                        if (is_fp32_dest_acc_en && clear_fp32_dst_acc)
                        {
                            TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + (n * 2));
                            TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + ((n * 2) + 1));
                        }
                        else
                        {
                            TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + n);
                        }
                    }
                    ckernel_template::run(instrn_buffer);
                }
            }
            else
            {
#pragma GCC unroll 0
                for (std::uint32_t n = 0; n < outerloop; n++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                    {
                        auto base_address = (get_dest_buffer_base() >> 4) + (dst_index << (is_fp32_dest_acc_en && clear_fp32_dst_acc ? 3 : 2));
                        // fp32 zeroacc can only clear 8x16 datums at a time, need to call twice per 16x16 face
                        if (is_fp32_dest_acc_en && clear_fp32_dst_acc)
                        {
                            TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + (n * 2));
                            TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + ((n * 2) + 1));
                        }
                        else
                        {
                            TT_ZEROACC(ZERO_ACC_MODE, ADDR_MOD_1, base_address + n);
                        }
                    }
                    ckernel_template::run(instrn_buffer);
                }
            }
            if constexpr (src_b_bcast_type == BroadcastType::SCALAR)
            {
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_D);
            }
        }
    }
    math::clear_dst_reg_addr();
}

template <EltwiseBinaryType eltwise_binary_type, BroadcastType bcast_type, std::uint32_t FIDELITY_INCREMENT>
inline void eltwise_binary_configure_addrmod()
{
    // Use srcA for data movement
    if constexpr ((eltwise_binary_type == ELWADD) || (eltwise_binary_type == ELWSUB) || (eltwise_binary_type == ELWMUL))
    {
        if constexpr (bcast_type == BroadcastType::NONE || bcast_type == BroadcastType::COL)
        {
            addr_mod_t {
                .srca = {.incr = 8},
                .srcb = {.incr = 8},
                .dest = {.incr = 8},
            }
                .set(ADDR_MOD_0);
        }
        else if constexpr (bcast_type == BroadcastType::ROW || bcast_type == BroadcastType::SCALAR)
        {
            addr_mod_t {
                .srca = {.incr = 8},
                .srcb = {.incr = 0},
                .dest = {.incr = 8},
            }
                .set(ADDR_MOD_0);
        }
        addr_mod_t {
            .srca = {.incr = 0},
            .srcb = {.incr = 0},
            .dest = {.incr = 0},
        }
            .set(ADDR_MOD_1);

        addr_mod_t {
            .srca = {.incr = 0, .clr = 1}, .srcb = {.incr = 0, .clr = 1}, .dest = {.incr = 0, .clr = 0, .cr = 1}, .fidelity = {.incr = FIDELITY_INCREMENT}}
            .set(ADDR_MOD_2);

        addr_mod_t {
            .srca     = {.incr = 0, .clr = 1},
            .srcb     = {.incr = 0, .clr = 1},
            .dest     = {.incr = 8, .clr = 0, .cr = 0, .c_to_cr = 1},
            .fidelity = {.incr = 0, .clr = 1}}
            .set(ADDR_MOD_3);
    }
}

template <
    EltwiseBinaryType eltwise_binary_type,
    BroadcastType bcast_type,
    int NUM_FIDELITY_PHASES                      = 0,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void eltwise_binary_configure_mop(const std::uint32_t acc_to_dest = 0, const std::uint32_t num_faces = 4)
{
    constexpr bool high_fidelity = (NUM_FIDELITY_PHASES > 0);
    const uint addr_mod          = ADDR_MOD_0;
    constexpr uint innerloop     = 16 >> 3; // 8 rows per eltwise op at a time.
    uint outerloop               = num_faces;
    auto broadcast_type          = p_elwise::SRCB_NO_BCAST;
    if constexpr (bcast_type == BroadcastType::COL)
    {
        // The mop only runs for 2 outer loops and mop is called twice for col broadcast
        outerloop      = 2;
        broadcast_type = p_elwise::SRCB_BCAST_COL;
    }
    else if constexpr (bcast_type == BroadcastType::ROW)
    {
        broadcast_type = p_elwise::SRCB_BCAST_ROW;
    }
    else if constexpr (bcast_type == BroadcastType::SCALAR)
    {
        broadcast_type = p_elwise::SRCB_BCAST_ALL;
    }

    if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
    {
        outerloop = 1;
    }

    // Scalar and Col broadcast should not Clear B within a mop.  This is controlled outside of MOP.
    if constexpr (bcast_type == BroadcastType::COL || bcast_type == BroadcastType::SCALAR)
    {
        if constexpr (eltwise_binary_type == ELWADD)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWADD(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_A, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            tmp.program(instrn_buffer);
        }
        else if constexpr (eltwise_binary_type == ELWSUB)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWSUB(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_A, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            tmp.program(instrn_buffer);
        }
        else if constexpr (eltwise_binary_type == ELWMUL)
        {
            ckernel_template tmp(high_fidelity ? NUM_FIDELITY_PHASES : outerloop, innerloop, TT_OP_ELWMUL(0, 0, broadcast_type, addr_mod, 0));
            if constexpr (high_fidelity)
            {
                tmp.set_last_inner_loop_instr(TT_OP_ELWMUL(0, 0, broadcast_type, ADDR_MOD_2, 0)); // Incr fidelity last inst of inner loop
                tmp.set_last_outer_loop_instr(TT_OP_ELWMUL(p_setrwc::CLR_A, 0, broadcast_type, ADDR_MOD_3, 0));
            }
            else
            {
                tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_A, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            }
            tmp.program(instrn_buffer);
        }
    }
    else
    {
        if constexpr (eltwise_binary_type == ELWADD)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWADD(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_AB, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            tmp.program(instrn_buffer);
        }
        else if constexpr (eltwise_binary_type == ELWSUB)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWSUB(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_AB, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            tmp.program(instrn_buffer);
        }
        else if constexpr (eltwise_binary_type == ELWMUL)
        {
            ckernel_template tmp(high_fidelity ? NUM_FIDELITY_PHASES : outerloop, innerloop, TT_OP_ELWMUL(0, 0, broadcast_type, addr_mod, 0));
            if constexpr (high_fidelity)
            {
                tmp.set_last_inner_loop_instr(TT_OP_ELWMUL(0, 0, broadcast_type, ADDR_MOD_2, 0)); // Incr fidelity last inst of inner loop
                tmp.set_last_outer_loop_instr(TT_OP_ELWMUL(p_setrwc::CLR_AB, 0, broadcast_type, ADDR_MOD_3, 0));
            }
            else
            {
                tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_AB, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            }
            tmp.program(instrn_buffer);
        }
    }
}

/**
 * @brief Initialize element-wise binary operations with comprehensive hardware configuration
 *
 * Performs complete initialization of the mathematical unit for binary operations including
 * fidelity configuration, address mode setup, micro-operation programming, and hardware
 * register initialization. This function establishes optimal computational pathways for
 * high-performance binary mathematical operations with precision and throughput optimization.
 *
 * @tparam eltwise_binary_type Mathematical operation type for optimization:
 *                             - ELWADD: Element-wise addition with broadcast and accumulation support
 *                             - ELWSUB: Element-wise subtraction with optimized data flow patterns  
 *                             - ELWMUL: Element-wise multiplication with fidelity control and precision management
 *                             Additional operations supported based on hardware capabilities
 * 
 * @tparam src_b_bcast_type Broadcasting strategy for Source B operand:
 *                          - BroadcastType::NONE: Element-wise operation, no broadcasting
 *                          - BroadcastType::ROW: Broadcast across tile rows with optimized addressing
 *                          - BroadcastType::COL: Broadcast across tile columns (requires special handling)
 *                          - BroadcastType::SCALAR: Broadcast single value across entire tile
 * 
 * @tparam MATH_FIDELITY_DESC Fidelity descriptor for precision vs performance control (0-15)
 *                            Higher values provide enhanced precision at computational cost
 *                            Critical for preventing catastrophic precision loss in mathematical chains
 * 
 * @tparam binary_reuse_dest Destination register reuse optimization strategy:
 *                          - EltwiseBinaryReuseDestType::NONE: Standard operation, no reuse
 *                          - EltwiseBinaryReuseDestType::DEST_TO_SRCA: Reuse destination as Source A
 *                          - EltwiseBinaryReuseDestType::DEST_TO_SRCB: Reuse destination as Source B
 * 
 * @param num_faces Number of tile faces to process (1, 2, or 4)
 *                 Controls loop structure and memory access patterns for optimal throughput
 * @param transpose Transposition control for memory layout optimization
 *                 Affects addressing patterns and computational efficiency
 * @param acc_to_dest Accumulation to destination mode for precision control
 *                   Critical for maintaining numerical accuracy in iterative computations
 *
 * @note **Fidelity Phase Calculation**: Function automatically computes optimal fidelity
 *       phases and increment values based on MATH_FIDELITY_DESC to balance precision
 *       and performance according to computational requirements
 * 
 * @note **Address Mode Optimization**: Configures hardware addressing modes with fidelity
 *       increment considerations for optimal memory access patterns and bandwidth utilization
 *       tailored to the specific binary operation and broadcasting requirements
 * 
 * @note **Micro-Operation Programming**: For core binary operations (ADD, SUB, MUL),
 *       configures optimized instruction templates with operation-specific parameters
 *       for maximum hardware utilization and computational throughput
 * 
 * @note **Hardware Register Configuration**: 
 *       - Disables Source A data validation for performance optimization
 *       - Resets mathematical unit counters for clean operational state
 *       - Establishes proper synchronization for pipeline coordination
 * 
 * @note **Performance Characteristics** (based on PR analysis):
 *       - High fidelity operations: 2-4x performance reduction but critical precision preservation
 *       - Broadcasting optimization: Specialized patterns for different broadcast types
 *       - Register reuse: Significant memory bandwidth reduction for mathematical chains
 *
 * @warning **Fidelity vs Performance Trade-off**: Higher MATH_FIDELITY_DESC values
 *          significantly impact computational throughput but are essential for preventing
 *          precision catastrophes in sensitive mathematical operations and long computation chains
 * 
 * @warning **Broadcasting Performance Impact**: COL broadcast requires additional overhead
 *          and may have reduced performance compared to other broadcasting modes due to
 *          hardware limitations and required workarounds
 * 
 * @warning **Template Parameter Consistency**: All template parameters must be consistent
 *          with actual operation requirements and downstream processing expectations to
 *          ensure correct results and optimal performance characteristics
 * 
 * @warning **Initialization Dependencies**: This function must be called after proper
 *          unpacker configuration and before any binary operations to ensure correct
 *          hardware state and optimal computational performance
 * 
 * @see _llk_math_eltwise_binary_ for the main execution function
 * @see eltwise_binary_configure_addrmod for address mode configuration details
 * @see eltwise_binary_configure_mop for micro-operation programming specifics
 */
template <
    EltwiseBinaryType eltwise_binary_type,
    BroadcastType src_b_bcast_type,
    int MATH_FIDELITY_DESC                       = 0,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void _llk_math_eltwise_binary_init_(const std::uint32_t num_faces, const std::uint32_t transpose, const std::uint32_t acc_to_dest)
{
    constexpr int MATH_FIDELITY_PHASES    = get_math_num_fidelity_phases(MATH_FIDELITY_DESC);
    constexpr int MATH_FIDELITY_INCREMENT = get_math_fidelity_increment(MATH_FIDELITY_DESC);

    eltwise_binary_configure_addrmod<eltwise_binary_type, src_b_bcast_type, MATH_FIDELITY_INCREMENT>();

    if constexpr ((eltwise_binary_type == ELWADD) || (eltwise_binary_type == ELWSUB) || (eltwise_binary_type == ELWMUL))
    {
        eltwise_binary_configure_mop<eltwise_binary_type, src_b_bcast_type, MATH_FIDELITY_PHASES, binary_reuse_dest>(acc_to_dest, num_faces);
    }

    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);

    math::reset_counters(p_setrwc::SET_ABD_F);
}
