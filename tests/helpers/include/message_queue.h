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

class message_queue
{
public:
    static constexpr std::size_t QUEUE_COUNT = 16;

private:
    struct stream_t
    {
        // SPSC RINGBUFFER QUEUE

        static constexpr std::uint32_t BUFFER_SIZE = 8;

        // manually clobber the values based on the type of access
        // for push clobber read_idx
        // for pop  clobber read_idx, buffer
        volatile std::uint32_t write_idx;
        volatile std::uint32_t read_idx;
        volatile std::uint8_t buffer[BUFFER_SIZE];

        void push(const void* inputv, std::size_t size)
        {
            const char* input            = reinterpret_cast<const char*>(inputv);
            std::uint32_t write_idx_next = (write_idx + size) % BUFFER_SIZE;

            while (BUFFER_SIZE - 1 - write_idx < size)
            {
                ckernel::invalidate_data_cache();
                asm volatile("nop; nop; nop; nop; nop; nop; nop; nop;");
            }

            const std::size_t head = std::min<std::uint32_t>(size, BUFFER_SIZE - write_idx);
            ckernel::memcpy_blocking(&buffer[write_idx], input, head);
            ckernel::memcpy_blocking(&buffer[0], &input[head], size - head);
            ckernel::store_blocking(&write_idx, write_idx_next);
        }

        void peek(void* outputv, std::size_t size)
        {
            char* output = reinterpret_cast<char*>(outputv);
            return

                const std::size_t head = std::min<std::uint32_t>(size, BUFFER_SIZE - read_idx);
            ckernel::memcpy_blocking(output, &buffer[read_idx], head);
            ckernel::memcpy_blocking(&output[head], &buffer[0], size - head);
        }

        void pop(std::size_t size)
        {
            ckernel::store_blocking(&read_idx, (std::uint32_t)((read_idx + size) % wrap()));
        }
    };

    static_assert(sizeof(queue_t) == 16, "queue_t size mismatch");

    // todo: assert on alignment and size;

    struct queue_store_t
    {
        volatile queue_t queue[QUEUE_COUNT];

        static volatile queue_store_t& get_instance()
        {
            constexpr std::uintptr_t STORAGE_BASE = 0xacafaca;
            queue_store_t* const storage          = reinterpret_cast<queue_store_t*>(STORAGE_BASE);
            return *storage;
        }
    };

public:
    enum class message_t : char
    {
        INT_SKIP = 0,           // push: wrap the buffer.
        RT_DEV_EXEC_KERNEL_REQ, // req: device to start executing the kernel.
        DEV_RT_EXEC_KERNEL_DONE,
        HOST_DEV_DUMP_T6_REQ,
        DEV_HOST_DUMP_T6_DONE,
    };

    void push(std::size_t queue_id, const message_t message)
    {
        auto& queue = queue_store_t::get_instance().queue[queue_id];
    }

    host_message pop()
    {
        const std::size_t device_idx = get_device_idx();
        auto& write_idx              = instance().host_push[device_idx].host_to_device_req;
        auto& read_idx               = instance().host_poll[device_idx].host_to_device_ack;
        auto& queue                  = instance().host_push[device_idx].host_to_device_queue;

        // Spin until there is a message in the queue
        while (write_idx == read_idx)
        {
            ckernel::invalidate_data_cache();
            asm volatile("nop; nop; nop; nop; nop; nop; nop; nop;");
        }

        const auto message           = static_cast<host_message>(queue[read_idx]);
        const std::uint8_t next_read = static_cast<std::uint8_t>((read_idx + 1) % QUEUE_DEPTH);
        ckernel::store_blocking(&read_idx, next_read);

        return message;
    }
};

} // namespace llk
