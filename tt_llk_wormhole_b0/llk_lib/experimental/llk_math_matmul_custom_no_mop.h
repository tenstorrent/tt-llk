// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "llk_math_matmul.h"

using namespace ckernel;

struct matmul_no_mop_state_t
{
    std::uint32_t in0_tile_r_dim = TILE_R_DIM;
    std::uint32_t in0_tile_c_dim = TILE_C_DIM;
    std::uint32_t in1_tile_r_dim = TILE_R_DIM;
    std::uint32_t in1_tile_c_dim = TILE_C_DIM;
    std::uint32_t transpose      = 0;
    std::uint32_t ct_dim         = 1;
    std::uint32_t rt_dim         = 1;
    bool partial_face            = false;
};

static matmul_no_mop_state_t matmul_no_mop_state {};

inline void matmul_store_no_mop_state(
    const std::uint32_t in0_tile_r_dim,
    const std::uint32_t in0_tile_c_dim,
    const std::uint32_t in1_tile_r_dim,
    const std::uint32_t in1_tile_c_dim,
    const bool partial_face,
    const std::uint32_t transpose,
    const std::uint32_t ct_dim,
    const std::uint32_t rt_dim)
{
    matmul_no_mop_state.in0_tile_r_dim = in0_tile_r_dim;
    matmul_no_mop_state.in0_tile_c_dim = in0_tile_c_dim;
    matmul_no_mop_state.in1_tile_r_dim = in1_tile_r_dim;
    matmul_no_mop_state.in1_tile_c_dim = in1_tile_c_dim;
    matmul_no_mop_state.partial_face   = partial_face;
    matmul_no_mop_state.transpose      = transpose;
    matmul_no_mop_state.ct_dim         = ct_dim;
    matmul_no_mop_state.rt_dim         = rt_dim;
}

inline std::uint32_t matmul_get_replay_buf_len_no_mop(
    const std::uint32_t in0_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in0_tile_c_dim = TILE_C_DIM,
    const std::uint32_t in1_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in1_tile_c_dim = TILE_C_DIM,
    const bool partial_face            = false)
{
    const bool is_in0_16x32 = (in0_tile_r_dim <= FACE_R_DIM) && (in0_tile_c_dim > FACE_C_DIM);
    const bool is_in1_32x16 = (in1_tile_r_dim > FACE_R_DIM) && (in1_tile_c_dim <= FACE_C_DIM);
    const bool is_in0_32x16 = (in0_tile_r_dim > FACE_R_DIM) && (in0_tile_c_dim <= FACE_C_DIM);
    const bool is_in1_16x32 = (in1_tile_r_dim <= FACE_R_DIM) && (in1_tile_c_dim > FACE_C_DIM);

    return (is_in0_16x32 && is_in1_32x16) ? 4 : ((is_in0_16x32 || is_in1_32x16 || is_in0_32x16 || is_in1_16x32) ? (partial_face ? 4 : 8) : 16);
}

template <MathFidelity math_fidelity, int THROTTLE_LEVEL = 0>
inline void matmul_configure_addrmod_no_mop(
    const bool transpose,
    const std::uint32_t in0_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in0_tile_c_dim = TILE_C_DIM,
    const std::uint32_t in1_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in1_tile_c_dim = TILE_C_DIM,
    const bool partial_face            = false,
    const std::uint32_t ct_dim         = 1,
    const std::uint32_t rt_dim         = 1)
{
    static_assert(THROTTLE_LEVEL == 0, "Wormhole custom no-mop matmul only supports THROTTLE_LEVEL == 0");

    matmul_configure_addrmod<math_fidelity, THROTTLE_LEVEL>(transpose, in0_tile_r_dim, in0_tile_c_dim, in1_tile_r_dim, in1_tile_c_dim, partial_face);

    const bool reuse_a        = ct_dim >= rt_dim;
    const std::uint32_t t_dim = reuse_a ? rt_dim : ct_dim;

    if (t_dim > 1)
    {
        if (reuse_a)
        {
            TTI_SETC16(CLR_DVALID_SrcB_Disable_ADDR32, CLR_DVALID_SrcB_Disable_MASK);
        }
        else
        {
            TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, CLR_DVALID_SrcA_Disable_MASK);
        }
    }
    else
    {
        TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);
    }
}

template <MathFidelity math_fidelity>
inline void matmul_record_replay_no_mop(
    const std::uint32_t ct_dim,
    const std::uint32_t rt_dim,
    const std::uint32_t in0_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in0_tile_c_dim = TILE_C_DIM,
    const std::uint32_t in1_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in1_tile_c_dim = TILE_C_DIM,
    const bool partial_face            = false)
{
    const bool reuse_a        = ct_dim >= rt_dim;
    const std::uint32_t t_dim = reuse_a ? rt_dim : ct_dim;

    const bool is_in0_16x32 = (in0_tile_r_dim <= FACE_R_DIM) && (in0_tile_c_dim > FACE_C_DIM);
    const bool is_in1_32x16 = (in1_tile_r_dim > FACE_R_DIM) && (in1_tile_c_dim <= FACE_C_DIM);
    const bool is_in0_32x16 = (in0_tile_r_dim > FACE_R_DIM) && (in0_tile_c_dim <= FACE_C_DIM);
    const bool is_in1_16x32 = (in1_tile_r_dim <= FACE_R_DIM) && (in1_tile_c_dim > FACE_C_DIM);

    constexpr bool high_fidelity       = is_high_fidelity(math_fidelity);
    const std::uint32_t replay_buf_len = matmul_get_replay_buf_len_no_mop(in0_tile_r_dim, in0_tile_c_dim, in1_tile_r_dim, in1_tile_c_dim, partial_face);

    lltt::record(ckernel::math::replay_buf_offset, replay_buf_len);

    if (is_in1_32x16)
    {
        if (is_in0_16x32)
        {
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0);
        }
        else
        {
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);

            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0);
        }
    }
    else if (is_in0_16x32 || is_in0_32x16)
    {
        if (partial_face)
        {
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0);
        }
        else
        {
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);

            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0);
        }
    }
    else
    {
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0);

        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0);
        if (!is_in1_16x32)
        {
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);

            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_2, 0);

            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_0, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
            TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_3, 0);
        }
    }

    if constexpr (high_fidelity)
    {
        TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
    }
    else
    {
        if (reuse_a)
        {
            if (t_dim > 1)
            {
                TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
            }
            else
            {
                TTI_MVMUL(p_setrwc::CLR_A, 0, ADDR_MOD_1, 0);
            }
        }
        else
        {
            if (t_dim > 1)
            {
                TTI_MVMUL(p_setrwc::CLR_NONE, 0, ADDR_MOD_1, 0);
            }
            else
            {
                TTI_MVMUL(p_setrwc::CLR_B, 0, ADDR_MOD_1, 0);
            }
        }
    }
}

template <MathFidelity math_fidelity>
inline void matmul_execute_replay_no_mop(const std::uint32_t replay_buf_len, const bool reuse_a, const std::uint32_t t_dim)
{
    constexpr bool high_fidelity       = is_high_fidelity(math_fidelity);
    constexpr std::uint32_t num_replay = high_fidelity ? to_underlying(math_fidelity) : 1;

    for (std::uint32_t replay = 0; replay < num_replay; replay++)
    {
        lltt::replay(ckernel::math::replay_buf_offset, replay_buf_len);
    }

    if constexpr (high_fidelity)
    {
        if (t_dim > 1)
        {
            TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_F);
        }
        else if (reuse_a)
        {
            TTI_SETRWC(p_setrwc::CLR_A, 0, 0, 0, 0, p_setrwc::SET_ABD_F);
        }
        else
        {
            TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_ABD_F);
        }
    }
}

template <MathFidelity math_fidelity = MathFidelity::LoFi, int THROTTLE_LEVEL = 0>
inline void matmul_configure_addrmod_reinit(const bool transpose = false)
{
    if constexpr (THROTTLE_LEVEL == 0)
    {
        matmul_no_mop_state.transpose = transpose;
        // Reinit must preserve the no-mop replay buffer recorded at init time.
        // Standard Wormhole matmul init re-records replay_buf_offset, so no-mop
        // reinit restores only the addrmods and dvalid contract needed by replay.
        matmul_configure_addrmod_no_mop<math_fidelity, THROTTLE_LEVEL>(
            matmul_no_mop_state.transpose,
            matmul_no_mop_state.in0_tile_r_dim,
            matmul_no_mop_state.in0_tile_c_dim,
            matmul_no_mop_state.in1_tile_r_dim,
            matmul_no_mop_state.in1_tile_c_dim,
            matmul_no_mop_state.partial_face,
            matmul_no_mop_state.ct_dim,
            matmul_no_mop_state.rt_dim);
    }
    else
    {
        matmul_no_mop_state.transpose = transpose;
        _llk_math_matmul_init_<math_fidelity, THROTTLE_LEVEL>(
            matmul_no_mop_state.in0_tile_r_dim,
            matmul_no_mop_state.in0_tile_c_dim,
            matmul_no_mop_state.in1_tile_r_dim,
            matmul_no_mop_state.in1_tile_c_dim,
            matmul_no_mop_state.partial_face,
            matmul_no_mop_state.transpose,
            matmul_no_mop_state.ct_dim,
            matmul_no_mop_state.rt_dim);
    }
}

template <MathFidelity math_fidelity = MathFidelity::LoFi, int THROTTLE_LEVEL = 0>
inline void matmul_configure_addrmod_reinit_after_sub()
{
    if constexpr (THROTTLE_LEVEL == 0)
    {
        // The overlapped SDPA SALAD path reuses the no-mop replay buffer across
        // the first-column sub/exp handoff. That path clobbers matmul addrmods
        // and counters, but replay contents remain intact, so after_sub must
        // restore the no-mop addrmod contract without rebuilding replay.
        matmul_configure_addrmod_no_mop<math_fidelity, THROTTLE_LEVEL>(
            matmul_no_mop_state.transpose,
            matmul_no_mop_state.in0_tile_r_dim,
            matmul_no_mop_state.in0_tile_c_dim,
            matmul_no_mop_state.in1_tile_r_dim,
            matmul_no_mop_state.in1_tile_c_dim,
            matmul_no_mop_state.partial_face,
            matmul_no_mop_state.ct_dim,
            matmul_no_mop_state.rt_dim);
        math::reset_counters(p_setrwc::SET_ABD_F);
    }
    else
    {
        _llk_math_matmul_init_<math_fidelity, THROTTLE_LEVEL>(
            matmul_no_mop_state.in0_tile_r_dim,
            matmul_no_mop_state.in0_tile_c_dim,
            matmul_no_mop_state.in1_tile_r_dim,
            matmul_no_mop_state.in1_tile_c_dim,
            matmul_no_mop_state.partial_face,
            matmul_no_mop_state.transpose,
            matmul_no_mop_state.ct_dim,
            matmul_no_mop_state.rt_dim);
    }
}

template <MathFidelity math_fidelity, int THROTTLE_LEVEL = 0>
inline void _llk_math_matmul_init_no_mop_(
    const std::uint32_t in0_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in0_tile_c_dim = TILE_C_DIM,
    const std::uint32_t in1_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in1_tile_c_dim = TILE_C_DIM,
    const bool partial_face            = false,
    const std::uint32_t transpose      = 0,
    const std::uint32_t ct_dim         = 1,
    const std::uint32_t rt_dim         = 1)
{
    matmul_store_no_mop_state(in0_tile_r_dim, in0_tile_c_dim, in1_tile_r_dim, in1_tile_c_dim, partial_face, transpose, ct_dim, rt_dim);

    if constexpr (THROTTLE_LEVEL == 0)
    {
        matmul_configure_addrmod_no_mop<math_fidelity, THROTTLE_LEVEL>(
            transpose, in0_tile_r_dim, in0_tile_c_dim, in1_tile_r_dim, in1_tile_c_dim, partial_face, ct_dim, rt_dim);
        matmul_record_replay_no_mop<math_fidelity>(ct_dim, rt_dim, in0_tile_r_dim, in0_tile_c_dim, in1_tile_r_dim, in1_tile_c_dim, partial_face);
        math::reset_counters(p_setrwc::SET_ABD_F);
    }
    else
    {
        _llk_math_matmul_init_<math_fidelity, THROTTLE_LEVEL>(
            in0_tile_r_dim, in0_tile_c_dim, in1_tile_r_dim, in1_tile_c_dim, partial_face, transpose, ct_dim, rt_dim);
    }
}

inline void _llk_math_matmul_uninit_no_mop_()
{
    _llk_math_matmul_uninit_();
}

template <MathFidelity math_fidelity, int THROTTLE_LEVEL = 0>
inline void _llk_math_matmul_no_mop_(
    std::uint32_t dst_index,
    const std::uint32_t ct_dim         = 1,
    const std::uint32_t rt_dim         = 1,
    const std::uint32_t in0_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in0_tile_c_dim = TILE_C_DIM,
    const std::uint32_t in1_tile_r_dim = TILE_R_DIM,
    const std::uint32_t in1_tile_c_dim = TILE_C_DIM,
    const bool partial_face            = false)
{
    if constexpr (THROTTLE_LEVEL != 0)
    {
        _llk_math_matmul_<math_fidelity, THROTTLE_LEVEL>(dst_index, ct_dim, rt_dim);
        return;
    }

    const bool reuse_a                 = ct_dim >= rt_dim;
    const std::uint32_t t_dim          = reuse_a ? rt_dim : ct_dim;
    const std::uint32_t rut_dim        = reuse_a ? ct_dim : rt_dim;
    const std::uint32_t replay_buf_len = matmul_get_replay_buf_len_no_mop(in0_tile_r_dim, in0_tile_c_dim, in1_tile_r_dim, in1_tile_c_dim, partial_face);

    for (std::uint32_t t = 0; t < t_dim; t++)
    {
        for (std::uint32_t rut = 0; rut < rut_dim; rut++)
        {
            math::set_dst_write_addr<DstTileShape::Tile32x32, UnpackDestination::SrcRegs>(dst_index + (reuse_a ? ct_dim * t + rut : t + rut * ct_dim));

            matmul_execute_replay_no_mop<math_fidelity>(replay_buf_len, reuse_a, t_dim);

            if (t_dim == 1)
            {
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
                if ((t + 1) < t_dim)
                {
                    if (reuse_a)
                    {
                        if (rut == (rut_dim - 1))
                        {
                            TTI_CLEARDVALID(p_setrwc::CLR_B, 0);
                        }
                        else
                        {
                            TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_ABD);
                        }
                    }
                    else
                    {
                        if (rut == (rut_dim - 1))
                        {
                            TTI_CLEARDVALID(p_setrwc::CLR_A, 0);
                        }
                        else
                        {
                            TTI_SETRWC(p_setrwc::CLR_A, 0, 0, 0, 0, p_setrwc::SET_ABD);
                        }
                    }

                    math::set_dst_write_addr<DstTileShape::Tile32x32, UnpackDestination::SrcRegs>(
                        dst_index + (reuse_a ? ct_dim * (t + 1) + rut : t + 1 + rut * ct_dim));
                    matmul_execute_replay_no_mop<math_fidelity>(replay_buf_len, reuse_a, t_dim);
                }

                if (reuse_a)
                {
                    if (rut == (rut_dim - 1))
                    {
                        TTI_SETRWC(p_setrwc::CLR_A, 0, 0, 0, 0, p_setrwc::SET_ABD);
                        TTI_CLEARDVALID(p_setrwc::CLR_B, 0);
                    }
                    else
                    {
                        TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_ABD);
                    }
                }
                else
                {
                    if (rut == (rut_dim - 1))
                    {
                        TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_ABD);
                        TTI_CLEARDVALID(p_setrwc::CLR_A, 0);
                    }
                    else
                    {
                        TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_ABD);
                    }
                }
            }
        }
        t++;
    }
}
