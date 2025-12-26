// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_assert.h"
#include "llk_san_types.h"

#define LLK_SAN_ASSERT(lhs, rhs, message) LLK_ASSERT(llk_san::_assert_condition(lhs, rhs), message)

#define LLK_SAN_PANIC(lhs, rhs, message) LLK_ASSERT(llk_san::_panic_condition(lhs, rhs), message)

#define LLK_SAN_PRINT(fmt, ...) // Something goes here
