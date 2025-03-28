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

template <ckernel::PoolType type, ckernel::ReduceDim dim>
inline void _llk_unpack_reduce_mop_config_()
{
#if SKIP_UNP == 1
    static constexpr uint unpack_srca = TT_OP_NOP;
#else
    static constexpr uint unpack_srca = TT_OP_UNPACR(ckernel::SrcA, 0b1, 0, 0, 0, 1, 1, ckernel::p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
#endif
    static constexpr uint unpack_zerosrca =
        TT_OP_UNPACR_NOP(ckernel::p_unpacr_nop::UNP0, 0, 0, 0, 0, 0, 0, ckernel::p_unpacr_nop::CLR_SRC_0, ckernel::p_unpacr_nop::CLR_SRC);
#if SKIP_UNP == 1
    static constexpr uint unpack_srcb = TT_OP_NOP;
#else
    static constexpr uint unpack_srcb = TT_OP_UNPACR(ckernel::SrcB, 0b0, 0, 0, 0, 1, 1, ckernel::p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
#endif
    ckernel::ckernel_unpack_template tmp = ckernel::ckernel_unpack_template(
        true, // src B
        true, // halo - just used for 4 unpacks
        unpack_zerosrca,
        unpack_srca,
        TT_OP_NOP,
        TT_OP_NOP,
        0,
        unpack_srcb,
        0);
    tmp.program(ckernel::instrn_buffer);
}

template <bool is_fp32_dest_acc_en = false, ckernel::StochRndType stoch_rnd_mode = ckernel::StochRndType::None>
inline void _llk_unpack_reduce_hw_configure_(
    const std::uint32_t unpA_src_format,
    const std::uint32_t unpB_src_format,
    const std::uint32_t unpA_dst_format,
    const std::uint32_t unpB_dst_format,
    const std::uint32_t unpA_face_r_dim             = ckernel::FACE_R_DIM,
    const std::uint32_t unpB_face_r_dim             = ckernel::FACE_R_DIM,
    const std::uint32_t within_face_16x16_transpose = 0,
    const std::uint32_t unpA_num_faces              = 4,
    const std::uint32_t unpB_num_faces              = 4)
{
    constexpr bool is_row_pool  = true;
    constexpr bool stoch_rnd_en = (stoch_rnd_mode == ckernel::StochRndType::All);
    constexpr bool fpu_srnd_en  = stoch_rnd_en || (stoch_rnd_mode == ckernel::StochRndType::Fpu);
    constexpr bool pack_srnd_en = stoch_rnd_en || (stoch_rnd_mode == ckernel::StochRndType::Pack);

    ckernel::unpacker::configure_unpack_AB<is_row_pool, is_fp32_dest_acc_en, fpu_srnd_en, pack_srnd_en>(
        unpA_src_format,
        unpB_src_format,
        unpA_dst_format,
        unpB_dst_format,
        unpA_face_r_dim,
        unpB_face_r_dim,
        within_face_16x16_transpose,
        unpA_num_faces,
        unpB_num_faces);
}

template <ckernel::PoolType type, ckernel::ReduceDim dim>
inline void _llk_unpack_reduce_init_(const std::uint32_t within_face_16x16_transpose = 0)
{
    // REDUCE_ROW requires transpose itself; additionaly, within_face_16x16_transpose flag could require transpose;
    // if we have the flag set with REDUCE_ROW, we don't need to do anything
    ckernel::cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(
        ckernel::ReduceDim::REDUCE_ROW == dim ? !within_face_16x16_transpose : within_face_16x16_transpose);

    TTI_SETADCXX(0b11, ckernel::FACE_R_DIM * ckernel::FACE_C_DIM - 1, 0x0);

    _llk_unpack_reduce_mop_config_<type, dim>();
}

template <ckernel::PoolType type, ckernel::ReduceDim dim>
inline void _llk_unpack_reduce_(const std::uint32_t address)
{
    // Clear z/w start counters
    TTI_SETADCZW(0b011, 0, 0, 0, 0, 0b1111);

    // Program srcA and srcB base addresses
    volatile uint tt_reg_ptr *cfg = ckernel::get_cfg_pointer(); // get pointer to registers for current state ID

    // Wait for free context
    ckernel::unpacker::wait_for_next_context(2);

    // Get tile address
    if (0 == unp_cfg_context)
    {
        cfg[THCON_SEC0_REG3_Base_address_ADDR32] = address;
    }
    else
    {
        cfg[THCON_SEC0_REG3_Base_cntx1_address_ADDR32] = address;
    }

    // Trisc::SEMPOST for context acquire
    ckernel::semaphore_post(ckernel::semaphore::UNPACK_SYNC);

    // Load only 16 datums into srcB
    TTI_SETADCXX(ckernel::p_setadc::UNP1, ckernel::FACE_C_DIM - 1, 0x0);

    // Stall unpacker until pending CFG writes from Trisc have completed
    TTI_STALLWAIT(ckernel::p_stall::STALL_UNPACK, ckernel::p_stall::TRISC_CFG);

    // Run MOP
    mop_run(0, 4);

    // Restore face height
    TTI_SETADCXX(p_setadc::UNP1, FACE_R_DIM * FACE_C_DIM - 1, 0x0);

    // T6::SEMGET for context release
    ckernel::t6_semaphore_get(ckernel::semaphore::UNPACK_SYNC);

    // Switch unpacker config context
    ckernel::unpacker::switch_config_context(unp_cfg_context);

#ifdef PERF_DUMP
    first_unpack_recorded = true;
#endif
}
