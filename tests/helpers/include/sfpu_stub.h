// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
//
// Stub for SFPU TRISC when test does not use the 4-TRISC SFPU pipeline.
// Include at end of Quasar test sources.

#ifdef LLK_TRISC_SFPU

#include "params.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
    (void)params;
    // Stub: SFPU TRISC not used in this test; trisc.cpp will signal completion
}

#endif
