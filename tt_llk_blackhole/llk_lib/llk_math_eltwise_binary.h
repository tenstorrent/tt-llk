// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
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

using namespace ckernel;

template <EltwiseBinaryType eltwise_binary_type, BroadcastType bcast_type, std::uint32_t FIDELITY_INCREMENT>
inline void eltwise_binary_configure_addrmod()
{
    constexpr uint32_t srcb_incr = (bcast_type == BroadcastType::NONE || bcast_type == BroadcastType::COL) ? 8 : 0;
    addr_mod_t {
        .srca = {.incr = 8},
        .srcb = {.incr = srcb_incr},
        .dest = {.incr = 8},
    }
        .set(ADDR_MOD_0);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_1);

    addr_mod_t {.srca = {.incr = 0, .clr = 1}, .srcb = {.incr = 0, .clr = 1}, .dest = {.incr = 0, .clr = 0, .cr = 1}, .fidelity = {.incr = FIDELITY_INCREMENT}}
        .set(ADDR_MOD_2);

    addr_mod_t {
        .srca = {.incr = 0, .clr = 1}, .srcb = {.incr = 0, .clr = 1}, .dest = {.incr = 8, .clr = 0, .cr = 0, .c_to_cr = 1}, .fidelity = {.incr = 0, .clr = 1}}
        .set(ADDR_MOD_3);
}

// Helper template to select the appropriate eltwise binary operation
template <EltwiseBinaryType eltwise_binary_type>
inline auto eltwise_binary_func(uint8_t clr_src, uint8_t acc_to_dest, uint8_t broadcast_type, uint8_t addr_mod)
{
    if constexpr (eltwise_binary_type == ELWADD)
    {
        return TT_OP_ELWADD(clr_src, acc_to_dest, broadcast_type, addr_mod, 0);
    }
    else if constexpr (eltwise_binary_type == ELWSUB)
    {
        return TT_OP_ELWSUB(clr_src, acc_to_dest, broadcast_type, addr_mod, 0);
    }
    else
    {
        return TT_OP_ELWMUL(clr_src, acc_to_dest, broadcast_type, addr_mod, 0);
    }
}

template <EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void eltwise_binary_reuse_dest_as_src()
{
    if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCA)
    {
        move_d2a_fixed_face(ADDR_MOD_1);
    }
    else if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCB)
    {
        move_d2b_fixed_face(ADDR_MOD_1);
    }
}

// Helper to run the eltwise binary loop with optional dest reuse and face clearing
template <bool is_fp32_dest_acc_en, EltwiseBinaryReuseDestType binary_reuse_dest>
inline void eltwise_binary_reuse_dest_helper_func(
    const uint32_t loop_count, const uint32_t face_base_offset, const bool clear_fp32_dst_acc, const uint dst_index)
{
#pragma GCC unroll 0
    for (std::uint32_t face_num = 0; face_num < loop_count; face_num++)
    {
        eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();

        // Clear DEST face-by-face when reusing dest as source
        if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
        {
            constexpr uint32_t ZERO_ACC_MODE = p_zeroacc::CLR_16;
            int clear_fp32                   = is_fp32_dest_acc_en && clear_fp32_dst_acc ? 1 : 0;
            auto buffer_base                 = is_fp32_dest_acc_en && clear_fp32_dst_acc ? get_dest_buffer_base_32b() : get_dest_buffer_base_16b();
            TT_ZEROACC(ZERO_ACC_MODE, clear_fp32, 0, ADDR_MOD_1, (buffer_base + get_dest_index_in_faces(dst_index, face_base_offset + face_num)));
        }

        ckernel_template::run();
    }
}

/**
 * @brief Perform an elementwise binary operation where Output = SrcA [+, -, *] SrcB
 * SrcA/SrcB contain 1 tile each, and output is 1 tile in destination register
 * @tparam eltwise_binary_type: Type of eltwise binary op, values = <ELWADD/ELWSUB/ELWMUL>
 * @tparam src_b_bcast_type: Broadcast type for source B, values = <NONE/COL/ROW/SCALAR>
 * @tparam Dst: Destination sync mode
 * @tparam is_fp32_dest_acc_en: Enable FP32 destination accumulator
 * @tparam NUM_FIDELITY_PHASES: Number of fidelity phases for high-fidelity math
 * @tparam binary_reuse_dest: Reuse destination as source type
 * @param num_faces: Number of faces to process (1, 2, or 4)
 * @param dst_index: Tile index into the destination register
 * @param clear_fp32_dst_acc: Whether to clear FP32 destination accumulator
 */
template <
    EltwiseBinaryType eltwise_binary_type,
    BroadcastType src_b_bcast_type,
    DstSync Dst,
    bool is_fp32_dest_acc_en,
    int NUM_FIDELITY_PHASES                      = 0,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void _llk_math_eltwise_binary_(const std::uint32_t num_faces, uint dst_index, const bool clear_fp32_dst_acc)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    constexpr bool high_fidelity = (NUM_FIDELITY_PHASES > 0);

    math::set_dst_write_addr<DstTileShape::Tile32x32, UnpackDestination::SrcRegs>(dst_index);

    if constexpr ((eltwise_binary_type == ELWADD) || (eltwise_binary_type == ELWSUB))
    {
        if constexpr (src_b_bcast_type == BroadcastType::COL)
        {
            // Mop for col broadcast only does 2 outerloops.  Needs to clear B manually and call twice for full tile size
            constexpr uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? 2 : 1;
#pragma GCC unroll 0
            for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
            {
                eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                ckernel_template::run();
            }
            TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            if (num_faces == 4)
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    ckernel_template::run();
                }
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            }
        }
        else
        {
            const uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? num_faces : 1;
#pragma GCC unroll 0
            for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
            {
                eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                ckernel_template::run();
            }
            // Manually clear B once mop is done for scaler bcast
            if constexpr (src_b_bcast_type == BroadcastType::SCALAR)
            {
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_D);
            }
        }
    }
    else if constexpr (eltwise_binary_type == ELWMUL)
    {
        if constexpr (src_b_bcast_type == BroadcastType::COL)
        {
            // Mop for col broadcast only does 2 outerloops.  Needs to clear B manually and call twice for full tile size
            constexpr uint32_t outerloop = (high_fidelity) ? 2 : ((binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? 2 : 1);
            eltwise_binary_reuse_dest_helper_func<is_fp32_dest_acc_en, binary_reuse_dest>(outerloop, 0 /*face_base_offset*/, clear_fp32_dst_acc, dst_index);
            TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);

            if (num_faces == 4)
            {
                eltwise_binary_reuse_dest_helper_func<is_fp32_dest_acc_en, binary_reuse_dest>(outerloop, 2 /*face_base_offset*/, clear_fp32_dst_acc, dst_index);
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            }
        }
        else
        {
            // Row and no broadcasted behaves similarly
            const uint32_t outerloop = (high_fidelity) ? num_faces : ((binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? num_faces : 1);
            eltwise_binary_reuse_dest_helper_func<is_fp32_dest_acc_en, binary_reuse_dest>(outerloop, 0 /*face_base_offset*/, clear_fp32_dst_acc, dst_index);

            if constexpr (src_b_bcast_type == BroadcastType::SCALAR)
            {
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_D);
            }
        }
    }
    math::clear_dst_reg_addr();
}

template <
    EltwiseBinaryType eltwise_binary_type,
    BroadcastType bcast_type,
    int NUM_FIDELITY_PHASES                      = 0,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void eltwise_binary_configure_mop(const std::uint32_t acc_to_dest = 0, const std::uint32_t num_faces = 4)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    constexpr bool high_fidelity = (NUM_FIDELITY_PHASES > 0);
    const uint addr_mod          = ADDR_MOD_0;
    constexpr uint innerloop     = 16 >> 3; // 8 rows per eltwise op at a time.

    // The mop only runs for 2 outer loops and mop is called twice for col broadcast
    const uint outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? 1 : (bcast_type == BroadcastType::COL) ? 2 : num_faces;

    constexpr auto broadcast_type = (bcast_type == BroadcastType::COL)      ? p_elwise::SRCB_BCAST_COL
                                    : (bcast_type == BroadcastType::ROW)    ? p_elwise::SRCB_BCAST_ROW
                                    : (bcast_type == BroadcastType::SCALAR) ? p_elwise::SRCB_BCAST_ALL
                                                                            : p_elwise::SRCB_NO_BCAST;

    // Scalar and Col broadcast should not Clear B within a mop.  This is controlled outside of MOP.
    constexpr auto CLR_SRC = (bcast_type == BroadcastType::COL || bcast_type == BroadcastType::SCALAR) ? p_setrwc::CLR_A : p_setrwc::CLR_AB;

    if constexpr ((eltwise_binary_type == ELWADD) || (eltwise_binary_type == ELWSUB))
    {
        ckernel_template tmp(outerloop, innerloop, eltwise_binary_func<eltwise_binary_type>(0, acc_to_dest, broadcast_type, addr_mod));
        tmp.set_end_op(TT_OP_SETRWC(CLR_SRC, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
        tmp.program();
    }
    else if constexpr (eltwise_binary_type == ELWMUL)
    {
        ckernel_template tmp(high_fidelity ? NUM_FIDELITY_PHASES : outerloop, innerloop, eltwise_binary_func<ELWMUL>(0, 0, broadcast_type, addr_mod));
        if constexpr (high_fidelity)
        {
            tmp.set_last_inner_loop_instr(eltwise_binary_func<ELWMUL>(0, 0, broadcast_type, ADDR_MOD_2)); // Incr fidelity last inst of inner loop
            tmp.set_last_outer_loop_instr(eltwise_binary_func<ELWMUL>(CLR_SRC, 0, broadcast_type, ADDR_MOD_3));
        }
        else
        {
            tmp.set_end_op(TT_OP_SETRWC(CLR_SRC, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
        }
        tmp.program();
    }
}

/**
 * @brief Initialize FPU to perform an elementwise binary operation where Output = SrcA [+, -, *] SrcB
 * SrcA/SrcB contain 1 tile each, and output is 1 tile in destination register
 * @tparam eltwise_binary_type: Type of eltwise binary op, values = <ELWADD/ELWSUB/ELWMUL>
 * @tparam src_b_bcast_type: Broadcast type for source B, values = <NONE/COL/ROW/SCALAR>
 * @tparam MATH_FIDELITY_DESC: Math fidelity descriptor for controlling precision
 * @tparam binary_reuse_dest: Reuse destination as source type
 * @param num_faces: Number of faces to process (1, 2, or 4)
 * @param acc_to_dest: Accumulate to destination flag
 */
template <
    EltwiseBinaryType eltwise_binary_type,
    BroadcastType src_b_bcast_type,
    int MATH_FIDELITY_DESC                       = 0,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void _llk_math_eltwise_binary_init_(const std::uint32_t num_faces, const std::uint32_t acc_to_dest)
{
    LLK_ASSERT(num_faces == 1 || num_faces == 2 || num_faces == 4, "num_faces must be 1, 2, or 4");
    LLK_ASSERT(
        (eltwise_binary_type == ELWADD) || (eltwise_binary_type == ELWSUB) || (eltwise_binary_type == ELWMUL),
        "eltwise_binary_type must be ELWADD, ELWSUB, or ELWMUL");
    constexpr int MATH_FIDELITY_PHASES    = get_math_num_fidelity_phases(MATH_FIDELITY_DESC);
    constexpr int MATH_FIDELITY_INCREMENT = get_math_fidelity_increment(MATH_FIDELITY_DESC);

    eltwise_binary_configure_addrmod<eltwise_binary_type, src_b_bcast_type, MATH_FIDELITY_INCREMENT>();
    eltwise_binary_configure_mop<eltwise_binary_type, src_b_bcast_type, MATH_FIDELITY_PHASES, binary_reuse_dest>(acc_to_dest, num_faces);

    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);

    math::reset_counters(p_setrwc::SET_ABD_F);
}

inline void _llk_math_eltwise_binary_uninit_()
{
    // No state to restore - all states are transient or default
}

/*************************************************************************

SDPA OPTIMIZATION
--------------------------------
Request is to do the following:
- Transpose srcB from rows to columns, that way we get effect of broadcasted column
- Move srcB counter to index 16 since transpose is done on rows 16 - 31.
- Subtract F0 and F1 in srcA and srcB
- Change srcB bank
- Repeat subtraction for F2 and F3 in srcA and srcB
*************************************************************************/

inline void _llk_math_eltwise_binary_bcastB_row_as_col_configure_addrmod()
{
    addr_mod_t {
        .srca = {.incr = 8},
        .srcb = {.incr = 8},
        .dest = {.incr = 8},
    }
        .set(ADDR_MOD_0);

    addr_mod_t {
        .srca = {.incr = 8},
        .srcb = {.incr = 0x3F & -8}, // decrement srcB by 8
        .dest = {.incr = 8},
    }
        .set(ADDR_MOD_1);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 16},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_2);

    addr_mod_t {
        .srca = {.incr = 16},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_3);
}

template <bool transpose_faces = false>
inline void _llk_math_eltwise_binary_bcastB_row_as_col_configure_mop()
{
    // Without transpose: 13 instructions
    // With transpose: 17 instructions (extra srcA jumps and manipulations between faces)
    constexpr uint32_t REPLAY_BUFFER_SIZE = transpose_faces ? 17 : 13;

    uint32_t innerloop           = 1;
    constexpr uint32_t outerloop = 1;

    ckernel_template tmp(outerloop, innerloop, TT_OP_REPLAY(0, REPLAY_BUFFER_SIZE, 0, 0));
    tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_AB)); // Clearing src A and B dvalid
    tmp.program();
}

template <EltwiseBinaryType eltwise_binary_type, bool transpose_faces = false>
inline void _llk_math_eltwise_binary_bcastB_row_as_col_init_()
{
    // Without transpose: 13 instructions
    // With transpose: 17 instructions (extra srcA jumps and manipulations between faces)
    constexpr uint32_t REPLAY_BUFFER_SIZE = transpose_faces ? 17 : 13;

    _llk_math_eltwise_binary_bcastB_row_as_col_configure_addrmod();

    auto eltwise_op = [](uint8_t addr_mod)
    {
        if constexpr (eltwise_binary_type == EltwiseBinaryType::ELWSUB)
        {
            TTI_ELWSUB(0, 0, 0, addr_mod, 0);
        }
        else if constexpr (eltwise_binary_type == EltwiseBinaryType::ELWADD)
        {
            TTI_ELWADD(0, 0, 0, addr_mod, 0);
        }
        else if constexpr (eltwise_binary_type == EltwiseBinaryType::ELWMUL)
        {
            TTI_ELWMUL(0, 0, 0, addr_mod, 0);
        }
    };

    // ********************************************************
    //  Setup replay buffer
    lltt::record<lltt::NoExec>(0, REPLAY_BUFFER_SIZE);

    TTI_TRNSPSRCB; // rows -> cols
    /*
    Dummy SFPLOAD with no effect to move srcB counter to be 16 since transpose is done on rows 16 - 31.
    Main thing is ADDR_MOD_2 that has srcB increment of 16
    */
    TTI_SFPLOAD(8, InstrModLoadStore::FP16B, ADDR_MOD_2, 0);

    if constexpr (transpose_faces)
    {
        // ********************************************************
        // TRANSPOSE FACES MODE:
        // srcA unpacked with face transpose: F0, F2, F1, F3
        // srcB unpacked normally (broadcast): F0/F1, then F2/F3
        // Need to align srcA face access with srcB broadcast groups:
        //   - srcA F0 with srcB F0/F1 → dest F0
        //   - srcA F2 with srcB F0/F1 → dest F1
        //   - srcA F1 with srcB F2/F3 → dest F2
        //   - srcA F3 with srcB F2/F3 → dest F3
        // ********************************************************

        // F0 (srcA at 0-15) - F0 (srcB) → dest 0-15
        eltwise_op(ADDR_MOD_0); // srcA: 0->8, dest: 0->8
        eltwise_op(ADDR_MOD_1); // srcA: 8->16, dest: 8->16

        // Jump srcA from 16 to 32 to skip F1 and access F2
        // Use ADDR_MOD_3 which has srcA incr=0, dest incr=16
        TTI_SFPLOAD(8, InstrModLoadStore::FP16B, ADDR_MOD_3, 0); // srcA: 16->32, dest stays at 16

        // F2 (srcA at 32-47) - F1 (srcB) → dest 16-31
        eltwise_op(ADDR_MOD_0); // srcA: 32->40, dest: 16->24
        eltwise_op(ADDR_MOD_1); // srcA: 40->48, dest: 24->32

        // BOTTOM FACES F2 AND F3
        TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0); // clear dvalid on srcB and reset srcB counters

        TTI_TRNSPSRCB; // rows -> cols

        // Jump srcA back from 48 to 16 to access F1
        // Reset srcA to 16, dest stays at 32
        TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_A); // srcA: 48->0
        TTI_SFPLOAD(8, InstrModLoadStore::FP16B, ADDR_MOD_3, 0);     // srcA: 0->16, dest stays at 32

        // F1 (srcA at 16-31) - F2 (srcB) → dest 32-47
        eltwise_op(ADDR_MOD_0); // srcA: 16->24, dest: 32->40
        eltwise_op(ADDR_MOD_1); // srcA: 24->32, dest: 40->48

        TTI_SFPLOAD(8, InstrModLoadStore::FP16B, ADDR_MOD_3, 0); // srcA: 32->48, dest stays at 48

        // F3 (srcA at 48-63) - F3 (srcB) → dest 48-63
        eltwise_op(ADDR_MOD_0); // srcA: 48->56, dest: 48->56
        eltwise_op(ADDR_MOD_1); // srcA: 56->64, dest: 56->64

        TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, 0); // clear both src dvalids
    }
    else
    {
        // ********************************************************
        // NORMAL MODE: Write faces in order F0, F1, F2, F3
        // ********************************************************

        eltwise_op(ADDR_MOD_0);
        eltwise_op(ADDR_MOD_1);

        eltwise_op(ADDR_MOD_0);
        eltwise_op(ADDR_MOD_1);

        // BOTTOM FACES F2 AND F3

        TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0); // clear dvalid on srcB and reset srcB counters

        TTI_TRNSPSRCB; // rows -> cols

        eltwise_op(ADDR_MOD_0);
        eltwise_op(ADDR_MOD_1);

        eltwise_op(ADDR_MOD_0);
        eltwise_op(ADDR_MOD_1);

        TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, 0); // clear both src dvalids
    }

    // ********************************************************

    _llk_math_eltwise_binary_bcastB_row_as_col_configure_mop<transpose_faces>();

    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);
    math::reset_counters(p_setrwc::SET_ABD_F);
}

inline void _llk_math_eltwise_binary_bcastB_row_as_col_(uint32_t dst_index)
{
    math::set_dst_write_addr<DstTileShape::Tile32x32, UnpackDestination::SrcRegs>(dst_index);
    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_AB); // clear all counters

    // run the MOP
    ckernel_template::run();

    math::clear_dst_reg_addr();
}

inline void _llk_math_eltwise_binary_bcastB_row_as_col_uninit_()
{
    // Reset address modifiers to defaults
    addr_mod_t {
        .srca = {.incr = 8},
        .srcb = {.incr = 8},
        .dest = {.incr = 8},
    }
        .set(ADDR_MOD_0);

    addr_mod_t {
        .srca = {.incr = 8},
        .srcb = {.incr = 8},
        .dest = {.incr = 8},
    }
        .set(ADDR_MOD_1);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_2);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_3);

    // Clear any pending dvalid signals
    TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, 0);
}
