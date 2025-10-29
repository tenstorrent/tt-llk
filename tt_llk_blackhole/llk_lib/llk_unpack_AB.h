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

// OPTIMIZED, DO NOT CALL UNLESS REGULAR TILE SIZE
/**
 * Configures MOP (Macro Operation) for specialized reduce_row_max unpacking operations.
 *
 * NOTE: This function is highly specialized for SDPA (Scaled Dot-Product Attention) use cases
 * and should NOT be used as a substitute for native reduce unpacking LLK MOP configuration.
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

// OPTIMIZED, DO NOT CALL UNLESS REGULAR TILE SIZE
/**
 * Initializes unpacker configuration for block-based reduce_max_row operations.
 * Sets up tile dimensions and saves unpacker state that will be modified during operation.
 *
 * NOTE: This function is highly specialized for SDPA (Scaled Dot-Product Attention) use cases
 * and should NOT be used as a substitute for native reduce unpacking LLK initialization.
 * Use the standard _llk_unpack_AB_reduce_init_ for general-purpose reduction operations.
 */
inline void _llk_unpack_AB_reduce_block_max_row_init_()
{
    TTI_SETADCXX(p_setadc::UNP_B, FACE_R_DIM * FACE_C_DIM - 1, 0x0);       // Unpack a single face of a scaler
    TTI_SETADCXX(p_setadc::UNP_A, 4 * (FACE_R_DIM * FACE_C_DIM) - 1, 0x0); // Unpack a tile of an operand

    // save the following state that is going to be modified:
    // tile x, y, and z dims for both unpackers
    // CH1 Z stride for both unpackers
    TTI_RDCFG(p_gpr_unpack::SR_UNPACK_UNTILIZER_STATE_0, THCON_SEC0_REG5_Tile_x_dim_cntx0_ADDR32);
    TTI_RDCFG(p_gpr_unpack::SR_UNPACK_UNTILIZER_STATE_1, THCON_SEC0_REG0_TileDescriptor_ADDR32 + 1);

    TTI_SETDMAREG(0, 1 * FACE_C_DIM * FACE_R_DIM, 0, LO_16(p_gpr_unpack::TMP0));
    TTI_SETDMAREG(0, 1 * FACE_C_DIM * FACE_R_DIM, 0, HI_16(p_gpr_unpack::TMP0));
    TTI_WRCFG(p_gpr_unpack::TMP0, p_cfg::WRCFG_32b, THCON_SEC0_REG5_Tile_x_dim_cntx0_ADDR32);
    TTI_SETDMAREG(0, 4 /* y_dim */, 0, LO_16(p_gpr_unpack::TMP0));
    TTI_SETDMAREG(0, 1 /* z_dim */, 0, HI_16(p_gpr_unpack::TMP0));
    TTI_WRCFG(p_gpr_unpack::TMP0, p_cfg::WRCFG_32b, THCON_SEC0_REG0_TileDescriptor_ADDR32 + 1);
}

/**
 * Configures MOP (Macro Operation) for block-based reduce_max_row unpacking operations.
 *
 * NOTE: This function is highly specialized for SDPA (Scaled Dot-Product Attention) use cases
 * and should NOT be used as a substitute for native reduce unpacking LLK MOP configuration.
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

/**
 * Performs unpacking for block-based reduce_max_row operation across multiple tiles.
 *
 * NOTE: This function is highly specialized for SDPA (Scaled Dot-Product Attention) use cases
 * and should NOT be used as a substitute for the native _llk_unpack_AB_ LLK.
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
 * NOTE: This function is highly specialized for SDPA (Scaled Dot-Product Attention) use cases
 * and should NOT be used as a substitute for native reduce unpacking cleanup.
 * Standard _llk_unpack_AB_reduce_init_ operations typically don't require explicit cleanup.
 */
inline void _llk_unpack_AB_reduce_block_max_row_uninit_()
{
    TTI_WRCFG(p_gpr_unpack::SR_UNPACK_UNTILIZER_STATE_0, p_cfg::WRCFG_32b, THCON_SEC0_REG5_Tile_x_dim_cntx0_ADDR32);
    TTI_WRCFG(p_gpr_unpack::SR_UNPACK_UNTILIZER_STATE_1, p_cfg::WRCFG_32b, THCON_SEC0_REG0_TileDescriptor_ADDR32 + 1);
    TTI_SETADCXX(p_setadc::UNP_AB, (FACE_R_DIM * FACE_C_DIM) - 1, 0x0);
}
