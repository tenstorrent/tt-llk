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

    volatile std::uint32_t* const mailbox_brisc  = reinterpret_cast<volatile std::uint32_t*>(0x1FFF0);
    volatile std::uint32_t* const mailbox_unpack = reinterpret_cast<volatile std::uint32_t*>(0x1FFF4);
    volatile std::uint32_t* const mailbox_math   = reinterpret_cast<volatile std::uint32_t*>(0x1FFF8);
    volatile std::uint32_t* const mailbox_pack   = reinterpret_cast<volatile std::uint32_t*>(0x1FFFC);

    LLK_ASSERT(ckernel::load_blocking(mailbox_brisc) == 0, "BRISC is not zero initialized");
    LLK_ASSERT(ckernel::load_blocking(mailbox_unpack) == 0, "UNPACK is not zero initialized");
    LLK_ASSERT(ckernel::load_blocking(mailbox_math) == 0, "MATH is not zero initialized");
    LLK_ASSERT(ckernel::load_blocking(mailbox_pack) == 0, "PACK is not zero initialized");

    // Setup the device and let triscs start running
    device_setup();
    clear_trisc_soft_reset();

    // Wait for the TRISC cores to finish running
    while (ckernel::load_blocking(mailbox_unpack) != 0xFF)
        ;
    while (ckernel::load_blocking(mailbox_math) != 0xFF)
        ;
    while (ckernel::load_blocking(mailbox_pack) != 0xFF)
        ;

    LLK_ASSERT(ckernel::load_blocking(mailbox_unpack) == 0xFF, "UNPACK is not done");
    LLK_ASSERT(ckernel::load_blocking(mailbox_math) == 0xFF, "MATH is not done");
    LLK_ASSERT(ckernel::load_blocking(mailbox_pack) == 0xFF, "PACK is not done");

    // Set the TRISC cores to be in reset
    set_trisc_soft_reset();

    // Reset mailboxes fror trisc communication
    ckernel::store_blocking(mailbox_unpack, 0);
    ckernel::store_blocking(mailbox_math, 0);
    ckernel::store_blocking(mailbox_pack, 0);

    // Signal to the host that we are done
    ckernel::store_blocking(mailbox_brisc, 0xFF);

#endif
}
