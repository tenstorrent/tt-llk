// SPDX-FileCopyrightText: © 2024 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "ckernel.h"
#ifndef ARCH_QUASAR
#include "ckernel_globals.h" // Only for WH/BH
// Necessary for ckernel variables
#include "ckernel_helper.h" // Only for WH/BH
#endif
#include "params.h"
#include "profiler.h"

#if defined(LLK_TRISC_UNPACK) && defined(LLK_BOOT_MODE_TRISC)
#include "boot.h"
#endif

#ifdef LLK_PROFILER

namespace llk_profiler
{
barrier_ptr_t barrier_ptr = reinterpret_cast<barrier_ptr_t>(BARRIER_START);
buffer_ptr_t buffer       = reinterpret_cast<buffer_ptr_t>(BUFFERS_START);
uint32_t write_idx        = 0;
uint32_t open_zone_cnt    = 0;

} // namespace llk_profiler

#endif

extern const volatile struct RuntimeParams __runtime_args_start[];
extern void run_kernel(const struct RuntimeParams& params);

void copy_runtimes_from_L1(struct RuntimeParams* temp_args)
{
    struct RuntimeParams* src = (struct RuntimeParams*)__runtime_args_start;
    asm volatile("" : "=m"(*src));
    memcpy(temp_args, src, sizeof(struct RuntimeParams));
}

int main()
{
#if defined(LLK_TRISC_UNPACK) && defined(LLK_BOOT_MODE_TRISC)
    device_setup();
    // Release the rest of the triscs
    clear_trisc_soft_reset();
#endif

#if defined(COVERAGE)
    constexpr std::uint32_t mailboxes_start = 0x60FF4;
#else
    constexpr std::uint32_t mailboxes_start = 0x1FFF4;
#endif

#if defined(LLK_TRISC_UNPACK)
    volatile std::uint32_t* const mailbox = reinterpret_cast<volatile std::uint32_t*>(mailboxes_start + 2 * sizeof(std::uint32_t));
#elif defined(LLK_TRISC_MATH)
    volatile std::uint32_t* const mailbox = reinterpret_cast<volatile std::uint32_t*>(mailboxes_start + sizeof(std::uint32_t));
#elif defined(LLK_TRISC_PACK)
    volatile std::uint32_t* const mailbox = reinterpret_cast<volatile std::uint32_t*>(mailboxes_start);
#endif

    asm volatile("fence");
    *mailbox = ckernel::KERNEL_START_RUNTIME_LOADING;
    asm volatile("fence");

    struct RuntimeParams temp_args;
    copy_runtimes_from_L1(&temp_args);

    asm volatile("fence");
    *mailbox = ckernel::KERNEL_LOADED_RUNTIMES;
    asm volatile("fence");

    std::fill(ckernel::regfile, ckernel::regfile + 64, 0);
#ifndef ARCH_QUASAR
    ckernel::reset_cfg_state_id();
    ckernel::reset_dest_offset_id();
#endif

#if defined(LLK_PROFILER)
    llk_profiler::reset();
    llk_profiler::sync_threads();
#endif

    asm volatile("fence");
    *mailbox = ckernel::KERNEL_STARTED_MAIN;
    asm volatile("fence");

    {
        ZONE_SCOPED("KERNEL")
        run_kernel(temp_args);
        ckernel::tensix_sync();
    }

    asm volatile("fence");
    *mailbox = ckernel::KERNEL_FINNISHED_MAIN;
    asm volatile("fence");
    *mailbox = ckernel::KERNEL_COMPLETE;
    asm volatile("fence");
}
