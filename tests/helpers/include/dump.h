// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>

#include "ckernel.h"

namespace llk::debug
{

class tensix_dump
{
private:
    static constexpr std::uintptr_t TENSIX_DUMP_MAILBOX_ADDRESS = 0x16AFE4;
    enum class mailbox_state_t : std::uint32_t
    {
        DONE      = 0,
        REQUESTED = 1,
    };

public:
    /**
     * @brief Requests a tensix state dump and blocks until the host completes it.
     * @note All TRISC cores must call this for the host to fulfill the request.
     */
    static void request()
    {
        ckernel::tensix_sync(); // make sure all changes to tensix state are written

        volatile mailbox_state_t* const DUMP_MAILBOX = reinterpret_cast<mailbox_state_t*>(TENSIX_DUMP_MAILBOX_ADDRESS);

        DUMP_MAILBOX[COMPILE_FOR_TRISC] = mailbox_state_t::REQUESTED; // signal host to start dumping tensix state

        do
        {
            ckernel::invalidate_data_cache(); // prevent polling from cache.
            asm volatile("nop; nop; nop; nop; nop; nop; nop; nop;");
        } while (DUMP_MAILBOX[COMPILE_FOR_TRISC] == mailbox_state_t::REQUESTED); // wait while the host is dumping tensix state.
    }
};

} // namespace llk::debug
