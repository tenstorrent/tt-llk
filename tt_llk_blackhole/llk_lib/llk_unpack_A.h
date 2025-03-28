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

#ifndef SKIP_UNP
#define SKIP_UNP 0
#endif

template <
    ckernel::BroadcastType BType                          = ckernel::BroadcastType::NONE,
    bool acc_to_dest                                      = false,
    ckernel::EltwiseBinaryReuseDestType binary_reuse_dest = ckernel::EltwiseBinaryReuseDestType::NONE,
    bool unpack_to_dest                                   = false>
inline void _llk_unpack_A_mop_config_(
    const bool transpose_of_faces, const std::uint32_t num_faces, const std::uint32_t unpack_src_format, const std::uint32_t unpack_dst_format = 0)
{
    static_assert(
        !((BType != ckernel::BroadcastType::NONE) && acc_to_dest && (binary_reuse_dest == ckernel::EltwiseBinaryReuseDestType::DEST_TO_SRCB)),
        "Not supported configuration!");
    static_assert(
        (((BType == ckernel::BroadcastType::NONE) && (!acc_to_dest) && (binary_reuse_dest == ckernel::EltwiseBinaryReuseDestType::NONE)) || (!unpack_to_dest)),
        "Not supported configuration when unpacking to dest!");

#if SKIP_UNP == 1
    static constexpr uint unpack_srca            = TT_OP_NOP;
    static constexpr uint unpack_srca_to_dest    = TT_OP_NOP;
    static constexpr uint unpack_srca_set_dvalid = TT_OP_NOP;
    static constexpr uint unpack_srcb            = TT_OP_NOP;
    static constexpr uint unpack_srcb_inc_z_0    = TT_OP_NOP;
    static constexpr uint unpack_srcb_zerosrc    = TT_OP_NOP;
    static constexpr uint unpack_srcb_set_dvalid = TT_OP_NOP;
    static constexpr uint srca_set_z_1           = TT_OP_NOP;
    static constexpr uint srcb_set_z_2           = TT_OP_NOP;
    static constexpr uint srcb_clear_z           = TT_OP_NOP;
    constexpr uint replay_buf_len                = 1;
    ckernel::load_replay_buf<0, 1>([] { TTI_NOP; });
#else
    static constexpr uint unpack_srca =
        TT_OP_UNPACR(ckernel::SrcA, 0b1 /*Z inc*/, 0, 0, 0, 1 /* Set OvrdThreadId*/, 1 /*Set Dvalid*/, ckernel::p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint unpack_srca_to_dest = TT_OP_UNPACR(
        ckernel::SrcA, 0b00010001 /*Z inc*/, 0, 0, 0, 1 /* Set OvrdThreadId*/, 0 /*Set Dvalid*/, ckernel::p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1); // ch0/ch1
                                                                                                                                                     // z_inc
    static constexpr uint unpack_srca_set_dvalid =
        TT_OP_UNPACR_NOP(ckernel::SrcA, 0, 0, ckernel::p_unpacr_nop::SET_DVALID, 0, 0, 0, 0, ckernel::p_unpacr_nop::UNP_ZEROSRC);
    static constexpr uint unpack_srcb =
        TT_OP_UNPACR(ckernel::SrcB, 0b1 /*Z inc*/, 0, 0, 0, 1 /* Set OvrdThreadId*/, 1 /*Set Dvalid*/, ckernel::p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint unpack_srcb_inc_z_0 =
        TT_OP_UNPACR(ckernel::SrcB, 0b0 /*Z inc*/, 0, 0, 0, 1 /* Set OvrdThreadId*/, 1 /*Set Dvalid*/, ckernel::p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint unpack_srcb_set_dvalid =
        TT_OP_UNPACR_NOP(ckernel::SrcB, 0, 0, ckernel::p_unpacr_nop::SET_DVALID, 0, 0, 0, 0, ckernel::p_unpacr_nop::UNP_ZEROSRC);
    static constexpr uint srca_set_z_1 = TT_OP_SETADCZW(ckernel::p_setadc::UNP_A, 0, 0, 0, 1, 0b0001); // set srcA ch0_z = 1
    static constexpr uint srcb_set_z_2 = TT_OP_SETADCZW(ckernel::p_setadc::UNP_B, 0, 0, 0, 2, 0b0001); // set srcB ch0_z = 2
    static constexpr uint srcb_clear_z = TT_OP_SETADCZW(ckernel::p_setadc::UNP_B, 0, 0, 0, 0, 0b0001); // set srcB ch0_z = 0
#endif

    if (unpack_to_dest && ckernel::unpacker::is_32bit_input(unpack_src_format, unpack_dst_format))
    {
        const uint32_t outerloop     = num_faces;
        constexpr uint32_t innerloop = 1;
        ckernel::ckernel_template tmp(outerloop, innerloop, unpack_srca_to_dest);
        tmp.program(ckernel::instrn_buffer);
    }
    else if constexpr (BType == ckernel::BroadcastType::COL)
    {
        if constexpr (acc_to_dest)
        {
            constexpr uint32_t innerloop = 1;
            constexpr uint32_t outerloop = 2; // TODO: add support for num_faces, add support for dest to srcB
            ckernel::ckernel_template tmp(outerloop, innerloop, unpack_srca_set_dvalid, unpack_srca_set_dvalid);
            tmp.set_start_op(unpack_srcb);
            tmp.set_end_op(srcb_set_z_2);
            tmp.program(ckernel::instrn_buffer);
        }
        else
        {
            constexpr uint32_t innerloop = 1;
            constexpr uint32_t outerloop = 1; // TODO: add support for num_faces, add support for dest to srcB
            ckernel::ckernel_template tmp(outerloop, innerloop, unpack_srcb, srcb_set_z_2);
            tmp.set_start_op(unpack_srca_set_dvalid);
            tmp.set_end_op(unpack_srcb);
            tmp.program(ckernel::instrn_buffer);
        }
    }
    else if constexpr (BType == ckernel::BroadcastType::ROW)
    {
        constexpr uint32_t innerloop = 2;
        constexpr uint32_t outerloop = 2; // TODO: add support for num_faces
        if constexpr (acc_to_dest)
        {
            ckernel::ckernel_template tmp(outerloop, innerloop, unpack_srcb, unpack_srca_set_dvalid);
            tmp.set_end_op(srcb_clear_z);
            tmp.program(ckernel::instrn_buffer);
        }
        else
        {
            ckernel::ckernel_template tmp(outerloop, innerloop, unpack_srcb);
            tmp.set_end_op(srcb_clear_z);
            tmp.program(ckernel::instrn_buffer);
        }
    }
    else if constexpr (BType == ckernel::BroadcastType::SCALAR)
    {
        static_assert((!acc_to_dest) && "accumulate into dest with broadcast scaler is not supported!");
        const uint32_t outerloop     = 1;
        constexpr uint32_t innerloop = 1;
        ckernel::ckernel_template tmp(outerloop, innerloop, unpack_srcb_inc_z_0);
        // ELWADD used in datacopy due to broadcast bug, use zerosrca regardless of acc_to_dest
        tmp.set_start_op(unpack_srca_set_dvalid);
        tmp.program(ckernel::instrn_buffer);
    }
    else
    {
        if (transpose_of_faces)
        {
#if SKIP_UNP == 0
            constexpr uint replay_buf_len = 2;
            ckernel::load_replay_buf<0, replay_buf_len>(
                [num_faces]
                {
                    TTI_UNPACR_NOP(ckernel::SrcB, 0, 0, ckernel::p_unpacr_nop::SET_DVALID, 0, 0, 0, 0, ckernel::p_unpacr_nop::UNP_ZEROSRC);
                    if (num_faces > 2)
                    {
                        TTI_UNPACR(ckernel::SrcA, 0b10, 0, 0, 0, 1, 1, ckernel::p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1); // inc srcA ch0_z+=2
                    }
                    else
                    {
                        TTI_UNPACR(ckernel::SrcA, 0b01, 0, 0, 0, 1, 1, ckernel::p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1); // inc srcA ch0_z+=1
                    }
                });
#endif
            const uint32_t outerloop = num_faces < 4 ? 1 : 2;
            const uint32_t innerloop = num_faces < 2 ? 1 : 2;
            ckernel::ckernel_template tmp(outerloop, innerloop, TT_OP_REPLAY(0, replay_buf_len, 0, 0)); // Unpack faces 0/2 && 1/3 to srcA
                                                                                                        // or 0/1 for 2 face tile
            if (num_faces > 2)
            {
                tmp.set_end_op(srca_set_z_1);
            }
            tmp.program(ckernel::instrn_buffer);
        }
        else
        {
            if constexpr (acc_to_dest)
            {
                static constexpr uint unpack_srca_reuse =
                    (binary_reuse_dest == ckernel::EltwiseBinaryReuseDestType::DEST_TO_SRCA) ? unpack_srca_set_dvalid : unpack_srca;

                static constexpr uint unpack_srcb_reuse =
                    (binary_reuse_dest == ckernel::EltwiseBinaryReuseDestType::DEST_TO_SRCB) ? unpack_srcb_set_dvalid : unpack_srcb;

                const uint32_t outerloop     = num_faces;
                constexpr uint32_t innerloop = 1;
                ckernel::ckernel_template tmp(outerloop, innerloop, unpack_srca_reuse, unpack_srcb_reuse);
                tmp.program(ckernel::instrn_buffer);
            }
            else
            {
                const uint32_t outerloop     = num_faces;
                constexpr uint32_t innerloop = 1;
                ckernel::ckernel_template tmp(outerloop, innerloop, unpack_srcb_set_dvalid);
                tmp.set_start_op(unpack_srca);
                tmp.program(ckernel::instrn_buffer);
            }
        }
    }
}

template <bool is_fp32_dest_acc_en = false, ckernel::StochRndType stoch_rnd_mode = ckernel::StochRndType::None, bool disable_src_zero_flag = false>
inline void _llk_unpack_A_hw_configure_(
    const std::uint32_t unpack_src_format,
    const std::uint32_t unpack_dst_format,
    const std::uint32_t face_r_dim                  = ckernel::FACE_R_DIM,
    const std::uint32_t within_face_16x16_transpose = 0,
    const std::uint32_t num_faces                   = 4)
{
    constexpr bool is_row_pool  = false;
    constexpr bool stoch_rnd_en = (stoch_rnd_mode == ckernel::StochRndType::All);
    constexpr bool fpu_srnd_en  = stoch_rnd_en || (stoch_rnd_mode == ckernel::StochRndType::Fpu);
    constexpr bool pack_srnd_en = stoch_rnd_en || (stoch_rnd_mode == ckernel::StochRndType::Pack);
    ckernel::unpacker::configure_unpack_AB<is_row_pool, is_fp32_dest_acc_en, fpu_srnd_en, pack_srnd_en, disable_src_zero_flag>(
        unpack_src_format, unpack_src_format, unpack_dst_format, unpack_dst_format, face_r_dim, face_r_dim, within_face_16x16_transpose, num_faces, num_faces);
}

template <
    ckernel::BroadcastType BType                          = ckernel::BroadcastType::NONE,
    bool acc_to_dest                                      = false,
    ckernel::EltwiseBinaryReuseDestType binary_reuse_dest = ckernel::EltwiseBinaryReuseDestType::NONE,
    bool unpack_to_dest                                   = false>
inline void _llk_unpack_A_init_(
    const std::uint32_t transpose_of_faces          = 0,
    const std::uint32_t within_face_16x16_transpose = 0,
    const std::uint32_t face_r_dim                  = ckernel::FACE_R_DIM,
    const std::uint32_t num_faces                   = 4,
    const std::uint32_t unpack_src_format           = 0,
    const std::uint32_t unpack_dst_format           = 0)
{
    constexpr std::uint32_t UNP_SEL = (BType == ckernel::BroadcastType::NONE) ? ckernel::p_setadc::UNP_A : ckernel::p_setadc::UNP_B;
    ckernel::unpacker::config_unpacker_x_end<UNP_SEL>(face_r_dim);
    _llk_unpack_A_mop_config_<BType, acc_to_dest, binary_reuse_dest, unpack_to_dest>(transpose_of_faces > 0, num_faces, unpack_src_format, unpack_dst_format);
}

template <
    ckernel::BroadcastType BType                          = ckernel::BroadcastType::NONE,
    bool acc_to_dest                                      = false,
    ckernel::EltwiseBinaryReuseDestType binary_reuse_dest = ckernel::EltwiseBinaryReuseDestType::NONE,
    bool unpack_to_dest                                   = false>
inline void _llk_unpack_A_(
    const std::uint32_t address, const bool transpose_of_faces = 0, const std::uint32_t unpack_src_format = 0, const std::uint32_t unpack_dst_format = 0)
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
        if constexpr ((BType == ckernel::BroadcastType::NONE) && (!acc_to_dest))
        {
            cfg[THCON_SEC0_REG3_Base_address_ADDR32] = address;
        }
        else
        {
            if constexpr (binary_reuse_dest == ckernel::EltwiseBinaryReuseDestType::DEST_TO_SRCB)
            {
                cfg[THCON_SEC0_REG3_Base_address_ADDR32] = address;
            }
            cfg[THCON_SEC1_REG3_Base_address_ADDR32] = address;
        }
    }
    else
    {
        if constexpr ((BType == ckernel::BroadcastType::NONE) && (!acc_to_dest))
        {
            cfg[THCON_SEC0_REG3_Base_cntx1_address_ADDR32] = address;
        }
        else
        {
            if constexpr (binary_reuse_dest == ckernel::EltwiseBinaryReuseDestType::DEST_TO_SRCB)
            {
                cfg[THCON_SEC0_REG3_Base_cntx1_address_ADDR32] = address;
            }
            cfg[THCON_SEC1_REG3_Base_cntx1_address_ADDR32] = address;
        }
    }

    // Trisc::SEMPOST for context acquire
    ckernel::semaphore_post(ckernel::semaphore::UNPACK_SYNC);

    if constexpr (unpack_to_dest)
    {
        if (ckernel::unpacker::is_32bit_input(unpack_src_format, unpack_dst_format))
        {
            ckernel::unpacker::set_dst_write_addr(unp_cfg_context, unpack_dst_format);
            ckernel::unpacker::wait_for_dest_available();
        }
    }

    // Stall unpacker until pending CFG writes from Trisc have completed
    TTI_STALLWAIT(ckernel::p_stall::STALL_UNPACK, ckernel::p_stall::TRISC_CFG);

    // Run MOP
    ckernel::ckernel_template::run(ckernel::instrn_buffer);

    // T6::SEMGET for context release
    ckernel::t6_semaphore_get(ckernel::semaphore::UNPACK_SYNC);

    if (unpack_to_dest)
    {
        if (ckernel::unpacker::is_32bit_input(unpack_src_format, unpack_dst_format))
        {
            ckernel::unpacker::unpack_to_dest_tile_done(unp_cfg_context);
        }
    }

    // Switch unpacker config context
    ckernel::unpacker::switch_config_context(unp_cfg_context);

#ifdef PERF_DUMP
    first_unpack_recorded = true;
#endif
}
