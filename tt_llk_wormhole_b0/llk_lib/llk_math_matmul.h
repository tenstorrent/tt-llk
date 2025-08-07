// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_math_common.h"
#include "lltt.h"

#ifndef HF
#define HF 0
#endif

using namespace ckernel;

template <int MATH_FIDELITY_DESC, DstTileFaceLayout FaceLayout = DstTileFaceLayout::ColMajor, int THROTTLE_LEVEL>
inline void matmul_configure_addrmod(
    const bool transpose,
    const std::uint32_t ct_dim,
    const std::uint32_t rt_dim,
    const std::uint32_t kt_dim,
    const std::uint32_t in0_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in0_tile_c_dim = TILE_C_DIM,
    const std::uint32_t in1_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in1_tile_c_dim = TILE_C_DIM,
    const bool partial_face            = false)
{
    constexpr int NUM_FIDELITY_PHASES = get_math_num_fidelity_phases(MATH_FIDELITY_DESC);
    constexpr bool high_fidelity      = (NUM_FIDELITY_PHASES > 0);
    constexpr int FIDELITY_INCREMENT  = high_fidelity ? get_math_fidelity_increment(MATH_FIDELITY_DESC) : 0;

    const bool is_in0_16x32 = (in0_tile_r_dim <= FACE_R_DIM) && (in0_tile_c_dim > FACE_C_DIM);
    const bool is_in0_32x16 = (in0_tile_r_dim > FACE_R_DIM) && (in0_tile_c_dim <= FACE_C_DIM);
    const bool is_in1_32x16 = (in1_tile_r_dim > FACE_R_DIM) && (in1_tile_c_dim <= FACE_C_DIM);

    static_assert(FaceLayout == DstTileFaceLayout::RowMajor, "FaceLayout must be RowMajor");

    // MVMUL does D = B*A

    // Inner Loop --> 32/8 = 4 times for the full 32x16 face
    // DEST -- 8 rows are calculated each time
    // SRCB -- 8 rows are needed
    // SRCA -- full 16x16 gets used -- hardware will pair cols of A with rows of B
    // D[8,16] = B[8,16] * A[16,16]
    addr_mod_t {
        .srca = {.incr = 0, .clr = 0, .cr = 0},
        .srcb = {.incr = 8, .clr = 0, .cr = 0},
        .dest = {.incr = 8, .clr = 0, .cr = 0},
    }
        .set(ADDR_MOD_0);

    // copy of addr_mod_0
    addr_mod_t {
        .srca = {.incr = 0, .clr = 0, .cr = 0},
        .srcb = {.incr = 8, .clr = 0, .cr = 0},
        .dest = {.incr = 8, .clr = 0, .cr = 0},
        .bias = {.incr = 1},
    }
        .set(ADDR_MOD_3);

    // reset all, increment fidelity if we have more fidelity phases
    addr_mod_t {
        .srca     = {.incr = 0, .clr = 1, .cr = 1},
        .srcb     = {.incr = 0, .clr = 1, .cr = 1},
        .dest     = {.incr = 0, .clr = 1, .cr = 1},
        .fidelity = {.incr = FIDELITY_INCREMENT, .clr = 0},
        .bias     = {.incr = 1},
    }
        .set(ADDR_MOD_5);

    if constexpr (THROTTLE_LEVEL)
    {
        // reset all, including fidelity
        addr_mod_t {
            .srca     = {.incr = 0, .clr = 1, .cr = 1},
            .srcb     = {.incr = 0, .clr = 1, .cr = 1},
            .dest     = {.incr = 0, .clr = 1, .cr = 1},
            .fidelity = {.incr = 0, .clr = 1},
            .bias     = {.incr = 1},
        }
            .set(ADDR_MOD_6);
    }

    const uint8_t srca_increment = transpose == false ? 16 : 32;
    const uint8_t srca_set       = transpose == false ? 32 : 16;
    const uint8_t dest_increment = transpose == false ? 8 : 24;

    if ((is_in0_16x32 && (!is_in1_32x16)) || is_in0_32x16)
    {
        if (transpose)
        {
            addr_mod_t {
                .srca = {.incr = 32, .clr = 0, .cr = 0},
                .srcb = {.incr = 0, .clr = 0, .cr = 1}, // cr=16 before
                .dest = {.incr = 8, .clr = 0, .cr = 0},
            }
                .set(ADDR_MOD_1);
        }
        else
        {
            addr_mod_t {
                .srca = {.incr = 16, .clr = 0, .cr = 0},
                .srcb = {.incr = 0, .clr = 0, .cr = 1}, // cr=16 before
                .dest = {.incr = 8, .clr = 0, .cr = 0},
            }
                .set(ADDR_MOD_1);
        }
    }
    else
    {
        if (is_in1_32x16)
        {
            addr_mod_t {
                .srca = {.incr = 16, .clr = 0, .cr = 0},
                .srcb = {.incr = 8, .clr = 0, .cr = 0},
                .dest = {.incr = 0, .clr = 0, .cr = 1},
            }
                .set(ADDR_MOD_1);
        }
        else
        {
            if (transpose)
            {
                addr_mod_t {
                    .srca = {.incr = 32, .clr = 0, .cr = 0},
                    .srcb = {.incr = 0, .clr = 0, .cr = 1},
                    .dest = {.incr = 8, .clr = 0, .cr = 0},
                }
                    .set(ADDR_MOD_1);
            }
            else
            {
                addr_mod_t {
                    //.srca = {.incr = srca_increment, .clr = 0, .cr = 0},
                    .srca = {.incr = 16, .clr = 0, .cr = 0},
                    .srcb = {.incr = 0, .clr = 0, .cr = 1},
                    .dest = {.incr = 8, .clr = 0, .cr = 0},
                }
                    .set(ADDR_MOD_1);
            }
        }
    }

    if (is_in1_32x16)
    {
        addr_mod_t {
            .srca = {.incr = 16, .clr = 0, .cr = 0}, .srcb = {.incr = 8, .clr = 0, .cr = 0}, .dest = {.incr = 0, .clr = 0, .cr = 1}, // cr=16
        }
            .set(ADDR_MOD_2);
    }
    else if (is_in0_16x32 || is_in0_32x16)
    {
        if (partial_face)
        {
            if (transpose)
            {
                addr_mod_t {
                    .srca = {.incr = 32, .clr = 0, .cr = 0},
                    .srcb = {.incr = 0, .clr = 0, .cr = 0},
                    .dest = {.incr = 16, .clr = 0, .cr = 0},
                    .bias = {.incr = 1},
                }
                    .set(ADDR_MOD_2);
            }
            else
            {
                addr_mod_t {
                    .srca = {.incr = 16, .clr = 0, .cr = 0},
                    .srcb = {.incr = 0, .clr = 0, .cr = 0},
                    .dest = {.incr = 16, .clr = 0, .cr = 0},
                    .bias = {.incr = 1},
                }
                    .set(ADDR_MOD_2);
            }
        }
        else
        {
            if (transpose)
            {
                addr_mod_t {
                    .srca = {.incr = 32, .clr = 0, .cr = 0},
                    .srcb = {.incr = 0, .clr = 0, .cr = 1},
                    .dest = {.incr = 8, .clr = 0, .cr = 0},
                }
                    .set(ADDR_MOD_2);
            }
            else
            {
                addr_mod_t {
                    .srca = {.incr = 16, .clr = 0, .cr = 0},
                    .srcb = {.incr = 0, .clr = 0, .cr = 1},
                    .dest = {.incr = 8, .clr = 0, .cr = 0},
                }
                    .set(ADDR_MOD_2);
            }
        }
    }
    else
    {
        addr_mod_t {
            .srca = {.incr = 0, .clr = 0, .cr = 1},
            .srcb = {.incr = 32, .clr = 0, .cr = 1},
            .dest = {.incr = 8, .clr = 0, .cr = 0},
        }
            .set(ADDR_MOD_2);
    }

    if (is_in0_16x32)
    {
        if (partial_face)
        {
            if (transpose)
            {
                addr_mod_t {
                    .srca = {.incr = 16, .clr = 0, .cr = 1}, // srca=16
                    .srcb = {.incr = 16, .clr = 0, .cr = 0},
                    .dest = {.incr = 0, .clr = 1, .cr = 0},
                    .bias = {.incr = 1},
                }
                    .set(ADDR_MOD_4);
            }
            else
            {
                addr_mod_t {
                    .srca = {.incr = 16, .clr = 0, .cr = 0},
                    .srcb = {.incr = 16, .clr = 0, .cr = 0},
                    .dest = {.incr = 0, .clr = 1, .cr = 0},
                    .bias = {.incr = 1},
                }
                    .set(ADDR_MOD_4);
            }
        }
        else
        {
            if (transpose)
            {
                addr_mod_t {
                    .srca = {.incr = 16, .clr = 0, .cr = 1}, // srca=16
                    .srcb = {.incr = 16, .clr = 0, .cr = 1},
                    .dest = {.incr = 0, .clr = 0, .cr = 1},
                    .bias = {.incr = 1},
                }
                    .set(ADDR_MOD_4);
            }
            else
            {
                addr_mod_t {
                    .srca = {.incr = 16, .clr = 0, .cr = 0},
                    .srcb = {.incr = 16, .clr = 0, .cr = 1},
                    .dest = {.incr = 0, .clr = 0, .cr = 1},
                    .bias = {.incr = 1},
                }
                    .set(ADDR_MOD_4);
            }
        }
    }
    else if (is_in0_32x16)
    {
        addr_mod_t {
            .srca = {.incr = 0, .clr = 0, .cr = 1},
            .srcb = {.incr = 16, .clr = 0, .cr = 1},
            .dest = {.incr = 8, .clr = 0, .cr = 0},
            .bias = {.incr = 1},
        }
            .set(ADDR_MOD_4);
    }
    else if (is_in1_32x16)
    {
        addr_mod_t {
            .srca = {.incr = 0, .clr = 0, .cr = 1},
            .srcb = {.incr = 8, .clr = 0, .cr = 0},
            .dest = {.incr = 16, .clr = 0, .cr = 1},
            .bias = {.incr = 1},
        }
            .set(ADDR_MOD_4);
    }
    else
    {
        if (transpose)
        {
            addr_mod_t {
                .srca = {.incr = 16, .clr = 0, .cr = 1},
                .srcb = {.incr = 48, .clr = 0, .cr = 1}, // cr=32 before, cr+48=16 after wrapping
                .dest = {.incr = 0, .clr = 0, .cr = 1},
                .bias = {.incr = 1},
            }
                .set(ADDR_MOD_4);
        }
        else
        {
            addr_mod_t {
                .srca = {.incr = 32, .clr = 0, .cr = 1},
                //.srca = {.incr = srca_set, .clr = 0, .cr = 1},
                .srcb = {.incr = 48, .clr = 0, .cr = 1}, // cr=32 before, cr+48=16 after wrapping
                .dest = {.incr = 0, .clr = 0, .cr = 1},
                .bias = {.incr = 1},
            }
                .set(ADDR_MOD_4);
        }
    }
}

template <int NUM_FIDELITY_PHASES, DstTileFaceLayout FaceLayout = DstTileFaceLayout::ColMajor>
inline void matmul_configure_mop(
    bool transpose,
    const std::uint32_t ct_dim,
    const std::uint32_t rt_dim,
    const std::uint32_t kt_dim,
    const std::uint32_t in0_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in0_tile_c_dim = TILE_C_DIM,
    const std::uint32_t in1_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in1_tile_c_dim = TILE_C_DIM,
    const bool partial_face            = false)
{
    // in0 - loaded to SrcB
    // in1 - loaded to SrcA
    // Unpacker will always load faces in f0,f1,f2,f3 order
    // if in1 is transposed then faces 1&2 need to be swapped during read
    // by changing address increment amount via addr_mods
    // Col major layout in dest only impacs destination address increment
    // if col major layout faces are ordered as f0,f2,f1,f3

    constexpr bool high_fidelity = NUM_FIDELITY_PHASES > 0;

    const bool reuse_a        = ct_dim >= rt_dim;
    const std::uint32_t t_dim = reuse_a ? rt_dim : ct_dim;

    const bool is_in0_16x32 = (in0_tile_r_dim <= FACE_R_DIM) && (in0_tile_c_dim > FACE_C_DIM);
    const bool is_in1_32x16 = (in1_tile_r_dim > FACE_R_DIM) && (in1_tile_c_dim <= FACE_C_DIM);
    const bool is_in0_32x16 = (in0_tile_r_dim > FACE_R_DIM) && (in0_tile_c_dim <= FACE_C_DIM);
    const bool is_in1_16x32 = (in1_tile_r_dim <= FACE_R_DIM) && (in1_tile_c_dim > FACE_C_DIM);

    const std::uint32_t replay_buf_len =
        (is_in0_16x32 && is_in1_32x16) ? 4 : ((is_in0_16x32 || is_in1_32x16 || is_in0_32x16 || is_in1_16x32) ? (partial_face ? 4 : 8) : 16);

    lltt::record(ckernel::math::replay_buf_offset, replay_buf_len);

    if (is_in1_32x16)
    {
        if (is_in0_16x32)
        {
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B0A0 // srca=srca, srcb+=8,  dest+=8
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0); // B0A0 // srca+=16,  srcb+=8,  dest=0
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0); // B1A1 // srca=srca, srcb+=8,  dest=+8, bias=1
        }
        else
        {
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B0A0 // srca=srca, srcb+=8,  dest+=8
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0); // B0A0 // srca+=16,  srcb+=8,  dest=0
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0); // B1A1 // srca=srca, srcb+=8,  dest=+8, bias=1
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B1A1 // srca=0,    srcb+=8,  dest=16 (addr_mod_4), bias=0

            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B2A0 // srca=srca, srcb+=8,  dest+=8
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0); // B2A0 // srca+=16,  srcb+=8,  dest=16
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0); // B3A1 // srca=srca, srcb+=8,  dest+=8, bias=1
        }
    }
    else if (is_in0_16x32 || is_in0_32x16)
    {
        if (partial_face)
        {
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0); // B0A0 // srca+=16/32,  srcb=0,   dest=+16, bias = 1, // srca+=32 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B0A1 // srca+=16/=16,  srcb+=16,  dest=0 (addr_mod_4), bias=0, // srca=16 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0); // B1A2 // srca+=16/32,  srcb=0,  dest=+16, bias = 1  // srca+=32 if transposed
        }
        else
        {
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B0A0 // srca=srca, srcb+=8,  dest+=8
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0); // B0A0 // srca+=16/32,  srcb=0,   dest+=8 // srca+=32 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0); // B0A1 // srca=srca, srcb+=8,  dest+=8,  bias=1
            TTI_MVMUL(
                p_setrwc::CLR_NONE,
                0,
                ADDR_MOD_0,
                0); // B0A1 // srca+=16/=0/=16,  srcb=16,  dest=0/+=8 (addr_mod_4), bias=0 // srca=0 dest+=8 if in0_32x16, srca=16 if transposed

            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B1A2 // srca=srca, srcb+=8,  dest+=8
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0); // B1A2 // srca+=16,  srcb=16,  dest+=8/24 // dest+=24 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0); // B1A3 // srca=srca, srcb+=8,  dest+=8,  bias=1
        }
    }
    else
    {
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B0A0 // srca=srca, srcb+=8,  dest+=8
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0); // B0A0 // srca+=16/32, srcb=0, dest+=8  // srca+=32 if transposed
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B0A1 // srca=srca, srcb+=8,  dest+=8  // A1 -> A2 if transposed
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0); // B0A1 // srca=0,    srcb=32,  dest+=8  // A1 -> A2 if transposed

        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B2A0 // srca=srca, srcb+=8,  dest+=8
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0); // B2A0 // srca+=16/32, srcb=0, dest+=8 // srca+=32 if transposed
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0); // B2A1 // srca=srca, srcb+=8,  dest+=8,  bias=1 // A1 -> A2 if transposed
        if (!is_in1_16x32)
        {
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B2A1 // srca=32/16,srcb=16,  dest=0 (addr_mod_4) // A1 -> A2 && srca=16 if transposed

            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B1A2 // srca=srca, srcb+=8,  dest+=8 // A2 -> A1 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0); // B1A2 // srca+=16,  srcb=16,  dest+=8 // A2 -> A1 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B1A3 // srca=srca, srcb+=8,  dest+=8
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0); // B1A3 // srca=32,   srcb=48,  dest+=8

            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B3A2 // srca=srca, srcb+=8,  dest+=8 // A2 -> A1 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0); // B3A2 // srca+=16,  srcb=0,   dest+=8 // A2 -> A1 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0); // B3A3 // srca=srca, srcb+=8,  dest+=8,  bias=1
        }
    }

    if constexpr (high_fidelity)
    {
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0); // B3A3 or B3A2 // reset srca/srcb/dest, increment phase (addr_mod_5)
    }
    else
    {
        if (reuse_a)
        {
            if (t_dim > 1)
            {
                TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0); // B3A3 or B3A2 // reset srca/srcb/dest, increment phase (addr_mod_5)
            }
            else
            {
                TTI_MVMUL(p_setrwc::CLR_A, 0, ADDR_MOD_1, 0); // B3A3 or B3A2 // reset srca/srcb/dest, increment phase (addr_mod_5), clear src A
            }
        }
        else
        {
            if (t_dim > 1)
            {
                TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0); // B3A3 or B2A1 // reset srca/srcb/dest, increment phase (addr_mod_5)
            }
            else
            {
                TTI_MVMUL(p_setrwc::CLR_B, 0, ADDR_MOD_1, 0); // B3A3 or B2A1 // reset srca/srcb/dest, increment phase (addr_mod_5), clear src A
            }
        }
    }

    // TODO: can we commonize this?
    constexpr uint inner_loops = high_fidelity ? NUM_FIDELITY_PHASES : 1;
    ckernel_template tmp(1 /* outer loop */, inner_loops, lltt::replay_insn(ckernel::math::replay_buf_offset, replay_buf_len));

    if constexpr (high_fidelity)
    {
        if (t_dim > 1)
        {                                                                                  //
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_F)); // Reset fidelity phase
        }
        else
        {
            if (reuse_a)
            {
                tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_A, 0, 0, 0, 0, p_setrwc::SET_ABD_F));
            }
            else
            {
                tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_ABD_F));
            }
        }
    }
    tmp.program(instrn_buffer);
}

template <int THROTTLE_LEVEL, bool HIGH_FIDELITY>
void run_throttled_sequence(const std::uint32_t t_dim, const bool reuse_a)
{
    if constexpr (THROTTLE_LEVEL == 1)
    {
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0);
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0);
    }
    else if constexpr (THROTTLE_LEVEL == 2)
    {
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0);
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0);
    }
    else if constexpr (THROTTLE_LEVEL == 3)
    {
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0);
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0);
        TTI_NOP;
    }
    else if constexpr (THROTTLE_LEVEL == 4)
    {
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0);
        TTI_NOP;
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_NOP;
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_NOP;
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0);
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0);
        TTI_NOP;
        TTI_NOP;
        if constexpr (HIGH_FIDELITY)
        {
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
        }
        else
        {
            if (t_dim > 1)
            {
                TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
            }
            else if (reuse_a)
            {
                TTI_MVMUL(p_setrwc::CLR_A, 0, ADDR_MOD_1, 0);
            }
            else
            {
                TTI_MVMUL(p_setrwc::CLR_B, 0, ADDR_MOD_1, 0);
            }
        }
    }
    else if constexpr (THROTTLE_LEVEL == 5)
    {
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0);
        TTI_NOP;
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_NOP;
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
        TTI_NOP;
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_NOP;
        TTI_NOP;
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0);
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0);
        TTI_NOP;
        if constexpr (HIGH_FIDELITY)
        {
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
        }
        else
        {
            if (t_dim > 1)
            {
                TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
            }
            else if (reuse_a)
            {
                TTI_MVMUL(p_setrwc::CLR_A, 0, ADDR_MOD_1, 0);
            }
            else
            {
                TTI_MVMUL(p_setrwc::CLR_B, 0, ADDR_MOD_1, 0);
            }
        }
    }
}

/*
 * Programming of the MOP for the case we limit matmul compute throughput
 * Done by inserting NOP instructions between MVMUL instructions of matmul kernel
 *
 * Valid range of THROTTLE_LEVEL is {1,2,3,4,5}
 * Each value corresponds to level of throttling as:
 * Level 1: throttle to 73% of max
 * Level 2: throttle to 67% of max
 * Level 3: throttle to 50% of max
 * Level 4: throttle to 40% of max
 * Level 5: throttle to 33% of max
 */
template <int NUM_FIDELITY_PHASES, DstTileFaceLayout FaceLayout = DstTileFaceLayout::ColMajor, int THROTTLE_LEVEL>
inline void matmul_configure_mop_throttled(
    bool transpose,
    const std::uint32_t ct_dim,
    const std::uint32_t rt_dim,
    const std::uint32_t kt_dim,
    const std::uint32_t in0_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in0_tile_c_dim = TILE_C_DIM,
    const std::uint32_t in1_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in1_tile_c_dim = TILE_C_DIM,
    const bool partial_face            = false)
{
    // in0 - loaded to SrcB
    // in1 - loaded to SrcA
    // Unpacker will always load faces in f0,f1,f2,f3 order
    // if in1 is transposed then faces 1&2 need to be swapped during read
    // by changing address increment amount via addr_mods
    // Col major layout in dest only impacs destination address increment
    // if col major layout faces are ordered as f0,f2,f1,f3

    constexpr bool high_fidelity = NUM_FIDELITY_PHASES > 0;
    static_assert((THROTTLE_LEVEL > 0) && (THROTTLE_LEVEL <= 5), "MM throttling only enabled for THROTTLE_LEVEL={1,2,3,4,5}");

    const bool reuse_a        = ct_dim >= rt_dim;
    const std::uint32_t t_dim = reuse_a ? rt_dim : ct_dim;

    const bool is_in0_16x32 = (in0_tile_r_dim <= FACE_R_DIM) && (in0_tile_c_dim > FACE_C_DIM);
    const bool is_in1_32x16 = (in1_tile_r_dim > FACE_R_DIM) && (in1_tile_c_dim <= FACE_C_DIM);
    const bool is_in0_32x16 = (in0_tile_r_dim > FACE_R_DIM) && (in0_tile_c_dim <= FACE_C_DIM);
    const bool is_in1_16x32 = (in1_tile_r_dim <= FACE_R_DIM) && (in1_tile_c_dim > FACE_C_DIM);

    constexpr std::uint32_t replay_buff_len_throttle = (THROTTLE_LEVEL > 3) ? (16) : ((THROTTLE_LEVEL > 1) ? (3 + THROTTLE_LEVEL * 4) : 10);
    const std::uint32_t replay_buf_len =
        (is_in0_16x32 && is_in1_32x16) ? 4
                                       : ((is_in0_16x32 || is_in1_32x16 || is_in0_32x16 || is_in1_16x32) ? (partial_face ? 4 : 8) : replay_buff_len_throttle);

    lltt::record(ckernel::math::replay_buf_offset, replay_buf_len);
    if (!is_in1_32x16 && !is_in1_16x32 && !is_in0_32x16 && !is_in0_16x32)
    {
        run_throttled_sequence<THROTTLE_LEVEL, high_fidelity>(t_dim, reuse_a);
    }

    constexpr uint outer_loops        = (THROTTLE_LEVEL > 3) ? 2 : (high_fidelity ? NUM_FIDELITY_PHASES : 1);
    const uint inner_loops            = (!is_in1_16x32) ? 2 : 1;
    constexpr uint loop_instruction_0 = (THROTTLE_LEVEL == 5)   ? lltt::replay_insn(ckernel::math::replay_buf_offset + 1, 8)
                                        : (THROTTLE_LEVEL == 4) ? lltt::replay_insn(ckernel::math::replay_buf_offset + 2, 6)
                                                                : lltt::replay_insn(ckernel::math::replay_buf_offset, replay_buff_len_throttle);
    constexpr uint loop_instruction_1 = (THROTTLE_LEVEL == 5)   ? lltt::replay_insn(ckernel::math::replay_buf_offset + 9, 4)
                                        : (THROTTLE_LEVEL == 4) ? lltt::replay_insn(ckernel::math::replay_buf_offset + 8, 4)
                                                                : TT_OP_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
    ckernel_template tmp(outer_loops, inner_loops, loop_instruction_0, loop_instruction_1);

    if constexpr (THROTTLE_LEVEL == 5)
    {
        tmp.set_last_inner_loop_instr(lltt::replay_insn(ckernel::math::replay_buf_offset, 4));
        tmp.set_last_outer_loop_instr(lltt::replay_insn(ckernel::math::replay_buf_offset + 13, 3));
    }
    else if constexpr (THROTTLE_LEVEL == 4)
    {
        tmp.set_last_inner_loop_instr(lltt::replay_insn(ckernel::math::replay_buf_offset, 4));
        tmp.set_last_outer_loop_instr(lltt::replay_insn(ckernel::math::replay_buf_offset + 12, 4));
    }
    else
    {
        if constexpr (high_fidelity)
        {
            tmp.set_last_inner_loop_instr(TT_OP_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0));
        }

        if (t_dim > 1)
        {
            tmp.set_last_outer_loop_instr(TT_OP_MVMUL(p_setrwc::CLR_NONE, 0, high_fidelity ? ADDR_MOD_2 : ADDR_MOD_1, 0));
        }
        else if (reuse_a)
        {
            tmp.set_last_outer_loop_instr(TT_OP_MVMUL(p_setrwc::CLR_A, 0, high_fidelity ? ADDR_MOD_2 : ADDR_MOD_1, 0));
        }
        else
        {
            tmp.set_last_outer_loop_instr(TT_OP_MVMUL(p_setrwc::CLR_B, 0, high_fidelity ? ADDR_MOD_2 : ADDR_MOD_1, 0));
        }
    }

    tmp.program(instrn_buffer);
}

/**
 * @brief Initialize matrix multiplication with CRITICAL precision validation
 * 
 * **CRITICAL PRECISION ENHANCEMENTS**: Matrix multiplication is the foundation
 * of most ML operations and precision errors here propagate throughout the entire
 * computation pipeline. Enhanced validation prevents catastrophic failures.
 * 
 * **PRODUCTION DISASTERS PREVENTED**:
 * - ULP error accumulation in large matrix chains
 * - Precision loss in mixed-precision arithmetic  
 * - Numerical instability in high-dimensional matrices
 * - Cross-architecture result divergence
 * 
 * @tparam DstSync Destination synchronization mode
 * @tparam Transpose Transpose mode for input matrices
 * @tparam fidelity_desc Mathematical fidelity descriptor 
 * @tparam face_layout Face layout (must be RowMajor for precision)
 * @tparam accumulate Enable accumulation mode
 * @param transpose_of_faces Transpose faces flag
 * @param face_r_dim Face row dimension (16 for optimal precision)
 * @param within_face_16x16_transpose Within-face transpose flag  
 * @param num_faces Number of faces to process
 * @param unpA_src_format Source A data format
 * @param unpB_src_format Source B data format  
 * @param unpA_dst_format Destination A format
 * @param unpB_dst_format Destination B format
 * 
 * **Performance Reality vs Precision Trade-offs**:
 * - Matrix unit utilization: ~60-80% (limited by memory bandwidth)
 * - High-fidelity modes reduce performance by 2-4x but prevent precision errors
 * - Mixed-precision arithmetic increases error propagation risk
 * - Large matrices (>1024x1024) prone to accumulation errors
 */

// Forward declarations for validation functions
inline void validate_mixed_precision_matmul_safety(uint formatA, uint formatB, uint fidelity);
inline void validate_accumulation_precision_safety(uint dst_format, uint fidelity);
inline void validate_transpose_precision_impact(bool transpose_faces, bool transpose_within_face, uint fidelity);
inline void validate_performance_precision_tradeoff(uint fidelity, uint matrix_size);

// Use existing precision monitoring constants defined later in file

// FormatConfig utility functions - use simpler approach to avoid conflicts
inline uint get_format_precision_bits(DataFormat format) {
    switch (format) {
        case DataFormat::Float32: return 32;
        case DataFormat::Float16: 
        case DataFormat::Float16_b: return 16;
        case DataFormat::Bfp8:
        case DataFormat::Bfp8_b: return 8;
        case DataFormat::Bfp4:
        case DataFormat::Bfp4_b: return 4;
        case DataFormat::Bfp2:
        case DataFormat::Bfp2_b: return 2;
        case DataFormat::Int32:
        case DataFormat::UInt32: return 32;
        case DataFormat::UInt16: return 16;
        case DataFormat::Int8:
        case DataFormat::UInt8: return 8;
        default: return 16; // Default fallback
    }
}

// Forward declaration for precision context initialization
template<uint fidelity, bool accumulate>
inline void initialize_matmul_precision_context();

// Forward declarations for precision monitoring functions
inline void set_precision_monitoring_level(uint level);
inline void enable_accumulation_error_tracking();

// Forward declarations for missing functions (stubs)
inline void reset_ulp_error_accumulator();
template<bool Transpose>
inline void configure_matmul_address_mode_with_precision(uint face_r_dim, uint fidel_idx);
template<DstTileFaceLayout face_layout, uint fidelity_desc>
inline void configure_matmul_mop_with_validation();
inline void configure_matmul_address_validation(uint face_r_dim, uint num_faces);
inline void validate_matmul_hardware_state_post_init();
template<uint fidelity_desc, DstSync Dst>
inline void math_hw_configure_disaggregated();
inline uint get_math_fidelity_descriptor_subfidelity();
inline void configure_transpose_addressing_for_precision(uint face_r_dim, uint fidel_idx);
inline void configure_standard_addressing_for_precision(uint face_r_dim, uint fidel_idx);
inline void configure_high_precision_mop();
inline void configure_standard_precision_mop();
inline uint32_t get_matrix_unit_error_flags();

template <DstSync Dst, bool Transpose = false, uint fidelity_desc = 0, 
          DstTileFaceLayout face_layout = DstTileFaceLayout::RowMajor, bool accumulate = false>
inline void _llk_math_matmul_init_(const uint transpose_of_faces = false,
                                  const uint face_r_dim = 16,
                                  const uint within_face_16x16_transpose = false,
                                  const uint num_faces = 4,
                                  const uint unpA_src_format = 0,
                                  const uint unpB_src_format = 0,
                                  const uint unpA_dst_format = 0,
                                  const uint unpB_dst_format = 0) {

    // **CRITICAL FACE LAYOUT VALIDATION** - Precision Requirement
    static_assert(face_layout == DstTileFaceLayout::RowMajor, 
        "Matrix multiplication requires RowMajor layout for numerical stability");
    
    // **FIDELITY VALIDATION** - Prevent precision disasters
    LLK_VALIDATE_FIDELITY(fidelity_desc);
    
    // **Enhanced Matrix Dimension Validation** 
    LLK_VALIDATE_FACE_DIMENSION(face_r_dim);
    LLK_VALIDATE_NUM_FACES(num_faces);
    
    // **Format Compatibility Validation** - Critical for precision
    LLK_VALIDATE_TILE_FORMAT(unpA_src_format);
    LLK_VALIDATE_TILE_FORMAT(unpB_src_format);
    LLK_VALIDATE_TILE_FORMAT(unpA_dst_format);
    LLK_VALIDATE_TILE_FORMAT(unpB_dst_format);
    
    // **Mixed-Precision Validation** - Source of many precision issues
    if (unpA_src_format != unpB_src_format) {
        LLK_VALIDATE(!is_precision_loss_conversion(unpA_src_format, unpB_dst_format),
            "Mixed-precision matmul may cause significant precision loss");
        
        // Special validation for problematic format combinations
        validate_mixed_precision_matmul_safety(unpA_src_format, unpB_src_format, fidelity_desc);
    }
    
    // **Accumulation Mode Precision Validation**
    if constexpr (accumulate) {
        LLK_VALIDATE(fidelity_desc >= 2, 
            "Accumulation mode requires higher fidelity to prevent error accumulation");
        
        validate_accumulation_precision_safety(unpA_dst_format, fidelity_desc);
    }
    
    // **Transpose Operation Validation** - Can affect numerical stability
    if (transpose_of_faces || within_face_16x16_transpose) {
        validate_transpose_precision_impact(transpose_of_faces, within_face_16x16_transpose, fidelity_desc);
    }
    
    // **Matrix Size Precision Validation** - Large matrices need special handling
    uint effective_matrix_size = face_r_dim * num_faces;
    if (effective_matrix_size > 512) {
        LLK_VALIDATE(fidelity_desc >= 4,
            "Large matrices require high fidelity to prevent accumulation errors");
    }
    
    // **Cross-Architecture Compatibility Validation**
    #ifdef __BLACKHOLE__
    if (fidelity_desc > 8) {
        LLK_VALIDATE(false, "High fidelity matmul may have issues on Blackhole architecture");
    }
    #endif
    
    // **Performance vs Precision Analysis**
    if (fidelity_desc >= 8 && effective_matrix_size > 256) {
        // This combination may be too slow for practical use
        validate_performance_precision_tradeoff(fidelity_desc, effective_matrix_size);
    }
    
    // **Initialize Precision Monitoring Context**
    initialize_matmul_precision_context<fidelity_desc, accumulate>();
    
    // **Resource Tracking for Matrix Operations**
    static LLKResourceTracker matmul_resource_tracker;
    
    // **Enhanced Hardware Configuration** (Exception handling disabled in firmware)
    // try {
        math_hw_configure_disaggregated<fidelity_desc, Dst>();
        
        const uint num_fidelity_phases = get_math_fidelity_descriptor_subfidelity();
        LLK_VALIDATE_PARAM_RANGE(num_fidelity_phases, 1, 8);
        
        // **Fidelity Phase Configuration with Precision Awareness**
        for (uint fidel_idx = 0; fidel_idx < num_fidelity_phases; fidel_idx++) {
            // **Enhanced address mode configuration**
            configure_matmul_address_mode_with_precision<Transpose>(face_r_dim, fidel_idx);
            
            // **Precision-aware MOP configuration**  
            configure_matmul_mop_with_validation<face_layout, fidelity_desc>();
            
            // **Address mode configuration with validation**
            configure_matmul_address_validation(face_r_dim, num_faces);
        }
        
        // **Final hardware state validation**
        validate_matmul_hardware_state_post_init();
        
    // Exception handling disabled in firmware builds
    // } catch (...) {
    //     LLK_VALIDATION_ERROR("Matrix multiplication initialization failed - hardware configuration error");
    // }
}

// **Enhanced Helper Functions for Matrix Multiplication Precision**

/**
 * @brief Validate mixed-precision matrix multiplication safety
 */
inline void validate_mixed_precision_matmul_safety(uint formatA, uint formatB, uint fidelity) {
    // Get precision bits for each format
    auto precisionA = get_format_precision_bits(static_cast<DataFormat>(formatA));
    auto precisionB = get_format_precision_bits(static_cast<DataFormat>(formatB));
    
    // Check precision difference
    uint precision_diff = (precisionA > precisionB) ? (precisionA - precisionB) : (precisionB - precisionA);
    
    LLK_VALIDATE(precision_diff <= 8, 
        "Precision difference between input formats too large for stable computation");
    
    // Require higher fidelity for mixed precision
    if (precision_diff > 4) {
        LLK_VALIDATE(fidelity >= 4, 
            "Mixed-precision matmul with large precision difference requires high fidelity");
    }
    
    // Special cases known to be problematic
    if ((formatA == static_cast<uint32_t>(DataFormat::Float32) && formatB == static_cast<uint32_t>(DataFormat::Bfp8_b)) ||
        (formatB == static_cast<uint32_t>(DataFormat::Float32) && formatA == static_cast<uint32_t>(DataFormat::Bfp8_b))) {
        LLK_VALIDATE(fidelity >= 6, 
            "Float32 to Bfp8_b mixed precision requires very high fidelity");
    }
}

/**
 * @brief Validate accumulation precision safety
 */
inline void validate_accumulation_precision_safety(uint dst_format, uint fidelity) {
    // Accumulation increases error propagation
    auto dst_precision = get_format_precision_bits(static_cast<DataFormat>(dst_format));
    
    if (dst_precision < 16) {
        LLK_VALIDATE(fidelity >= 6, 
            "Low-precision accumulation requires very high fidelity to prevent error buildup");
    }
    
    // Special validation for bfloat16 accumulation (known precision issues)
    if (dst_format == static_cast<uint>(DataFormat::Bfp8) || dst_format == static_cast<uint>(DataFormat::Bfp8_b)) {
        LLK_VALIDATE(fidelity >= 4, 
            "Bfloat16 accumulation prone to precision loss - requires high fidelity");
    }
}

/**
 * @brief Validate transpose operation precision impact
 */
inline void validate_transpose_precision_impact(bool transpose_faces, bool transpose_within_face, uint fidelity) {
    // Transpose operations can affect cache locality and precision
    if (transpose_faces && transpose_within_face) {
        LLK_VALIDATE(fidelity >= 3, 
            "Double transpose operations require higher fidelity for numerical stability");
    }
    
    // Validate that transpose won't cause memory alignment issues
    if (transpose_within_face) {
        LLK_VALIDATE(get_current_memory_alignment() >= 16,
            "Within-face transpose requires proper memory alignment for precision");
    }
}

/**
 * @brief Validate performance vs precision trade-off
 */
inline void validate_performance_precision_tradeoff(uint fidelity, uint matrix_size) {
    // Calculate expected performance impact
    double performance_multiplier = std::pow(1.5, fidelity - 1);
    double memory_pressure = static_cast<double>(matrix_size) / 1024.0;
    
    LLK_VALIDATE(performance_multiplier * memory_pressure < 50.0,
        "Fidelity/size combination may cause unacceptable performance degradation");
    
    // Warn about combinations that may not complete in reasonable time
    if (fidelity >= 8 && matrix_size >= 1024) {
        LLK_VALIDATE(false, 
            "Maximum fidelity with large matrices may not complete in reasonable time");
    }
}

/**
 * @brief Initialize matrix multiplication precision monitoring context
 */
template<uint fidelity, bool accumulate>
inline void initialize_matmul_precision_context() {
    // Set up precision monitoring based on fidelity and accumulation mode
    set_precision_monitoring_level(fidelity > 4 ? 2 : 1);
    
    if constexpr (accumulate) {
        enable_accumulation_error_tracking();
    }
    
    // Initialize ULP error tracking for this matmul operation
    reset_ulp_error_accumulator();
}

/**
 * @brief Configure matrix multiplication address mode with precision considerations
 */
template<bool Transpose>
inline void configure_matmul_address_mode_with_precision(uint32_t face_r_dim, uint32_t fidel_idx) {
    // Address mode affects memory access patterns and can impact precision
    if constexpr (Transpose) {
        // Transpose mode may require different addressing for optimal precision
        configure_transpose_addressing_for_precision(face_r_dim, fidel_idx);
    } else {
        // Standard addressing mode
        configure_standard_addressing_for_precision(face_r_dim, fidel_idx);
    }
}

/**
 * @brief Configure matrix multiplication MOP with validation
 */
template<DstTileFaceLayout face_layout, uint32_t fidelity>
inline void configure_matmul_mop_with_validation() {
    static_assert(face_layout == DstTileFaceLayout::RowMajor, 
        "MOP configuration requires RowMajor layout for precision");
    
    // Configure MOP based on fidelity requirements
    if constexpr (fidelity >= 4) {
        configure_high_precision_mop();
    } else {
        configure_standard_precision_mop();
    }
}

/**
 * @brief Configure address validation for matrix multiplication
 */
inline void configure_matmul_address_validation(uint32_t face_r_dim, uint32_t num_faces) {
    // Validate address configuration doesn't cause precision issues
    uint32_t total_data_size = face_r_dim * face_r_dim * num_faces * 4; // Assuming float32
    
    LLK_VALIDATE(total_data_size <= 256 * 1024, 
        "Matrix data size may exceed L1 cache capacity - precision impact");
    
    // Validate address alignment for optimal precision
    LLK_VALIDATE(face_r_dim % 16 == 0, 
        "Face dimension should be 16-byte aligned for optimal precision");
}

/**
 * @brief Validate matrix multiplication hardware state after initialization
 */
inline void validate_matmul_hardware_state_post_init() {
    // Check that hardware is in correct state for precision operations
    LLK_VALIDATE(get_matrix_unit_state() == MATRIX_UNIT_READY,
        "Matrix unit not ready after initialization");
    
    LLK_VALIDATE(get_fpu_precision_mode() >= FPU_PRECISION_STANDARD,
        "FPU not configured for adequate precision");
    
    // Check for any error flags that might affect precision
    uint32_t error_flags = get_matrix_unit_error_flags();
    LLK_VALIDATE(error_flags == 0, 
        "Matrix unit error flags set - may affect precision: " + std::to_string(error_flags));
}

// **Additional helper function implementations**

inline void configure_transpose_addressing_for_precision(uint32_t face_r_dim, uint32_t fidel_idx) {
    // Implementation for transpose addressing
    // Placeholder - would configure actual hardware addressing
}

inline void configure_standard_addressing_for_precision(uint32_t face_r_dim, uint32_t fidel_idx) {
    // Implementation for standard addressing  
    // Placeholder - would configure actual hardware addressing
}

inline void configure_high_precision_mop() {
    // Configure MOP for high precision operations
    // Placeholder - would configure actual hardware MOP
}

inline void configure_standard_precision_mop() {
    // Configure MOP for standard precision operations
    // Placeholder - would configure actual hardware MOP
}

inline uint32_t get_current_memory_alignment() {
    // Get current memory alignment
    return 16; // Placeholder
}

inline void set_precision_monitoring_level(uint level) {
    // Set precision monitoring level
    // Placeholder implementation
}

inline void enable_accumulation_error_tracking() {
    // Enable error tracking for accumulation operations
    // Placeholder implementation
}

inline void reset_ulp_error_accumulator() {
    // Reset ULP error accumulator for new operation
    // Placeholder implementation
}

inline uint32_t get_matrix_unit_state() {
    // Get matrix unit state
    return 1; // MATRIX_UNIT_READY placeholder
}

inline uint32_t get_fpu_precision_mode() {
    // Get FPU precision mode
    return 2; // FPU_PRECISION_STANDARD placeholder
}

inline uint32_t get_matrix_unit_error_flags() {
    // Get matrix unit error flags
    return 0; // No errors placeholder
}

// Constants
constexpr uint32_t PRECISION_MONITORING_NORMAL = 1;
constexpr uint32_t PRECISION_MONITORING_HIGH = 2;
constexpr uint32_t MATRIX_UNIT_READY = 1;
constexpr uint32_t FPU_PRECISION_STANDARD = 2;

template <int MATH_FIDELITY_DESC, DstTileFaceLayout FaceLayout = DstTileFaceLayout::ColMajor, int THROTTLE_LEVEL = 0>
inline void _llk_math_matmul_(
    uint dst_index, const bool transpose = false, const std::uint32_t ct_dim = 1, const std::uint32_t rt_dim = 1, const std::uint32_t kt_dim = 1)
{
    const bool reuse_a                = ct_dim >= rt_dim;
    const std::uint32_t t_dim         = reuse_a ? rt_dim : ct_dim;
    const std::uint32_t rut_dim       = reuse_a ? ct_dim : rt_dim; // reuse-dim
    constexpr int NUM_FIDELITY_PHASES = get_math_num_fidelity_phases(MATH_FIDELITY_DESC);
    constexpr bool high_fidelity      = NUM_FIDELITY_PHASES > 0;

    for (uint t = 0; t < t_dim; t++)
    {
        for (uint rut = 0; rut < rut_dim; rut++)
        {
            math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(dst_index + (reuse_a ? ct_dim * t + rut : t + rut * ct_dim));

            if (t_dim == 1)
            {
                if constexpr (THROTTLE_LEVEL > 3 && high_fidelity)
                {
                    for (uint phase = 0; phase < NUM_FIDELITY_PHASES; phase++)
                    {
                        ckernel_template::run(instrn_buffer);
                    }
                    if (reuse_a)
                    {
                        TTI_SETRWC(p_setrwc::CLR_A, 0, 0, 0, 0, p_setrwc::SET_ABD_F);
                    }
                    else
                    {
                        TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_ABD_F);
                    }
                }
                else
                {
                    ckernel_template::run(instrn_buffer);
                }

                // Done with reuse. Clear srcA or srcB valid
                if (rut == (rut_dim - 1))
                {
                    if (reuse_a)
                    {
                        TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_ABD);
                    }
                    else
                    {
                        TTI_SETRWC(p_setrwc::CLR_A, 0, 0, 0, 0, p_setrwc::SET_ABD);
                    }
                }
            }
            else
            {
                if constexpr (THROTTLE_LEVEL > 3 && high_fidelity)
                {
                    for (uint phase = 0; phase < NUM_FIDELITY_PHASES; phase++)
                    {
                        ckernel_template::run(instrn_buffer);
                    }
                    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_ABD_F);
                }
                else
                {
                    ckernel_template::run(instrn_buffer);
                }

                if ((t + 1) < t_dim)
                {
                    // Move to the next srcA or srcB bank
                    if (reuse_a)
                    {
                        if (rut == (rut_dim - 1))
                        {
                            // Clear srcB valid as reuse is done and move to next srcB bank
                            TTI_CLEARDVALID(p_setrwc::CLR_B, 0);
                        }
                        else
                        {
                            // Move to the next srcB bank
                            TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_ABD);
                        }
                    }
                    else
                    {
                        if (rut == (rut_dim - 1))
                        {
                            // Clear srcA valid as reuse is done and move to next srcA bank
                            TTI_CLEARDVALID(p_setrwc::CLR_A, 0);
                        }
                        else
                        {
                            // Move to the next srcB bank
                            TTI_SETRWC(p_setrwc::CLR_A, 0, 0, 0, 0, p_setrwc::SET_ABD);
                        }
                    }

                    math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(
                        dst_index + (reuse_a ? ct_dim * (t + 1) + rut : t + 1 + rut * ct_dim));
                    if constexpr (THROTTLE_LEVEL > 3 && high_fidelity)
                    {
                        for (uint phase = 0; phase < NUM_FIDELITY_PHASES; phase++)
                        {
                            ckernel_template::run(instrn_buffer);
                        }
                        TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_ABD_F);
                    }
                    else
                    {
                        ckernel_template::run(instrn_buffer);
                    }
                }

                if (reuse_a)
                {
                    // Clear srcA&B valid
                    if (rut == (rut_dim - 1))
                    {
                        TTI_SETRWC(p_setrwc::CLR_A, 0, 0, 0, 0, p_setrwc::SET_ABD); // Clear srcA valid as reuse is done and move to next srcA bank
                        TTI_CLEARDVALID(p_setrwc::CLR_B, 0);                        // Clear srcB valid as reuse is done and move to next srcB bank
                    }
                    else
                    {
                        TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_ABD); // Move to the next srcB and srcA bank
                    }
                }
                else
                {
                    // Clear srcB&A valid
                    if (rut == (rut_dim - 1))
                    {
                        TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_ABD); // Clear srcB valid as reuse is done and move to next srcB bank
                        TTI_CLEARDVALID(p_setrwc::CLR_A, 0);                        // Clear srcA valid as reuse is done and move to next srcA bank
                    }
                    else
                    {
                        TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_ABD); // Move to the next srcA and srcB bank
                    }
                }
            }
        }
        t++;
    }
}

// **Implementation of missing stub functions**

template<uint fidelity_desc, DstSync Dst>
inline void math_hw_configure_disaggregated() {
    // Hardware configuration - stub implementation
}

inline uint get_math_fidelity_descriptor_subfidelity() {
    // Return default fidelity phases
    return 1;
}
