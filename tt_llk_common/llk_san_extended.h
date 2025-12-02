// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_san_backend.h"

// Goes in LLK_LIB in Init and Uninit
// Set or clear extended state mask bits
// If set, check that set mask covers all ones currently set in state
template <bool clear = false, typename... Ts>
llk_san_extended_state_mask(Ts... args);
