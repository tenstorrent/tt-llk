// SPDX-FileCopyrightText: © 2024 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>

#ifdef LLK_BOOT_MODE_BRISC
#include "boot.h"
#include "counters.h"
#endif

int main()
{
#ifdef LLK_BOOT_MODE_BRISC
    device_setup();

    // Pre-configure performance counter banks for all zones before TRISCs start.
    // If no counter config is written to L1, this is a no-op.
    llk_perf::configure_perf_counters_from_brisc();

    // Release reset of triscs here in order to achieve brisc <-> trisc synchronization
    clear_trisc_soft_reset();
#endif
}
