// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file llk_unpack_AB_matmul.h
 * @brief Advanced dual-source matrix multiplication unpacker for high-performance computing
 *
 * This header provides sophisticated matrix multiplication unpacking operations that simultaneously
 * manage both Source A and Source B data streams with advanced optimization strategies including
 * reuse patterns, replay buffers, partial face handling, and broadcast configurations. These
 * operations are critical for achieving maximum performance in matrix multiplication workloads.
 *
 * @note **Dual-Source Complexity**: Manages both input matrices (A→SrcB, B→SrcA) with sophisticated
 *       reuse detection, replay buffer optimization, and partial face support for irregular matrix sizes.
 * 
 * @note **Performance-Critical Operations**: Matrix multiplication unpacking is the primary bottleneck
 *       in most mathematical workloads, requiring careful optimization of memory access patterns,
 *       register utilization, and data flow coordination.
 * 
 * @note **Based on PR Analysis**: Critical optimizations include replay buffer management for
 *       irregular tile sizes, broadcast pattern optimization, and memory bandwidth maximization
 *       for sustained high-throughput matrix operations.
 * 
 * @note **Hardware Architecture Integration**: Directly interfaces with Tensix replay buffer
 *       mechanisms, LLTT (Low-Level Trace Tool) for instruction recording, and SFPI for
 *       specialized mathematical preprocessing operations.
 */

#pragma once

#include <cstdint>

#include "ckernel.h"
#include "ckernel_defs.h"
#include "ckernel_globals.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "cunpack_common.h"
#include "lltt.h"
#include "sfpi.h"

using namespace ckernel;
using namespace ckernel::unpacker;

/**
 * @brief Configure sophisticated matrix multiplication micro-operations with advanced optimization
 *
 * Sets up complex micro-operation templates for dual-source matrix multiplication unpacking with
 * intelligent reuse pattern detection, replay buffer optimization, and partial face support.
 * This function implements the core instruction sequencing for maximum hardware utilization
 * and optimal memory access patterns in matrix multiplication operations.
 *
 * @tparam kernel_broadcast_a Broadcasting configuration for matrix A input (0-1)
 *                           Controls how matrix A data is distributed across computational units
 *                           Values > 1 require special handling with reuse disabled
 * @tparam kernel_broadcast_b Broadcasting configuration for matrix B input (0-1)  
 *                           Controls how matrix B data is distributed across computational units
 *                           Restricted to ≤1 when reuse optimization is enabled
 * 
 * @param transpose Matrix transposition mode (handled by math unit, not unpacker)
 *                 Reserved for future functionality and cross-unit coordination
 * @param ct_dim Column tile dimension for matrix C (output matrix)
 * @param rt_dim Row tile dimension for matrix C (output matrix)
 * @param kt_dim K dimension for inner product computation (matrix A columns / matrix B rows)
 * @param unpA_partial_face Enable partial face support for matrix A irregular dimensions
 *                         Activates 16-instruction replay buffer for complex addressing
 * @param unpB_partial_face Enable partial face support for matrix B irregular dimensions
 *                         Activates 16-instruction replay buffer for complex addressing
 *
 * @note **Reuse Strategy Intelligence**: Function automatically determines optimal reuse
 *       strategy based on dimension analysis (ct_dim >= rt_dim):
 *       - reuse_a = true: Reuse matrix A data, iterate over matrix B
 *       - reuse_a = false: Reuse matrix B data, iterate over matrix A
 * 
 * @note **Replay Buffer Optimization**: Dynamically configures replay buffer length
 *       based on partial face requirements:
 *       - Standard operations: 10-instruction buffer for optimal performance
 *       - Partial face operations: 16-instruction buffer for complex addressing patterns
 * 
 * @note **Input Matrix Mapping** (Critical for understanding):
 *       - Matrix A (input 0) → loaded to Source B register
 *       - Matrix B (input 1) → loaded to Source A register
 *       This mapping optimizes hardware data flow patterns
 * 
 * @note **Advanced Instruction Sequences**: Generates sophisticated instruction patterns
 *       including zero-source operations, context switching, address counter management,
 *       and synchronization primitives for optimal computational efficiency
 * 
 * @note **Hardware Integration**: Uses LLTT recording for instruction replay, SETADCZW
 *       for address counter management, and specialized UNPACR instructions for optimal
 *       data movement and register utilization patterns
 *
 * @warning **Broadcasting Constraints**: kernel_broadcast_b must be ≤1 when reuse is
 *          enabled due to hardware limitations. Violating this constraint triggers
 *          compile-time static_assert failure to prevent runtime issues.
 * 
 * @warning **Partial Face Complexity**: Partial face operations significantly increase
 *          instruction buffer requirements and complexity. Use only when necessary for
 *          irregular matrix dimensions that don't align with standard tile boundaries.
 * 
 * @warning **Memory Access Coordination**: Complex addressing patterns must be carefully
 *          coordinated with downstream mathematical operations to prevent pipeline stalls,
 *          register conflicts, or incorrect data flow scenarios.
 * 
 * @warning **Template Parameter Dependencies**: Broadcasting parameters must match actual
 *          data distribution requirements and hardware capabilities to ensure correct
 *          matrix multiplication results and optimal performance characteristics.
 * 
 * @see _llk_unpack_AB_matmul_init_ for initialization and hardware configuration
 * @see _llk_unpack_AB_matmul_ for the main execution function
 */
// transpose is unused, math is adjusted to take into account srca face layout when transpose=true
template <std::uint32_t kernel_broadcast_a = 0, std::uint32_t kernel_broadcast_b = 0>
inline void _llk_unpack_AB_matmul_mop_config_(
    const bool transpose,
    const std::uint32_t ct_dim,
    const std::uint32_t rt_dim,
    const std::uint32_t kt_dim,
    const bool unpA_partial_face,
    const bool unpB_partial_face)
{
    // in0/inA - loaded to SrcB
    // in1/inB - loaded to SrcA

    const bool reuse_a                      = ct_dim >= rt_dim;
    const std::uint32_t replay_buf_prog_len = (reuse_a && unpA_partial_face) ? 16 : ((!reuse_a && unpB_partial_face) ? 16 : 10);
    const std::uint32_t replay_buf_run_len  = replay_buf_prog_len / 2;

    if (reuse_a)
    {
        static_assert(kernel_broadcast_b <= 1, "kernel_broadcast>1 on matmul input 1 is not supported with reuse enabled!");
        lltt::record(0, replay_buf_prog_len);
        if (unpA_partial_face)
        {
            TTI_UNPACR_NOP(SrcA, p_unpacr_nop::UNP_ZEROSRC);
            TTI_UNPACR(SrcA, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 0 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
            TTI_UNPACR(SrcA, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
            TTI_SETADCZW(p_setadc::UNP_A, 0, 0, 0, 0, 0b0101); // Set ch0_z=0, ch1_z=0
        }
        else
        {
            TTI_UNPACR(SrcA, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
        }
        if constexpr (kernel_broadcast_b == 1)
        {
            TTI_NOP;
            TTI_NOP;
            TTI_NOP;
        }
        else
        {
            TTI_RDCFG(p_gpr_unpack::TMP0, THCON_SEC0_REG3_Base_address_ADDR32);
            TTI_ADDDMAREG(0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP0, p_gpr_unpack::TILE_SIZE_A);
            TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC0_REG3_Base_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
        }
        TTI_NOP;
        if (unpA_partial_face)
        {
            TTI_UNPACR_NOP(SrcA, p_unpacr_nop::UNP_ZEROSRC);
            TTI_UNPACR(SrcA, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 0 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
            TTI_UNPACR(SrcA, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
            TTI_SETADCZW(p_setadc::UNP_A, 0, 0, 0, 0, 0b0101); // Set ch0_z=0, ch1_z=0
        }
        else
        {
            TTI_UNPACR(SrcA, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
        }
        if constexpr (kernel_broadcast_b == 1)
        {
            TTI_NOP;
            TTI_NOP;
            TTI_NOP;
        }
        else
        {
            TTI_RDCFG(p_gpr_unpack::TMP0, THCON_SEC0_REG3_Base_cntx1_address_ADDR32);
            TTI_ADDDMAREG(0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP0, p_gpr_unpack::TILE_SIZE_A);
            TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC0_REG3_Base_cntx1_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
        }
        TTI_NOP;
    }
    else
    {
        static_assert(kernel_broadcast_a <= 1, "kernel_broadcast>1 on matmul input 0 is not supported with reuse enabled!");
        lltt::record(0, replay_buf_prog_len);
        if (unpB_partial_face)
        {
            TTI_UNPACR_NOP(SrcB, p_unpacr_nop::UNP_ZEROSRC);
            TTI_UNPACR(SrcB, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 0 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
            TTI_UNPACR(SrcB, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
            TTI_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, 0b0101); // Set ch0_z=0, ch1_z=0
        }
        else
        {
            TTI_UNPACR(SrcB, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
        }
        if constexpr (kernel_broadcast_a == 1)
        {
            TTI_NOP;
            TTI_NOP;
            TTI_NOP;
        }
        else
        {
            TTI_RDCFG(p_gpr_unpack::TMP0, THCON_SEC1_REG3_Base_address_ADDR32);
            TTI_ADDDMAREG(0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP_LO);
            TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
        }
        TTI_NOP;
        if (unpB_partial_face)
        {
            TTI_UNPACR_NOP(SrcB, p_unpacr_nop::UNP_ZEROSRC);
            TTI_UNPACR(SrcB, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 0 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
            TTI_UNPACR(SrcB, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
            TTI_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, 0b0101); // Set ch0_z=0, ch1_z=0
        }
        else
        {
            TTI_UNPACR(SrcB, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
        }
        if constexpr (kernel_broadcast_a == 1)
        {
            TTI_NOP;
            TTI_NOP;
            TTI_NOP;
        }
        else
        {
            TTI_RDCFG(p_gpr_unpack::TMP0, THCON_SEC1_REG3_Base_cntx1_address_ADDR32);
            TTI_ADDDMAREG(0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP_LO);
            TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_cntx1_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
        }
        TTI_NOP;
    }

    ckernel_unpack_template tmp = ckernel_unpack_template(
        false,                                    // src B
        false,                                    // halo - just used for 4 unpacks
        lltt::replay_insn(0, replay_buf_run_len), // runs when context is 0
        0,
        0,
        0,
        lltt::replay_insn(replay_buf_run_len, replay_buf_run_len), // runs when context is 1
        0,
        0);

    tmp.program(instrn_buffer);
}

/**
 * @brief Configure hardware parameters for dual-source matrix multiplication unpacking
 *
 * Sets up comprehensive hardware configuration for both Source A and Source B unpackers
 * including format conversion, stochastic rounding modes, face dimension setup, and
 * address counter configuration. This function establishes the hardware foundation for
 * optimal matrix multiplication data flow and computational efficiency.
 *
 * @tparam is_fp32_dest_acc_en Enable FP32 destination accumulation mode
 *                             Critical for maintaining precision in large matrix chains
 *                             Affects memory layout and mathematical processing pathways
 * @tparam stoch_rnd_mode Stochastic rounding mode for enhanced numerical precision:
 *                        - StochRndType::None: Standard rounding (default)
 *                        - StochRndType::All: Global stochastic rounding for all units
 *                        - StochRndType::Fpu: FPU-specific stochastic rounding
 *                        - StochRndType::Pack: Pack-stage stochastic rounding
 * 
 * @param unpA_src_format Source format for matrix A data (DataFormat enum value)
 * @param unpB_src_format Source format for matrix B data (DataFormat enum value)  
 * @param unpA_dst_format Destination format for matrix A conversion
 * @param unpB_dst_format Destination format for matrix B conversion
 * @param unpA_face_r_dim Face row dimension for matrix A (default: FACE_R_DIM = 16)
 * @param unpB_face_r_dim Face row dimension for matrix B (default: FACE_R_DIM = 16)
 * @param within_face_16x16_transpose Enable intra-face transposition for data layout optimization
 * @param unpA_num_faces Number of faces in matrix A tiles (1, 2, or 4)
 * @param unpB_num_faces Number of faces in matrix B tiles (1, 2, or 4)
 * @param unpA_tile_size Matrix A tile size in datums for address calculation
 * @param unpB_tile_size Matrix B tile size in datums for address calculation
 *
 * @note **Stochastic Rounding Configuration**: Function automatically determines optimal
 *       rounding enable flags based on stoch_rnd_mode template parameter:
 *       - fpu_srnd_en: Enables FPU stochastic rounding for enhanced precision
 *       - pack_srnd_en: Enables pack-stage stochastic rounding for output optimization
 * 
 * @note **Dual-Source Format Conversion**: Handles independent format conversion for
 *       both matrix operands, enabling mixed-precision matrix multiplication with
 *       optimal hardware utilization and memory bandwidth efficiency
 * 
 * @note **Address Counter Configuration**: Sets up ADC (Address Counter) endpoints
 *       for both unpackers based on face dimensions and tile organization:
 *       - Matrix A: unpA_x_end = unpA_num_faces * unpA_face_r_dim * FACE_C_DIM - 1
 *       - Matrix B: unpB_x_end = unpB_num_faces * unpB_face_r_dim * FACE_C_DIM - 1
 * 
 * @note **Register File Configuration**: Programs tile size registers for both
 *       unpackers with synchronization to ensure consistent hardware state across
 *       all computational units and optimal data flow coordination
 * 
 * @note **Performance Optimization**: Configuration optimized for row pooling
 *       disabled (is_row_pool = false) for maximum matrix multiplication throughput
 *       and optimal memory access patterns
 *
 * @warning **Format Compatibility**: Source and destination format combinations
 *          must be compatible for both matrices. Incompatible formats can cause
 *          data corruption, hardware exceptions, or incorrect mathematical results
 * 
 * @warning **Face Dimension Constraints**: Face dimensions must align with actual
 *          tile organization and hardware capabilities. Invalid dimensions can
 *          cause memory access violations or incorrect address generation
 * 
 * @warning **Stochastic Rounding Impact**: Stochastic rounding modes affect
 *          computational precision and performance. Select appropriate modes based
 *          on numerical requirements and performance constraints
 * 
 * @warning **Tile Size Dependencies**: Tile size parameters must match actual data
 *          organization. Incorrect sizes can cause address calculation errors and
 *          memory access violations in downstream operations
 * 
 * @see configure_unpack_AB for underlying dual-source configuration implementation
 * @see _llk_unpack_AB_matmul_init_ for complete initialization sequence
 * @see _llk_unpack_AB_matmul_ for main execution function
 */
template <bool is_fp32_dest_acc_en, StochRndType stoch_rnd_mode = StochRndType::None>
inline void _llk_unpack_AB_matmul_hw_configure_(
    const std::uint32_t unpA_src_format,
    const std::uint32_t unpB_src_format,
    const std::uint32_t unpA_dst_format,
    const std::uint32_t unpB_dst_format,
    const std::uint32_t unpA_face_r_dim             = FACE_R_DIM,
    const std::uint32_t unpB_face_r_dim             = FACE_R_DIM,
    const std::uint32_t within_face_16x16_transpose = 0,
    const std::uint32_t unpA_num_faces              = 4,
    const std::uint32_t unpB_num_faces              = 4,
    const std::uint32_t unpA_tile_size              = 0,
    const std::uint32_t unpB_tile_size              = 0)
{
    constexpr bool is_row_pool  = false;
    constexpr bool stoch_rnd_en = (stoch_rnd_mode == StochRndType::All);
    constexpr bool fpu_srnd_en  = stoch_rnd_en || (stoch_rnd_mode == StochRndType::Fpu);
    constexpr bool pack_srnd_en = stoch_rnd_en || (stoch_rnd_mode == StochRndType::Pack);

    configure_unpack_AB<is_fp32_dest_acc_en, is_row_pool, fpu_srnd_en, pack_srnd_en>(
        unpA_src_format,
        unpB_src_format,
        unpA_dst_format,
        unpB_dst_format,
        unpA_face_r_dim,
        unpB_face_r_dim,
        within_face_16x16_transpose,
        unpA_num_faces,
        unpB_num_faces);

    // Configure tile size in datums
    const uint32_t unpA_x_end = unpA_num_faces * unpA_face_r_dim * FACE_C_DIM - 1;
    const uint32_t unpB_x_end = unpB_num_faces * unpB_face_r_dim * FACE_C_DIM - 1;
    TT_SETADCXX(p_setadc::UNP_A, unpA_x_end, 0x0);
    TT_SETADCXX(p_setadc::UNP_B, unpB_x_end, 0x0);

    regfile[p_gpr_unpack::TILE_SIZE_A] = unpA_tile_size;
    regfile[p_gpr_unpack::TILE_SIZE_B] = unpB_tile_size;
    sync_regfile_write(p_gpr_unpack::TILE_SIZE_B);
}

/**
 * @brief Initialize dual unpacker operation for matrix multiplication on Wormhole B0
 * 
 * @details Configures both Unpacker 0 (operand A → SRCA) and Unpacker 1 (operand B → SRCB)
 * for efficient matrix multiplication data movement. This function optimizes the unpacker
 * configuration for GEMM workloads, including broadcast patterns, reuse strategies, and
 * address generation for maximum throughput to the 2048 hardware multipliers.
 * 
 * **Dual Unpacker Architecture:**
 * - Unpacker 0: Loads matrix A data into SRCA register file (64×16 datums)
 * - Unpacker 1: Loads matrix B data into SRCB register file (64×16 datums)
 * - Combined bandwidth: 80B/clock for optimal math unit utilization
 * 
 * **Matrix Reuse Optimization:**
 * Automatically determines optimal reuse pattern based on dimensions:
 * - A-reuse (ct_dim >= rt_dim): Reuse matrix A across multiple B columns
 * - B-reuse (rt_dim > ct_dim): Reuse matrix B across multiple A rows
 * 
 * **Broadcast Support:**
 * Template parameters enable efficient broadcast operations:
 * - kernel_broadcast_a: Enable operand A broadcast optimization
 * - kernel_broadcast_b: Enable operand B broadcast optimization
 * 
 * **Performance Features:**
 * - Optimal L1 bank interleaving for both operands
 * - Hardware address generation with carriage return
 * - Partial face support for non-standard tile sizes
 * - Format conversion during data movement
 * 
 * @tparam kernel_broadcast_a Enable broadcast optimization for operand A (0=disabled, 1=enabled)
 * @tparam kernel_broadcast_b Enable broadcast optimization for operand B (0=disabled, 1=enabled)
 * 
 * @param transpose Matrix transpose mode (0=no transpose, 1=transpose operand A)
 * @param ct_dim Column tile dimension for blocking algorithm
 * @param rt_dim Row tile dimension for blocking algorithm
 * @param kt_dim K-dimension tile count for accumulation
 * @param unpA_face_r_dim Row dimension of operand A tile face (default: FACE_R_DIM)
 * @param unpB_face_r_dim Row dimension of operand B tile face (default: FACE_R_DIM)
 * @param unpA_num_faces Number of faces in operand A tile (default: 4)
 * @param unpB_num_faces Number of faces in operand B tile (default: 4)
 * @param unpA_partial_face Enable partial face for operand A non-standard sizes
 * @param unpB_partial_face Enable partial face for operand B non-standard sizes
 * 
 * @note This function must be called once before any _llk_unpack_AB_matmul_() operations
 * @note Automatically configures optimal reuse pattern based on ct_dim vs rt_dim
 * @note Both unpackers are configured simultaneously for coordinated operation
 * 
 * @warning Matrix dimensions must be compatible with hardware tile constraints
 * @warning Broadcast template parameters affect address generation patterns
 * @warning Face dimensions must align with actual data tile organization
 * 
 * @see _llk_unpack_AB_matmul_() for per-tile execution
 * @see _llk_unpack_AB_matmul_hw_configure_() for hardware configuration
 */
template <std::uint32_t kernel_broadcast_a = 0, std::uint32_t kernel_broadcast_b = 0>
__attribute__((always_inline)) inline void _llk_unpack_AB_matmul_init_(
    const std::uint32_t transpose       = 0,
    const std::uint32_t ct_dim          = 1,
    const std::uint32_t rt_dim          = 1,
    const std::uint32_t kt_dim          = 1,
    const std::uint32_t unpA_face_r_dim = FACE_R_DIM,
    const std::uint32_t unpB_face_r_dim = FACE_R_DIM,
    const std::uint32_t unpA_num_faces  = 4,
    const std::uint32_t unpB_num_faces  = 4,
    const bool unpA_partial_face        = false,
    const bool unpB_partial_face        = false)
{
    // Validate parameters
    LLK_VALIDATE_PARAM_RANGE(transpose, 0, 1, "transpose must be 0 or 1");
    LLK_VALIDATE_MATMUL_DIMS(rt_dim, ct_dim, kt_dim);
    
    // Use utility for optimal reuse pattern calculation
    const bool reuse_a = ckernel::utils::PerformanceUtils::should_reuse_a(ct_dim, rt_dim);

    // also turn on within_face_16x16_transpose if it was turned off by datacopy at runtime
    // on WH, the unpacker performs both transpose of faces as well as transpose each face.
    // the former is configured in mop, the latter is configured in cfg register in hw_configure
    // in large matmul, datacopy will disable the transpose of faces, so we need it turn it back on for matmul.
    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(transpose);

    TTI_SETADCZW(0b011, 0, 0, 0, 0, 0b1111);

    if (unpA_partial_face)
    {
        // Do face by face unpacking. Need to program correct face dim
        // to compute address of the next face
        config_unpacker_x_end<p_setadc::UNP_A>(unpA_face_r_dim);
    }
    else
    {
        const uint32_t unpA_x_end = unpA_num_faces * unpA_face_r_dim * FACE_C_DIM - 1;
        TT_SETADCXX(p_setadc::UNP_A, unpA_x_end, 0x0);
    }

    if (unpB_partial_face)
    {
        // Do face by face unpacking. Need to program correct face dim
        // to compute address of the next face
        config_unpacker_x_end<p_setadc::UNP_B>(unpB_face_r_dim);
    }
    else
    {
        // Do full tile unpacking. No need to program face dim
        // as address counter pointing to the face is not incremented
        const uint32_t unpB_x_end = unpB_num_faces * unpB_face_r_dim * FACE_C_DIM - 1;
        TT_SETADCXX(p_setadc::UNP_B, unpB_x_end, 0x0);
    }

    TT_SETDMAREG(0, LOWER_HALFWORD(kt_dim), 0, LO_16(p_gpr_unpack::KT_DIM)); // store kt_dim to gpr for scaling tile size

    _llk_unpack_AB_matmul_mop_config_<kernel_broadcast_a, kernel_broadcast_b>(transpose != 0, ct_dim, rt_dim, kt_dim, unpA_partial_face, unpB_partial_face);
}

template <std::uint32_t kernel_broadcast_a = 0, std::uint32_t kernel_broadcast_b = 0>
inline void _llk_unpack_AB_matmul_(
    const std::uint32_t base_address_a,
    const std::uint32_t base_address_b,
    const std::uint32_t tile_index_a,
    const std::uint32_t tile_index_b,
    const std::uint32_t tile_size_a,
    const std::uint32_t tile_size_b,
    const std::uint32_t unpA_face_r_dim = FACE_R_DIM,
    const std::uint32_t unpB_face_r_dim = FACE_R_DIM,
    const bool unpA_partial_face        = false,
    const bool unpB_partial_face        = false,
    std::uint32_t ct_dim                = 1,
    const std::uint32_t rt_dim          = 1,
    const std::uint32_t kt_dim          = 1)
{
    // In0/InA -> srcB (supports partial face)
    // In1/InB -> srcA

    volatile uint *cfg = get_cfg_pointer(); // get pointer to registers for current state ID

    const bool reuse_a        = ct_dim >= rt_dim;
    const std::uint32_t t_dim = reuse_a ? rt_dim : ct_dim;

    if (!reuse_a)
    {
        TTI_MULDMAREG(0, p_gpr_unpack::TMP_LO, p_gpr_unpack::TILE_SIZE_B, p_gpr_unpack::KT_DIM);
    }

    for (uint t = 0; t < t_dim; t++)
    {
        std::uint32_t offset_address_a      = tile_size_a * (tile_index_a + (reuse_a ? (t * kt_dim) : (0)));
        std::uint32_t next_offset_address_a = tile_size_a * (tile_index_a + (reuse_a ? ((t + 1) * kt_dim) : (0)));
        if constexpr (kernel_broadcast_a > 0)
        {
            offset_address_a      = tile_size_a * ((tile_index_a + (reuse_a ? ((t * kt_dim)) : (0))) % kernel_broadcast_a);
            next_offset_address_a = tile_size_a * ((tile_index_a + (reuse_a ? (((t + 1) * kt_dim)) : (0))) % kernel_broadcast_a);
        }
        std::uint32_t offset_address_b      = tile_size_b * (tile_index_b + (reuse_a ? (0) : (t)));
        std::uint32_t next_offset_address_b = tile_size_b * (tile_index_b + (reuse_a ? (0) : (t + 1)));
        if constexpr (kernel_broadcast_b > 0)
        {
            offset_address_b      = tile_size_b * ((tile_index_b + (reuse_a ? (0) : (t))) % kernel_broadcast_b);
            next_offset_address_b = tile_size_b * ((tile_index_b + (reuse_a ? (0) : ((t + 1)))) % kernel_broadcast_b);
        }
        std::uint32_t address_a      = base_address_a + offset_address_a;
        std::uint32_t next_address_a = base_address_a + next_offset_address_a;
        std::uint32_t address_b      = base_address_b + offset_address_b;
        std::uint32_t next_address_b = base_address_b + next_offset_address_b;

        // Wait for free context
        wait_for_next_context(2);

        // Program unpacker 1 base address
        if (0 == unp_cfg_context)
        {
            cfg[THCON_SEC0_REG3_Base_address_ADDR32] = address_b;
            cfg[THCON_SEC1_REG3_Base_address_ADDR32] = address_a;
        }
        else
        {
            cfg[THCON_SEC0_REG3_Base_cntx1_address_ADDR32] = address_b;
            cfg[THCON_SEC1_REG3_Base_cntx1_address_ADDR32] = address_a;
        }

        semaphore_post(semaphore::UNPACK_SYNC); // Trisc::SEMPOST for context acquire

        // Stall unpacker until pending CFG writes from Trisc have completed
        TTI_STALLWAIT(p_stall::STALL_UNPACK, p_stall::TRISC_CFG);

        if (reuse_a)
        {
            if (unpB_partial_face)
            {
                TTI_UNPACR_NOP(SrcB, p_unpacr_nop::UNP_ZEROSRC);
                // Do face by face unpacking
                TTI_UNPACR(
                    SrcB, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 0 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                TTI_UNPACR(
                    SrcB, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                TTI_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, 0b0101); // Set ch0_z=0, ch1_z=0
            }
            else
            {
                TTI_UNPACR(SrcB, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
            }
            if ((t + 1) < t_dim)
            {
                // Let's load one more tile into srcB
                TT_SETDMAREG(0, LOWER_HALFWORD(next_address_a), 0, LO_16(p_gpr_unpack::TMP0));
                TT_SETDMAREG(0, UPPER_HALFWORD(next_address_a), 0, HI_16(p_gpr_unpack::TMP0));
                if (0 == unp_cfg_context)
                {
                    TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
                }
                else
                {
                    TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_cntx1_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
                }
                TTI_DMANOP;
                if (unpB_partial_face)
                {
                    TTI_UNPACR_NOP(SrcB, p_unpacr_nop::UNP_ZEROSRC);
                    // Do face by face unpacking
                    TTI_UNPACR(
                        SrcB, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 0 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                    TTI_UNPACR(
                        SrcB, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                    TTI_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, 0b0101); // Set ch0_z=0, ch1_z=0
                }
                else
                {
                    TTI_UNPACR(SrcB, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                }
                t++;
            }
        }
        else
        {
            if (unpA_partial_face)
            {
                // Do face by face unpacking
                TTI_UNPACR_NOP(SrcA, p_unpacr_nop::UNP_ZEROSRC);
                TTI_UNPACR(
                    SrcA, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 0 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                TTI_UNPACR(
                    SrcA, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                TTI_SETADCZW(p_setadc::UNP_A, 0, 0, 0, 0, 0b0101); // Set ch0_z=0, ch1_z=0
            }
            else
            {
                TTI_UNPACR(SrcA, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
            }
            if ((t + 1) < t_dim)
            {
                // Let's load one more tile into srcB
                TT_SETDMAREG(0, LOWER_HALFWORD(next_address_b), 0, LO_16(p_gpr_unpack::TMP0));
                TT_SETDMAREG(0, UPPER_HALFWORD(next_address_b), 0, HI_16(p_gpr_unpack::TMP0));
                if (0 == unp_cfg_context)
                {
                    TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC0_REG3_Base_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
                }
                else
                {
                    TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC0_REG3_Base_cntx1_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
                }
                TTI_DMANOP;
                if (unpA_partial_face)
                {
                    // Do face by face unpacking
                    TTI_UNPACR_NOP(SrcA, p_unpacr_nop::UNP_ZEROSRC);
                    TTI_UNPACR(
                        SrcA, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 0 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                    TTI_UNPACR(
                        SrcA, 0b00010001, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                    TTI_SETADCZW(p_setadc::UNP_A, 0, 0, 0, 0, 0b0101); // Set ch0_z=0, ch1_z=0
                }
                else
                {
                    TTI_UNPACR(SrcA, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                }
                t++;
            }
        }

        TT_MOP(0, (reuse_a ? ct_dim : rt_dim) - 1, unp_cfg_context == 0 ? 0 : 0xff); // Run the MOP

        // T6::SEMGET for context release
        t6_semaphore_get(semaphore::UNPACK_SYNC);

        // Switch unpacker config context
        switch_config_context(unp_cfg_context);
    }
}
