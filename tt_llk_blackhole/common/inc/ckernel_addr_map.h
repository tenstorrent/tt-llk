// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

// Temp address map in bytes
#define TRISC0_PARAMS_BASE_ADDRESS 32 * 1024
#define TRISC1_PARAMS_BASE_ADDRESS 48 * 1024
#define TRISC2_PARAMS_BASE_ADDRESS 64 * 1024

std::uint32_tparams_base[3] = {TRISC0_PARAMS_BASE_ADDRESS, TRISC1_PARAMS_BASE_ADDRESS, TRISC2_PARAMS_BASE_ADDRESS};
