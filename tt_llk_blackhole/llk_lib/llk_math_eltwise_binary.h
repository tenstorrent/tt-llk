// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "../../common/tensor_shape.h"
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
    constexpr std::uint8_t srcb_incr = (bcast_type == BroadcastType::NONE || bcast_type == BroadcastType::COL) ? MAX_FPU_ROWS : 0;
    addr_mod_t {
        .srca = {.incr = MAX_FPU_ROWS},
        .srcb = {.incr = srcb_incr},
        .dest = {.incr = MAX_FPU_ROWS},
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
        .srca     = {.incr = 0, .clr = 1},
        .srcb     = {.incr = 0, .clr = 1},
        .dest     = {.incr = MAX_FPU_ROWS, .clr = 0, .cr = 0, .c_to_cr = 1},
        .fidelity = {.incr = 0, .clr = 1}}
        .set(ADDR_MOD_3);
}

// Helper template to select the appropriate eltwise binary operation
template <EltwiseBinaryType eltwise_binary_type>
inline auto eltwise_binary_func(std::uint8_t clr_src, std::uint8_t acc_to_dest, std::uint8_t broadcast_type, std::uint8_t addr_mod)
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
    const std::uint32_t loop_count,
    const std::uint32_t face_base_offset,
    const bool clear_fp32_dst_acc,
    const std::uint32_t dst_index,
    const TensorShape &tensor_shape)
{
#pragma GCC unroll 0
    for (std::uint32_t face_num = 0; face_num < loop_count; face_num++)
    {
        eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();

        // Clear DEST face-by-face when reusing dest as source
        if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
        {
            constexpr std::uint32_t ZERO_ACC_MODE = p_zeroacc::CLR_16;
            int clear_fp32                        = is_fp32_dest_acc_en && clear_fp32_dst_acc ? 1 : 0;
            const std::uint32_t tiles_per_bank    = clear_fp32 ? 4 : 8;
            const std::uint32_t local_tile        = dst_index & (tiles_per_bank - 1);
            TT_ZEROACC(ZERO_ACC_MODE, clear_fp32, 0, ADDR_MOD_1, get_dest_index_in_faces(local_tile, face_base_offset + face_num));
        }

        ckernel_template::run();
    }
}

/**
 * @brief Perform an elementwise binary operation where Output = SrcA [+, -, *] SrcB
 * SrcA/SrcB contain 1 tile each, and output is 1 tile in destination register
 * @tparam eltwise_binary_type: Type of eltwise binary op, values = <ELWADD/ELWSUB/ELWMUL>
 * @tparam src_b_bcast_type: Broadcast type for source B, values = <NONE/COL/ROW/SCALAR>
 * @tparam Dst: Destination sync mode, values = <Half, Full>
 * @tparam is_fp32_dest_acc_en: Enable FP32 mode in destination register
 * @tparam NUM_FIDELITY_PHASES: Number of fidelity phases for high-fidelity math, values = <LoFi, HiFi2, HiFi3, HiFi4>
 * @tparam binary_reuse_dest: Reuse destination as source type, values = <NONE, DEST_TO_SRCA, DEST_TO_SRCB>
 * @param shape: Tensor shape describing tile dimensions
 * @param dst_index: Tile index into the destination register
 * @param clear_fp32_dst_acc: Clears index in destination register when float32 mode is enabled
 */
template <
    EltwiseBinaryType eltwise_binary_type,
    BroadcastType src_b_bcast_type,
    DstSync Dst,
    bool is_fp32_dest_acc_en,
    int NUM_FIDELITY_PHASES                      = 0,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void _llk_math_eltwise_binary_(const ckernel::TensorShape &tensor_shape, std::uint32_t dst_index, const bool clear_fp32_dst_acc)
{
    validate_tensor_shape_tile_dependent_ops_(tensor_shape);
    const std::uint32_t num_faces = tensor_shape.total_num_faces();
    LLK_ASSERT(NUM_FIDELITY_PHASES == 0 || eltwise_binary_type == ELWMUL, "Math fidelity larger than LoFi only works with Eltwise multiply");
    constexpr bool high_fidelity = (NUM_FIDELITY_PHASES > 0);

    math::set_dst_write_addr<DstTileShape::Tile32x32, UnpackDestination::SrcRegs>(dst_index);

    if constexpr ((eltwise_binary_type == ELWADD) || (eltwise_binary_type == ELWSUB))
    {
        if constexpr (src_b_bcast_type == BroadcastType::COL)
        {
            // Mop for col broadcast only does 2 outerloops.  Needs to clear B manually and call twice for full tile size
            constexpr std::uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? 2 : 1;
#pragma GCC unroll 0
            for (std::uint32_t n = 0; n < outerloop; n++)
            {
                eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                ckernel_template::run();
            }
            TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            if (num_faces == 4)
            {
#pragma GCC unroll 0
                for (std::uint32_t n = 0; n < outerloop; n++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    ckernel_template::run();
                }
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            }
        }
        else
        {
            const std::uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? num_faces : 1;
#pragma GCC unroll 0
            for (std::uint32_t n = 0; n < outerloop; n++)
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
            constexpr std::uint32_t outerloop = (high_fidelity) ? 2 : ((binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? 2 : 1);
            eltwise_binary_reuse_dest_helper_func<is_fp32_dest_acc_en, binary_reuse_dest>(
                outerloop, 0 /*face_base_offset*/, clear_fp32_dst_acc, dst_index, tensor_shape);
            TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);

            if (num_faces == 4)
            {
                eltwise_binary_reuse_dest_helper_func<is_fp32_dest_acc_en, binary_reuse_dest>(
                    outerloop, 2 /*face_base_offset*/, clear_fp32_dst_acc, dst_index, tensor_shape);
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            }
        }
        else
        {
            // Row and no broadcasted behaves similarly
            const std::uint32_t outerloop = (high_fidelity) ? num_faces : ((binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? num_faces : 1);
            eltwise_binary_reuse_dest_helper_func<is_fp32_dest_acc_en, binary_reuse_dest>(
                outerloop, 0 /*face_base_offset*/, clear_fp32_dst_acc, dst_index, tensor_shape);

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
inline void eltwise_binary_configure_mop(const std::uint32_t acc_to_dest, const ckernel::TensorShape tensor_shape)
{
    validate_tensor_shape_tile_dependent_ops_(tensor_shape);
    const std::uint32_t num_faces   = tensor_shape.total_num_faces();
    constexpr bool high_fidelity    = (NUM_FIDELITY_PHASES > 0);
    constexpr std::uint8_t addr_mod = ADDR_MOD_0;
    const std::uint8_t innerloop =
        tensor_shape.face_r_dim > MAX_FPU_ROWS ? (tensor_shape.face_r_dim >> MAX_FPU_ROWS_LOG2) : 1; // 8 rows per eltwise op at a time.

    // The mop only runs for 2 outer loops and mop is called twice for col broadcast
    const std::uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? 1 : (bcast_type == BroadcastType::COL) ? 2 : num_faces;

    constexpr auto broadcast_type = (bcast_type == BroadcastType::COL)      ? p_elwise::SRCB_BCAST_COL
                                    : (bcast_type == BroadcastType::ROW)    ? p_elwise::SRCB_BCAST_ROW
                                    : (bcast_type == BroadcastType::SCALAR) ? p_elwise::SRCB_BCAST_ALL
                                                                            : p_elwise::SRCB_NO_BCAST;

    // Scalar and Col broadcast should not Clear B within a mop.  This is controlled outside of MOP.
    constexpr auto CLR_SRC = (bcast_type == BroadcastType::COL || bcast_type == BroadcastType::SCALAR) ? p_setrwc::CLR_A : p_setrwc::CLR_AB;

    if constexpr ((eltwise_binary_type == ELWADD) || (eltwise_binary_type == ELWSUB))
    {
        ckernel_template tmp(outerloop, innerloop, eltwise_binary_func<eltwise_binary_type>(0, acc_to_dest, broadcast_type, addr_mod));
        if (tensor_shape.face_r_dim <= MAX_FPU_ROWS)
        {
            tmp.set_loop_op1(TT_OP_INCRWC(0, MAX_FPU_ROWS, MAX_FPU_ROWS, MAX_FPU_ROWS));
        }
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

            if (tensor_shape.face_r_dim <= MAX_FPU_ROWS)
            {
                tmp.set_end_op(TT_OP_INCRWC(0, MAX_FPU_ROWS, MAX_FPU_ROWS, MAX_FPU_ROWS));
            }
        }
        else
        {
            if (tensor_shape.face_r_dim <= MAX_FPU_ROWS)
            {
                tmp.set_end_ops(TT_OP_INCRWC(0, MAX_FPU_ROWS, MAX_FPU_ROWS, MAX_FPU_ROWS), TT_OP_SETRWC(CLR_SRC, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            }
            else
            {
                tmp.set_end_op(TT_OP_SETRWC(CLR_SRC, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            }
        }
        tmp.program();
    }
}

/**
 * @brief Initialize FPU to perform an elementwise binary operation where Output = SrcA [+, -, *] SrcB
 * SrcA/SrcB contain 1 tile each, and output is 1 tile in destination register
 * @tparam eltwise_binary_type: Type of eltwise binary op, values = <ELWADD/ELWSUB/ELWMUL>
 * @tparam src_b_bcast_type: Broadcast type for source B, values = <NONE/COL/ROW/SCALAR>
 * @tparam MATH_FIDELITY_DESC: Math fidelity descriptor for controlling precision, values = <LoFi, HiFi2, HiFi3, HiFi4>
 * @tparam binary_reuse_dest: Reuse destination as source type, values = <NONE, DEST_TO_SRCA, DEST_TO_SRCB>
 * @param tensor_shape: Tensor shape describing tile dimensions
 * @param acc_to_dest: Accumulate result to destination register instead of overwriting
 */
template <
    EltwiseBinaryType eltwise_binary_type,
    BroadcastType src_b_bcast_type,
    int MATH_FIDELITY_DESC                       = 0,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void _llk_math_eltwise_binary_init_(const ckernel::TensorShape &tensor_shape, const std::uint32_t acc_to_dest)
{
    validate_tensor_shape_tile_dependent_ops_(tensor_shape);
    LLK_ASSERT(MATH_FIDELITY_DESC == 0 || eltwise_binary_type == ELWMUL, "Math fidelity larger than LoFi only works with Eltwise multiply");
    LLK_ASSERT(
        (eltwise_binary_type == ELWADD) || (eltwise_binary_type == ELWSUB) || (eltwise_binary_type == ELWMUL),
        "eltwise_binary_type must be ELWADD, ELWSUB, or ELWMUL");
    constexpr int MATH_FIDELITY_PHASES    = get_math_num_fidelity_phases(MATH_FIDELITY_DESC);
    constexpr int MATH_FIDELITY_INCREMENT = get_math_fidelity_increment(MATH_FIDELITY_DESC);

    eltwise_binary_configure_addrmod<eltwise_binary_type, src_b_bcast_type, MATH_FIDELITY_INCREMENT>();
    eltwise_binary_configure_mop<eltwise_binary_type, src_b_bcast_type, MATH_FIDELITY_PHASES, binary_reuse_dest>(acc_to_dest, tensor_shape);

    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);

    math::reset_counters(p_setrwc::SET_ABD_F);
}

inline void _llk_math_eltwise_binary_uninit_()
{
    // No state to restore - all states are transient or default
}
