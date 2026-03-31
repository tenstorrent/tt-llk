// SPDX-FileCopyrightText: (c) 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>

#include "ckernel_trisc_common.h"
#include "cmath_common.h"

// SFPLOADMACRO optimization disabled for now: the Quasar SFPLOADMACRO pipeline
// macro configuration (InstructionTemplate selector encoding, shift register timing)
// needs further investigation. The basic path (DISABLE_SFPLOADMACRO) is functionally
// correct and passes all tests. The optimizer agent can re-enable this once the
// SFPLOADMACRO configuration is validated on Quasar hardware/simulator.
#ifndef DISABLE_SFPLOADMACRO
#define DISABLE_SFPLOADMACRO
#endif

namespace ckernel
{
namespace sfpu
{
template <bool APPROXIMATION_MODE>
inline void _init_where_()
{
    // Configure ADDR_MOD_6 with dest increment of 2 for replay buffer store path.
    // Each SFPSTORE using ADDR_MOD_6 advances the Dest RWC by 2, enabling the replay
    // buffer to iterate through consecutive row pairs without explicit address arithmetic.
    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 2},
    }
        .set(ADDR_MOD_6, csr_read<CSR::TRISC_ID>());

#ifndef DISABLE_SFPLOADMACRO
    // InstructionTemplate[0]: SFPSETCC(imm12=0, lreg_c=0, lreg_dest=12, mod1=6)
    // Backdoor load via VD=12 -> InstructionTemplate[0]
    // opcode=0x7b, params = (0<<12) + (0<<8) + (12<<4) + (6<<0) = 0xC6
    INSTRUCTION_WORD(TRISC_OP_SWIZZLE(TT_OP(0x7b, 0xC6)));

    // InstructionTemplate[1]: SFPENCC(imm12=0, lreg_c=0, lreg_dest=13, mod1=0)
    // Backdoor load via VD=13 -> InstructionTemplate[1]
    // opcode=0x8a, params = (0<<12) + (0<<8) + (13<<4) + (0<<0) = 0xD0
    INSTRUCTION_WORD(TRISC_OP_SWIZZLE(TT_OP(0x8a, 0xD0)));

    // Sequence[0] (Macro 0): SIMPLE sel=4 delay=0 (IT[0]=SFPSETCC), STORE sel=3 delay=2
    // Selector encoding: sel=4+IT_index (sel=4->IT[0], sel=5->IT[1]), sel=3 is store
    {
        constexpr std::uint32_t simple_bits = (0 << 3) | 4; // delay=0, sel=4 -> IT[0] (SFPSETCC)
        constexpr std::uint32_t mad_bits    = 0;
        constexpr std::uint32_t round_bits  = 0;
        constexpr std::uint32_t store_bits  = (2 << 3) | 3; // delay=2, sel=3 (store)

        TTI_SFPLOADI(p_sfpu::LREG0, 10, (mad_bits << 8) | simple_bits);
        TTI_SFPLOADI(p_sfpu::LREG0, 8, (store_bits << 8) | round_bits);
        TTI_SFPCONFIG(0, 4 + 0, 0);
    }

    // Sequence[1] (Macro 1): SIMPLE sel=4 delay=0 (IT[0]=SFPSETCC)
    TTI_SFPCONFIG(0x0004, 4 + 1, 1);

    // Sequence[2] (Macro 2): SIMPLE sel=5 delay=0 (IT[1]=SFPENCC)
    TTI_SFPCONFIG(0x0005, 4 + 2, 1);

    // Misc register: UsesLoadMod0ForStore + WaitForElapsedInstructions config
    TTI_SFPCONFIG(0x770, 8, 1);
#endif
}

// Implements element-wise conditional select: result = (condition == 0) ? false_value : true_value
// Operates on 3 input tiles (condition, true_value, false_value) at explicit Dest offsets
template <bool APPROXIMATION_MODE, DataFormat data_format, int ITERATIONS>
inline void _calculate_where_(
    const std::uint32_t dst_index_in0, const std::uint32_t dst_index_in1, const std::uint32_t dst_index_in2, const std::uint32_t dst_index_out)
{
    // Compute base Dest row offsets: each tile is 32 rows, <<1 for 16-bit addressing
    int offset0 = (dst_index_in0 * 32) << 1; // condition
    int offset1 = (dst_index_in1 * 32) << 1; // true_value
    int offset2 = (dst_index_in2 * 32) << 1; // false_value

#ifdef DISABLE_SFPLOADMACRO
    int offset3 = (dst_index_out * 32) << 1; // output

    TTI_SFPENCC(1, 2); // enable conditional execution globally

    // Record 6 instructions into replay buffer slot 0. The loop body uses:
    //   - ADDR_MOD_7 (dest.incr=0) for all loads: no Dest RWC advance on load
    //   - ADDR_MOD_6 (dest.incr=2) for the store: advances Dest RWC by 2 each iteration
    // The Dest RWC advance from the store shifts all subsequent addresses (loads + store)
    // by 2 rows per iteration, progressing through the tile without explicit d<<1 arithmetic.
    load_replay_buf<0, 6>(
        [offset0, offset1, offset2, offset3]()
        {
            // Load condition into LREG0
            TT_SFPLOAD(p_sfpu::LREG0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset0);

            // Load true_value into LREG1
            TT_SFPLOAD(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset1);

            // Set CC where LREG0 == 0 (condition is zero)
            TTI_SFPSETCC(0, p_sfpu::LREG0, 0x6);

            // Load false_value into LREG1 -- CC-gated, only writes to lanes where condition == 0
            TT_SFPLOAD(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset2);

            // Reset CC (all lanes active for store)
            TTI_SFPENCC(0, 0);

            // Store result from LREG1 to output (ADDR_MOD_6 advances Dest RWC by 2)
            TT_SFPSTORE(p_sfpu::LREG1, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_6, 0, offset3);
        });

    // Replay the recorded 6-instruction sequence for each iteration
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        TTI_REPLAY(0, 6, 0, 0, 0, 0);
    }

    TTI_SFPENCC(0, 2); // disable conditional execution globally

#else
    if (dst_index_out == dst_index_in0)
    {
        // Case 1: 3 cycles/row -- output overwrites condition input
        // Macros 0 and 2 schedule SFPSETCC and SFPENCC on Simple unit
        // Macro 0 also schedules SFPSTORE at delay=2

        load_replay_buf<0, 3>(
            [offset0, offset1, offset2]()
            {
                TT_SFPLOADMACRO(0, 0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset0, 0);
                TT_SFPLOADMACRO(2, 0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset1, 0);
                TT_SFPLOAD(0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_6, 0, offset2);
            });

#pragma GCC unroll 8
        for (int d = 0; d < ITERATIONS; d++)
        {
            TTI_REPLAY(0, 3, 0, 0, 0, 0);
        }
    }
    else
    {
        // Case 2: 4 cycles/row -- output is a different tile
        // Macros 1 and 2 schedule SFPSETCC and SFPENCC on Simple unit
        // SFPSTORE is issued explicitly
        int offset3 = (dst_index_out * 32) << 1;

        load_replay_buf<0, 4>(
            [offset0, offset1, offset2, offset3]()
            {
                TT_SFPLOADMACRO(1, 0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset0, 0);
                TT_SFPLOADMACRO(2, 0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset1, 0);
                TT_SFPLOAD(0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset2);
                TT_SFPSTORE(0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_6, 0, offset3);
            });

#pragma GCC unroll 8
        for (int d = 0; d < ITERATIONS; d++)
        {
            TTI_REPLAY(0, 4, 0, 0, 0, 0);
        }
    }
#endif
}

} // namespace sfpu
} // namespace ckernel
