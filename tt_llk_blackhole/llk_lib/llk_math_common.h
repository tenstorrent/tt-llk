// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_defs.h"
#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "cmath_common.h"

using namespace ckernel::math;

template <bool untilize_en = false, bool skip_inputs = false>
inline void _llk_math_hw_configure_(const std::uint32_t srca_data_format, const std::uint32_t srcb_data_format)
{
    // Need to wait for all DEST accesses to be finished before changing
    // remap_addrs and swizzle_32b bits
    tensix_sync();
    while (semaphore_read(semaphore::MATH_PACK) > 0)
    {
    }; // Wait for previous packs to finish before claiming all dest

    // Untilize mode needs dest read access with a stride of 16
    // Following bits are needed for enabling stride of 16
    cfg_reg_rmw_tensix<DEST_ACCESS_CFG_remap_addrs_RMW>(untilize_en);
    cfg_reg_rmw_tensix<DEST_ACCESS_CFG_swizzle_32b_RMW>(untilize_en);

    // Legacy mode for ZEROACC
    cfg_reg_rmw_tensix<DEST_ACCESS_CFG_zeroacc_absolute_tile_mode_RMW>(1);

    if constexpr (skip_inputs == false)
    {
        TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::MATH);
        uint int8_math_enabled = ((uint)(srca_data_format & 0xF) == (uint)DataFormat::Int8) || ((uint)(srcb_data_format & 0xF) == (uint)DataFormat::Int8) ||
                                 ((uint)srca_data_format == (uint)DataFormat::Int32) || ((uint)srcb_data_format == (uint)DataFormat::Int32);
        cfg_reg_rmw_tensix<ALU_ACC_CTRL_INT8_math_enabled_RMW>(int8_math_enabled);
    }
}

template <DstSync Dst>
inline void _llk_math_wait_for_dest_available_()
{
    // These lightweight functions for sync with packer imply
    // no mode change - entire epoch is either double buffer or single buffer
    math_dest_wait();
}

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

template <bool mail2math = true, bool mail2pack = true>
inline void _llk_math_get_tile_(std::uint32_t tile_index, std::uint32_t* p_tile)
{
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

// Following functions do not need to program ALU_FORMAT_SPEC_REG0_SrcA/ALU_FORMAT_SPEC_REG1_SrcB
// for blackhole since ALU format is inferred
template <bool is_fp32_dest_acc_en, bool to_from_int8 = false>
inline void _llk_math_reconfig_data_format_srca_(const std::uint32_t srca_data_format)
{
    if constexpr (to_from_int8)
    {
        static_assert(is_fp32_dest_acc_en, "Reconfiguring math to/from Int8 formats requires FP32 Dest mode enabled");
        TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::MATH);
        uint int8_math_enabled = ((uint)(srca_data_format & 0xF) == (uint)DataFormat::Int8) || ((uint)srca_data_format == (uint)DataFormat::Int32);
        cfg_reg_rmw_tensix<ALU_ACC_CTRL_INT8_math_enabled_RMW>(int8_math_enabled);
    }
}

template <bool is_fp32_dest_acc_en, bool to_from_int8 = false>
inline void _llk_math_reconfig_data_format_srcb_(const std::uint32_t srcb_data_format)
{
    if constexpr (to_from_int8)
    {
        static_assert(is_fp32_dest_acc_en, "Reconfiguring math to/from Int8 formats requires FP32 Dest mode enabled");
        TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::MATH);
        uint int8_math_enabled = ((uint)(srcb_data_format & 0xF) == (uint)DataFormat::Int8) || ((uint)srcb_data_format == (uint)DataFormat::Int32);
        cfg_reg_rmw_tensix<ALU_ACC_CTRL_INT8_math_enabled_RMW>(int8_math_enabled);
    }
}

template <bool is_fp32_dest_acc_en, bool to_from_int8 = false>
inline void _llk_math_reconfig_data_format_(const std::uint32_t srca_data_format, const std::uint32_t srcb_data_format)
{
    if constexpr (to_from_int8)
    {
        static_assert(is_fp32_dest_acc_en, "Reconfiguring math to/from Int8 formats requires FP32 Dest mode enabled");
        TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::MATH);
        uint int8_math_enabled = ((uint)(srca_data_format & 0xF) == (uint)DataFormat::Int8) || ((uint)(srcb_data_format & 0xF) == (uint)DataFormat::Int8) ||
                                 ((uint)srca_data_format == (uint)DataFormat::Int32) || ((uint)srcb_data_format == (uint)DataFormat::Int32);
        cfg_reg_rmw_tensix<ALU_ACC_CTRL_INT8_math_enabled_RMW>(int8_math_enabled);
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
