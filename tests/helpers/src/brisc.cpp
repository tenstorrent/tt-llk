// SPDX-FileCopyrightText: © 2024 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>

#include "ckernel.h"
#include "llk_assert.h"

#ifdef LLK_BOOT_MODE_BRISC
#include "boot.h"
#endif

int main()
{
#ifdef LLK_BOOT_MODE_BRISC

#ifdef COVERAGE
    constexpr std::uint32_t mailboxes_start = 0x63FC0;
#else
    constexpr std::uint32_t mailboxes_start = 0x1FFC0;
#endif

    volatile std::uint32_t* const mailbox_brisc  = reinterpret_cast<volatile std::uint32_t*>(mailboxes_start);
    volatile std::uint32_t* const mailbox_unpack = reinterpret_cast<volatile std::uint32_t*>(mailboxes_start + sizeof(std::uint32_t));
    volatile std::uint32_t* const mailbox_math   = reinterpret_cast<volatile std::uint32_t*>(mailboxes_start + 2 * sizeof(std::uint32_t));
    volatile std::uint32_t* const mailbox_pack   = reinterpret_cast<volatile std::uint32_t*>(mailboxes_start + 3 * sizeof(std::uint32_t));

    device_setup();
    clear_trisc_soft_reset();

    while (*mailbox_unpack != 0xFF)
        ;
    while (*mailbox_math != 0xFF)
        ;
    while (*mailbox_pack != 0xFF)
        ;

    *mailbox_brisc = 0x1;
#endif
}
