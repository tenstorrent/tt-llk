// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "api/debug/dprint.h"
#include "llk_assert.h"
#include "llk_san_types.h"

#define LLK_SAN_ASSERT(condition, message)                    \
    do                                                        \
    {                                                         \
        if (UNLIKELY(!(condition)))                           \
        {                                                     \
            DPRINT << "LLK_SAN PANIC: " << message << ENDL(); \
        }                                                     \
    } while (0)

#define LLK_SAN_PANIC(condition, message) LLK_SAN_ASSERT(!(condition), message)

#define LLK_SAN_OP_ASSERT(lhs, rhs, message) LLK_SAN_ASSERT(llk_san::_assert_condition(lhs, rhs), message)

#define LLK_SAN_OP_PANIC(lhs, rhs, message) LLK_SAN_ASSERT(llk_san::_panic_condition(lhs, rhs), message)

#define LLK_SAN_PRINT(fmt, ...) // Something goes here
