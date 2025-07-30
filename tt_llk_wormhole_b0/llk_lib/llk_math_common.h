// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file llk_math_common.h
 * @brief Core mathematical hardware configuration and synchronization functions for Tensix
 *
 * This header provides fundamental mathematical unit configuration and synchronization primitives
 * used across all LLK mathematical operations. These functions directly interface with the 
 * Tensix mathematical processing unit and must be called before any math operations.
 *
 * @note Based on PR analysis: Architecture differences between Wormhole and Blackhole require
 *       careful format configuration and synchronization patterns.
 */

#pragma once

#include <cstdint>

#include "ckernel_defs.h"
#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "cmath_common.h"

using namespace ckernel::math;

/**
 * @brief Configure Tensix mathematical hardware for source data formats
 *
 * Initializes the Tensix mathematical processing unit with the specified source data formats.
 * This function must be called before any mathematical operations to ensure proper data handling
 * and precision control.
 *
 * @tparam untilize_en Enable untilization support for data layout conversion
 * @tparam row_pool Enable row pooling optimization for reduction operations
 * 
 * @param srca_data_format Source A data format (DataFormat enum value)
 * @param srcb_data_format Source B data format (DataFormat enum value)
 *
 * @note Critical: This function configures ALU format registers and must complete before
 *       any mathematical operations. Uses STALLWAIT to ensure configuration is applied.
 * 
 * @warning Format mismatch between architectures can cause precision issues. See PR analysis
 *          showing format-related bugs in cross-platform operations.
 * 
 * @warning Row pool mode changes srcb format handling - use only for reduction operations
 */
template <bool untilize_en = false, bool row_pool = false>
inline void _llk_math_hw_configure_(const std::uint32_t srca_data_format, const std::uint32_t srcb_data_format)
{
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::MATH | p_stall::WAIT_SFPU);
    uint exp_width         = ((uint)srca_data_format >> 2) & 0x1; // 0=5-bit, 1=8-bit
    uint int8_math_enabled = ((uint)(srca_data_format & 0xF) == (uint)DataFormat::Int8) || ((uint)(srcb_data_format & 0xF) == (uint)DataFormat::Int8) ||
                             ((uint)srca_data_format == (uint)DataFormat::Int32) || ((uint)srcb_data_format == (uint)DataFormat::Int32);
    uint srcb_format = (row_pool ? ((uint)DataFormat::Float16 | (exp_width << 2)) : srcb_data_format);
    uint config_data = (srca_data_format << ALU_FORMAT_SPEC_REG0_SrcA_SHAMT) | (srcb_format << ALU_FORMAT_SPEC_REG1_SrcB_SHAMT) |
                       (int8_math_enabled << ALU_ACC_CTRL_INT8_math_enabled_SHAMT);
    constexpr uint config_mask = ALU_FORMAT_SPEC_REG0_SrcA_MASK | ALU_FORMAT_SPEC_REG1_SrcB_MASK | ALU_ACC_CTRL_INT8_math_enabled_MASK;
    cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_ADDR32, 0, config_mask>(config_data);
}

/**
 * @brief Wait for destination registers to become available for mathematical operations
 *
 * Synchronizes with the packer to ensure destination registers are available for writing.
 * This lightweight synchronization function should be called before mathematical operations
 * that write to destination registers.
 *
 * @tparam Dst Destination synchronization mode (SyncFull, SyncHalf)
 *
 * @note Synchronization mode should match the mathematical operation's requirements:
 *       - SyncFull: Complete tile synchronization
 *       - SyncHalf: Half-tile synchronization for optimized throughput
 * 
 * @warning Race conditions possible if sync mode doesn't match operation. Based on PR #96/97
 *          analysis showing packer thread synchronization issues.
 */
template <DstSync Dst>
inline void _llk_math_wait_for_dest_available_()
{
    // These lightweight functions for sync with packer imply
    // no mode change - entire epoch is either double buffer or single buffer
    math_dest_wait();
}

/**
 * @brief Signal completion of mathematical operation and manage destination buffers
 *
 * Notifies the packer that mathematical operations are complete and manages destination
 * buffer state. Handles buffer flipping for double-buffered operations and semaphore
 * coordination with downstream processing stages.
 *
 * @tparam Dst Destination synchronization mode (SyncFull, SyncHalf) 
 * @tparam is_fp32_dest_acc_en Enable FP32 destination accumulation mode
 *
 * @note Buffer management behavior:
 *       - SyncHalf: Flips destination section and resets tile index
 *       - SyncFull: Maintains current buffer state
 * 
 * @note FP32 accumulation affects precision and memory usage patterns
 * 
 * @warning Improper sync mode can cause data corruption. Ensure sync mode matches
 *          the mathematical operation's buffer usage pattern.
 */
template <DstSync Dst, bool is_fp32_dest_acc_en>
inline void _llk_math_dest_section_done_()
{
    set_math_semaphores();
    if constexpr (Dst == DstSync::SyncHalf)
    {
        math_sync_tile_dst_index = 0;
        dest_section_flip();
    }
}

/**
 * @brief Initialize synchronization between mathematical and packer units
 *
 * Sets up semaphore-based synchronization between the mathematical processing unit
 * and the packer. This initialization is required before any math-pack operation
 * sequences to ensure proper coordination.
 *
 * @tparam Dst Destination synchronization mode for pack operations
 * @tparam is_fp32_dest_acc_en Enable FP32 destination accumulation mode
 *
 * @note Must be called once per mathematical operation sequence before any
 *       _llk_math_wait_for_dest_available_ or _llk_math_dest_section_done_ calls
 * 
 * @note FP32 accumulation mode affects memory layout and synchronization timing
 *
 * @warning Initialization must complete before mathematical operations begin.
 *          Missing initialization can cause deadlocks or data corruption.
 */
template <DstSync Dst, bool is_fp32_dest_acc_en>
inline void _llk_math_pack_sync_init_()
{
    tensix_sync();
    while (semaphore_read(semaphore::MATH_PACK) > 0)
    {
    }; // Wait for previous packs to finish before claiming all dest
    if constexpr (Dst == DstSync::SyncFull)
    {
        TTI_SEMINIT(1, 0, p_stall::SEMAPHORE_1);
        reset_dest_offset_id();
        set_dest_section_base<StartZero>();
    }
    else
    {
        static_assert(Dst == DstSync::SyncHalf);
        TTI_SEMINIT(2, 0, p_stall::SEMAPHORE_1);
        reset_dest_offset_id();
        set_dest_section_base<StartZero>();
    }
}

/**
 * @brief Retrieve tile data from mailbox for mathematical processing
 *
 * Loads tile data from the mailbox system into the mathematical processing unit.
 * This function handles the data transfer coordination between the mailbox system
 * and the math unit, supporting both math and pack mailbox configurations.
 *
 * @tparam mail2math Enable mailbox-to-math data transfer pathway
 * @tparam mail2pack Enable mailbox-to-pack data transfer pathway
 * 
 * @param tile_index Index of the tile to retrieve from mailbox
 * @param p_tile Pointer to tile data structure for output
 *
 * @note Mailbox configuration affects data routing and must match the operation's
 *       data flow requirements. Both pathways can be enabled simultaneously.
 * 
 * @note Tile index must be valid within the current mailbox configuration range
 *
 * @warning Invalid tile index can cause data corruption or hardware exceptions.
 *          Ensure tile_index is within bounds before calling.
 * 
 * @warning Mailbox configuration must be established before tile retrieval.
 *          Missing configuration can cause deadlocks.
 */
template <bool mail2math = true, bool mail2pack = true>
inline void _llk_math_get_tile_(std::uint32_t tile_index, std::uint32_t* p_tile)
{
    constexpr uint32_t wait_sem = (mail2math && mail2pack) ? (2) : (1);
    while (semaphore_read(semaphore::UNPACK_OPERAND_SYNC) < wait_sem)
        ;
    if constexpr (mail2math)
    {
        *p_tile = mailbox_read(ThreadId::UnpackThreadId);
    }
    else
    {
        *p_tile = 0x0;
    }
}

template <bool mail2math = true, bool mail2pack = true>
inline void _llk_math_release_tile_()
{
    if constexpr (mail2math)
    {
        semaphore_get(semaphore::UNPACK_OPERAND_SYNC);
    }
}

inline void _llk_math_debug_dump_(std::uint8_t* data, std::uint32_t byte_size)
{
    debug_dump(data, byte_size);
}

inline void _llk_math_debug_dump_seek_(std::uint8_t offset)
{
    debug_dump_seek(offset);
}

template <bool is_fp32_dest_acc_en, bool to_from_int8 = false>
inline void _llk_math_reconfig_data_format_srca_(const std::uint32_t srca_data_format)
{
    if constexpr (to_from_int8)
    {
        static_assert(is_fp32_dest_acc_en, "Reconfiguring math to/from Int8 formats requires FP32 Dest mode enabled");
        TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::MATH | p_stall::WAIT_SFPU);
        uint int8_math_enabled     = ((uint)(srca_data_format & 0xF) == (uint)DataFormat::Int8) || ((uint)srca_data_format == (uint)DataFormat::Int32);
        uint config_data           = (srca_data_format << ALU_FORMAT_SPEC_REG0_SrcA_SHAMT) | (int8_math_enabled << ALU_ACC_CTRL_INT8_math_enabled_SHAMT);
        constexpr uint config_mask = ALU_FORMAT_SPEC_REG0_SrcA_MASK | ALU_ACC_CTRL_INT8_math_enabled_MASK;
        cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_ADDR32, 0, config_mask>(config_data);
    }
}

template <bool is_fp32_dest_acc_en, bool to_from_int8 = false>
inline void _llk_math_reconfig_data_format_srcb_(const std::uint32_t srcb_data_format)
{
    if constexpr (to_from_int8)
    {
        static_assert(is_fp32_dest_acc_en, "Reconfiguring math to/from Int8 formats requires FP32 Dest mode enabled");
        TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::MATH | p_stall::WAIT_SFPU);
        uint int8_math_enabled     = ((uint)(srcb_data_format & 0xF) == (uint)DataFormat::Int8) || ((uint)srcb_data_format == (uint)DataFormat::Int32);
        uint config_data           = (srcb_data_format << ALU_FORMAT_SPEC_REG1_SrcB_SHAMT) | (int8_math_enabled << ALU_ACC_CTRL_INT8_math_enabled_SHAMT);
        constexpr uint config_mask = ALU_FORMAT_SPEC_REG1_SrcB_MASK | ALU_ACC_CTRL_INT8_math_enabled_MASK;
        cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_ADDR32, 0, config_mask>(config_data);
    }
}

template <bool is_fp32_dest_acc_en, bool to_from_int8 = false>
inline void _llk_math_reconfig_data_format_(const std::uint32_t srca_data_format, const std::uint32_t srcb_data_format)
{
    if constexpr (to_from_int8)
    {
        static_assert(is_fp32_dest_acc_en, "Reconfiguring math to/from Int8 formats requires FP32 Dest mode enabled");
        TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::MATH | p_stall::WAIT_SFPU);
        uint int8_math_enabled = ((uint)(srca_data_format & 0xF) == (uint)DataFormat::Int8) || ((uint)(srcb_data_format & 0xF) == (uint)DataFormat::Int8) ||
                                 ((uint)srca_data_format == (uint)DataFormat::Int32) || ((uint)srcb_data_format == (uint)DataFormat::Int32);
        uint config_data = (srca_data_format << ALU_FORMAT_SPEC_REG0_SrcA_SHAMT) | (srcb_data_format << ALU_FORMAT_SPEC_REG1_SrcB_SHAMT) |
                           (int8_math_enabled << ALU_ACC_CTRL_INT8_math_enabled_SHAMT);
        constexpr uint config_mask = ALU_FORMAT_SPEC_REG0_SrcA_MASK | ALU_FORMAT_SPEC_REG1_SrcB_MASK | ALU_ACC_CTRL_INT8_math_enabled_MASK;
        cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_ADDR32, 0, config_mask>(config_data);
    }
}

inline std::uint32_t _llk_math_get_compute_special_value_flags_()
{
    return reg_read(RISCV_DEBUG_REG_FPU_STICKY_BITS);
}

inline void _llk_math_clear_compute_special_value_flags_()
{
    reg_write(RISCV_DEBUG_REG_FPU_STICKY_BITS, 0);
}
