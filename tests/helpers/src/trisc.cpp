// SPDX-FileCopyrightText: © 2024 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>

#include "ckernel.h"
#ifndef ARCH_QUASAR
#include "ckernel_globals.h" // Only for WH/BH
// Necessary for ckernel variables
#include "ckernel_helper.h" // Only for WH/BH
#endif
#include "boot.h"
#include "profiler.h"

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
extern void run_unpack_kernel(const volatile struct RuntimeParams* params);
extern void run_math_kernel(const volatile struct RuntimeParams* params);
extern void run_pack_kernel(const volatile struct RuntimeParams* params);

thread_local uint32_t TRISC_ID;

void (*kernel_array[])(const volatile struct RuntimeParams* params) = {run_unpack_kernel, run_math_kernel, run_pack_kernel};

enum KERNEL_THREAD
{
    UNPACK = 0,
    MATH   = 1,
    PACK   = 2,
};

int main(void)
{
    volatile std::uint32_t* mailbox = nullptr;

    switch (static_cast<KERNEL_THREAD>(TRISC_ID))
    {
        case UNPACK:
            device_setup();
            clear_trisc_soft_reset();
            mailbox = reinterpret_cast<volatile std::uint32_t*>(0x19FFC);
            break;
        case MATH:
            mailbox = reinterpret_cast<volatile std::uint32_t*>(0x19FF8);
            break;
        case PACK:
            mailbox = reinterpret_cast<volatile std::uint32_t*>(0x19FF4);
            break;
    }

    std::fill(ckernel::regfile, ckernel::regfile + 64, 0);
#ifndef ARCH_QUASAR
    ckernel::reset_cfg_state_id();
    ckernel::reset_dest_offset_id();
#endif

#if defined(LLK_PROFILER)
    llk_profiler::reset();
    llk_profiler::sync_threads();
#endif

    {
        ZONE_SCOPED("KERNEL")
        kernel_array[static_cast<KERNEL_THREAD>(TRISC_ID)](__runtime_args_start);
        ckernel::tensix_sync();
    }

    *mailbox = ckernel::KERNEL_COMPLETE; // 0x1
}
