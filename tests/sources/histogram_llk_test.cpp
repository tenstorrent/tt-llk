// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "../../tt_llk_wormhole_b0/llk_lib/llk_histogram.h"
#include "build.h"
#include "ckernel.h"
#include "llk_defs.h"

#ifdef LLK_TRISC_UNPACK
static constexpr uint32_t core_idx = 0;
#endif

#ifdef LLK_TRISC_MATH
static constexpr uint32_t core_idx = 1;
#endif

#ifdef LLK_TRISC_PACK
static constexpr uint32_t core_idx = 2;
#endif

#ifdef LLK_BRISC
static constexpr uint32_t core_idx = 3;
#endif

// Test configuration - using same values as original test
static constexpr uint32_t buffer_size     = HISTOGRAM_BUFFER_SIZE;
static constexpr uint32_t num_cores       = HISTOGRAM_NUM_CORES;
static constexpr uint32_t pipeline_factor = HISTOGRAM_PIPELINE_FACTOR;
static constexpr uint32_t version         = HISTOGRAM_VERSION;

// Only version 18 is supported by the LLK
static_assert(version == 18, "LLK histogram test only supports version 18");

#if defined(LLK_TRISC_UNPACK) || HISTOGRAM_NUM_CORES > 1

void run_kernel()
{
    // Only unpack core runs the histogram computation
    // Other cores do nothing in this LLK-based test
#ifdef LLK_TRISC_UNPACK
    // The LLK histogram function handles everything:
    // - Register setup and stack management
    // - Address configuration
    // - Synchronization (wait_start)
    // - Timing measurement (start/stop measuring)
    // - The complete histogram computation pipeline
    // - Cleanup and restoration

    llk_histogram_run();
#endif
}

#endif

// Math core - empty for single core configurations
#if defined(LLK_TRISC_MATH) && HISTOGRAM_NUM_CORES == 1
void run_kernel()
{
    // Math core does nothing in single-core histogram test
}
#endif

// Pack core - empty for single core configurations
#if defined(LLK_TRISC_PACK) && HISTOGRAM_NUM_CORES == 1
void run_kernel()
{
    // Pack core does nothing in single-core histogram test
}
#endif

// BRISC core - empty for single core configurations
#if defined(LLK_BRISC) && HISTOGRAM_NUM_CORES == 1
void run_kernel()
{
    // BRISC core does nothing in single-core histogram test
}
#endif
