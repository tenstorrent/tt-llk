// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

#include "llk_assert.h"
#include "llk_san_types.h"

namespace llk_san
{

// Goes in ComputeAPI
// State set only
// sstanisic todo: implement support_backtrace_impl
// static inline void support_backtrace_impl(std::string function_name)
// {
//     LLK_ASSERT(false, "not implemented");
// }

// Goes in LLK_API
// State set only
// sstanisic todo: implement support_globals_impl
// static inline void support_globals_impl(bool dst_acc_mode, DstSync dst_sync, bool approx, std::int32_t math_fidelity)
// {
//     LLK_ASSERT(false, "not implemented");
// }

// // State set only
// sstanisic todo: implement support_operand_impl
// template <llk_san_operand_t operand>
// static inline void llk_san_support_operand_impl(
//     llk_san_state_t& llk_san_state,
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
//     LLK_SAN_PANIC(false, "not implemented");
// }

// Goes in LLK_LIB in HWConfigure and HWReconfig
// State set + no hw config within kernel check
template <bool reconfig>
static inline void unpack_hw_configure_impl(
    unpack_state_t& state,
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
    LLK_PANIC(!reconfig && state.is_configured, "panic: llk_san: user tried to configure unpacker twice");
    LLK_PANIC(reconfig && !state.is_configured, "panic: llk_san: user tried to reconfigure unpacker before configuring it");

    unpack_src_state_t& src_a = state.src_a;

    src_a.input_format  = src_fmt_A;
    src_a.output_format = dst_fmt_A;
    src_a.face_height   = face_height_A;
    src_a.num_faces     = num_faces_A;

    unpack_src_state_t& src_b = state.src_b;

    src_b.input_format  = src_fmt_B;
    src_b.output_format = dst_fmt_B;
    src_b.face_height   = face_height_B;
    src_b.num_faces     = num_faces_B;

    state.dest_width_32 = dst_acc_en;
    state.is_configured = true;
}

// State set + no hw config within kernel check
template <bool reconfig = false>
static inline void math_hw_configure_impl(math_state_t& state, state_t<uint32_t> math_fmt_A, state_t<uint32_t> math_fmt_B)
{
    LLK_PANIC(!reconfig && state.is_configured, "panic: llk_san: user tried to configure math twice");
    LLK_PANIC(reconfig && !state.is_configured, "panic: llk_san: user tried to reconfigure math before configuring it");

    state.src_a.input_format = math_fmt_A;
    state.src_b.input_format = math_fmt_B;
    state.is_configured      = true;
}

// State set + no hw config within kernel check
template <bool reconfig>
static inline void pack_hw_configure_impl(
    pack_state_t& state,
    state_t<bool> dest_acc_en,
    state_t<uint32_t> src_fmt,
    state_t<uint32_t> dst_fmt,
    state_t<uint32_t> face_height,
    state_t<uint32_t> tile_width,
    state_t<uint32_t> num_faces,
    state_t<bool> partial_face,
    state_t<bool> narrow_tile)
{
    LLK_PANIC(reconfig && !state.is_configured, "panic: llk_san: user tried to reconfigure packer before configuring it");
    LLK_PANIC(!reconfig && state.is_configured, "panic: llk_san: user tried to configure packer twice");

    state.input_format  = src_fmt;
    state.output_format = dst_fmt;
    state.face_height   = face_height;
    state.tile_width    = tile_width;
    state.num_faces     = num_faces;
    state.partial_face  = partial_face;
    state.narrow_tile   = narrow_tile;
    state.dest_width_32 = dest_acc_en;
    state.is_configured = true;
}

// Goes in LLK_LIB in Init, Execute and Uninit
// No state set, just check that non x arguments match the stored ones
static inline void unpack_operand_check_impl(
    unpack_state_t& state,
    state_t<bool> dest_acc_en,
    state_t<uint32_t> src_fmt_A,
    state_t<uint32_t> src_fmt_B,
    state_t<uint32_t> dst_fmt_A,
    state_t<uint32_t> dst_fmt_B,
    state_t<uint32_t> face_height_A,
    state_t<uint32_t> face_height_B,
    state_t<uint32_t> num_faces_A,
    state_t<uint32_t> num_faces_B)
{
    LLK_PANIC(!state.is_configured, "panic: llk_san: executing init/execute/uninit before hwconfigure");

    LLK_SAN_ASSERT(state.dest_width_32, dest_acc_en, "panic: llk_san: dest_acc_en doesn't match state.dest_width_32");

    LLK_SAN_ASSERT(state.src_a.input_format, src_fmt_A, "panic: llk_san: src_fmt_A doesn't match state.src_a.input_format");
    LLK_SAN_ASSERT(state.src_b.input_format, src_fmt_B, "panic: llk_san: src_fmt_B doesn't match state.src_b.input_format");
    LLK_SAN_ASSERT(state.src_a.output_format, dst_fmt_A, "panic: llk_san: dst_fmt_A doesn't match state.src_a.output_format");
    LLK_SAN_ASSERT(state.src_b.output_format, dst_fmt_B, "panic: llk_san: dst_fmt_B doesn't match state.src_b.output_format");
    LLK_SAN_ASSERT(state.src_a.face_height, face_height_A, "panic: llk_san: face_height_A doesn't match state.src_a.face_height");
    LLK_SAN_ASSERT(state.src_b.face_height, face_height_B, "panic: llk_san: face_height_B doesn't match state.src_b.face_height");
    LLK_SAN_ASSERT(state.src_a.num_faces, num_faces_A, "panic: llk_san: num_faces_A doesn't match state.src_a.num_faces");
    LLK_SAN_ASSERT(state.src_b.num_faces, num_faces_B, "panic: llk_san: num_faces_B doesn't match state.src_b.num_faces");
}

// No state set, just check that non x arguments match the stored ones
static inline void math_operand_check_impl(math_state_t& state, state_t<uint32_t> math_fmt_A, state_t<uint32_t> math_fmt_B)
{
    LLK_PANIC(!state.is_configured, "panic: llk_san: executing init/execute/uninit before hwconfigure");
    LLK_SAN_ASSERT(state.src_a.input_format, math_fmt_A, "panic: llk_san: math_fmt_A doesn't match state.src_a.input_format");
    LLK_SAN_ASSERT(state.src_b.input_format, math_fmt_B, "panic: llk_san: math_fmt_B doesn't match state.src_b.input_format");
}

// No state set, just check that non x arguments match the stored ones
static inline void pack_operand_check_impl(
    pack_state_t& state,
    state_t<bool> dest_acc_en,
    state_t<uint32_t> src_fmt,
    state_t<uint32_t> dst_fmt,
    [[maybe_unused]] state_t<uint32_t> face_height,
    state_t<uint32_t> tile_width,
    state_t<uint32_t> num_faces,
    state_t<bool> partial_face,
    state_t<bool> narrow_tile)
{
    LLK_PANIC(!state.is_configured, "panic: llk_san: executing init/execute/uninit before hwconfigure");
    LLK_SAN_ASSERT(state.dest_width_32, dest_acc_en, "panic: llk_san: dest_acc_en doesn't match state.dest_width_32");
    LLK_SAN_ASSERT(state.input_format, src_fmt, "panic: llk_san: src_fmt doesn't match state.input_format");
    LLK_SAN_ASSERT(state.output_format, dst_fmt, "panic: llk_san: dst_fmt doesn't match state.output_format");
    // sstanisic fixme: LLK_SAN_ASSERT(state.face_height, face_height, "panic: llk_san: face_height doesn't match state.face_height");
    LLK_SAN_ASSERT(state.tile_width, tile_width, "panic: llk_san: tile_width doesn't match state.tile_width");
    LLK_SAN_ASSERT(state.num_faces, num_faces, "panic: llk_san: num_faces doesn't match state.num_faces");
    LLK_SAN_ASSERT(state.partial_face, partial_face, "panic: llk_san: partial_face doesn't match state.partial_face");
    LLK_SAN_ASSERT(state.narrow_tile, narrow_tile, "panic: llk_san: narrow_tile doesn't match state.narrow_tile");
}

// Goes in LLK_LIB in Init
// Store operation type and push arguments to state stack
// sstanisic todo: implement init_impl
// static inline void init_impl(Ts... args)
// {
//     LLK_ASSERT(false, "not implemented");
// }

// Goes in LLK_LIB in Execute
// Check operation type and arguments against stored ones
// sstanisic todo: implement operation_impl
// template <llk_san_op op, typename... Ts>
// static inline void operation_impl(Ts... args)
// {
//     LLK_ASSERT(false, "not implemented");
// }

// Goes in LLK_LIB in Uninit
// Check operation type and clear must uninit flag
// sstanisic todo: implement uninit_impl
// template <llk_san_op op>
// static inline void uninit_impl()
// {
//     LLK_ASSERT(false, "not implemented");
// }

} // namespace llk_san
