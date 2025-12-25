// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstring>
#include <limits>
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
    LLK_PANIC(!reconfig && state.is_configured, "panic: llk_san: user tried to configure packer twice");
    LLK_PANIC(reconfig && !state.is_configured, "panic: llk_san: user tried to reconfigure packer before configuring it");

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

template <typename... Ts>
constexpr uint8_t _args_count()
{
    static_assert((sizeof...(Ts) <= std::numeric_limits<uint8_t>::max()), "llk_san: fault: argument count can't fit in uint8_t");

    return static_cast<uint8_t>(sizeof...(Ts));
}

template <typename... Ts>
constexpr std::array<uint8_t, sizeof...(Ts)> _args_sizeof()
{
    static_assert(((sizeof(Ts) <= std::numeric_limits<uint8_t>::max()) && ...), "llk_san: fault: sizeof can't fit in uint8_t");

    return {static_cast<uint8_t>(sizeof(Ts))...};
}

template <typename... Ts>
static inline constexpr std::array<uint8_t, sizeof...(Ts)> _args_alignof()
{
    static_assert(((alignof(Ts) <= std::numeric_limits<uint8_t>::max()) && ...), "llk_san: fault: alignof can't fit in uint8_t");

    return {static_cast<uint8_t>(alignof(Ts))...};
}

template <size_t N>
static inline constexpr std::array<size_t, N + 1> _args_offsetof(const std::array<uint8_t, N>& args_sizeof, const std::array<uint8_t, N>& args_alignof)
{
    std::array<size_t, N + 1> args_offset = {};

    args_offset[0] = 0;
    for (size_t i = 1; i < N; i++)
    {
        size_t align   = args_alignof[i];
        size_t end     = args_offset[i - 1] + args_sizeof[i - 1];
        size_t padding = (align - end % align) % align;
        args_offset[i] = end + padding;
    }

    constexpr size_t max_align = alignof(max_align_t);
    size_t final_end           = args_offset[N - 1] + args_sizeof[N - 1];
    size_t final_padding       = (max_align - final_end % max_align) % max_align;
    args_offset[N]             = final_end + final_padding;

    return args_offset;
}

// Goes in LLK_LIB in Init
// Store operation type and push arguments to state stack
template <operation_t op, typename... Ts>
static inline void operation_save_impl(operation_state_t& state, Ts... args)
{
    state.operation = op;

    constexpr uint8_t args_count = _args_count<Ts...>();

    constexpr std::array<uint8_t, args_count> args_sizeof      = _args_sizeof<Ts...>();
    constexpr std::array<uint8_t, args_count> args_alignof     = _args_alignof<Ts...>();
    constexpr std::array<size_t, args_count + 1> args_offsetof = _args_offsetof(args_sizeof, args_alignof);

    constexpr size_t entry_size = sizeof(args_count) + args_count * sizeof(args_sizeof[0]) + args_count * sizeof(args_alignof[0]) + args_offsetof[args_count];

    static_assert(entry_size <= operation_state_t::BUFFER_SIZE, "llk_san: operation entry will overflow the buffer");

    // | ARG_COUNT | SIZEOF(args[0]) ... | ALIGNOF(args[1]) ... | args[0] PADDING ... |

    char* ptr = state.buffer;

    memcpy(ptr, &args_count, sizeof(args_count));
    ptr += sizeof(args_count);

    if constexpr (args_count > 0)
    {
        memcpy(ptr, args_sizeof.data(), args_count * sizeof(args_sizeof[0]));
        ptr += args_count * sizeof(args_sizeof[0]);

        memcpy(ptr, args_alignof.data(), args_count * sizeof(args_alignof[0]));
        ptr += args_count * sizeof(args_alignof[0]);
    }

    constexpr size_t max_align = alignof(max_align_t);
    size_t padding             = (max_align - reinterpret_cast<uintptr_t>(ptr) % max_align) % max_align;
    ptr += padding;

    size_t i = 0;
    (memcpy(ptr + args_offsetof[i++], &args, sizeof(args)), ...);
}

// Goes in LLK_LIB in Execute
// Check operation type and arguments against stored ones
template <operation_t op, typename... Ts>
static inline void operation_check_impl(operation_state_t& state, Ts... args)
{
    LLK_PANIC(state.operation != op, "llk_san: fault: operation type doesn't match stored operation");

    constexpr uint8_t args_count = _args_count<Ts...>();

    constexpr std::array<uint8_t, args_count> args_sizeof      = _args_sizeof<Ts...>();
    constexpr std::array<uint8_t, args_count> args_alignof     = _args_alignof<Ts...>();
    constexpr std::array<size_t, args_count + 1> args_offsetof = _args_offsetof(args_sizeof, args_alignof);

    constexpr size_t entry_size = sizeof(args_count) + args_count * sizeof(args_sizeof[0]) + args_count * sizeof(args_alignof[0]) + args_offsetof[args_count];

    static_assert(entry_size <= operation_state_t::BUFFER_SIZE, "llk_san: fault: operation entry will overflow the buffer");

    // | ARG_COUNT | SIZEOF(args[0]) ... | ALIGNOF(args[1]) ... | args[0] PADDING ... |

    char* ptr = state.buffer;

    LLK_ASSERT(memcmp(&args_count, ptr, sizeof(args_count)) == 0, "llk_san: fault: saved vs provided args_count mismatch");
    ptr += sizeof(args_count);

    if constexpr (args_count > 0)
    {
        LLK_ASSERT(memcmp(args_sizeof.data(), ptr, args_count * sizeof(args_sizeof[0])) == 0, "llk_san: fault: saved vs provided args_sizeof mismatch");
        ptr += args_count * sizeof(args_sizeof[0]);

        LLK_ASSERT(memcmp(args_alignof.data(), ptr, args_count * sizeof(args_alignof[0])) == 0, "llk_san: fault: saved vs provided args_sizeof mismatch");
        ptr += args_count * sizeof(args_alignof[0]);
    }

    constexpr size_t max_align = alignof(max_align_t);
    size_t padding             = (max_align - reinterpret_cast<uintptr_t>(ptr) % max_align) % max_align;
    ptr += padding;

    // sstanisic fixme: remove the awful lambda
    size_t i = 0;
    ([&] { LLK_ASSERT(memcmp(ptr + args_offsetof[i++], &args, sizeof(args)) == 0, "llk_san: panic: saved vs provided args mismatch"); }(), ...);
}

// Goes in LLK_LIB in Uninit
// Check operation type and clear must uninit flag
// sstanisic todo: implement uninit_impl
// template <llk_san_op op>
// static inline void uninit_impl()
// {
//     LLK_ASSERT(false, "not implemented");
// }

} // namespace llk_san
