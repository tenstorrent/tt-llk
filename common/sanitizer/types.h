// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <iostream>
#include <type_traits>
#include <utility>

#include "ckernel.h"
#include "llk_defs.h"

namespace llk_san
{

struct ignore_t
{
    constexpr ignore_t() = default;
};

struct unknown_t
{
    constexpr unknown_t() = default;
};

static constexpr ignore_t IGNORE   = ignore_t();
static constexpr unknown_t UNKNOWN = unknown_t();

enum class state_type_t : uint8_t
{
    known,
    unknown,
    ignore
};

template <typename T>
class state_t;

template <typename T>
struct is_state_t : std::false_type
{
};

template <typename T>
struct is_state_t<state_t<T>> : std::true_type
{
};

template <typename T>
inline constexpr bool is_state_t_v = is_state_t<T>::value;

template <typename T>
class state_t
{
private:
    T underlying;
    state_type_t state_type;

public:
    // CONSTRUCTION
    // Default to UNKNOWN state because hardware is not initialized
    state_t() noexcept(std::is_nothrow_default_constructible_v<T>) : underlying {}, state_type(state_type_t::unknown)
    {
    }

    state_t(const state_t&) noexcept(std::is_nothrow_copy_constructible_v<T>) = default;
    state_t(state_t&&) noexcept(std::is_nothrow_move_constructible_v<T>)      = default;

    // CONVERSION
    // - llk_san::IGNORE     -> state_t with state_type == ignore
    // - llk_san::UNKNOWN    -> state_t with state_type == unknown
    // - other               -> state_t with state_type == known (storing the value)

    // Constructor for IGNORE
    constexpr state_t(const ignore_t&) noexcept : underlying {}, state_type(state_type_t::ignore)
    {
    }

    // Constructor for UNKNOWN
    constexpr state_t(const unknown_t&) noexcept : underlying {}, state_type(state_type_t::unknown)
    {
    }

    // Constructor for KNOWN value
    template <
        typename U,
        typename =
            std::enable_if_t<!is_state_t_v<std::decay_t<U>> && !std::is_same_v<std::decay_t<U>, ignore_t> && !std::is_same_v<std::decay_t<U>, unknown_t>>>
    constexpr state_t(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>) : underlying(std::forward<U>(value)), state_type(state_type_t::known)
    {
    }

    // ASSIGNMENT
    // if RHS of assignment is state_type_t::ignore, noop (stays old value)
    // otherwise take the state_type and underlying of RHS

    template <typename U>
    state_t& operator=(const state_t<U>& rhs) noexcept(std::is_nothrow_copy_assignable_v<T>)
    {
        if (rhs.state_type == state_type_t::ignore)
        {
            return *this;
        }

        state_type = rhs.state_type;
        underlying = rhs.underlying;

        return *this;
    }

    template <typename U>
    state_t& operator=(state_t<U>&& rhs) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        if (rhs.state_type == state_type_t::ignore)
        {
            return *this; // No-op
        }

        state_type = rhs.state_type;
        underlying = std::move(rhs.underlying);

        return *this;
    }

    state_t& operator=(const state_t& rhs) noexcept(std::is_nothrow_copy_assignable_v<T>)
    {
        return this->template operator= <T>(rhs);
    }

    state_t& operator=(state_t&& rhs) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        return this->template operator= <T>(std::move(rhs));
    }

    // RHS of assignment is:
    // - compatible with T
    // - unknown_t
    // - ignore_t
    template <typename U, typename = std::enable_if_t<!is_state_t_v<std::decay_t<U>>>>
    state_t& operator=(U&& rhs) noexcept(std::is_nothrow_constructible_v<T, U&&>)
    {
        *this = state_t<T>(std::forward<U>(rhs));
        return *this;
    }

    bool is_known() const noexcept
    {
        return state_type == state_type_t::known;
    }

    bool is_unknown() const noexcept
    {
        return state_type == state_type_t::unknown;
    }

    bool is_ignore() const noexcept
    {
        return state_type == state_type_t::ignore;
    }

    const T& get_underlying() const
    {
        LLK_ASSERT(is_known(), "panic: llk_san: underlying value is not known");
        return underlying;
    }
};

template <typename T, typename U>
static inline bool _assert_condition(const state_t<T>& lhs, const state_t<U>& rhs)
{
    if (lhs.is_ignore() || rhs.is_ignore())
    {
        return true;
    }

    if (lhs.is_unknown() || rhs.is_unknown())
    {
        return false;
    }

    return lhs.get_underlying() == rhs.get_underlying();
}

template <typename T, typename U>
static inline bool _panic_condition(const state_t<T>& lhs, const state_t<U>& rhs)
{
    if (lhs.is_ignore() || rhs.is_ignore())
    {
        return true;
    }

    if (lhs.is_unknown() || rhs.is_unknown())
    {
        return false;
    }

    return lhs.get_underlying() != rhs.get_underlying();
}

// TODO: refactor below

enum class llk_san_cfg_t
{
    Addrmod,
    Mop,
    DvalidDisable,
    CH0Strides,
    CH1Strides,
    TileDesc,
    AdcXX,
    Transpose,
    L1Offset
};

enum class llk_san_operand_t
{
    SrcA,
    SrcB,
    Dst
};

// UNPACK operand state
struct unpack_src_state_t
{
    state_t<uint32_t> input_format;
    state_t<uint32_t> output_format;
    state_t<uint32_t> face_height;
    state_t<uint32_t> num_faces;
};

struct unpack_operand_state_t
{
    unpack_src_state_t src_a;
    unpack_src_state_t src_b;
    state_t<bool> dest_width_32;
    bool is_configured = false;
};

// MATH operand state
struct math_src_state_t
{
    state_t<uint32_t> input_format;
};

struct math_operand_state_t
{
    math_src_state_t src_a;
    math_src_state_t src_b;
    bool is_configured = false;
};

// PACK operand state
struct pack_operand_state_t
{
    state_t<uint32_t> input_format;
    state_t<uint32_t> output_format;
    state_t<uint32_t> face_height;
    state_t<uint32_t> tile_width;
    state_t<uint32_t> num_faces;
    state_t<bool> partial_face;
    state_t<bool> narrow_tile;
    state_t<bool> dest_width_32;
    bool is_configured = false;
};

struct operand_state_t
{
    unpack_operand_state_t unpack;
    math_operand_state_t math;
    pack_operand_state_t pack;
};

enum class operation_t : uint8_t
{
    UnpackA,
    UnpackABMatmul,
    UnpackUntilize,
    EltwiseUnaryDatacopy,
    Matmul,
    Pack,
    PackUntilize
};

// START: get this working before we require uninits for everything

template <operation_t op>
struct operation_must_uninit_t : std::false_type
{
};

template <>
struct operation_must_uninit_t<operation_t::PackUntilize> : std::true_type
{
};

template <operation_t op>
inline constexpr bool operation_must_uninit = operation_must_uninit_t<op>::value;

// END: temp fix for uninits

struct operation_state_t
{
    static constexpr size_t BUFFER_SIZE = 96;

    // aligned to max alignment so that content of buffer
    // is accessible through T* irrespective of the alignment of T
    alignas(alignof(max_align_t)) char buffer[BUFFER_SIZE];

    operation_t operation;

    // enabled by operation_init if the operation must be uninitializer
    // disabled by operation_uninit
    bool expect_uninit;
};

enum class fsm_state_t : uint32_t
{
    INITIAL,
    CONFIGURED,
    INITIALIZED,
    EXECUTED,
    UNINITIALIZED,
    RECONFIGURED
};

struct sanitizer_state_t
{
    // used to validate operation order
    operand_state_t operand;
    operation_state_t operation[MAX_THREADS] = {};
    fsm_state_t fsm[MAX_THREADS]             = {fsm_state_t::INITIAL, fsm_state_t::INITIAL, fsm_state_t::INITIAL};
};

} // namespace llk_san
