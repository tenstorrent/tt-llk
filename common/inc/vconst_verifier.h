// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <type_traits>

namespace vconst_verifier
{

struct disable
{
    // This is used as a default to mark that verification is not enabled for given function. Allows to be backward compatible and not break when some path has
    // not yet enabled verification.
};

struct unhandled
{
    // This is used to mark paths that are not yet handling verification. This allows to detect incorrect init/compute configuration (e.g. init went unhandled
    // path when compute expected initialization).
};

struct no_used_regs
{
    // This is default, when no vConst register are yet used.
};

template <typename B = no_used_regs>
struct use_vconst0 : public B
{
    // Mark that vConst0 is set in the init function.
    static void vconst0();
};

template <typename B = no_used_regs>
struct use_vconst1 : public B
{
    // Mark that vConst1 is set in the init function.
    static void vconst1();
};

template <typename B = no_used_regs>
struct use_vconst2 : public B
{
    // Mark that vConst2 is set in the init function.
    static void vconst2();
};

template <typename verifier>
constexpr bool is_unhandled_path()
{
    return std::is_same_v<verifier, unhandled> || std::is_base_of_v<unhandled, verifier>;
}

template <typename verifier>
inline void assert_vconst_0()
{
    static_assert(!is_unhandled_path<verifier>(), "vconst verifier unhandled path used");
    if constexpr (!std::is_same_v<verifier, disable>)
    {
        static_assert(std::is_void<decltype(verifier::vconst0())>::value, "vConst0 not marked as set.");
    }
}

template <typename verifier>
inline void assert_vconst_1()
{
    static_assert(!is_unhandled_path<verifier>(), "vconst verifier unhandled path used");
    if constexpr (!std::is_same_v<verifier, disable>)
    {
        static_assert(std::is_void<decltype(verifier::vconst1())>::value, "vConst1 not marked as set.");
    }
}

template <typename verifier>
inline void assert_vconst_2()
{
    static_assert(!is_unhandled_path<verifier>(), "vconst verifier unhandled path used");
    if constexpr (!std::is_same_v<verifier, disable>)
    {
        static_assert(std::is_void<decltype(verifier::vconst2())>::value, "vConst2 not marked as set.");
    }
}
} // namespace vconst_verifier
