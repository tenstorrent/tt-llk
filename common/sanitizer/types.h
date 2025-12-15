// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <iostream>
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
    template <typename U, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, state_t>>>
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
    template <typename U, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, state_t>>>
    state_t& operator=(U&& rhs)
    {
        *this = state_t<T>(std::forward<U>(rhs));
        return *this;
    }

    // EQUALITY
    // if ANY side of comparison is state_type_t::dontcare return true
    // else if ANY side of comparison is state_type_t::indeterminate return false
    // otherwise return lhs.value == rhs.value

    // We need to friend state_t<U> in case U != T because otherwise U->state_type is private
    template <typename U>
    friend class state_tconst;

    // state_t == state_t
    template <typename U>
    bool operator==(state_t<U>& rhs) const
    {
        // DONTCARE always returns true
        if (this->state_type == state_type_t::dontcare || rhs.state_type == state_type_t::dontcare)
        {
            return true;
        }

        // If either is indeterminate, return false
        if (this->state_type == state_type_t::indeterminate || rhs.state_type == state_type_t::indeterminate)
        {
            return false;
        }

        // Compare underlying values
        return this->underlying == rhs.underlying;
    }

    // state_t == llk_san::DONTCARE
    bool operator==(const dontcare_t& rhs) const
    {
        return *this == state_t<T>(rhs);
    }

    // state_t == llk_san::INDETERMINATE
    bool operator==(const indeterminate_t& rhs) const
    {
        return *this == state_t<T>(rhs);
    }

    // state_t == value
    template <typename U, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, state_t>>>
    bool operator==(const U& rhs) const
    {
        return *this == state_t<U>(rhs);
    }
};

template <typename U, typename V>
static inline bool operator==(const V& lhs, const state_t<U>& rhs)
{
    // Equality is commutative
    return rhs == lhs;
}

template <typename U, typename V>
static inline bool operator!=(const V& lhs, const state_t<U>& rhs)
{
    // Inequality is commutative
    return !(rhs == lhs);
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
