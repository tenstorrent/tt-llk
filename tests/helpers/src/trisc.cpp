// SPDX-FileCopyrightText: © 2024 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>

#include "ckernel.h"
#ifndef ARCH_QUASAR
#include "ckernel_globals.h" // Only for WH/BH
#include "llk_assert.h"
// Necessary for ckernel variables
#include "ckernel_helper.h" // Only for WH/BH
#endif
#include "boot.h"
#include "profiler.h"

#ifdef LLK_PROFILER

namespace llk_profiler
{
barrier_ptr_t barrier_ptr   = reinterpret_cast<barrier_ptr_t>(BARRIER_START);
buffer_ptr_t buffer         = reinterpret_cast<buffer_ptr_t>(BUFFERS_START);
std::uint32_t write_idx     = 0;
std::uint32_t open_zone_cnt = 0;

} // namespace llk_profiler

#endif

// Mailbox addresses
#ifdef COVERAGE
extern "C"
{
    extern void gcov_dump(void);
}
constexpr std::uint32_t mailboxes_start = 0x6DFC0;
#else
constexpr std::uint32_t mailboxes_start = 0x1FFC0;
#endif

#if defined(LLK_TRISC_UNPACK)
extern void (*kernel_main)(void)[] = void(*__kernel_main_unpack[])(void);
#elif defined(LLK_TRISC_MATH)
extern void (*kernel_main)(void)[] = void(*__kernel_main_unpack[])(void);
#elif defined(LLK_TRISC_PACK)
extern void (*kernel_main)(void)[] = __kernel_main_pack;
#endif

__attribute__((no_profile_instrument_function)) int main(void)
{
    volatile std::uint32_t temp_var = 0;

    while (temp_var == 0)
    {
        kernel_main();
#ifdef COVERAGE
        gcov_dump();
#endif
    }
}

extern "C" __attribute__((section(".init"), naked, noreturn, no_profile_instrument_function)) std::uint32_t _start()
{
    do_crt0();

    main();

    for (;;)
    {
    } // Loop forever
}
