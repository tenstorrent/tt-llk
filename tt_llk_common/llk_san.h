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
extern hw_state_t hw_state;

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
static inline void unpack_hw_configure(
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
    unpack_hw_configure_impl<reconfig>(
        hw_state.unpack, dst_acc_en, src_fmt_A, src_fmt_B, dst_fmt_A, dst_fmt_B, face_height_A, face_height_B, num_faces_A, num_faces_B);
}

// State set + no hw config within kernel check
template <bool reconfig = false>
static inline void math_hw_configure(state_t<uint32_t> math_fmt_A, state_t<uint32_t> math_fmt_B)
{
    math_hw_configure_impl<reconfig>(hw_state.math, math_fmt_A, math_fmt_B);
}

// State set + no hw config within kernel check
template <bool reconfig = false>
static inline void pack_hw_configure(
    state_t<bool> dest_acc_en,
    state_t<uint32_t> src_fmt,
    state_t<uint32_t> dst_fmt,
    state_t<uint32_t> face_height,
    state_t<uint32_t> tile_width,
    state_t<uint32_t> num_faces,
    state_t<bool> partial_face,
    state_t<bool> narrow_tile)
{
    pack_hw_configure_impl<reconfig>(hw_state.pack, dest_acc_en, src_fmt, dst_fmt, face_height, tile_width, num_faces, partial_face, narrow_tile);
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
    unpack_operand_check_impl(hw_state.unpack, dst_acc_en, src_fmt_A, src_fmt_B, dst_fmt_A, dst_fmt_B, face_height_A, face_height_B, num_faces_A, num_faces_B);
}

// No state set, just check that non x arguments match the stored ones
static inline void math_operand_check(state_t<uint32_t> math_fmt_A, state_t<uint32_t> math_fmt_B)
{
    math_operand_check_impl(hw_state.math, math_fmt_A, math_fmt_B);
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
    pack_operand_check_impl(hw_state.pack, dest_acc_en, src_fmt, dst_fmt, face_height, tile_width, num_faces, partial_face, narrow_tile);
}

// Goes in LLK_LIB in Init
// Store operation type and push arguments to state stack
// sstanisic todo: implement init_impl
// template <llk_san_op op, typename... Ts>
// static inline void init(Ts... args)
// {
//     init_impl<op, Ts...>(unpack_state, op, args...);
// }

// Set must uninit flag for operation
// sstanisic todo: implement must_uninit_impl
// template <llk_san_op op>
// static inline void llk_san_must_uninit()
// {
//     must_uninit_impl<op>(unpack_state, op);
// }

// Goes in LLK_LIB in Execute
// Check operation type and arguments against stored ones
// sstanisic todo: implement operation_impl
// template <llk_san_op op, typename... Ts>
// static inline void llk_san_operation(Ts... args)
// {
//     llk_san_operation_impl<op, Ts...>(llk_san_state, op, args...);
// }

// Goes in LLK_LIB in Uninit
// Check operation type and clear must uninit flag
// sstanisic todo: implement uninit_impl
// template <llk_san_op op>
// static inline void llk_san_uninit()
// {
//     llk_san_uninit_impl<op>(llk_san_state, op);
// }

}; // namespace llk_san
