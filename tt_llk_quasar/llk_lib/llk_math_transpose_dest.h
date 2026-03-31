// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
// SPDX-FileCopyrightText: © 2026 Jason Davies <jason@jasondavies.com>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "llk_math_common.h"

using namespace ckernel;

// Notes on these template parameters:
// 1. <transpose_of_faces=false, is_32bit=false>: not supported.
// 2. <transpose_of_faces=false, is_32bit=true>: 4x 16x16 face transpose; can be combined with _llk_unpack_A_ with transpose_of_faces=true.
// 3. <transpose_of_faces=true, is_32bit=false>: the default case (full 32x32 tile transpose, non-32-bit).
// 4. <transpose_of_faces=true, is_32bit=true>: full 32x32 tile transpose for 32-bit.
template <bool transpose_of_faces, bool EN_32BIT_DEST>
inline void _llk_math_transpose_dest_(const std::uint32_t dst_index)
{
    _set_dst_write_addr_<DstTileShape::Tile32x32>(dst_index);

    // Wait condition SRCA_VLD is required as MOVB2A doesn't automatically wait
    // for SrcA[MatrixUnit.SrcABank].AllowedClient == SrcClient::MatrixUnit.
    // Wait condition SRCB_VLD is required as MOVD2B doesn't automatically wait
    // for SrcB[MatrixUnit.SrcBBank].AllowedClient == SrcClient::MatrixUnit.
    TTI_STALLWAIT(p_stall::STALL_MATH, 0, p_stall::SRCB_VLD, p_stall::SRCA_VLD);

    if constexpr (EN_32BIT_DEST)
    {
        if constexpr (transpose_of_faces)
        {
        }
        else
        {
        }
    }
    else
    {
        ckernel::ckernel_template::run_bank0_sw_cntl(instrn_buffer);
    }

    _reset_counters_<p_setrwc::SET_ABD_F>();
}

template <bool EN_32BIT_DEST>
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
        .dest = {.incr = EN_32BIT_DEST ? 2 : 0x3ff & -16},
    }
        .set(ADDR_MOD_2);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 32},
    }
        .set(ADDR_MOD_3);
}

template <bool transpose_of_faces, bool EN_32BIT_DEST>
inline void transpose_dest_configure_mop()
{
    if (EN_32BIT_DEST)
    {
        if (transpose_of_faces)
        {
        }
    }
    else
    {
        load_replay_buf<0, 18>(
            []
            {
                // CDEF
                TTI_MOVD2B(0, 0, ADDR_MOD_1, p_movd2b::MOV_8_ROWS, 1 /*transpose*/, 0);
                TTI_MOVD2B(0, 8, ADDR_MOD_1, p_movd2b::MOV_8_ROWS, 1, 8);

                TTI_MOVB2D(0, 0, ADDR_MOD_1, p_mov_src_to_dest::MOV_8_ROWS, 0 /*bcast*/, 0);
                TTI_MOVB2D(0, 8, ADDR_MOD_0, p_mov_src_to_dest::MOV_8_ROWS, 0, 8); // dst += 16

                // CD
                TTI_MOVD2B(0, 0, ADDR_MOD_1, p_movd2b::MOV_8_ROWS, 1 /*transpose*/, 0);
                TTI_MOVD2B(0, 8, ADDR_MOD_1, p_movd2b::MOV_8_ROWS, 1, 8);

                // ABC
                TTI_MOVB2A(0, ADDR_MOD_1, p_movb2a::MOV_8_ROWS, 0);
                TTI_MOVB2A(8, ADDR_MOD_0, p_movb2a::MOV_8_ROWS, 8); // dst += 16

                TTI_MOVD2B(0, 0, ADDR_MOD_1, p_movd2b::MOV_8_ROWS, 1 /*transpose*/, 0);

                // P
                TTI_MOVD2B(0, 8, ADDR_MOD_2, p_movd2b::MOV_8_ROWS, 1 /*transpose*/, 8); // dst -= 16

                // E
                TTI_MOVB2D(0, 0, ADDR_MOD_1, p_mov_src_to_dest::MOV_8_ROWS, 0 /*bcast*/, 0);

                // Q
                TTI_MOVB2D(0, 8, ADDR_MOD_3, p_mov_src_to_dest::MOV_8_ROWS, 0 /*bcast*/, 8); // dst += 32

                // CDEF
                TTI_MOVD2B(0, 0, ADDR_MOD_1, p_movd2b::MOV_8_ROWS, 1 /*transpose*/, 0);
                TTI_MOVD2B(0, 8, ADDR_MOD_1, p_movd2b::MOV_8_ROWS, 1, 8);

                TTI_MOVB2D(0, 0, ADDR_MOD_1, p_mov_src_to_dest::MOV_8_ROWS, 0 /*bcast*/, 0);
                TTI_MOVB2D(0, 8, ADDR_MOD_0, p_mov_src_to_dest::MOV_8_ROWS, 0, 8); // dst += 16

                // GH
                // Note: the 0x3ff mask ensures the negative offsets are 10 bits.
                TTI_MOVA2D(0 /*dest_32b_lo*/, 0, ADDR_MOD_2, p_mov_src_to_dest::MOV_8_ROWS, 0x3ff & (0 - 32)); // dst -= 16
                TTI_MOVA2D(0 /*dest_32b_lo*/, 8, ADDR_MOD_2, p_mov_src_to_dest::MOV_8_ROWS, 0x3ff & (8 - 16)); // dst -= 16
            });

        // The next 7 instructions expand as follows:
        // Face 0: 2x MOVD2B with transpose, 2x MOVB2D, dst += 16 (CDEF)
        // Face 1: 2x MOVD2B with transpose, 2x MOVB2A, dst += 16 (CD, AB..)
        // Face 2: 2x MOVD2B (dst -= 16) with transpose, 2x MOVB2D, dst += 32 (..C, P, E, Q)
        // Face 3: 2x MOVD2B with transpose, 2x MOVB2D, dst += 16 (CDEF..)
        // Face 1: 2x MOVA2D (2x dst -= 16) (..GH)

        // CDEF, CD, ABC, P, E, Q, CDEFGH
        ckernel_template temp(1 /*MOP_OUTER_LOOP*/, 1 /*MOP_INNER_LOOP*/, TT_OP_REPLAY(0, 18, 0, 0, 0, 0));
        temp.set_end_op(TT_OP_CLEARDVALID(p_cleardvalid::CLR_SRCAB_VLD, 0, 0, 0, 0, 0));
        temp.program_bank0_sw_cntl(instrn_buffer);
    }
}

template <bool transpose_of_faces, bool EN_32BIT_DEST>
inline void _llk_math_transpose_dest_init_()
{
    transpose_dest_configure_addrmod<EN_32BIT_DEST>();
    transpose_dest_configure_mop<transpose_of_faces, EN_32BIT_DEST>();

    // Reset all counters
    _reset_counters_<p_setrwc::SET_ABD_F>();
    // TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);
}
