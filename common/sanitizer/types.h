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

struct dontcare_t
{
    constexpr dontcare_t() = default;
};

struct indeterminate_t
{
    constexpr indeterminate_t() = default;
};

static constexpr dontcare_t DONTCARE           = dontcare_t();
static constexpr indeterminate_t INDETERMINATE = indeterminate_t();

enum class state_type_t : uint8_t
{
    determinate,
    indeterminate,
    dontcare
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

    // CONVERSION
    // - llk_san::DONTCARE      -> state_t with state_type == dontcare
    // - llk_san::INDETERMINATE -> state_t with state_type == indeterminate
    // - other                  -> state_t with state_type == determinate (storing the value)

    // Constructor for DONTCARE
    constexpr state_t(dontcare_t) : underlying {}, state_type(state_type_t::dontcare)
    {
    }

    // Constructor for INDETERMINATE
    constexpr state_t(indeterminate_t) : underlying {}, state_type(state_type_t::indeterminate)
    {
    }

    // Constructor for DETERMINATE value
    template <typename U, typename = std::enable_if_t<!is_state_t_v<std::decay_t<U>>>>
    constexpr state_t(U&& value) : underlying(std::forward<U>(value)), state_type(state_type_t::determinate)
    {
    }

    // ASSIGNMENT
    // if RHS of assignment is state_type_t::dontcare, noop (stays old value)
    // otherwise take the state_type and underlying of RHS

    // RHS of assignment is state_t lvalue
    template <typename U>
    state_t& operator=(const state_t<U>& rhs)
    {
        if (rhs.state_type == state_type_t::dontcare)
        {
            return *this;
        }

        state_type = rhs.state_type;
        underlying = rhs.underlying;

        return *this;
    }

    // RHS of assignment is state_t rvalue
    template <typename U>
    state_t& operator=(state_t<U>&& rhs)
    {
        if (rhs.state_type == state_type_t::dontcare)
        {
            return *this; // No-op
        }

        state_type = rhs.state_type;
        underlying = std::move(rhs.underlying);

        return *this;
    }

    // RHS of assignment is:
    // - compatible with T
    // - indeterminate_t
    // - dontcare_t
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

    bool is_dontcare() const
    {
        return state_type == state_type_t::dontcare;
    }

    T get_underlying() const
    {
        LLK_PANIC(!is_indeterminate(), "panic: llk_san: tried to get underlying value when state is not determinate");
        return underlying;
    }
};

template <typename T, typename U>
static inline bool _assert_condition(const state_t<T>& lhs, const state_t<U>& rhs)
{
    if (lhs.state_type == state_type_t::dontcare || rhs.state_type == state_type_t::dontcare)
    {
        return true;
    }

    if (lhs.state_type == state_type_t::indeterminate || rhs.state_type == state_type_t::indeterminate)
    {
        return false;
    }

    return lhs.underlying == rhs.underlying;
}

template <typename T, typename U>
static inline bool _panic_condition(const state_t<T>& lhs, const state_t<U>& rhs)
{
    if (lhs.state_type == state_type_t::dontcare || rhs.state_type == state_type_t::dontcare)
    {
        return true;
    }

    if (lhs.state_type == state_type_t::indeterminate || rhs.state_type == state_type_t::indeterminate)
    {
        return false;
    }

    return lhs.underlying != rhs.underlying;
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

}; // namespace llk_san
