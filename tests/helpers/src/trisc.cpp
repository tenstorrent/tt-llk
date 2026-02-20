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
#include "profiler.h"

#if defined(LLK_TRISC_UNPACK) && defined(LLK_BOOT_MODE_TRISC)
#include "boot.h"
#endif

#ifdef LLK_PROFILER

namespace llk_profiler
{
barrier_ptr_t barrier_ptr   = reinterpret_cast<barrier_ptr_t>(BARRIER_START);
buffer_ptr_t buffer         = reinterpret_cast<buffer_ptr_t>(BUFFERS_START);
std::uint32_t write_idx     = 0;
std::uint32_t open_zone_cnt = 0;

} // namespace llk_profiler

#endif

extern const volatile struct RuntimeParams __runtime_args_start[];
extern void run_kernel(const volatile struct RuntimeParams* params);

// Debug breadcrumb addresses in L1 for diagnosing soft resets
// Each TRISC gets its own address to track execution state
#if defined(LLK_TRISC_UNPACK)
constexpr std::uint32_t DEBUG_STATE_ADDR    = 0x64FF8;
constexpr std::uint32_t DEBUG_LOOP_CNT_ADDR = 0x65004; // Loop counter for trisc0
#elif defined(LLK_TRISC_MATH)
constexpr std::uint32_t DEBUG_STATE_ADDR    = 0x64FFC;
constexpr std::uint32_t DEBUG_LOOP_CNT_ADDR = 0x65008; // Loop counter for trisc1
#elif defined(LLK_TRISC_PACK)
constexpr std::uint32_t DEBUG_STATE_ADDR    = 0x65000;
constexpr std::uint32_t DEBUG_LOOP_CNT_ADDR = 0x6500C; // Loop counter for trisc2
#endif

// Debug marker values to track execution progress
// High nibble encodes restart count (0xN), low 3 nibbles encode state
constexpr std::uint32_t TRISC_MARKER_ENTRY         = 0xDEAD0001;
constexpr std::uint32_t TRISC_MARKER_AFTER_SETUP   = 0xDEAD0002;
constexpr std::uint32_t TRISC_MARKER_AFTER_UNRESET = 0xDEAD0003;
constexpr std::uint32_t TRISC_MARKER_BEFORE_INIT   = 0xDEAD0004;
constexpr std::uint32_t TRISC_MARKER_AFTER_INIT    = 0xDEAD0005;
constexpr std::uint32_t TRISC_MARKER_BEFORE_KERNEL = 0xDEAD0006;
constexpr std::uint32_t TRISC_MARKER_AFTER_KERNEL  = 0xDEAD0007;
constexpr std::uint32_t TRISC_MARKER_AFTER_SYNC    = 0xDEAD0008;
constexpr std::uint32_t TRISC_MARKER_DONE          = 0xDEAD0009;

int main()
{
    volatile std::uint32_t* const debug_state = reinterpret_cast<volatile std::uint32_t*>(DEBUG_STATE_ADDR);
    volatile std::uint32_t* const loop_cnt    = reinterpret_cast<volatile std::uint32_t*>(DEBUG_LOOP_CNT_ADDR);

    // Increment loop counter to detect restarts (soft resets)
    // If this value is > 1 after a run, the TRISC was restarted
    std::uint32_t current_loop = ckernel::load_blocking(loop_cnt);
    ckernel::store_blocking(loop_cnt, current_loop + 1);

    ckernel::store_blocking(debug_state, TRISC_MARKER_ENTRY);

#if defined(LLK_TRISC_UNPACK) && defined(LLK_BOOT_MODE_TRISC) // Just in case
    device_setup();
    ckernel::store_blocking(debug_state, TRISC_MARKER_AFTER_SETUP);
    // Release the rest of the triscs
    clear_trisc_soft_reset();
    ckernel::store_blocking(debug_state, TRISC_MARKER_AFTER_UNRESET);
#endif

#if defined(LLK_TRISC_UNPACK)
    volatile std::uint32_t* const mailbox = reinterpret_cast<volatile std::uint32_t*>(0x1FFF4);
#elif defined(LLK_TRISC_MATH)
    volatile std::uint32_t* const mailbox = reinterpret_cast<volatile std::uint32_t*>(0x1FFF8);
#elif defined(LLK_TRISC_PACK)
    volatile std::uint32_t* const mailbox = reinterpret_cast<volatile std::uint32_t*>(0x1FFFC);
    ;
#endif
    ckernel::store_blocking(debug_state, TRISC_MARKER_BEFORE_INIT);
    // Before setup
    ckernel::store_blocking(mailbox, 0xBD);
    std::fill(ckernel::regfile, ckernel::regfile + 64, 0);
#ifndef ARCH_QUASAR
    ckernel::reset_cfg_state_id();
    ckernel::reset_dest_offset_id();
#endif

#if defined(LLK_PROFILER)
    llk_profiler::reset();
    llk_profiler::sync_threads();
#endif
    ckernel::store_blocking(debug_state, TRISC_MARKER_AFTER_INIT);

    // Before Execution
    ckernel::store_blocking(mailbox, 0xBF);
    ckernel::store_blocking(debug_state, TRISC_MARKER_BEFORE_KERNEL);
    {
        ZONE_SCOPED("KERNEL")
        run_kernel(__runtime_args_start);
        ckernel::store_blocking(debug_state, TRISC_MARKER_AFTER_KERNEL);
        ckernel::store_blocking(mailbox, 0xAF);
        ckernel::tensix_sync();
        ckernel::store_blocking(debug_state, TRISC_MARKER_AFTER_SYNC);
        // Code Done
        ckernel::store_blocking(mailbox, 0xCD);
    }

    ckernel::store_blocking(debug_state, TRISC_MARKER_DONE);
    ckernel::store_blocking(mailbox, 0xFF);
}
