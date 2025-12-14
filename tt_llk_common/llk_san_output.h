// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_assert.h"

#define LLK_SAN_PANIC(condition, message) LLK_ASSERT(!(condition), message)

#define LLK_SAN_PRINT(fmt, ...) // Something goes here
