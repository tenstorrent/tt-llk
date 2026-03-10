// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "ckernel.h"
#include "llk_assert.h"

namespace llk
{

template <std::uint32_t BUFFER_SIZE>
class stream_t
{
    // these not volatile because we use force_load/force_store explicitly
    std::uint32_t write_idx;
    std::uint32_t read_idx;

    volatile char buffer[BUFFER_SIZE];

    std::uint32_t capacity() const
    {
        return BUFFER_SIZE - 1;
    }

    // TODO: document
    std::uint32_t wait_free()
    {
        auto poll_free = [&]() -> std::uint32_t { return (capacity() - write_idx + ckernel::force_load(read_idx)) % BUFFER_SIZE; };

        std::uint32_t free;
        for (free = poll_free(); free == 0; free = poll_free())
        {
            ckernel::invalidate_data_cache();
            asm volatile("nop; nop; nop; nop; nop; nop; nop; nop;");
        }

        return free;
    }

    // TODO: document
    std::uint32_t wait_avail()
    {
        auto poll_avail = [&]() -> std::uint32_t { return (ckernel::force_load(write_idx) - read_idx + BUFFER_SIZE) % BUFFER_SIZE; };

        std::uint32_t avail;
        for (avail = poll_avail(); avail == 0; avail = poll_avail())
        {
            ckernel::invalidate_data_cache();
            asm volatile("nop; nop; nop; nop; nop; nop; nop; nop;");
        }

        return avail;
    }

public:
    stream_t()                           = delete;
    stream_t(const stream_t&)            = delete;
    stream_t(stream_t&&)                 = delete;
    stream_t& operator=(const stream_t&) = delete;
    stream_t& operator=(stream_t&&)      = delete;

    void push(const void* inputv, std::size_t size)
    {
        const char* input     = reinterpret_cast<const char*>(inputv);
        std::size_t remaining = size;
        std::size_t offset    = 0;

        while (remaining > 0)
        {
            std::uint32_t free = wait_free();

            std::size_t chunk = std::min<std::size_t>(remaining, free);

            const std::size_t head = std::min<std::size_t>(chunk, BUFFER_SIZE - write_idx);
            ckernel::memcpy_blocking(&buffer[write_idx], &input[offset], head);
            ckernel::memcpy_blocking(&buffer[0], &input[offset + head], chunk - head);

            ckernel::force_store(write_idx = (write_idx + chunk) % BUFFER_SIZE);

            offset += chunk;
            remaining -= chunk;
        }
    }

    char peek()
    {
        wait_avail();
        return buffer[read_idx];
    }

    void pop(void* outputv, std::size_t size)
    {
        char* output          = reinterpret_cast<char*>(outputv);
        std::size_t remaining = size;
        std::size_t offset    = 0;

        while (remaining > 0)
        {
            std::uint32_t avail = wait_avail();

            std::size_t chunk = std::min<std::size_t>(remaining, avail);

            const std::size_t head = std::min<std::size_t>(chunk, BUFFER_SIZE - read_idx);
            ckernel::memcpy_blocking(&output[offset], &buffer[read_idx], head);
            ckernel::memcpy_blocking(&output[offset + head], &buffer[0], chunk - head);

            ckernel::force_store(read_idx = (read_idx + chunk) % BUFFER_SIZE);

            offset += chunk;
            remaining -= chunk;
        }
    }
};

} // namespace llk
