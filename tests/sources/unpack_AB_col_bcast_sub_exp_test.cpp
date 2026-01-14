// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * Test for unpackAB with column broadcast, subtraction, and fast-approximate exponential.
 * Modified: Packing moved from TRISC2 to TRISC1 after SFPU completion.
 *
 * Pipeline:
 * 1. Unpack srcA (regular unpack) - TRISC0/Unpacker
 * 2. Unpack srcB with column broadcast - TRISC0/Unpacker
 * 3. FPU subtraction: dest = srcA - broadcast_column(srcB) - TRISC1/Math
 * 4. SFPU fast-approx exponential on dest - TRISC2/Packer
 * 5. Pack result to L1 - TRISC1/Math (moved from TRISC2)
 *
 * Thread synchronization:
 * - Math thread signals done via _llk_math_dest_section_done_
 * - Packer waits for math via _llk_packer_wait_for_math_done_
 * - Packer runs SFPU exponential and signals math when done
 * - Math thread waits for SFPU completion then packs result
 *
 * Configuration:
 * - Format: Float16_b input and output
 * - Approximation mode: Yes (fast approximation)
 * - Fast mode: Parameterized (FAST_MODE template param)
 *   - false: Standard approximation algorithm
 *   - true: Schraudolph fast approximation algorithm
 * - Dest accumulation: No
 * - Tile count: 1 (single 32x32 tile = 1024 elements)
 * - Both inputs are tilized before being written to L1 (required for broadcast)
 */

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "ckernel.h"
#include "llk_defs.h"

// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

// NEW: Synchronization functions for SFPU-to-MATH signaling
inline void signal_sfpu_done_()
{
    ckernel::semaphore_post(ckernel::semaphore::UNPACK_MATH_DONE);
}

inline void wait_for_sfpu_done_()
{
    ckernel::semaphore_get(ckernel::semaphore::UNPACK_MATH_DONE);
}

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    // Configure hardware for unpacking AB (two inputs)
    // srcA: regular unpack
    // srcB: column broadcast
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src, formats.unpack_src, formats.unpack_dst, formats.unpack_dst, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);

    // Initialize unpack AB with column broadcast on srcB
    _llk_unpack_AB_init_<BROADCAST_TYPE>(FACE_R_DIM, 4 /* num_faces */, false /* narrow_tile */, 0 /* transpose_of_faces - no transpose on srcA */);

    // Unpack tiles: srcA regular, srcB with column broadcast
    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_unpack_AB_<BROADCAST_TYPE>(L1_ADDRESS(buffer_A[i]), L1_ADDRESS(buffer_B[i]));
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "llk_pack.h"        // NEW: Added pack headers
#include "llk_pack_common.h" // NEW: Added pack headers
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    // Initialize for FPU binary subtraction with column broadcast
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_(formats.math, formats.math);
    _llk_math_eltwise_binary_init_<ELTWISE_BINARY_OP, BROADCAST_TYPE, MATH_FIDELITY>(4 /* num_faces */, 0);

    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        // Wait for destination to be available
        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();

        // Perform FPU subtraction: dest = srcA - broadcast_col(srcB)
        _llk_math_eltwise_binary_<ELTWISE_BINARY_OP, BROADCAST_TYPE, DstSync::SyncHalf, is_fp32_dest_acc_en, MATH_FIDELITY, EltwiseBinaryReuseDestType::NONE>(
            4 /* num_faces */, i /* dst_index */, false /* clear_fp32_dst_acc */);
    }

    // Signal to packer (TRISC2) that math (subtraction) is done
    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

    // NEW: Wait for SFPU to complete on TRISC2
    wait_for_sfpu_done_();

    // NEW: Configure and run packing (moved from TRISC2)
#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(formats.pack_src, formats.pack_dst, 16 * 16 * 4);
#endif

    _llk_pack_init_<false, false>(formats.pack_dst);

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
#else
    _llk_pack_dest_init_<DstSync::SyncHalf, false, false>();
#endif

    // Pack the result after SFPU exponential
    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(i, L1_ADDRESS(buffer_Res[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif

// Include SFPU headers outside of conditional for packer thread access
#include "llk_math_eltwise_unary_sfpu.h"

#ifdef LLK_TRISC_PACK

#include "ckernel_sfpu.h"
#include "llk_pack_common.h"
#include "params.h"
#include "sfpu_operations.h"

using namespace ckernel;
using namespace ckernel::sfpu;

// Number of SFPU iterations - 32 iterations processes one full tile
const int iterations = 32;

void run_kernel(const volatile struct RuntimeParams *params)
{
    // Wait for math (subtraction) to complete
    _llk_packer_wait_for_math_done_();

    // SFPU CODE ON PACKER (only SFPU, no packing)
    _llk_math_eltwise_unary_sfpu_init_<SFPU_UNARY_OPERATION>();

    for (int i = 0; i < params->TILE_CNT; ++i)
    {
        _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(i);

        // Call SFPU exponential with:
        // - APPROX_MODE = true (approximation enabled)
        // - is_fp32_dest_acc_en = false (no dest accumulation, 16-bit mode)
        // - iterations = 32 (process full tile)
        // - FAST_MODE = template parameter (enable/disable Schraudolph fast approximation algorithm)

        if constexpr (FAST_MODE)
        {
            for (int j = 0; j < 4; j++)
            {
                test_utils::call_sfpu_operation<APPROX_MODE, is_fp32_dest_acc_en, iterations, FAST_MODE>(SFPU_UNARY_OPERATION, formats.math);
                TTI_INCRWC(0, 4, 0, 0);
                TTI_INCRWC(0, 4, 0, 0);
                TTI_INCRWC(0, 4, 0, 0);
                TTI_INCRWC(0, 4, 0, 0);
            }
        }
        else
        {
            test_utils::call_sfpu_operation<APPROX_MODE, is_fp32_dest_acc_en, iterations, FAST_MODE>(SFPU_UNARY_OPERATION, formats.math);
        }

        _llk_math_eltwise_unary_sfpu_done_();
    }

    // NEW: Signal to math thread that SFPU is done
    signal_sfpu_done_();
}

#endif
