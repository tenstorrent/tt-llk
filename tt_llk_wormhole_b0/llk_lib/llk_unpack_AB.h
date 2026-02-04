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
#include "llk_assert.h"
#include "llk_unpack_common.h"
#include "lltt.h"

using namespace ckernel;
using namespace ckernel::unpacker;

template <BroadcastType BType = BroadcastType::NONE>
inline void _llk_unpack_AB_mop_config_(const bool transpose_of_faces = false, const std::uint32_t num_faces = 4, const bool narrow_tile = false)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");

    if (transpose_of_faces)
    {
        LLK_ASSERT(num_faces == 4, "num_faces must be 4 when transpose_of_faces is true");
    }

    static constexpr uint unpack_srca           = TT_OP_UNPACR(SrcA, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint unpack_srcb           = TT_OP_UNPACR(SrcB, 0b1, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    static constexpr uint unpack_srca_transpose = TT_OP_UNPACR(SrcA, 0b10, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    const uint32_t outerloop = num_faces < 4 ? 1 : 2;
    const uint32_t innerloop = num_faces < 2 ? 1 : 2;

    const uint srca_op     = transpose_of_faces ? unpack_srca_transpose : unpack_srca;
    const uint srca_end_op = TT_OP_SETADCZW(p_setadc::UNP_A, 0, 0, 0, 1, 0b0001);

    // Helper to set end op(s) based on transpose mode
    auto set_end_op_with_transpose = [&](ckernel_template &tmp, uint primary_end_op)
    {
        if (transpose_of_faces)
        {
            tmp.set_end_ops(primary_end_op, srca_end_op);
        }
        else
        {
            tmp.set_end_op(primary_end_op);
        }
    };

    if constexpr (BType == BroadcastType::COL)
    {
        static constexpr uint unpack_srcb_set_z = TT_OP_SETADCZW(0b010, 0, 0, 0, 2, 0b0001);

        ckernel_template tmp(outerloop, innerloop, srca_op);
        tmp.set_start_op(unpack_srcb);
        set_end_op_with_transpose(tmp, narrow_tile ? unpack_srcb : unpack_srcb_set_z);
        tmp.program();
    }
    else if constexpr (BType == BroadcastType::ROW)
    {
        static constexpr uint unpack_srcb_clear_z  = TT_OP_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, 0b0001);
        static constexpr uint unpack_srcb_no_z_inc = TT_OP_UNPACR(SrcB, 0b0, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

        ckernel_template tmp(outerloop, innerloop, narrow_tile ? unpack_srcb_no_z_inc : unpack_srcb, srca_op);
        set_end_op_with_transpose(tmp, unpack_srcb_clear_z);
        tmp.program();
    }
    else if constexpr (BType == BroadcastType::SCALAR)
    {
        LLK_ASSERT(!transpose_of_faces, "SrcA transpose is not supported with scalar broadcast");

        ckernel_template tmp(1, num_faces, unpack_srca);
        tmp.set_start_op(unpack_srcb);
        tmp.program();
    }
    else // BType == BroadcastType::NONE
    {
        if (transpose_of_faces)
        {
            static constexpr uint srca_set_z = TT_OP_SETADCZW(p_setadc::UNP_A, 0, 0, 0, 1, 0b0001);
            ckernel_template tmp(outerloop, innerloop, srca_op, unpack_srcb);
            tmp.set_end_op(srca_set_z);
            tmp.program();
        }
        else
        {
            ckernel_template tmp(1, num_faces, unpack_srca, unpack_srcb);
            tmp.program();
        }
    }
}

template <BroadcastType BType = BroadcastType::NONE>
inline void _llk_unpack_AB_init_(
    const std::uint32_t face_r_dim = FACE_R_DIM, const std::uint32_t num_faces = 4, const bool narrow_tile = false, const std::uint32_t transpose = 0)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(transpose); // transpose within the face

    config_unpacker_x_end<p_setadc::UNP_AB>(face_r_dim);

    _llk_unpack_AB_mop_config_<BType>(transpose > 0, num_faces, narrow_tile); // transpose of faces 0,2,1,3
}

template <ReduceDim dim, BroadcastType BType = BroadcastType::NONE, bool enforce_fp32_accumulation = false>
inline void _llk_unpack_AB_reduce_init_(
    const std::uint32_t face_r_dim,
    const std::uint32_t num_faces,
    const bool narrow_tile,
    const std::uint32_t transpose,
    const std::uint32_t within_face_16x16_transpose)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");

    if constexpr (enforce_fp32_accumulation)
    {
        // Set necessary config regs for MOVB2D hi16/lo16 to work
        cfg_reg_rmw_tensix<ALU_ACC_CTRL_Zero_Flag_disabled_src_RMW>(1);
    }

    // REDUCE_ROW requires transpose itself; additionally, within_face_16x16_transpose flag could require transpose;
    // if we have the flag set with REDUCE_ROW, we don't need to do anything
    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(ReduceDim::REDUCE_ROW == dim ? !within_face_16x16_transpose : within_face_16x16_transpose);

    constexpr std::uint32_t UNP_SEL = p_setadc::UNP_AB;
    config_unpacker_x_end<UNP_SEL>(face_r_dim);

    _llk_unpack_AB_mop_config_<BType>(transpose > 0, num_faces, narrow_tile); // transpose of faces 0,2,1,3
}

inline void _llk_unpack_AB_uninit_(const std::uint32_t face_r_dim = FACE_R_DIM)
{
    TT_SETADCXX(p_setadc::UNP_AB, face_r_dim * FACE_C_DIM - 1, 0x0);
}

template <BroadcastType BType = BroadcastType::NONE>
inline void _llk_unpack_AB_(const std::uint32_t address_a, const std::uint32_t address_b)
{
    TTI_SETADCZW(0b011, 0, 0, 0, 0, 0b1111); // reset counters

    // Program srcA and srcB base addresses
    volatile uint tt_reg_ptr *cfg = get_cfg_pointer(); // get pointer to registers for current state ID

    // Wait for free context
    wait_for_next_context(2);

    // Validate and configure addresses
    _llk_unpack_configure_addresses_(address_a, address_b, cfg);

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

inline void _llk_unpack_bcastA_B_uninit_(const std::uint32_t y_stride = FACE_R_DIM * 2, const std::uint32_t face_r_dim = FACE_R_DIM)
{
    // Revisit default stride value in tt-llk#1015
    cfg_reg_rmw_tensix<UNP0_ADDR_CTRL_XY_REG_1_Ystride_RMW>(y_stride);
    TT_SETADCXX(p_setadc::UNP_AB, face_r_dim * FACE_C_DIM - 1, 0x0);
}

inline void _llk_unpack_bcastA_B_(const std::uint32_t address_a, const std::uint32_t address_b, uint32_t srca_reuse_count = 4)
{
    TTI_SETADCZW(p_setadc::UNP_AB, 0, 0, 0, 0, SETADC_CH01(p_setadc::ZW)); // reset counters
    TTI_SETADCXY(p_setadc::UNP_AB, 0, 0, 0, 0, SETADC_CH01(p_setadc::Y));  // Clear Y counter on src side

    // Program srcA and srcB base addresses
    volatile uint tt_reg_ptr *cfg = get_cfg_pointer(); // get pointer to registers for current state ID

    // Wait for free context
    wait_for_next_context(2);

    // Validate and configure addresses
    _llk_unpack_configure_addresses_(address_a, address_b, cfg);

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

/*************************************************************************
SDPA OPTIMIZATION
--------------------------------
Request is to do the following:
- On the LHS of input we expect a tile of elements, but on RHS we expect basically a row of elements in a tile. This row will be generated by some previous
function called in SDPA.
- We need to unpack srcA as a full tile, and on srcB we need to treat that row as a column that gets broadcasted. That comes as a request since we optimize
sub_exp_bcast_col function.
- This is achieved by unpacking to srcB like we are doing broadcast row. If we repeat that row across whole face and then transpose srcB from rows to columns,
we get effect of broadcasted column.

Template parameters:
- transpose_of_faces: If true, srcA faces are reordered (f0,f1,f2,f3 -> f0,f2,f1,f3)
- transpose_within_face: If true, 16x16 transposition is applied within each srcA face

*************************************************************************/

template <bool transpose_of_faces = false, bool transpose_within_face = false>
inline void _llk_unpackA_bcastB_row_as_col_init_()
{
    // Configure within-face transpose for srcA via hardware register
    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(transpose_within_face ? 1 : 0);

    // Setup srcB strides for row broadcast
    cfg_reg_rmw_tensix<UNP1_ADDR_CTRL_XY_REG_1_Ystride_RMW>(32);
    cfg_reg_rmw_tensix<UNP1_ADDR_CTRL_ZW_REG_1_Zstride_RMW>(16);
    cfg_reg_rmw_tensix<UNP1_ADDR_CTRL_XY_REG_0_Ystride_RMW>(0);
    TTI_SETADCXX(p_setadc::UNP_B, FACE_R_DIM - 1, 0);

    constexpr uint8_t ADDRMOD_CH1Y_0_CH1Z_0_CH0Y_0_CH0Z_0 = 0b00'00'00'00; // no increment
    constexpr uint8_t ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0 = 0b01'00'00'00; // Increment CH1_Y by 1 Y_STRIDE

    if constexpr (transpose_of_faces)
    {
        // For face transpose: srcA X counter for one face (256 elements)
        // srcA will be unpacked with 4 explicit UNPACR instructions in execute
        TTI_SETADCXX(p_setadc::UNP_A, FACE_R_DIM * FACE_C_DIM - 1, 0);

        // Only record srcB broadcast instructions (no srcA in replay buffer)
        lltt::record<lltt::NoExec>(0, 16);
        for (int i = 0; i < 16; i++)
        {
            TTI_UNPACR(SrcB, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
        }
    }
    else
    {
        // No face transpose: srcA X counter for full tile (1024 elements)
        TTI_SETADCXX(p_setadc::UNP_A, TILE_R_DIM * TILE_C_DIM - 1, 0);

        // Record srcB + srcA in replay buffer
        lltt::record<lltt::NoExec>(0, 17);
        for (int i = 0; i < 16; i++)
        {
            TTI_UNPACR(SrcB, ADDRMOD_CH1Y_1_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 0 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
        }
        TTI_UNPACR(SrcA, ADDRMOD_CH1Y_0_CH1Z_0_CH0Y_0_CH0Z_0, 0, 0, 0, 1, 1 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    }
}

template <bool transpose_of_faces = false>
inline void _llk_unpackA_bcastB_row_as_col_(const std::uint32_t address_a, const std::uint32_t address_b, const std::uint32_t row_index = 0)
{
    TTI_SETADCZW(p_setadc::UNP_AB, 0, 0, 0, 0, SETADC_CH01(p_setadc::ZW));
    TTI_SETADCXY(p_setadc::UNP_AB, 0, 0, 0, 0, SETADC_CH01(p_setadc::Y));

    // Calculate address offset based on row_index
    // row_index 0-15: broadcast from F0Rn/F1Rn (top half of tile)
    // row_index 16-31: broadcast from F2R(n-16)/F3R(n-16) (bottom half of tile)
    //
    // In tilized Float16_b format (2 bytes per element):
    // - Each face is 16x16 = 256 elements = 512 bytes
    // - Each row within a face is 16 elements = 32 bytes
    // - F0 starts at offset 0, F1 at 512, F2 at 1024, F3 at 1536
    //
    // IMPORTANT: L1 addresses use 16-byte granularity (address_b is already in 16-byte units)
    // So we must convert byte offsets to 16-byte units by dividing by 16 (>> 4)
    //
    // For row_index < 16: offset = row_index * 32 bytes = row_index * 2 (in 16-byte units)
    // For row_index >= 16: offset = 1024 + (row_index - 16) * 32 bytes = 64 + (row_index - 16) * 2 (in 16-byte units)
    const std::uint32_t row_within_face    = row_index & 0xF;                       // row_index % 16
    const std::uint32_t face_offset_16B    = (row_index >= 16) ? 64 : 0;            // 1024 bytes / 16 = 64
    const std::uint32_t row_offset_16B     = face_offset_16B + row_within_face * 2; // 32 bytes / 16 = 2
    const std::uint32_t address_b_adjusted = address_b + row_offset_16B;

    // Program srcA and srcB base addresses
    volatile uint tt_reg_ptr *cfg = get_cfg_pointer();
    wait_for_next_context(2);

    if (0 == unp_cfg_context)
    {
        cfg[THCON_SEC0_REG3_Base_address_ADDR32] = address_a;
        cfg[THCON_SEC1_REG3_Base_address_ADDR32] = address_b_adjusted;
    }
    else
    {
        cfg[THCON_SEC0_REG3_Base_cntx1_address_ADDR32] = address_a;
        cfg[THCON_SEC1_REG3_Base_cntx1_address_ADDR32] = address_b_adjusted;
    }

    t6_semaphore_post(semaphore::UNPACK_SYNC);
    TTI_STALLWAIT(p_stall::STALL_UNPACK, p_stall::TRISC_CFG);

    constexpr uint8_t ADDRMOD_CH1Y_0_CH1Z_0_CH0Y_0_CH0Z_1 = 0b00'00'00'01; // Increment CH0_Z only
    constexpr uint8_t ADDRMOD_CH1Y_0_CH1Z_1_CH0Y_0_CH0Z_1 = 0b00'01'00'01; // Increment CH1_Z and CH0_Z

    if constexpr (transpose_of_faces)
    {
        // Full 32x32 tile transpose = face transpose + within-face transpose
        // In unpacker part only within-face transpose is applied.
        // Face transpose is applied in math part with usage of addr_mod.
        // Within-face transpose set via THCON_SEC0_REG2_Haloize_mode_RMW in init
        //
        // First: Unpack srcA completely with 4 explicit UNPACR instructions (one per face)
        // Z counter increments between each to move to next face
        // 4th instruction sets dvalid

        TTI_UNPACR(SrcA, ADDRMOD_CH1Y_0_CH1Z_1_CH0Y_0_CH0Z_1, 0, 0, 0, 1, 0 /* no dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
        TTI_UNPACR(SrcA, ADDRMOD_CH1Y_0_CH1Z_1_CH0Y_0_CH0Z_1, 0, 0, 0, 1, 0 /* no dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
        TTI_UNPACR(SrcA, ADDRMOD_CH1Y_0_CH1Z_1_CH0Y_0_CH0Z_1, 0, 0, 0, 1, 0 /* no dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
        TTI_UNPACR(SrcA, ADDRMOD_CH1Y_0_CH1Z_1_CH0Y_0_CH0Z_1, 0, 0, 0, 1, 1 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

        // Second: Unpack srcB with row broadcast
        // First half: broadcast to F0/F1 of srcB dest (32 unpacks)
        lltt::replay(0, 16);
        lltt::replay(0, 15);
        TTI_UNPACR(SrcB, ADDRMOD_CH1Y_0_CH1Z_0_CH0Y_0_CH0Z_1, 0, 0, 0, 1, 1 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

        // Reset srcB Y,Z counters for second half
        TTI_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, SETADC_CH1(p_setadc::ZW));
        TTI_SETADCXY(p_setadc::UNP_B, 0, 0, 0, 0, SETADC_CH1(p_setadc::Y));

        // Second half: broadcast to F2/F3 of srcB dest (32 unpacks, dvalid on last)
        lltt::replay(0, 16);
        lltt::replay(0, 15);
        TTI_UNPACR(SrcB, ADDRMOD_CH1Y_0_CH1Z_0_CH0Y_0_CH0Z_1, 0, 0, 0, 1, 1 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    }
    else
    {
        // Without face transpose - original behavior
        // Bcast selected row over F0 and F1 in srcB and unpack full srcA tile
        lltt::replay(0, 17);
        lltt::replay(0, 15);

        TTI_UNPACR(SrcB, ADDRMOD_CH1Y_0_CH1Z_0_CH0Y_0_CH0Z_1, 0, 0, 0, 1, 1 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

        // reset Y,Z,W counters on srcB side - channel 1
        TTI_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, SETADC_CH1(p_setadc::ZW));
        TTI_SETADCXY(p_setadc::UNP_B, 0, 0, 0, 0, SETADC_CH1(p_setadc::Y));

        // Unpack from next face (F1Rn or F3Rn) to F2 and F3 in srcB (broadcast to 32 rows)
        // The Z increment from previous UNPACR already moved address by one face (512 bytes)
        lltt::replay(0, 16);
        lltt::replay(0, 15);

        TTI_UNPACR(SrcB, ADDRMOD_CH1Y_0_CH1Z_0_CH0Y_0_CH0Z_1, 0, 0, 0, 1, 1 /* dvalid */, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    }

    t6_semaphore_get(semaphore::UNPACK_SYNC);
    switch_config_context(unp_cfg_context);
}

inline void _llk_unpackA_bcastB_row_as_col_uninit_()
{
    // Reset transpose register to default (no transpose)
    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(0);

    // Reset srcB strides to defaults
    cfg_reg_rmw_tensix<UNP1_ADDR_CTRL_XY_REG_1_Ystride_RMW>(FACE_R_DIM * 2);
    cfg_reg_rmw_tensix<UNP1_ADDR_CTRL_ZW_REG_1_Zstride_RMW>(FACE_R_DIM * FACE_C_DIM * 2);
    cfg_reg_rmw_tensix<UNP1_ADDR_CTRL_XY_REG_0_Ystride_RMW>(FACE_R_DIM * 2);

    // Reset X counters to full tile dimensions
    TTI_SETADCXX(p_setadc::UNP_A, TILE_R_DIM * TILE_C_DIM - 1, 0x0);
    TTI_SETADCXX(p_setadc::UNP_B, FACE_R_DIM * FACE_C_DIM - 1, 0x0);
}
