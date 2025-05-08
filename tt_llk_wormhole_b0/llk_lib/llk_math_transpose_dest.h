// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_globals.h"
#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_math_common.h"

using namespace ckernel;

// local function declarations
template <bool is_32bit>
inline void transpose_dest_configure_addrmod();
template <bool is_32bit>
inline void transpose_dest_configure_mop();

template <bool is_32bit = false>
inline void _llk_math_transpose_dest_(const std::uint32_t dst_index)
{
    math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(dst_index);

    TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::WAIT_SFPU);

    if constexpr (is_32bit)
    {
#pragma GCC unroll 2
        for (int offset = 16; offset < 32; offset += 8) {
            TTI_REPLAY(offset + 4, 4, 0, 0);
            TTI_TRNSPSRCB;
            TTI_REPLAY(offset + 0, 8, 0, 0);
            TTI_TRNSPSRCB;
            TTI_REPLAY(offset + 0, 8, 0, 0);
            TTI_TRNSPSRCB;
            TTI_REPLAY(offset + 0, 8, 0, 0);
            TTI_TRNSPSRCB;
            TTI_REPLAY(offset + 0, 4, 0, 0);

            math::clear_dst_reg_addr();
        }
        ckernel_template::run(instrn_buffer);
    }
    else
    {
        ckernel_unpack_template::run(instrn_buffer, 2, 2);
    }

    TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_ABD);
}

template <bool is_32bit>
inline void transpose_dest_configure_addrmod()
{
    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 16},
    }
        .set(ADDR_MOD_0);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_1);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = is_32bit ? 2 : 0x3ff & -16},
    }
        .set(ADDR_MOD_2);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 32},
    }
        .set(ADDR_MOD_3);
}

template <bool is_32bit>
inline void transpose_dest_configure_mop()
{
    if (is_32bit)
    {
        TTI_REPLAY(16, 16, 0, 1);

#pragma GCC unroll 2
        for (int dest_32b_lo = 0; dest_32b_lo < 2; ++dest_32b_lo)
        {
            TTI_MOVB2D(dest_32b_lo, 16, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 0);
            TTI_MOVB2D(dest_32b_lo, 20, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 4);
            TTI_MOVB2D(dest_32b_lo, 24, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 8);
            TTI_MOVB2D(dest_32b_lo, 28, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 12);
            TTI_MOVD2B(dest_32b_lo, 16, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 0);
            TTI_MOVD2B(dest_32b_lo, 20, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 4);
            TTI_MOVD2B(dest_32b_lo, 24, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 8);
            TTI_MOVD2B(dest_32b_lo, 28, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 12);
        }

        uint A = TT_OP_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::INT32, ADDR_MOD_1, 16);
        uint B = TT_OP_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::INT32, ADDR_MOD_1, 32);
        uint C = TT_OP_SFPSTORE(p_sfpu::LREG1, InstrModLoadStore::INT32, ADDR_MOD_1, 16);
        uint D = TT_OP_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::INT32, ADDR_MOD_2, 32);

        // Runs {A,B,C,D} 8 times, swapping the 2nd and 3rd tiles.
        ckernel_template tmp(8, 1, B, C);
        tmp.set_start_op(A);
        tmp.set_end_op(D);
        tmp.program(instrn_buffer);
    }
    else
    {
        TTI_REPLAY(16, 15, 0, 1);

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
        TTI_MOVA2D(0, 0, ADDR_MOD_2, p_mova2d::MOV_8_ROWS, 0x3ff & (0 - 32)); // dst -= 16
        TTI_MOVA2D(0, 8, ADDR_MOD_2, p_mova2d::MOV_8_ROWS, 0x3ff & (8 - 16)); // dst -= 16

        uint EFGHIJKLM   = TT_OP_REPLAY(20, 9, 0, 0);
        uint EFGHI       = TT_OP_REPLAY(20, 5, 0, 0);
        uint ABCDEFG     = TT_OP_REPLAY(16, 7, 0, 0);
        uint P           = TT_OP_MOVD2B(0, 28, ADDR_MOD_2, p_movd2b::MOV_4_ROWS, 12); // dst -= 16
        uint IJKL        = TT_OP_REPLAY(24, 4, 0, 0);
        uint Q           = TT_OP_MOVB2D(0, 28, ADDR_MOD_3, p_movb2d::MOV_4_ROWS, 12); // dst += 32
        uint EFGHIJKLMNO = TT_OP_REPLAY(20, 11, 0, 0);

        ckernel_unpack_template tmp(true, true, EFGHIJKLM, EFGHI, ABCDEFG, P, /* skip A */ Q, /* B */ IJKL, /* skip B */ EFGHIJKLMNO);
        tmp.program(instrn_buffer);
    }
}

template <bool is_32bit = false>
inline void _llk_math_transpose_dest_init_()
{
    transpose_dest_configure_addrmod<is_32bit>();
    transpose_dest_configure_mop<is_32bit>();
}
