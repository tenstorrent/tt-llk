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
#include "llk_unpack_common.h"
#include "lltt.h"

using namespace ckernel;
using namespace ckernel::unpacker;

template <BroadcastType BType = BroadcastType::NONE>
inline void _llk_unpack_AB_mop_config_(const bool transpose_of_faces = false, const std::uint32_t num_faces = 4, const bool narrow_tile = false)
{
    static constexpr uint unpack_srca = TT_OP_UNPACR(SrcA, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint unpack_srcb = TT_OP_UNPACR(SrcB, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    if constexpr (BType == BroadcastType::COL)
    {
        static constexpr uint unpack_srcb_set_z = TT_OP_SETADCZW(0b010, 0, 0, 0, 2, 0b0001);
        const uint32_t outerloop                = num_faces < 4 ? 1 : 2;
        const uint32_t innerloop                = num_faces < 2 ? 1 : 2;
        ckernel_template tmp(outerloop, innerloop, unpack_srca);
        tmp.set_start_op(unpack_srcb);
        if (narrow_tile)
        {
            tmp.set_end_op(unpack_srcb); // Read face 1
        }
        else
        {
            tmp.set_end_op(unpack_srcb_set_z);
        }
        tmp.program();
    }
    else if constexpr (BType == BroadcastType::ROW)
    {
        static constexpr uint unpack_srcb_clear_z  = TT_OP_SETADCZW(0b010, 0, 0, 0, 0, 0b0001);
        static constexpr uint unpack_srcb_no_z_inc = TT_OP_UNPACR(SrcB, 0b0, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
        const uint32_t outerloop                   = num_faces < 4 ? 1 : 2;
        const uint32_t innerloop                   = num_faces < 2 ? 1 : 2;
        ckernel_template tmp(outerloop, innerloop, narrow_tile ? unpack_srcb_no_z_inc : unpack_srcb, unpack_srca);
        tmp.set_end_op(unpack_srcb_clear_z);
        tmp.program();
    }
    else if constexpr (BType == BroadcastType::SCALAR)
    {
        const uint32_t outerloop = 1;
        const uint32_t innerloop = num_faces;
        ckernel_template tmp(outerloop, innerloop, unpack_srca);
        tmp.set_start_op(unpack_srcb);
        tmp.program();
    }
    else
    {
        if (transpose_of_faces)
        {
            static constexpr uint srca_set_z         = TT_OP_SETADCZW(0b001, 0, 0, 0, 1, 0b0001);                                         // set z to 1
            static constexpr uint unpack_srca_skip_z = TT_OP_UNPACR(SrcA, 0b10, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1); // inc z by 2
            const uint32_t outerloop                 = num_faces < 4 ? 1 : 2;
            const uint32_t innerloop                 = num_faces < 2 ? 1 : 2;
            ckernel_template tmp(outerloop, innerloop, num_faces < 4 ? unpack_srca : unpack_srca_skip_z, unpack_srcb);
            tmp.set_end_op(srca_set_z);
            tmp.program();
        }
        else
        {
            constexpr uint32_t outerloop = 1;
            const uint32_t innerloop     = num_faces;
            ckernel_template tmp(outerloop, innerloop, unpack_srca, unpack_srcb);
            tmp.program();
        }
    }
}

/**
 * Configures MOP (Macro Operation) for specialized reduce_row_max unpacking operations.
 *
 * This function works with the following assumptions:
 * - Scaler values are 1.0 and are contained inside F0 of the scaler tile
 * - The scaler doesn't change for the duration of the whole tile operation
 * - Operand and scaler data format is bfloat16_b
 * - Operand tile size is 32x32
 * - Can work on both 16-bit or 32-bit DEST register modes based on is_fp32_dest_acc_en flag
 * - Does only MAX pool on ROW dimension
 *
 * This function should NOT be used as a substitute for native reduce unpacking LLK MOP configuration.
 * Use the standard _llk_unpack_AB_mop_config_ for general-purpose reduction operations.
 */
inline void _llk_unpack_AB_reduce_row_max_mop_config_()
{
    // Unpack three faces of operand into SrcA
    // Unpack last face of the operand into SrcA and a single face of scaler in SrcB, will set the Dvalid bits.
    static constexpr uint unpack_srca_end_op =
        TT_OP_UNPACR(SrcA, 0b00010001 /* Z_ch0_inc and Z_ch1_inc */, 0, 0, 0, 1, 1 /* Set Dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint unpack_srcb_end_op =
        TT_OP_UNPACR(SrcB, 0b00010001 /* Z_ch0_inc and Z_ch1_inc */, 0, 0, 0, 1, 1 /* Set Dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    constexpr uint32_t outerloop = 1;
    const uint32_t innerloop     = 1;
    ckernel_template tmp(outerloop, innerloop, unpack_srca_end_op, unpack_srcb_end_op);
    tmp.program();
}

/**
 * Initializes specialized unpack for reduce_row_max operation.
 *
 * This function works with the following assumptions:
 * - Scaler values are 1.0 and are contained inside F0 of the scaler tile
 * - The scaler doesn't change for the duration of the whole tile operation
 * - Operand and scaler data format is bfloat16_b
 * - Operand tile size is 32x32
 * - Can work on both 16-bit or 32-bit DEST register modes based on is_fp32_dest_acc_en flag
 * - Does only MAX pool on ROW dimension
 *
 * This function should NOT be used as a substitute for native llk_unpack_AB_reduce_init LLK.
 * Use the standard llk_unpack_AB_reduce_init<ReduceDim::REDUCE_ROW> for general-purpose reduction.
 */
template <bool is_fp32_dest_acc_en = false>
inline void _llk_unpack_AB_reduce_row_max_init_()
{
    if constexpr (is_fp32_dest_acc_en)
    {
        // Set necessary config regs for MOVB2D hi16/lo16 to work
        _llk_unpack_dbg_feature_disable_();
        cfg_reg_rmw_tensix<ALU_ACC_CTRL_Zero_Flag_disabled_src_RMW>(1);
    }
    // REDUCE_ROW requires transpose itself; additionally, within_face_16x16_transpose flag could require transpose;
    // if we have the flag set with REDUCE_ROW, we don't need to do anything
    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(1);

    TTI_SETADCXX(p_setadc::UNP_B, FACE_R_DIM * FACE_C_DIM - 1, 0x0);       // Unpack a single face of a scaler
    TTI_SETADCXX(p_setadc::UNP_A, 4 * (FACE_R_DIM * FACE_C_DIM) - 1, 0x0); // Unpack a whole tile of an operand
    _llk_unpack_AB_reduce_row_max_mop_config_();                           // Unpack operand and scaler
}

/**
 * Configures MOP (Macro Operation) for block-based reduce_max_row unpacking operations.
 *
 * This function works with the following assumptions:
 * - Scaler values are 1.0 and are contained inside F0 of the scaler tile
 * - The scaler doesn't change for the duration of the whole block operation
 * - Operand and scaler data format is bfloat16_b
 * - Operand tile size is 32x32
 * - Can work on both 16-bit or 32-bit DEST register modes based on is_fp32_dest_acc_en flag
 * - Does only MAX pool on ROW dimension
 *
 * This function should NOT be used as a substitute for native reduce unpacking LLK MOP configuration.
 * Use the standard _llk_unpack_AB_mop_config_ for general-purpose block reduction operations.
 */
template <uint32_t block_ct_dim>
inline void _llk_unpack_AB_reduce_block_max_row_mop_config_()
{
    // Constraint on the outerloop and innerloop dim
    static_assert(block_ct_dim < 128, "block_ct_dim must be less than 128");
    // Single UNPACR because TTI_SETADCXX for UNP_A is 1023, increment Z counter to point to the next tile, set dvalid each time
    static constexpr uint unpack_srca_op =
        TT_OP_UNPACR(SrcA, 0b00000001 /* Z_ch0_inc and Z_ch1_inc */, 0, 0, 0, 1, 1 /* Set Dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    constexpr uint32_t outerloop = block_ct_dim;
    const uint32_t innerloop     = 1; // Unpack tile by tile of the input operand into SrcA
    ckernel_template tmp(outerloop, innerloop, unpack_srca_op);
    tmp.program();
}

/**
 * Initializes unpacker configuration for block-based reduce_max_row operations.
 * Sets up tile dimensions and saves unpacker state that will be modified during operation.
 *
 * This function works with the following assumptions:
 * - Scaler values are 1.0 and are contained inside F0 of the scaler tile
 * - The scaler doesn't change for the duration of the whole block operation
 * - Operand and scaler data format is bfloat16_b
 * - Operand tile size is 32x32
 * - Can work on both 16-bit or 32-bit DEST register modes based on is_fp32_dest_acc_en flag
 * - Does only MAX pool on ROW dimension
 *
 * This function should NOT be used as a substitute for native reduce unpacking LLK initialization.
 * Use the standard _llk_unpack_AB_reduce_init_ for general-purpose reduction operations.
 */
template <uint32_t block_ct_dim, bool is_fp32_dest_acc_en = false>
inline void _llk_unpack_AB_reduce_block_max_row_init_()
{
    if constexpr (is_fp32_dest_acc_en)
    {
        // Set necessary config regs for MOVB2D hi16/lo16 to work
        _llk_unpack_dbg_feature_disable_();
        cfg_reg_rmw_tensix<ALU_ACC_CTRL_Zero_Flag_disabled_src_RMW>(1);
    }
    // REDUCE_ROW requires transpose itself; additionally, within_face_16x16_transpose flag could require transpose;
    // if we have the flag set with REDUCE_ROW, we don't need to do anything
    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(1);

    TTI_SETADCXX(p_setadc::UNP_B, FACE_R_DIM * FACE_C_DIM - 1, 0x0);       // Unpack a single face of a scaler
    TTI_SETADCXX(p_setadc::UNP_A, 4 * (FACE_R_DIM * FACE_C_DIM) - 1, 0x0); // Unpack a whole tile of an operand

    // save the following state that is going to be modified:
    // tile x, y, and z dims for both unpackers
    // CH1 Z stride for both unpackers
    TTI_RDCFG(p_gpr_unpack::SR_UNPACK_UNTILIZER_STATE_0, THCON_SEC0_REG5_Tile_x_dim_cntx0_ADDR32);
    TTI_RDCFG(p_gpr_unpack::SR_UNPACK_UNTILIZER_STATE_1, THCON_SEC0_REG0_TileDescriptor_ADDR32 + 1);

    TTI_SETDMAREG(p_setdmareg::PAYLOAD_IMMEDIATE, 1 * FACE_C_DIM * FACE_R_DIM, p_setdmareg::MODE_IMMEDIATE, LO_16(p_gpr_unpack::TMP0));
    TTI_SETDMAREG(p_setdmareg::PAYLOAD_IMMEDIATE, 1 * FACE_C_DIM * FACE_R_DIM, p_setdmareg::MODE_IMMEDIATE, HI_16(p_gpr_unpack::TMP0));
    TTI_WRCFG(p_gpr_unpack::TMP0, p_cfg::WRCFG_32b, THCON_SEC0_REG5_Tile_x_dim_cntx0_ADDR32);
    TTI_SETDMAREG(p_setdmareg::PAYLOAD_IMMEDIATE, 4 /* y_dim */, p_setdmareg::MODE_IMMEDIATE, LO_16(p_gpr_unpack::TMP0));
    TTI_SETDMAREG(p_setdmareg::PAYLOAD_IMMEDIATE, 1 /* z_dim */, p_setdmareg::MODE_IMMEDIATE, HI_16(p_gpr_unpack::TMP0));
    TTI_WRCFG(p_gpr_unpack::TMP0, p_cfg::WRCFG_32b, THCON_SEC0_REG0_TileDescriptor_ADDR32 + 1);

    _llk_unpack_AB_reduce_block_max_row_mop_config_<block_ct_dim>(); // Unpack operand and scaler
}

template <bool is_fp32_dest_acc_en, StochRndType stoch_rnd_mode = StochRndType::None>
inline void _llk_unpack_AB_hw_configure_(
    const std::uint32_t unpA_src_format,
    const std::uint32_t unpB_src_format,
    const std::uint32_t unpA_dst_format,
    const std::uint32_t unpB_dst_format,
    const std::uint32_t face_r_dim                  = FACE_R_DIM,
    const std::uint32_t within_face_16x16_transpose = 0,
    const std::uint32_t num_faces                   = 4)
{
    constexpr bool is_row_pool  = false;
    constexpr bool stoch_rnd_en = (stoch_rnd_mode == StochRndType::All);
    constexpr bool fpu_srnd_en  = stoch_rnd_en || (stoch_rnd_mode == StochRndType::Fpu);
    constexpr bool pack_srnd_en = stoch_rnd_en || (stoch_rnd_mode == StochRndType::Pack);
    configure_unpack_AB<is_fp32_dest_acc_en, is_row_pool, fpu_srnd_en, pack_srnd_en>(
        unpA_src_format, unpB_src_format, unpA_dst_format, unpB_dst_format, face_r_dim, face_r_dim, within_face_16x16_transpose, num_faces, num_faces);
}

template <BroadcastType BType = BroadcastType::NONE>
inline void _llk_unpack_AB_init_(
    const std::uint32_t face_r_dim                   = FACE_R_DIM,
    const std::uint32_t num_faces                    = 4,
    const bool narrow_tile                           = false,
    const std::uint32_t transpose                    = 0,
    [[maybe_unused]] const std::uint32_t acc_to_dest = 0)
{
    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(transpose); // transpose within the face

    constexpr std::uint32_t UNP_SEL = p_setadc::UNP_AB;
    config_unpacker_x_end<UNP_SEL>(face_r_dim);

    _llk_unpack_AB_mop_config_<BType>(transpose > 0, num_faces, narrow_tile); // transpose of faces 0,2,1,3
}

template <BroadcastType BType = BroadcastType::NONE>
inline void _llk_unpack_AB_(const std::uint32_t address_a, const std::uint32_t address_b, [[maybe_unused]] const bool transpose_of_faces = 0)
{
    TTI_SETADCZW(0b011, 0, 0, 0, 0, 0b1111); // reset counters

    // Program srcA and srcB base addresses
    volatile uint tt_reg_ptr *cfg = get_cfg_pointer(); // get pointer to registers for current state ID

    // Wait for free context
    wait_for_next_context(2);

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
    semaphore_post(semaphore::UNPACK_SYNC);

    // Stall unpacker until pending CFG writes from Trisc have completed
    TTI_STALLWAIT(p_stall::STALL_UNPACK, p_stall::TRISC_CFG);

    // Run MOP
    ckernel::ckernel_template::run();

    // T6::SEMGET for context release
    t6_semaphore_get(semaphore::UNPACK_SYNC);

    // Switch unpacker config context
    switch_config_context(unp_cfg_context);
}

/*************************************************************************
 * LLK sub_bcast_row_tile unpacker implementation for SDPA
 *************************************************************************/

template <bool is_fp32_dest_acc_en>
inline void _llk_unpack_bcastA_B_hw_config_(
    const std::uint32_t unpA_src_format,
    const std::uint32_t unpB_src_format,
    const std::uint32_t unpA_dst_format,
    const std::uint32_t unpB_dst_format,
    const std::uint32_t face_r_dim                  = FACE_R_DIM,
    const std::uint32_t within_face_16x16_transpose = 0,
    const std::uint32_t num_faces                   = 4)
{
    constexpr bool is_row_pool = false;
    configure_unpack_AB<is_fp32_dest_acc_en, is_row_pool>(
        unpA_src_format, unpB_src_format, unpA_dst_format, unpB_dst_format, face_r_dim, face_r_dim, within_face_16x16_transpose, num_faces, num_faces);
}

inline void _llk_unpack_bcastA_B_mop_config_()
{
    // Setup address modifiers for unpacker instructions
    constexpr uint8_t ADDRMOD_CH1Y_0_CH1Z_0_CH0Y_0_CH0Z_0 = 0b00'00'00'00;
    constexpr uint8_t ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0 = 0b01'00'00'00; // Increment CH1_Y by 1 Y_STRIDE

    /*

        Configuration of unpacker MOP. Setting halo and unpackB to true which
        enables usage of all unpack instrructions 0-3 and unpack_B instruction.

        Unpack_A instructions are set up as expected with reading F0R0 and F1R0 and setting dvalid.
        One unpack_A which is A3 is used for unpacking B in first iteration and then SKIP_A and B are
        used for other 3 unpacks on B.

    */

    ckernel_unpack_template tmp = ckernel_unpack_template(
        true,                                                                                                                          // unpackB
        true,                                                                                                                          // unpackHalo
        TT_OP_REPLAY(0, 16, 0, 0),                                                                                                     // A0_instr
        TT_OP_NOP,                                                                                                                     // A1_instr
        TT_OP_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 1 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1), // A2_instr
        TT_OP_UNPACR(SrcB, ADDRMOD_CH1Y_0_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 1 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1), // A3_instr // UNPACK_A3
        TT_OP_UNPACR(SrcB, ADDRMOD_CH1Y_0_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 1 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1), // skipA_instr

        TT_OP_INCADCZW(p_setadc::UNP_B, 0, 0, 0, 4), // B_instr
        TT_OP_INCADCZW(p_setadc::UNP_B, 0, 0, 0, 4)  // skipB_instr
    );

    tmp.program();
}

inline void _llk_unpack_bcastA_B_init_()
{
    // Manual setup for unpacker A
    // Only srcA Y stride can be set because other strides are never used.
    // Stride is applied in each UNPACR instruction with ADDRMOD.
    // When applied it moves to next row on srcA side.
    cfg_reg_rmw_tensix<UNP0_ADDR_CTRL_XY_REG_1_Ystride_RMW>(32);

    TTI_SETADCXX(p_setadc::UNP_A, FACE_R_DIM - 1, 0);              // Directly set unpacker A counter to unpack one row
    TTI_SETADCXX(p_setadc::UNP_B, TILE_R_DIM * TILE_C_DIM - 1, 0); // Directly set unpacker B counter to unpack whole tile

    // Setup address modifiers for unpacker instructions
    constexpr uint8_t ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0 = 0b01'00'00'00; // Increment CH1_Y by 1 Y_STRIDE

    /*

        Fill replay buffer with 8 unpack instructions but none of them sets dvalid.
        They are used for contiguous unpacking od faces.
        Last instruction that increments Y counter by Y_STRIDE on channel 0 is alose inside of replay.

        Intended use inside of MOP:

        TT_OP_REPLAY(0, 9, 0, 0) -> Unpack F0R0 row 8 times and move to F1
        TT_OP_REPLAY(0, 7, 0, 0) ->  Unpack F1R0 7 times
        TT_OP_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1), -> Unpack F1R0 once and set dvalid

        As can be seen before dvalid needs to be set replay is called with 7 iterations and 8th will be one with dvalid = 1.


    */

    lltt::record<lltt::NoExec>(0, 16);
    // ************************************
    // F0
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_INCADCXY(p_setadc::UNP_A, 0, 0, 1, 0); // Increment Y to point to next needed data in L1

    // F1
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    // dvalid will be set by UNPACR instruction in MOP
    // ************************************

    _llk_unpack_bcastA_B_mop_config_();
}

inline void _llk_unpack_bcastA_B_(const std::uint32_t address_a, const std::uint32_t address_b, uint32_t srca_reuse_count = 4)
{
    TTI_SETADCZW(p_setadc::UNP_AB, 0, 0, 0, 0, SETADC_CH01(p_setadc::ZW)); // reset counters
    TTI_SETADCXY(p_setadc::UNP_AB, 0, 0, 0, 0, SETADC_CH01(p_setadc::Y));  // Clear Y counter on src side

    // Program srcA and srcB base addresses
    volatile uint tt_reg_ptr *cfg = get_cfg_pointer(); // get pointer to registers for current state ID

    // Wait for free context
    wait_for_next_context(2);

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
    semaphore_post(semaphore::UNPACK_SYNC);

    // Stall unpacker until pending CFG writes from Trisc have completed
    TTI_STALLWAIT(p_stall::STALL_UNPACK, p_stall::TRISC_CFG);

    /*
        Run the MOP in following way: The second parameter of ckernel_unpack_template::run specifies the mask for the unpacking operations.
        In this case bit 1 is set to 1 which means that unacker MOP will execute "else" part of it's loop on second pass.

        LOOP:
        if (!zmask[iteration]):
            UNPACR_A0
            UNPACR_A1
            UNPACR_A2
            UNPACR_A3
            UNPACR_B
        else
            SKIP_A
            SKIP_B

        In iteration 0 zmask will be 0 so unopacker will execute first 5 instructions. Those will unpack F0R0 and F1R0 into srcA and set dvalid.
        After that it will unpack full tile on B on increment Z counter on B so it moves to next tile.
        Next iterations have zmask on 1 and execute SKIP instructions which are just unpacks on B and after every unpack increment of Z counter on CH0

        The full unrolled code is:

        TT_OP_REPLAY(0, 16, 0, 0),
        TT_OP_NOP,
        TT_OP_UNPACR(SrcA, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1),

        First B tile

        TT_OP_UNPACR(SrcB, ADDRMOD_CH1Y_0_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1),
        TT_OP_INCADCZW(p_setadc::UNP_B, 0, 0, 0, 4),

        Other B tiles

        TT_OP_UNPACR(SrcB, ADDRMOD_CH1Y_0_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1),
        TT_OP_INCADCZW(p_setadc::UNP_B, 0, 0, 0, 4)
        TT_OP_UNPACR(SrcB, ADDRMOD_CH1Y_0_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1),
        TT_OP_INCADCZW(p_setadc::UNP_B, 0, 0, 0, 4)
        TT_OP_UNPACR(SrcB, ADDRMOD_CH1Y_0_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1),
        TT_OP_INCADCZW(p_setadc::UNP_B, 0, 0, 0, 4)
        ...

    */

    uint32_t unpack_mask = 0xFFFE;

    ckernel_unpack_template::run(srca_reuse_count, unpack_mask);

    TTI_SETADCXY(p_setadc::UNP_AB, 0, 0, 0, 0, SETADC_CH01(p_setadc::Y)); // Clear all counters

    // T6::SEMGET for context release
    t6_semaphore_get(semaphore::UNPACK_SYNC);

    // Switch unpacker config context
    switch_config_context(unp_cfg_context);
}

/**
 * Performs unpacking for block-based reduce_max_row operation across multiple tiles.
 *
 * This function works with the following assumptions:
 * - Scaler values are 1.0 and are contained inside F0 of the scaler tile
 * - The scaler doesn't change for the duration of the whole block operation
 * - Operand and scaler data format is bfloat16_b
 * - Operand tile size is 32x32
 * - Can work on both 16-bit or 32-bit DEST register modes based on is_fp32_dest_acc_en flag
 * - Does only MAX pool on ROW dimension
 *
 * This function should NOT be used as a substitute for the native _llk_unpack_AB_ LLK.
 * Use the standard _llk_unpack_AB_ in a loop for general-purpose block reduction operations.
 */
inline void _llk_unpack_AB_reduce_block_max_row_(const std::uint32_t address_a, const std::uint32_t address_b)
{
    TTI_SETADCZW(0b011, 0, 0, 0, 0, 0b1111); // reset counters

    // Program srcA and srcB base addresses
    volatile uint tt_reg_ptr *cfg = get_cfg_pointer(); // get pointer to registers for current state ID

    // Wait for free context
    wait_for_next_context(2);

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
    semaphore_post(semaphore::UNPACK_SYNC);

    // Stall unpacker until pending CFG writes from Trisc have completed
    TTI_STALLWAIT(p_stall::STALL_UNPACK, p_stall::TRISC_CFG);

    TTI_UNPACR(SrcB, 0b00000000 /* Z_ch0_inc and Z_ch1_inc */, 0, 0, 0, 1, 1 /* Set Dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    // Run MOP
    ckernel::ckernel_template::run();

    // T6::SEMGET for context release
    t6_semaphore_get(semaphore::UNPACK_SYNC);

    // Switch unpacker config context
    switch_config_context(unp_cfg_context);
}

/**
 * Uninitializes block-based reduce_max_row unpacker operation.
 * Restores the unpacker state that was saved during initialization.
 *
 * This function works with the following assumptions:
 * - Scaler values are 1.0 and are contained inside F0 of the scaler tile
 * - The scaler doesn't change for the duration of the whole block operation
 * - Operand and scaler data format is bfloat16_b
 * - Operand tile size is 32x32
 * - Can work on both 16-bit or 32-bit DEST register modes based on is_fp32_dest_acc_en flag
 * - Does only MAX pool on ROW dimension
 *
 * This function should NOT be used as a substitute for native reduce unpacking cleanup.
 * Standard _llk_unpack_AB_reduce_init_ operations typically don't require explicit cleanup.
 */
inline void _llk_unpack_AB_reduce_block_max_row_uninit_()
{
    TTI_WRCFG(p_gpr_unpack::SR_UNPACK_UNTILIZER_STATE_0, p_cfg::WRCFG_32b, THCON_SEC0_REG5_Tile_x_dim_cntx0_ADDR32);
    TTI_WRCFG(p_gpr_unpack::SR_UNPACK_UNTILIZER_STATE_1, p_cfg::WRCFG_32b, THCON_SEC0_REG0_TileDescriptor_ADDR32 + 1);
    TTI_SETADCXX(p_setadc::UNP_AB, (FACE_R_DIM * FACE_C_DIM) - 1, 0x0);
}
