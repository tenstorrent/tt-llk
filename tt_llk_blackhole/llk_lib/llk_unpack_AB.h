// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel.h"
#include "ckernel_defs.h"
#include "ckernel_globals.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "cunpack_common.h"

template <ckernel::BroadcastType BType = ckernel::BroadcastType::NONE>
inline void _llk_unpack_AB_mop_config_(const bool transpose_of_faces = false, const std::uint32_t num_faces = 4, const bool narrow_tile = false)
{
#if SKIP_UNP == 1
    static constexpr uint unpack_srca = TT_OP_NOP;
    static constexpr uint unpack_srcb = TT_OP_NOP;
#else
    static constexpr uint unpack_srca = TT_OP_UNPACR(ckernel::SrcA, 0b1, 0, 0, 0, 1, 1, ckernel::p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint unpack_srcb = TT_OP_UNPACR(ckernel::SrcB, 0b1, 0, 0, 0, 1, 1, ckernel::p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
#endif

    if constexpr (BType == ckernel::BroadcastType::COL)
    {
        static constexpr uint unpack_srcb_set_z = TT_OP_SETADCZW(0b010, 0, 0, 0, 2, 0b0001);
        const uint32_t outerloop                = num_faces < 4 ? 1 : 2;
        const uint32_t innerloop                = num_faces < 2 ? 1 : 2;
        ckernel::ckernel_template tmp(outerloop, innerloop, unpack_srca);
        tmp.set_start_op(unpack_srcb);
        if (narrow_tile)
        {
            tmp.set_end_op(unpack_srcb); // Read face 1
        }
        else
        {
            tmp.set_end_op(unpack_srcb_set_z);
        }
        tmp.program(ckernel::instrn_buffer);
    }
    else if constexpr (BType == ckernel::BroadcastType::ROW)
    {
        static constexpr uint unpack_srcb_clear_z  = TT_OP_SETADCZW(0b010, 0, 0, 0, 0, 0b0001);
        static constexpr uint unpack_srcb_no_z_inc = TT_OP_UNPACR(ckernel::SrcB, 0b0, 0, 0, 0, 1, 1, ckernel::p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
        const uint32_t outerloop                   = num_faces < 4 ? 1 : 2;
        const uint32_t innerloop                   = num_faces < 2 ? 1 : 2;
        ckernel::ckernel_template tmp(outerloop, innerloop, narrow_tile ? unpack_srcb_no_z_inc : unpack_srcb, unpack_srca);
        tmp.set_end_op(unpack_srcb_clear_z);
        tmp.program(ckernel::instrn_buffer);
    }
    else if constexpr (BType == ckernel::BroadcastType::SCALAR)
    {
        const uint32_t outerloop = 1;
        const uint32_t innerloop = num_faces;
        ckernel::ckernel_template tmp(outerloop, innerloop, unpack_srca);
        tmp.set_start_op(unpack_srcb);
        tmp.program(ckernel::instrn_buffer);
    }
    else
    {
        if (transpose_of_faces)
        {
            static constexpr uint srca_set_z = TT_OP_SETADCZW(0b001, 0, 0, 0, 1, 0b0001); // set z to 1
            static constexpr uint unpack_srca_skip_z =
                TT_OP_UNPACR(ckernel::SrcA, 0b10, 0, 0, 0, 1, 1, ckernel::p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1); // inc z by 2
            const uint32_t outerloop = num_faces < 4 ? 1 : 2;
            const uint32_t innerloop = num_faces < 2 ? 1 : 2;
            ckernel::ckernel_template tmp(outerloop, innerloop, num_faces < 4 ? unpack_srca : unpack_srca_skip_z, unpack_srcb);
            tmp.set_end_op(srca_set_z);
            tmp.program(ckernel::instrn_buffer);
        }
        else
        {
            constexpr uint32_t outerloop = 1;
            const uint32_t innerloop     = num_faces;
            ckernel::ckernel_template tmp(outerloop, innerloop, unpack_srca, unpack_srcb);
            tmp.program(ckernel::instrn_buffer);
        }
    }
}

template <bool is_fp32_dest_acc_en = false, ckernel::StochRndType stoch_rnd_mode = ckernel::StochRndType::None>
inline void _llk_unpack_AB_hw_configure_(
    const std::uint32_t unpA_src_format,
    const std::uint32_t unpB_src_format,
    const std::uint32_t unpA_dst_format,
    const std::uint32_t unpB_dst_format,
    const std::uint32_t face_r_dim                  = ckernel::FACE_R_DIM,
    const std::uint32_t within_face_16x16_transpose = 0,
    const std::uint32_t num_faces                   = 4)
{
    constexpr bool is_row_pool  = false;
    constexpr bool stoch_rnd_en = (stoch_rnd_mode == ckernel::StochRndType::All);
    constexpr bool fpu_srnd_en  = stoch_rnd_en || (stoch_rnd_mode == ckernel::StochRndType::Fpu);
    constexpr bool pack_srnd_en = stoch_rnd_en || (stoch_rnd_mode == ckernel::StochRndType::Pack);
    ckernel::unpacker::configure_unpack_AB<is_row_pool, is_fp32_dest_acc_en, fpu_srnd_en, pack_srnd_en>(
        unpA_src_format, unpB_src_format, unpA_dst_format, unpB_dst_format, face_r_dim, face_r_dim, within_face_16x16_transpose, num_faces, num_faces);
}

template <ckernel::BroadcastType BType = ckernel::BroadcastType::NONE>
inline void _llk_unpack_AB_init_(
    const std::uint32_t face_r_dim  = ckernel::FACE_R_DIM,
    const std::uint32_t num_faces   = 4,
    const bool narrow_tile          = false,
    const std::uint32_t transpose   = 0,
    const std::uint32_t acc_to_dest = 0)
{
    ckernel::cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(transpose); // transpose within the face

    constexpr std::uint32_t UNP_SEL = ckernel::p_setadc::UNP_AB;
    ckernel::unpacker::config_unpacker_x_end<UNP_SEL>(face_r_dim);

    _llk_unpack_AB_mop_config_<BType>(transpose > 0, num_faces, narrow_tile); // transpose of faces 0,2,1,3
}

template <ckernel::BroadcastType BType = ckernel::BroadcastType::NONE>
inline void _llk_unpack_AB_(const std::uint32_t address_a, const std::uint32_t address_b, const bool transpose_of_faces = 0 /*not used*/)
{
    TTI_SETADCZW(0b011, 0, 0, 0, 0, 0b1111); // reset counters

    // Program srcA and srcB base addresses
    volatile uint tt_reg_ptr *cfg = ckernel::get_cfg_pointer(); // get pointer to registers for current state ID

    // Wait for free context
    ckernel::unpacker::wait_for_next_context(2);

    // Get tile address
    if (0 == unp_cfg_context)
    {
        cfg[THCON_SEC0_REG3_Base_address_ADDR32] = address_a;
        cfg[THCON_SEC1_REG3_Base_address_ADDR32] = address_b;
    }
    else
    {
        cfg[THCON_SEC0_REG3_Base_cntx1_address_ADDR32] = address_a;
        cfg[THCON_SEC1_REG3_Base_cntx1_address_ADDR32] = address_b;
    }

    // Trisc::SEMPOST for context acquire
    ckernel::semaphore_post(ckernel::semaphore::UNPACK_SYNC);

    // Stall unpacker until pending CFG writes from Trisc have completed
    TTI_STALLWAIT(ckernel::p_stall::STALL_UNPACK, ckernel::p_stall::TRISC_CFG);

    // Run MOP
    ckernel::ckernel_template::run(ckernel::instrn_buffer);

    // T6::SEMGET for context release
    ckernel::t6_semaphore_get(ckernel::semaphore::UNPACK_SYNC);

    // Switch unpacker config context
    ckernel::unpacker::switch_config_context(unp_cfg_context);

#ifdef PERF_DUMP
    first_unpack_recorded = true;
#endif
}
