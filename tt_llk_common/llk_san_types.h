// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <iostream>
#include <type_traits>
#include <utility>

#include "llk_defs.h"

namespace llk_san
{

struct ignore_t
{
    constexpr ignore_t() = default;
};

struct indeterminate_t
{
    constexpr indeterminate_t() = default;
};

static constexpr ignore_t IGNORE               = ignore_t();
static constexpr indeterminate_t INDETERMINATE = indeterminate_t();

enum class state_type_t : uint8_t
{
    determinate,
    indeterminate,
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
    // Default to INDETERMINATE state because hardware is not initialized
    state_t() : underlying {}, state_type(state_type_t::indeterminate)
    {
    }

    state_t(const state_t&) = default;
    state_t(state_t&&)      = default;

    // CONVERSION
    // - llk_san::IGNORE      -> state_t with state_type == ignore
    // - llk_san::INDETERMINATE -> state_t with state_type == indeterminate
    // - other                  -> state_t with state_type == determinate (storing the value)

    // Constructor for IGNORE
    constexpr state_t(const ignore_t&) : underlying {}, state_type(state_type_t::ignore)
    {
    }

    // Constructor for INDETERMINATE
    constexpr state_t(const indeterminate_t&) : underlying {}, state_type(state_type_t::indeterminate)
    {
    }

    // Constructor for DETERMINATE value
    template <
        typename U,
        typename =
            std::enable_if_t<!is_state_t_v<std::decay_t<U>> && !std::is_same_v<std::decay_t<U>, ignore_t> && !std::is_same_v<std::decay_t<U>, indeterminate_t>>>
    constexpr state_t(U&& value) : underlying(std::forward<U>(value)), state_type(state_type_t::determinate)
    {
    }

    // ASSIGNMENT
    // if RHS of assignment is state_type_t::ignore, noop (stays old value)
    // otherwise take the state_type and underlying of RHS

    // Template operators contain the actual logic
    template <typename U>
    state_t& operator=(const state_t<U>& rhs)
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
    state_t& operator=(state_t<U>&& rhs)
    {
        if (rhs.state_type == state_type_t::ignore)
        {
            return *this; // No-op
        }

        state_type = rhs.state_type;
        underlying = std::move(rhs.underlying);

        return *this;
    }

    // Non-template overloads delegate to templates (to override implicit operators)
    state_t& operator=(const state_t& rhs)
    {
        return this->template operator= <T>(rhs);
    }

    state_t& operator=(state_t&& rhs)
    {
        return this->template operator= <T>(std::move(rhs));
    }

    // RHS of assignment is:
    // - compatible with T
    // - indeterminate_t
    // - ignore_t
    template <typename U, typename = std::enable_if_t<!is_state_t_v<std::decay_t<U>>>>
    state_t& operator=(U&& rhs)
    {
        *this = state_t<T>(std::forward<U>(rhs));
        return *this;
    }

    bool is_determinate() const
    {
        return state_type == state_type_t::determinate;
    }

    bool is_indeterminate() const
    {
        return state_type == state_type_t::indeterminate;
    }

    bool is_ignore() const
    {
        return state_type == state_type_t::ignore;
    }

    const T& get_underlying() const
    {
        LLK_ASSERT(is_determinate(), "panic: llk_san: underlying value is not determinate");
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

    if (lhs.is_indeterminate() || rhs.is_indeterminate())
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

    if (lhs.is_indeterminate() || rhs.is_indeterminate())
    {
        return false;
    }

    return lhs.get_underlying() != rhs.get_underlying();
}

// TODO: refactor below

enum class llk_san_op_t
{
    UnpackA,
    UnpackABMatmul,
    UnpackUntilize,
    EltwiseUnaryDatacopy,
    Matmul,
    Pack,
    PackUntilize
};

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

// Unpacker state
struct unpack_src_state_t
{
    state_t<uint32_t> input_format;
    state_t<uint32_t> output_format;
    state_t<uint32_t> face_height;
    state_t<uint32_t> num_faces;
};

struct unpack_state_t
{
    unpack_src_state_t src_a;
    unpack_src_state_t src_b;
    state_t<bool> dest_width_32;
    bool is_configured = false;
};

// Math state
struct math_src_state_t
{
    state_t<uint32_t> input_format;
};

struct math_state_t
{
    math_src_state_t src_a;
    math_src_state_t src_b;
    bool is_configured = false;
};

// Pack state
struct pack_state_t
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

struct hw_state_t
{
    unpack_state_t unpack;
    math_state_t math;
    pack_state_t pack;
};

}; // namespace llk_san
