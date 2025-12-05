
// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#ifdef LLK_TRISC_UNPACK

void run_kernel()
{
    return;
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_assert.h"

void run_kernel()
{
    LLK_ASSERT(false, "Device assertion");
}

#endif

#ifdef LLK_TRISC_PACK

void run_kernel()
{
    return;
}
#endif
