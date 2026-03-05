// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

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
    static constexpr std::size_t QUEUE_DEPTH = 2;

private:
    struct ring_t
    {
        // SPSC RINGBUFFER
        volatile std::uint32_t write_idx;
        volatile std::uint32_t read_idx;
        volatile std::uint32_t queue[QUEUE_DEPTH];
    };

    // todo: assert on alignment and size;

    struct queue_store_t
    {
        volatile ring_t ring[QUEUE_COUNT];

        static volatile queue_store_t& get_instance()
        {
            constexpr std::uintptr_t STORAGE_BASE = 0xacafaca;
            queue_store_t* const storage          = reinterpret_cast<queue_store_t*>(STORAGE_BASE);
            return *storage;
        }
    };

    static std::size_t get_free_slots(const volatile ring_t& ring)
    {
        const std::size_t used = (QUEUE_DEPTH + ring.write_idx - ring.read_idx) % QUEUE_DEPTH;
        return (QUEUE_DEPTH - 1) - used;
    }

public:
    enum class message_t : std::uint32_t
    {
        RT_DEV_EXEC_KERNEL_REQ, // req: device to start executing the kernel.
        DEV_RT_EXEC_KERNEL_DONE,
        HOST_DEV_DUMP_T6_REQ,
        DEV_HOST_DUMP_T6_DONE,
    };

    void push(std::size_t queue_id, const message_t message)
    {
        auto& ring       = queue_store_t::get_instance().ring[queue_id];
        auto write_idx_p = &ring.write_idx;
        auto queue_p     = &ring.queue[0];

        // sstanisic todo: fix the single slot message constraint before merge.
        static_assert(sizeof(message_t) == sizeof(std::uint32_t), "couldn't be bothered yet.");
        std::uint32_t write_idx_next = *write_idx_p + 1;
        while (get_free_slots(ring) < 1)
        {
            ckernel::invalidate_data_cache();
            asm volatile("nop; nop; nop; nop; nop; nop; nop; nop;");
        }

        ckernel::store_blocking(&queue[write_idx], ckernel::to_underlying(message));
        ckernel::store_blocking(write_idx_p, write_idx_next);
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
