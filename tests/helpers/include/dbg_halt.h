// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include "ckernel.h"

template <ckernel::ThreadId thread_id>
inline void dbg_thread_halt()
{
    static_assert(
        (thread_id == ckernel::ThreadId::MathThreadId) || (thread_id == ckernel::ThreadId::UnpackThreadId) || (thread_id == ckernel::ThreadId::PackThreadId),
        "Invalid thread id set in dbg_wait_for_thread_idle(...)");

    if constexpr (thread_id == ckernel::ThreadId::UnpackThreadId)
    {
        // Wait for all instructions on the running thread to complete
        ckernel::tensix_sync();
        // Notify math thread that unpack thread is idle
        ckernel::mailbox_write(ckernel::ThreadId::MathThreadId, 1);
        // Wait for math thread to complete debug dump
        volatile uint32_t temp = ckernel::mailbox_read(ckernel::ThreadId::MathThreadId);
    }
    else if constexpr (thread_id == ckernel::ThreadId::MathThreadId)
    {
        // Wait for all instructions on the running thread to complete
        ckernel::tensix_sync();
        // Wait for unpack thread to complete
        volatile uint32_t temp = ckernel::mailbox_read(ckernel::ThreadId::UnpackThreadId);
        // Wait for previous packs to finish
        while (ckernel::semaphore_read(ckernel::semaphore::MATH_PACK) > 0)
        {
        };
    }
}

void dbg_halt_math()
{
    dbg_thread_halt<ckernel::MathThreadId>();
}

void dbg_halt_unpack()
{
    dbg_thread_halt<ckernel::UnpackThreadId>();
}

void dbg_halt_pack()
{
    dbg_thread_halt<ckernel::PackThreadId>();
}

void dbg_halt_tensix()
{
    dbg_halt_unpack();
    dbg_halt_math();
    dbg_halt_pack();
}
