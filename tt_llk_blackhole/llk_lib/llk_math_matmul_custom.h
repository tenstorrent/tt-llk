// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_assert.h"
#include "llk_math_common.h"

#ifndef HF
#define HF 0
#endif

using namespace ckernel;

template <int MATH_FIDELITY_DESC>
inline void matmul_custom_configure_addrmod(const bool transpose)
{
    constexpr int NUM_FIDELITY_PHASES = get_math_num_fidelity_phases(MATH_FIDELITY_DESC);
    constexpr bool high_fidelity      = (NUM_FIDELITY_PHASES > 0);
    constexpr int FIDELITY_INCREMENT  = high_fidelity ? get_math_fidelity_increment(MATH_FIDELITY_DESC) : 0;

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

    // reset all, increment fidelity if we have more fidelity phases
    addr_mod_t {
        .srca     = {.incr = 0, .clr = 1, .cr = 1},
        .srcb     = {.incr = 0, .clr = 1, .cr = 1},
        .dest     = {.incr = 0, .clr = 1, .cr = 1},
        .fidelity = {.incr = FIDELITY_INCREMENT, .clr = 0},
    }
        .set(ADDR_MOD_5);

    // Standard 32x32 tile configuration
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
            .srca = {.incr = 16, .clr = 0, .cr = 0},
            .srcb = {.incr = 0, .clr = 0, .cr = 1},
            .dest = {.incr = 8, .clr = 0, .cr = 0},
        }
            .set(ADDR_MOD_1);
    }

    addr_mod_t {
        .srca = {.incr = 0, .clr = 0, .cr = 1},
        .srcb = {.incr = 32, .clr = 0, .cr = 1},
        .dest = {.incr = 8, .clr = 0, .cr = 0},
    }
        .set(ADDR_MOD_2);

    if (transpose)
    {
        addr_mod_t {
            .srca = {.incr = 16, .clr = 0, .cr = 1},
            .srcb = {.incr = 48, .clr = 0, .cr = 1}, // cr=32 before, cr+48=16 after wrapping
            .dest = {.incr = 0, .clr = 0, .cr = 1},
        }
            .set(ADDR_MOD_4);
    }
    else
    {
        addr_mod_t {
            .srca = {.incr = 32, .clr = 0, .cr = 1},
            .srcb = {.incr = 48, .clr = 0, .cr = 1}, // cr=32 before, cr+48=16 after wrapping
            .dest = {.incr = 0, .clr = 0, .cr = 1},
        }
            .set(ADDR_MOD_4);
    }
}

template <int NUM_FIDELITY_PHASES>
inline void matmul_custom_configure_mop(const std::uint32_t ct_dim, const std::uint32_t rt_dim)
{
    constexpr bool high_fidelity = NUM_FIDELITY_PHASES > 0;

    const bool reuse_a        = ct_dim >= rt_dim;
    const std::uint32_t t_dim = reuse_a ? rt_dim : ct_dim;

    constexpr std::uint32_t replay_buf_len = 16;

    constexpr uint inner_loops = high_fidelity ? NUM_FIDELITY_PHASES : 1;
    ckernel_template tmp(1 /* outer loop */, inner_loops, lltt::replay_insn(ckernel::math::replay_buf_offset, replay_buf_len));

    if constexpr (high_fidelity)
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
    tmp.program();
}

template <int MATH_FIDELITY_DESC>
inline void _llk_math_matmul_custom_init_(const std::uint32_t transpose = 0, const std::uint32_t ct_dim = 1, const std::uint32_t rt_dim = 1)
{
    matmul_custom_configure_addrmod<MATH_FIDELITY_DESC>(transpose);

    constexpr int MATH_FIDELITY_PHASES = get_math_num_fidelity_phases(MATH_FIDELITY_DESC);
    matmul_custom_configure_mop<MATH_FIDELITY_PHASES>(ct_dim, rt_dim);

    math::reset_counters(p_setrwc::SET_ABD_F);
}

inline void _llk_math_matmul_custom_uninit_()
{
    // No state to restore - all states are transient or default
}

template <int MATH_FIDELITY_DESC>
inline void _llk_math_matmul_custom_(uint dst_index, const std::uint32_t ct_dim = 1, const std::uint32_t rt_dim = 1)
{
    const bool reuse_a          = ct_dim >= rt_dim;
    const std::uint32_t t_dim   = reuse_a ? rt_dim : ct_dim;
    const std::uint32_t rut_dim = reuse_a ? ct_dim : rt_dim; // reuse-dim

    for (uint t = 0; t < t_dim; t++)
    {
        for (uint rut = 0; rut < rut_dim; rut++)
        {
            math::set_dst_write_addr<DstTileShape::Tile32x32, UnpackDestination::SrcRegs>(dst_index + (reuse_a ? ct_dim * t + rut : t + rut * ct_dim));

            ckernel_template::run();

            // Clear srcB or srcA at end of reuse (once per u block row)
            if (rut == (rut_dim - 1))
            {
                if (reuse_a)
                {
                    TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_ABD_F);
                }
                else
                {
                    TTI_SETRWC(p_setrwc::CLR_A, 0, 0, 0, 0, p_setrwc::SET_ABD_F);
                }
            }
        }
    }
}

// Purpose o f this is to preload the replay buffer with the matmul instructions for the given ct_dim and rt_dim
template <int NUM_FIDELITY_PHASES>
inline void matmul_custom_preload_replay_buf_(const std::uint32_t ct_dim, const std::uint32_t rt_dim)
{
    constexpr bool high_fidelity = NUM_FIDELITY_PHASES > 0;

    const bool reuse_a        = ct_dim >= rt_dim;
    const std::uint32_t t_dim = reuse_a ? rt_dim : ct_dim;

    constexpr std::uint32_t replay_buf_len = 16;

    load_replay_buf(
        ckernel::math::replay_buf_offset,
        replay_buf_len,
        // Lambda function to load reply buffer
        [high_fidelity, reuse_a]
        {
            // Standard 32x32 tile matmul sequence
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B0A0 // srca=srca, srcb+=8,  dest+=8
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0); // B0A0 // srca+=16/32, srcb=0, dest+=8  // srca+=32 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B0A1 // srca=srca, srcb+=8,  dest+=8  // A1 -> A2 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0); // B0A1 // srca=0,    srcb=32,  dest+=8  // A1 -> A2 if transposed

            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B2A0 // srca=srca, srcb+=8,  dest+=8
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0); // B2A0 // srca+=16/32, srcb=0, dest+=8 // srca+=32 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B2A1 // srca=srca, srcb+=8,  dest+=8 // A1 -> A2 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_4, 0); // B2A1 // srca=32/16,srcb=16,  dest=0 (addr_mod_4) // A1 -> A2 && srca=16 if transposed

            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B1A2 // srca=srca, srcb+=8,  dest+=8 // A2 -> A1 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0); // B1A2 // srca+=16,  srcb=16,  dest+=8 // A2 -> A1 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B1A3 // srca=srca, srcb+=8,  dest+=8
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0); // B1A3 // srca=32,   srcb=48,  dest+=8

            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B3A2 // srca=srca, srcb+=8,  dest+=8 // A2 -> A1 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0); // B3A2 // srca+=16,  srcb=0,   dest+=8 // A2 -> A1 if transposed
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0); // B3A3 // srca=srca, srcb+=8,  dest+=8

            if constexpr (high_fidelity)
            {
                TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_5, 0); // B3A3 // reset srca/srcb/dest, increment phase (addr_mod_5)
            }
            else
            {
                if (reuse_a)
                {
                    TTI_MVMUL(p_setrwc::CLR_A, 0, ADDR_MOD_5, 0); // B3A3 // reset srca/srcb/dest, increment phase (addr_mod_5), clear src A
                }
                else
                {
                    TTI_MVMUL(p_setrwc::CLR_B, 0, ADDR_MOD_5, 0); // B3A3 // reset srca/srcb/dest, increment phase (addr_mod_5), clear src B
                }
            }
        });
}
