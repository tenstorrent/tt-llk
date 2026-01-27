// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_globals.h"
#include "ckernel_include.h"
#include "llk_defs.h"
#include "llk_operands.h"

using namespace ckernel;

/*************************************************************************
 * LLK MUL REDUCE SCALAR UNPACK - Unpacker operations for fused mul+reduce
 *************************************************************************/

/**
 * @brief Switch UNPACK state for mul_reduce_scalar reduce phase
 *
 * Resets UNPACK counters and sets DVALID flags to prepare for the reduce phase
 * where the math thread will reuse destination registers as source operands.
 *
 * This function:
 * - Resets UNPACK address counters
 * - Signals context switch via semaphore
 * - Sets DVALID flags for srcA and srcB
 *
 * Should be called after multiply phase and before reduce phase.
 */
inline void _llk_unpack_mul_reduce_scalar_switch_to_reduce_()
{
    // Reset UNPACK counters to prepare for reusing destination as source
    TTI_SETADCZW(0b011, 0, 0, 0, 0, 0b1111);

    // Signal context switch
    semaphore_post(semaphore::UNPACK_SYNC);

    // Set DVALID flags for srcA and srcB (math thread will use dest->src)
    TTI_UNPACR_NOP(SrcA, 0, 0, p_unpacr_nop::SET_DVALID, 0, 0, 0, 0, p_unpacr_nop::UNP_ZEROSRC);
    TTI_UNPACR_NOP(SrcB, 0, 0, p_unpacr_nop::SET_DVALID, 0, 0, 0, 0, p_unpacr_nop::UNP_ZEROSRC);

    // Wait for context switch to complete
    t6_semaphore_get(semaphore::UNPACK_SYNC);
}
