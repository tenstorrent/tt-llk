// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>

#include "ckernel.h"
#include "ckernel_instr_params.h"
#include "ckernel_ops.h"

#ifdef LLK_TRISC_UNPACK
void run_kernel()
{
}
#endif

#ifdef LLK_TRISC_MATH

using namespace ckernel;

void run_kernel()
{
    TTI_SETRWC(0, 0, 0x10, 0x08, 0x04, p_setrwc::SET_ABD); // rwc_a=4, rwc_b=8, rwc_d=16

    // Maximum values (check bit widths)
    // TTI_SETRWC(0, 0, 0x3FF, 0x3F, 0x3F, p_setrwc::SET_ABD);  // rwc_a=63 (6-bit max), rwc_b=63, rwc_d=1023 (10-bit max)

    // Alternating bit patterns
    // TTI_SETRWC(0, 0, 0x2AA, 0x2A, 0x15, p_setrwc::SET_ABD); // rwc_a=0x15 (10101), rwc_b=0x2A (101010), rwc_d=0x2AA (1010101010)

    // Power of 2 values
    // TTI_SETRWC(0, 0, 0x200, 0x20, 0x10, p_setrwc::SET_ABD);
    // rwc_a=16, rwc_b=32, rwc_d=512

    // Zero all counters
    // TTI_SETRWC(0, 0, 0, 0, 0, p_setrwc::SET_ABD);   //rwc_a=0, rwc_b=0, rwc_d=0

    // Set only srcA counter
    // TTI_SETRWC(0, 0, 0, 0, 0x3F, p_setrwc::SET_A); //rwc_a=63, rwc_b=unchanged, rwc_d=unchanged

    ckernel::tensix_sync();
}

#endif

#ifdef LLK_TRISC_PACK
void run_kernel()
{
}
#endif
