// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include "ckernel.h"

#define ASSERT(condition, message)  \
    do                              \
    {                               \
        if (!LIKELY(condition))     \
        {                           \
            asm volatile("ebreak"); \
            UNREACHABLE();          \
        }                           \
    } while (0)
