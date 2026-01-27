// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstring>
#include <limits>
#include <string>

#include "llk_san_output.h"
#include "llk_san_types.h"

namespace llk_san
{

// Goes in ComputeAPI
// State set only
// sstanisic todo: implement support_backtrace_impl
// static inline void support_backtrace_impl(std::string function_name)
// {
//     LLK_SAN_ASSERT(false, "not implemented");
// }

// Goes in LLK_API
// State set only
// sstanisic todo: implement support_globals_impl
// static inline void support_globals_impl(bool dst_acc_mode, DstSync dst_sync, bool approx, std::int32_t math_fidelity)
// {
//     LLK_SAN_ASSERT(false, "not implemented");
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
//     LLK_SAN_OP_PANIC(false, "not implemented");
// }

// Goes in LLK_LIB in HWConfigure and HWReconfig
// State set + no hw config within kernel check
template <bool reconfig>
static inline void unpack_operand_configure_impl(
    unpack_operand_state_t& state,
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
static inline void math_operand_configure_impl(math_operand_state_t& state, state_t<uint32_t> math_fmt_A, state_t<uint32_t> math_fmt_B)
{
    state.src_a.input_format = math_fmt_A;
    state.src_b.input_format = math_fmt_B;
    state.is_configured      = true;
}

// State set + no hw config within kernel check
template <bool reconfig>
static inline void pack_operand_configure_impl(
    pack_operand_state_t& state,
    state_t<bool> dest_acc_en,
    state_t<uint32_t> src_fmt,
    state_t<uint32_t> dst_fmt,
    state_t<uint32_t> face_height,
    state_t<uint32_t> tile_width,
    state_t<uint32_t> num_faces,
    state_t<bool> partial_face,
    state_t<bool> narrow_tile)
{
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
    unpack_operand_state_t& state,
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
    LLK_SAN_OP_ASSERT(state.dest_width_32, dest_acc_en, "UODW");       // panic: dest_acc_en doesn't match state.dest_width_32
    LLK_SAN_OP_ASSERT(state.src_a.input_format, src_fmt_A, "UOAI");    // panic: src_fmt_A doesn't match state.src_a.input_format
    LLK_SAN_OP_ASSERT(state.src_b.input_format, src_fmt_B, "UOBI");    // panic: src_fmt_B doesn't match state.src_b.input_format
    LLK_SAN_OP_ASSERT(state.src_a.output_format, dst_fmt_A, "UOAO");   // panic: dst_fmt_A doesn't match state.src_a.output_format
    LLK_SAN_OP_ASSERT(state.src_b.output_format, dst_fmt_B, "UOBO");   // panic: dst_fmt_B doesn't match state.src_b.output_format
    LLK_SAN_OP_ASSERT(state.src_a.face_height, face_height_A, "UOFH"); // panic: face_height_A doesn't match state.src_a.face_height
    LLK_SAN_OP_ASSERT(state.src_b.face_height, face_height_B, "UOFH"); // panic: face_height_B doesn't match state.src_b.face_height
    LLK_SAN_OP_ASSERT(state.src_a.num_faces, num_faces_A, "UONF");     // panic: num_faces_A doesn't match state.src_a.num_faces
    LLK_SAN_OP_ASSERT(state.src_b.num_faces, num_faces_B, "UONF");     // panic: num_faces_B doesn't match state.src_b.num_faces
}

// No state set, just check that non x arguments match the stored ones
static inline void math_operand_check_impl(math_operand_state_t& state, state_t<uint32_t> math_fmt_A, state_t<uint32_t> math_fmt_B)
{
    LLK_SAN_OP_ASSERT(state.src_a.input_format, math_fmt_A, "MOAI"); // panic: math_fmt_A doesn't match state.src_a.input_format
    LLK_SAN_OP_ASSERT(state.src_b.input_format, math_fmt_B, "MOBI"); // panic: math_fmt_B doesn't match state.src_b.input_format
}

// No state set, just check that non x arguments match the stored ones
static inline void pack_operand_check_impl(
    pack_operand_state_t& state,
    state_t<bool> dest_acc_en,
    state_t<uint32_t> src_fmt,
    state_t<uint32_t> dst_fmt,
    [[maybe_unused]] state_t<uint32_t> face_height,
    state_t<uint32_t> tile_width,
    state_t<uint32_t> num_faces,
    state_t<bool> partial_face,
    state_t<bool> narrow_tile)
{
    LLK_SAN_OP_ASSERT(state.dest_width_32, dest_acc_en, "PODW"); // panic: dest_acc_en doesn't match state.dest_width_32
    LLK_SAN_OP_ASSERT(state.input_format, src_fmt, "POIF");      // panic: src_fmt doesn't match state.input_format
    LLK_SAN_OP_ASSERT(state.output_format, dst_fmt, "POOF");     // panic: dst_fmt doesn't match state.output_format
    // sstanisic fixme: LLK_SAN_OP_ASSERT(state.face_height, face_height, "POFH"); // panic: face_height doesn't match state.face_height
    LLK_SAN_OP_ASSERT(state.tile_width, tile_width, "POTW");     // panic: tile_width doesn't match state.tile_width
    LLK_SAN_OP_ASSERT(state.num_faces, num_faces, "PONF");       // panic: num_faces doesn't match state.num_faces
    LLK_SAN_OP_ASSERT(state.partial_face, partial_face, "POPF"); // panic: partial_face doesn't match state.partial_face
    LLK_SAN_OP_ASSERT(state.narrow_tile, narrow_tile, "POINT");  // panic: narrow_tile doesn't match state.narrow_tile
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
constexpr std::array<size_t, N + 1> _args_offsetof(const std::array<uint8_t, N>& args_sizeof, const std::array<uint8_t, N>& args_alignof)
{
    if constexpr (N == 0)
    {
        return {0};
    }

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

template <uint8_t N>
constexpr size_t _operation_entry_size(
    const std::array<uint8_t, N>& args_sizeof, const std::array<uint8_t, N>& args_alignof, const std::array<size_t, N + 1>& args_offsetof)
{
    if constexpr (N == 0)
    {
        return sizeof(N);
    }

    constexpr size_t max_align = alignof(max_align_t);
    constexpr size_t metadata  = sizeof(N) + N * sizeof(args_sizeof[0]) + N * sizeof(args_alignof[0]);
    constexpr size_t padding   = (max_align - metadata % max_align) % max_align;

    return metadata + padding + args_offsetof[N];
}

// Goes in LLK_LIB in Init
// Store operation type and push arguments to state stack
template <operation_t op, typename... Ts>
static inline void operation_init_impl(operation_state_t& state, const Ts... args)
{
    state.operation     = op;
    state.expect_uninit = operation_must_uninit<op>;

    constexpr uint8_t args_count = _args_count<Ts...>();

    constexpr std::array<uint8_t, args_count> args_sizeof      = _args_sizeof<Ts...>();
    constexpr std::array<uint8_t, args_count> args_alignof     = _args_alignof<Ts...>();
    constexpr std::array<size_t, args_count + 1> args_offsetof = _args_offsetof(args_sizeof, args_alignof);

    constexpr size_t entry_size = _operation_entry_size<args_count>(args_sizeof, args_alignof, args_offsetof);

    static_assert(entry_size <= operation_state_t::BUFFER_SIZE, "llk_san: fault: operation entry will overflow the buffer");

    // | ARG_COUNT | SIZEOF(args[0]) ... | ALIGNOF(args[1]) ... | args[0] PADDING ... |

    char* ptr = state.buffer;

    std::memcpy(ptr, &args_count, sizeof(args_count));
    ptr += sizeof(args_count);

    if constexpr (args_count > 0)
    {
        std::memcpy(ptr, args_sizeof.data(), args_count * sizeof(args_sizeof[0]));
        ptr += args_count * sizeof(args_sizeof[0]);

        std::memcpy(ptr, args_alignof.data(), args_count * sizeof(args_alignof[0]));
        ptr += args_count * sizeof(args_alignof[0]);

        constexpr size_t max_align = alignof(max_align_t);
        size_t padding             = (max_align - reinterpret_cast<uintptr_t>(ptr) % max_align) % max_align;
        ptr += padding;

        size_t i = 0;
        (std::memcpy(ptr + args_offsetof[i++], &args, sizeof(args)), ...);
    }
}

// Goes in LLK_LIB in Execute
// Check operation type and arguments against stored ones
template <operation_t op, typename... Ts>
void operation_check_impl(operation_state_t& state, const Ts... args)
{
    LLK_SAN_ASSERT(state.operation == op, "OPOP"); // fault: operation type doesn't match stored operation

    constexpr uint8_t args_count = _args_count<Ts...>();

    constexpr std::array<uint8_t, args_count> args_sizeof      = _args_sizeof<Ts...>();
    constexpr std::array<uint8_t, args_count> args_alignof     = _args_alignof<Ts...>();
    constexpr std::array<size_t, args_count + 1> args_offsetof = _args_offsetof(args_sizeof, args_alignof);

    constexpr size_t entry_size = _operation_entry_size<args_count>(args_sizeof, args_alignof, args_offsetof);

    static_assert(entry_size <= operation_state_t::BUFFER_SIZE, "operation entry will overflow the buffer");

    // | ARG_COUNT | SIZEOF(args[0]) ... | ALIGNOF(args[1]) ... | args[0] PADDING ... |

    char* ptr = state.buffer;

    LLK_SAN_ASSERT(std::memcmp(&args_count, ptr, sizeof(args_count)) == 0, "OPCO"); // fault: saved vs provided args_count mismatch
    ptr += sizeof(args_count);

    if constexpr (args_count > 0)
    {
        LLK_SAN_ASSERT(std::memcmp(args_sizeof.data(), ptr, args_count * sizeof(args_sizeof[0])) == 0, "OPSI"); // fault: saved vs provided args_sizeof mismatch
        ptr += args_count * sizeof(args_sizeof[0]);

        LLK_SAN_ASSERT(
            std::memcmp(args_alignof.data(), ptr, args_count * sizeof(args_alignof[0])) == 0, "OPAL"); // fault: saved vs provided args_alignof mismatch
        ptr += args_count * sizeof(args_alignof[0]);

        constexpr size_t max_align = alignof(max_align_t);
        size_t padding             = (max_align - reinterpret_cast<uintptr_t>(ptr) % max_align) % max_align;
        ptr += padding;

        // sstanisic fixme: remove the awful lambda
        size_t i = 0;
        ([&] { LLK_ASSERT(std::memcmp(ptr + args_offsetof[i++], &args, sizeof(args)) == 0, "OPAR"); }(),
         ...); // llk_san: panic: saved vs provided args[i] mismatch
    }
}

// Goes in LLK_LIB in Uninit
// Check operation type and clear must uninit flag
template <operation_t op>
void operation_uninit_impl(operation_state_t& state)
{
    state.expect_uninit = false;
}

template <fsm_state_t next>
void fsm_advance_impl(fsm_state_t& current, const operation_state_t& operation)
{
    LLK_SAN_PANIC(current == fsm_state_t::INITIAL && next != fsm_state_t::CONFIGURED, "FIC"); // expected first operation to be configure

    LLK_SAN_PANIC(current == fsm_state_t::CONFIGURED && next != fsm_state_t::INITIALIZED, "FCI"); // expected init after configure

    LLK_SAN_PANIC(current == fsm_state_t::INITIALIZED && next != fsm_state_t::EXECUTED, "FIE"); // expected execute after init

    LLK_SAN_PANIC(
        current == fsm_state_t::EXECUTED && operation.expect_uninit && (next != fsm_state_t::UNINITIALIZED && next != fsm_state_t::EXECUTED),
        "FEEU"); // expected execute or uninit after execute

    // // shouldn't be possible to trigger, sanity check
    LLK_SAN_PANIC(
        current == fsm_state_t::EXECUTED && !operation.expect_uninit && next == fsm_state_t::UNINITIALIZED,
        "FEUU"); // unexpected uninit after execute that doesn't require uninit

    LLK_SAN_PANIC(
        current == fsm_state_t::EXECUTED && !operation.expect_uninit &&
            (next != fsm_state_t::EXECUTED && next != fsm_state_t::INITIALIZED && next != fsm_state_t::RECONFIGURED),
        "FEEIR"); // expected execute, init or reconfig operation after execute

    LLK_SAN_PANIC(
        current == fsm_state_t::EXECUTED && !operation.expect_uninit &&
            (next != fsm_state_t::EXECUTED && next != fsm_state_t::INITIALIZED && next != fsm_state_t::RECONFIGURED),
        "FEEIR"); // expected execute, init or reconfig operation after execute

    LLK_SAN_PANIC(
        current == fsm_state_t::UNINITIALIZED && (next != fsm_state_t::INITIALIZED && next != fsm_state_t::RECONFIGURED),
        "FUIR"); // expected init or reconfigure after uninit

    // valid transition -> commit
    current = next;
}

} // namespace llk_san
