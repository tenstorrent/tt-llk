// SPDX-FileCopyrightText: © 2024 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>

#ifdef LLK_BOOT_MODE_BRISC
#include "boot.h"
#endif

// Address used to signal the host that BRISC has finished device_setup().
// Sits just below the TRISC mailboxes (0x19FF4..0x19FFC).
static constexpr std::uint32_t BRISC_DONE_ADDR = 0x19FF0;

int main()
{
#ifdef LLK_BOOT_MODE_BRISC
    device_setup();

    // Signal host that hardware initialisation is complete.
    // The host will release the TRISCs from reset *after* it observes this
    // flag, guaranteeing that all TTI instructions pushed by device_setup()
    // (ZEROACC, SEMINIT, …) have been committed to the Tensix pipeline before
    // any TRISC begins execution.
    volatile std::uint32_t* brisc_done = reinterpret_cast<volatile std::uint32_t*>(BRISC_DONE_ADDR);
    *brisc_done                        = 1;

    // Do NOT release TRISCs from firmware – the host does it after reading
    // the flag above.  This eliminates the race between asynchronous TTI
    // execution and the debug-register write in clear_trisc_soft_reset().
#endif

    // Park: spin until the host puts us back in reset.
    while (true)
        ;
}
