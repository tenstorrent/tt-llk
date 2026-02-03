// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>

#include "ckernel.h"

namespace llk_t6_dump
{

constexpr inline std::uintptr_t TRISC_CFG_DUMP_MAILBOX_ADDRESS = 0x16AFE4;

enum class dump_t : std::uint32_t
{
    DONE = 0,
    WAIT = 1,
};

volatile dump_t* const CFG_DUMP_MAILBOX = reinterpret_cast<dump_t*>(TRISC_CFG_DUMP_MAILBOX_ADDRESS);

inline void wait()
{
    ckernel::tensix_sync(); // make sure all changes to tensix state are committed

    CFG_DUMP_MAILBOX[COMPILE_FOR_TRISC] = dump_t::WAIT; // signal host to start dumping cfg

    do
    {
        asm volatile("fence" ::: "memory");
        asm volatile("nop; nop; nop; nop; nop; nop; nop; nop;");
    } while (CFG_DUMP_MAILBOX[COMPILE_FOR_TRISC] == dump_t::WAIT); // wait for host to finish dumping cfg
}

} // namespace llk_t6_dump
