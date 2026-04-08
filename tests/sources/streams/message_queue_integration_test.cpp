// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>

#include "ckernel.h"
#include "message_queue.h"
#include "params.h"

// Globals required by LLK infrastructure
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

constexpr std::uintptr_t MQUEUE_ADDRESS = 0x70000;

using namespace llk;

#ifdef LLK_TRISC_UNPACK

#include "llk_assert.h"

// T0 performs a two-step handshake with the host:
//   1. EXEC_REQUEST  (no payload)  → host
//   2. EXEC_DONE                   ← host  (verified via peek before receive)
//   3. MEMCPY_REQUEST (with data)  → host
//   4. MEMCPY_DONE                 ← host  (verified via peek before receive)
void run_kernel(RUNTIME_PARAMETERS params)
{
    MessageQueue& mqueue = *reinterpret_cast<MessageQueue*>(MQUEUE_ADDRESS);

    // --- step 1 & 2: EXEC handshake (no payload) ---
    mqueue.send(MessageStreamId::TRISC_HOST, MessageType::EXEC_REQUEST);

    while (!mqueue.peek(MessageStreamId::HOST_TRISC).has_value())
    {
    }
    LLK_ASSERT(*mqueue.peek(MessageStreamId::HOST_TRISC) == MessageType::EXEC_DONE, "MessageQueue: expected EXEC_DONE from host");
    mqueue.receive(MessageStreamId::HOST_TRISC, MessageType::EXEC_DONE);

    // --- step 3 & 4: MEMCPY handshake (with payload) ---
    const MemcpyRequestData req {
        reinterpret_cast<void*>(0x10000),
        reinterpret_cast<void*>(0x20000),
        64,
    };
    mqueue.send(MessageStreamId::TRISC_HOST, MessageType::MEMCPY_REQUEST, req);

    while (!mqueue.peek(MessageStreamId::HOST_TRISC).has_value())
    {
    }
    LLK_ASSERT(*mqueue.peek(MessageStreamId::HOST_TRISC) == MessageType::MEMCPY_DONE, "MessageQueue: expected MEMCPY_DONE from host");
    mqueue.receive(MessageStreamId::HOST_TRISC, MessageType::MEMCPY_DONE);
}

#endif

#ifdef LLK_TRISC_MATH

void run_kernel(RUNTIME_PARAMETERS params)
{
}

#endif

#ifdef LLK_TRISC_PACK

void run_kernel(RUNTIME_PARAMETERS params)
{
}

#endif
