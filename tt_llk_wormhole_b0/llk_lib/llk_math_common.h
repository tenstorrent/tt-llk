// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_defs.h"
#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "cmath_common.h"
#include "llk_static_asserts.h"

using namespace ckernel::math;

template <bool untilize_en = false, bool row_pool = false>
inline void _llk_math_hw_configure_(const std::uint32_t srca_data_format, const std::uint32_t srcb_data_format)
{
    // Validate compile-time template parameters
    static_assert(
        std::is_same_v<decltype(untilize_en), bool> && std::is_same_v<decltype(row_pool), bool>,
        "CRITICAL: untilize_en and row_pool must be boolean compile-time parameters");
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

template <DstSync Dst>
inline void _llk_math_wait_for_dest_available_()
{
    // Validate DstSync parameter at compile time
    static_assert(Dst == DstSync::SyncHalf || Dst == DstSync::SyncFull, "CRITICAL: DstSync must be SyncHalf or SyncFull for proper synchronization");
    // These lightweight functions for sync with packer imply
    // no mode change - entire epoch is either double buffer or single buffer
    math_dest_wait();
}

template <DstSync Dst, bool is_fp32_dest_acc_en>
inline void _llk_math_dest_section_done_()
{
    // Validate template parameters at compile time
    static_assert(Dst == DstSync::SyncHalf || Dst == DstSync::SyncFull, "CRITICAL: DstSync must be SyncHalf or SyncFull for proper synchronization");
    static_assert(std::is_same_v<decltype(is_fp32_dest_acc_en), bool>, "CRITICAL: is_fp32_dest_acc_en must be a boolean compile-time parameter");
    set_math_semaphores();
    if constexpr (Dst == DstSync::SyncHalf)
    {
        math_sync_tile_dst_index = 0;
        dest_section_flip();
    }
}

template <DstSync Dst, bool is_fp32_dest_acc_en>
inline void _llk_math_pack_sync_init_()
{
    // Validate template parameters at compile time
    static_assert(Dst == DstSync::SyncHalf || Dst == DstSync::SyncFull, "CRITICAL: DstSync must be SyncHalf or SyncFull for proper synchronization");
    static_assert(std::is_same_v<decltype(is_fp32_dest_acc_en), bool>, "CRITICAL: is_fp32_dest_acc_en must be a boolean compile-time parameter");
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

template <bool mail2math = true, bool mail2pack = true>
inline void _llk_math_get_tile_(std::uint32_t tile_index, std::uint32_t* p_tile)
{
    // Validate mailbox communication parameters
    static_assert(
        std::is_same_v<decltype(mail2math), bool> && std::is_same_v<decltype(mail2pack), bool>,
        "CRITICAL: mail2math and mail2pack must be boolean compile-time parameters");
    static_assert(mail2math || mail2pack, "CRITICAL: At least one of mail2math or mail2pack must be true for valid communication");
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
    // Validate mailbox communication parameters
    static_assert(
        std::is_same_v<decltype(mail2math), bool> && std::is_same_v<decltype(mail2pack), bool>,
        "CRITICAL: mail2math and mail2pack must be boolean compile-time parameters");
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
    // Validate template parameters at compile time
    static_assert(
        std::is_same_v<decltype(is_fp32_dest_acc_en), bool> && std::is_same_v<decltype(to_from_int8), bool>,
        "CRITICAL: Boolean template parameters must be compile-time booleans");

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
    // Validate template parameters at compile time
    static_assert(
        std::is_same_v<decltype(is_fp32_dest_acc_en), bool> && std::is_same_v<decltype(to_from_int8), bool>,
        "CRITICAL: Boolean template parameters must be compile-time booleans");

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
    // Validate template parameters at compile time
    static_assert(
        std::is_same_v<decltype(is_fp32_dest_acc_en), bool> && std::is_same_v<decltype(to_from_int8), bool>,
        "CRITICAL: Boolean template parameters must be compile-time booleans");

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
