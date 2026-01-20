// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

#include "llk_san_extended.h"
#include "llk_san_impl.h"

namespace llk_san
{

// per thread state
extern sanitizer_state_t* const sanitizer;

// Goes in ComputeAPI
// State set only
// sstanisic todo: implement support_backtrace_impl
// static inline void support_backtrace(std::string function_name)
// {
//     support_backtrace_impl(function_name);
// }

// Goes in LLK_API
// State set only
// sstanisic todo: implement support_globals_impl
// static inline void support_globals(bool dst_acc_mode, DstSync dst_sync, bool approx, std::int32_t math_fidelity)
// {
//     support_globals_impl(dst_acc_mode, dst_sync, approx, math_fidelity);
// }

// State set only
// sstanisic todo: implement support_operand_impl
// template <llk_san_operand operand>
// static inline void llk_san_support_operand(
//     std::int32_t src_fmt,
//     std::int32_t dst_fmt,
//     std::int32_t num_faces,
//     std::int32_t partial_face,
//     std::int32_t face_r_dim,
//     std::int32_t narrow_tile,
//     std::int32_t tile_r_dim,
//     std::int32_t tile_c_dim,
//     std::int32_t tile_size,
//     std::int32_t page_size)
// {
//     support_operand_impl<operand>(
//         src_fmt, dst_fmt, num_faces, partial_face, face_r_dim, narrow_tile, tile_r_dim, tile_c_dim, tile_size, page_size);
// }

// Goes in LLK_LIB in HWConfigure and HWReconfig
// State set + no hw config within kernel check
template <bool reconfig = false>
static inline void unpack_operand_configure(
    state_t<bool> dst_acc_en,
    state_t<uint32_t> src_fmt_A,
    state_t<uint32_t> src_fmt_B,
    state_t<uint32_t> dst_fmt_A,
    state_t<uint32_t> dst_fmt_B,
    state_t<uint32_t> face_height_A,
    state_t<uint32_t> face_height_B,
    state_t<uint32_t> num_faces_A,
    state_t<uint32_t> num_faces_B)
{
    if constexpr (!reconfig)
    {
        fsm_advance_impl<fsm_state_t::CONFIGURED>(sanitizer->fsm[COMPILE_FOR_TRISC], sanitizer->operation[COMPILE_FOR_TRISC]);
    }
    else
    {
        fsm_advance_impl<fsm_state_t::RECONFIGURED>(sanitizer->fsm[COMPILE_FOR_TRISC], sanitizer->operation[COMPILE_FOR_TRISC]);
    }

    unpack_operand_configure_impl<reconfig>(
        sanitizer->operand.unpack, dst_acc_en, src_fmt_A, src_fmt_B, dst_fmt_A, dst_fmt_B, face_height_A, face_height_B, num_faces_A, num_faces_B);
}

// State set + no hw config within kernel check
template <bool reconfig = false>
static inline void math_operand_configure(state_t<uint32_t> math_fmt_A, state_t<uint32_t> math_fmt_B)
{
    if constexpr (!reconfig)
    {
        fsm_advance_impl<fsm_state_t::CONFIGURED>(sanitizer->fsm[COMPILE_FOR_TRISC], sanitizer->operation[COMPILE_FOR_TRISC]);
    }
    else
    {
        fsm_advance_impl<fsm_state_t::RECONFIGURED>(sanitizer->fsm[COMPILE_FOR_TRISC], sanitizer->operation[COMPILE_FOR_TRISC]);
    }

    math_operand_configure_impl<reconfig>(sanitizer->operand.math, math_fmt_A, math_fmt_B);
}

// State set + no hw config within kernel check
template <bool reconfig = false>
static inline void pack_operand_configure(
    state_t<bool> dest_acc_en,
    state_t<uint32_t> src_fmt,
    state_t<uint32_t> dst_fmt,
    state_t<uint32_t> face_height,
    state_t<uint32_t> tile_width,
    state_t<uint32_t> num_faces,
    state_t<bool> partial_face,
    state_t<bool> narrow_tile)
{
    if constexpr (!reconfig)
    {
        fsm_advance_impl<fsm_state_t::CONFIGURED>(sanitizer->fsm[COMPILE_FOR_TRISC], sanitizer->operation[COMPILE_FOR_TRISC]);
    }
    else
    {
        fsm_advance_impl<fsm_state_t::RECONFIGURED>(sanitizer->fsm[COMPILE_FOR_TRISC], sanitizer->operation[COMPILE_FOR_TRISC]);
    }

    pack_operand_configure_impl<reconfig>(
        sanitizer->operand.pack, dest_acc_en, src_fmt, dst_fmt, face_height, tile_width, num_faces, partial_face, narrow_tile);
}

// Goes in LLK_LIB in Init, Execute and Uninit
// No state set, just check that non x arguments match the stored ones
static inline void unpack_operand_check(
    state_t<bool> dst_acc_en,
    state_t<uint32_t> src_fmt_A,
    state_t<uint32_t> src_fmt_B,
    state_t<uint32_t> dst_fmt_A,
    state_t<uint32_t> dst_fmt_B,
    state_t<uint32_t> face_height_A,
    state_t<uint32_t> face_height_B,
    state_t<uint32_t> num_faces_A,
    state_t<uint32_t> num_faces_B)
{
    unpack_operand_check_impl(
        sanitizer->operand.unpack, dst_acc_en, src_fmt_A, src_fmt_B, dst_fmt_A, dst_fmt_B, face_height_A, face_height_B, num_faces_A, num_faces_B);
}

// No state set, just check that non x arguments match the stored ones
static inline void math_operand_check(state_t<uint32_t> math_fmt_A, state_t<uint32_t> math_fmt_B)
{
    math_operand_check_impl(sanitizer->operand.math, math_fmt_A, math_fmt_B);
}

// No state set, just check that non x arguments match the stored ones
static inline void pack_operand_check(
    state_t<bool> dest_acc_en,
    state_t<uint32_t> src_fmt,
    state_t<uint32_t> dst_fmt,
    state_t<uint32_t> face_height,
    state_t<uint32_t> tile_width,
    state_t<uint32_t> num_faces,
    state_t<bool> partial_face,
    state_t<bool> narrow_tile)
{
    pack_operand_check_impl(sanitizer->operand.pack, dest_acc_en, src_fmt, dst_fmt, face_height, tile_width, num_faces, partial_face, narrow_tile);
}

// Goes in LLK_LIB in Init
// Store operation type and save arguments
template <operation_t op, typename... Ts>
static inline void operation_init(Ts... args)
{
    fsm_advance_impl<fsm_state_t::INITIALIZED>(sanitizer->fsm[COMPILE_FOR_TRISC], sanitizer->operation[COMPILE_FOR_TRISC]);

    operation_init_impl<op, Ts...>(sanitizer->operation[COMPILE_FOR_TRISC], args...);
}

// Goes in LLK_LIB in Execute
// Check operation type and arguments against stored ones
template <operation_t op, typename... Ts>
static inline void operation_check(Ts... args)
{
    fsm_advance_impl<fsm_state_t::EXECUTED>(sanitizer->fsm[COMPILE_FOR_TRISC], sanitizer->operation[COMPILE_FOR_TRISC]);
    operation_check_impl<op, Ts...>(sanitizer->operation[COMPILE_FOR_TRISC], args...);
}

// Goes in LLK_LIB in Uninit
// Check operation type and clear must uninit flag
template <operation_t op>
void operation_uninit()
{
    fsm_advance_impl<fsm_state_t::UNINITIALIZED>(sanitizer->fsm[COMPILE_FOR_TRISC], sanitizer->operation[COMPILE_FOR_TRISC]);
    operation_uninit_impl<op>(sanitizer->operation[COMPILE_FOR_TRISC]);
}

} // namespace llk_san
