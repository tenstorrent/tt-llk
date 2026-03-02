// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
// SPDX-FileCopyrightText: © 2026 Jason Davies <jason@jasondavies.com>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

// Non-LOADMACRO version of transpose_dest_configure_mop.
// For is_32bit && transpose_of_faces=true: the SFPU-based middle-face row swap macros
// are replaced by TT_OP_SFPNOP; face transpositions still work correctly.

template <bool transpose_of_faces, bool is_32bit>
inline void transpose_dest_configure_mop()
{
    if (is_32bit)
    {
        load_replay_buf(
            16,
            16,
            []
            {
#pragma GCC unroll 2
                for (int dest_32b_lo = 0; dest_32b_lo < 2; ++dest_32b_lo)
                {
                    TTI_MOVD2B(dest_32b_lo, 16, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 0);
                    TTI_MOVD2B(dest_32b_lo, 20, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 4);
                    TTI_MOVD2B(dest_32b_lo, 24, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 8);
                    TTI_MOVD2B(dest_32b_lo, 28, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 12);
                    TTI_MOVB2D(dest_32b_lo, 16, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 0);
                    TTI_MOVB2D(dest_32b_lo, 20, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 4);
                    TTI_MOVB2D(dest_32b_lo, 24, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 8);
                    TTI_MOVB2D(dest_32b_lo, 28, dest_32b_lo == 1 ? ADDR_MOD_0 : ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 12);
                }
            });

        // SFPU-based middle-face row swap is not available without LOADMACRO.
        // macro0 and macro1 are NOP; face transpositions still execute correctly.
        constexpr std::uint32_t macro0 = TT_OP_SFPNOP;
        constexpr std::uint32_t macro1 = TT_OP_SFPNOP;

        std::uint32_t movd2b_hi        = lltt::replay_insn(16, 4);
        std::uint32_t movb2d_hi_d2b_lo = lltt::replay_insn(20, 8);
        std::uint32_t movb2d_lo        = lltt::replay_insn(28, 4);
        std::uint32_t transpose        = TT_OP_TRNSPSRCB;

        ckernel_unpack_template tmp(true, true, movd2b_hi, transpose, movb2d_hi_d2b_lo, transpose, /* skip A */ macro0, /* B */ movb2d_lo, /* skip B */ macro1);
        tmp.program();
    }
    else
    {
        load_replay_buf(
            16,
            15,
            []
            {
                // ABCD
                TTI_MOVB2A(0, ADDR_MOD_1, p_movb2a::MOV_4_ROWS, 16);
                TTI_MOVB2A(4, ADDR_MOD_1, p_movb2a::MOV_4_ROWS, 20);
                TTI_MOVB2A(8, ADDR_MOD_1, p_movb2a::MOV_4_ROWS, 24);
                TTI_MOVB2A(12, ADDR_MOD_0, p_movb2a::MOV_4_ROWS, 28); // dst += 16

                // EFGHIJKLM
                TTI_MOVD2B(0, 16, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 0);
                TTI_MOVD2B(0, 20, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 4);
                TTI_MOVD2B(0, 24, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 8);
                TTI_MOVD2B(0, 28, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 12);
                TTI_TRNSPSRCB;
                TTI_MOVB2D(0, 16, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 0);
                TTI_MOVB2D(0, 20, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 4);
                TTI_MOVB2D(0, 24, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 8);
                TTI_MOVB2D(0, 28, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 12); // dst += 16

                // NO
                // Note: the 0x3ff mask ensures the negative offsets are 10 bits.
                TTI_MOVA2D(0, 0, ADDR_MOD_2, p_mova2d::MOV_8_ROWS, 0x3ff & (0 - 32)); // dst -= 16
                TTI_MOVA2D(0, 8, ADDR_MOD_2, p_mova2d::MOV_8_ROWS, 0x3ff & (8 - 16)); // dst -= 16
            });

        std::uint32_t EFGHIJKLM   = lltt::replay_insn(20, 9);
        std::uint32_t EFGHI       = lltt::replay_insn(20, 5);
        std::uint32_t ABCDEFG     = lltt::replay_insn(16, 7);
        std::uint32_t P           = TT_OP_MOVD2B(0, 28, ADDR_MOD_2, p_movd2b::MOV_4_ROWS, 12); // dst -= 16
        std::uint32_t IJKL        = lltt::replay_insn(24, 4);
        std::uint32_t Q           = TT_OP_MOVB2D(0, 28, ADDR_MOD_3, p_movb2d::MOV_4_ROWS, 12); // dst += 32
        std::uint32_t EFGHIJKLMNO = lltt::replay_insn(20, 11);

        ckernel_unpack_template tmp(true, true, EFGHIJKLM, EFGHI, ABCDEFG, P, /* skip A */ Q, /* B */ IJKL, /* skip B */ EFGHIJKLMNO);
        tmp.program();
    }
}
