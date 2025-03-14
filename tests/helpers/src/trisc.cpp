// SPDX-FileCopyrightText: © 2024 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>

#include "ckernel.h"
#include "ckernel_addr_map.h"
#include "ckernel_globals.h"
#include "ckernel_main.h"
#include "ckernel_pcbuf.h"
// Necessary for ckernel variables
#include "ckernel_helper.h"
#include "params.h"

int main()
{
    volatile std::uint64_t *TIMESTAMP_ADDRESS = reinterpret_cast<volatile std::uint64_t*>(0x19000);
#if defined(LLK_TRISC_UNPACK)
    static constexpr uint32_t core_idx = 0;
    volatile std::uint32_t* mailbox = reinterpret_cast<volatile std::uint32_t*>(0x19FFC);
#elif defined(LLK_TRISC_MATH)
    static constexpr uint32_t core_idx = 1;
    volatile std::uint32_t* mailbox = reinterpret_cast<volatile std::uint32_t*>(0x19FF8);
#elif defined(LLK_TRISC_PACK)
    static constexpr uint32_t core_idx = 2;
    volatile std::uint32_t* mailbox = reinterpret_cast<volatile std::uint32_t*>(0x19FF4);
#endif
    *mailbox = 0xDEADBEEF;
    
    volatile std::uint32_t* mb1 = reinterpret_cast<volatile std::uint32_t*>(0x19FF4);
    volatile std::uint32_t* mb2 = reinterpret_cast<volatile std::uint32_t*>(0x19FF8);
    volatile std::uint32_t* mb3 = reinterpret_cast<volatile std::uint32_t*>(0x19FFC);

    while(*mb1 != 0xDEADBEEF && *mb2 != 0xDEADBEEF && *mb3 != 0xDEADBEEF)
    {
    }
    uint64_t wall_clock = ckernel::read_wall_clock();
    
    *(TIMESTAMP_ADDRESS + core_idx * 2) = wall_clock;
    *mailbox = 0x2; // write value different than 1 to mailbox to indicate kernel is running

    std::fill(ckernel::regfile, ckernel::regfile + 64, 0);
    *mailbox = 0x3;
    ckernel::reset_cfg_state_id();
    *mailbox = 0x4;
    ckernel::reset_dest_offset_id();
    *mailbox = 0x5;
    ckernel::tensix_sync();
    *mailbox = 0x6;
    run_kernel();
    *mailbox = 0x7;
    *mailbox = ckernel::KERNEL_COMPLETE; // 0x1

    // Use a volatile variable to prevent the compiler from optimizing away the loop
    volatile bool run = true;

    // Infinite loop
    while (run)
    {
    }
}
