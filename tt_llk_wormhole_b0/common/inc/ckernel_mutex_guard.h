// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel.h"

namespace ckernel
{

class [[nodiscard]] T6MutexLockGuard final
{
public:
    explicit T6MutexGuard(const uint8_t index) noexcept : mutex_index(index)
    {
        t6_mutex_acquire(mutex_index);
    }

    ~T6MutexGuard()
    {
        t6_mutex_release(mutex_index);
    }

    // Non-copyable
    T6MutexGuard(const T6MutexGuard&)            = delete;
    T6MutexGuard& operator=(const T6MutexGuard&) = delete;

    // Non-movable
    T6MutexGuard(T6MutexGuard&&)            = delete;
    T6MutexGuard& operator=(T6MutexGuard&&) = delete;

private:
    const uint8_t mutex_index;
};

} // namespace ckernel
