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

inline void _llk_unpack_AB_matmul_tilize_A_mop_config() {
    const std::uint32_t replay_buf_run_len = 24;

    // Open replay
    TT_REPLAY(0, replay_buf_run_len, 0, 1);

    // Replay buffer is out of order

    //Unpacks 1x16 row of datums to SrcB, 16 times
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    // Move to the bottom faces
    TTI_SETADCXY(p_setadc::UNP_B, 0, 0, 0, 0, 0b0010);
    // Move to the right face
    TTI_UNPACR(SrcB, 0b01000100/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, 0b0001);
    // This is also needed for moving to face 4
    TTI_INCADCZW(p_setadc::UNP_B, 0, 0, 1, 0);
    // Move to next tile
    TTI_UNPACR(SrcB, 0b01000000/*CH1_Y+=1*/, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_SETADCXY(p_setadc::UNP_B, 0, 0, 0, 0, 0b1010);
    TTI_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, 0b0001);
    TTI_INCADCZW(p_setadc::UNP_B, 0, 0, 1, 0);

    // We hack unpack mop to execute 7 different instructions in a loop by using both masks and doubling loop count
    ckernel_unpack_template tmp = ckernel_unpack_template(
        true,  // src B
        true,  // halo - just used for 4 unpacks
        TT_OP_REPLAY(0, 15, 0, 0),  // A0 (1)
        TT_OP_REPLAY(17, 2, 0, 0),  // A1 (2)
        TT_OP_REPLAY(0, 17, 0, 0),  // A2 (3)
        TT_OP_REPLAY(0, 15, 0, 0),  // A3 (4)
        TT_OP_REPLAY(0, 15, 0, 0),  // SKIP_A (6)
        TT_OP_REPLAY(17, 3, 0, 0),  // B (5)
        TT_OP_REPLAY(20, 4, 0, 0)); // SKIP_B (7)

    tmp.program(instrn_buffer);

    /* What is actually executed per tile is as follows:

    F0:
    TTI_UNPACR(SrcB, 0b01000001, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1); X 15

    TTI_UNPACR(SrcB, 0b01000100, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, 0b0001);

    F1:
    TTI_UNPACR(SrcB, 0b01000001, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1); X 15

    TTI_UNPACR(SrcB, 0b01000001, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_SETADCXY(p_setadc::UNP_B, 0, 0, 0, 0, 0b0010);

    F2:
    TTI_UNPACR(SrcB, 0b01000001, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1); X 15

    TTI_UNPACR(SrcB, 0b01000100, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, 0b0001);
    TTI_INCADCZW(p_setadc::UNP_B, 0, 0, 1, 0);

    F3:
    TTI_UNPACR(SrcB, 0b01000001, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1); X 15

    TTI_UNPACR(SrcB, 0b01000000, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    TTI_SETADCXY(p_setadc::UNP_B, 0, 0, 0, 0, 0b1010);
    TTI_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, 0b0001);
    TTI_INCADCZW(p_setadc::UNP_B, 0, 0, 1, 0);

    */
}

template <bool is_fp32_dest_acc_en = false>
inline void _llk_unpack_AB_matmul_tilize_A_hw_configure(const std::uint32_t unpA_src_format, const std::uint32_t unpB_src_format, const std::uint32_t unpA_dst_format, const std::uint32_t unpB_dst_format, const std::uint32_t unpA_face_r_dim, const std::uint32_t unpB_face_r_dim, const std::uint32_t unpA_num_faces, const std::uint32_t unpB_num_faces) {
    configure_unpack_AB<false, is_fp32_dest_acc_en, false, false>(
        unpA_src_format,
        unpB_src_format,
        unpA_dst_format,
        unpB_dst_format,
        unpA_face_r_dim,
        unpB_face_r_dim,
        0,
        unpA_num_faces,
        unpB_num_faces);
}

__attribute__((always_inline)) inline void _llk_unpack_AB_matmul_tilize_A_init(const std::uint32_t ct_dim, const std::uint32_t rt_dim, const std::uint32_t kt_dim, const std::uint32_t unpA_face_r_dim, const std::uint32_t unpB_face_r_dim, const std::uint32_t unpA_num_faces, const std::uint32_t unpB_num_faces, const std::uint32_t reuse_a_hint = 0) {
    volatile uint tt_reg_ptr *cfg = get_cfg_pointer();
    unpack_tile_descriptor_u tile_descriptor;
    for (uint i = 0; i < TILE_DESC_SIZE; i++)
    {
        tile_descriptor.val[i] = cfg[THCON_SEC1_REG0_TileDescriptor_ADDR32 + i];
    }
    tile_descriptor.f.x_dim          = FACE_C_DIM;
    tile_descriptor.f.y_dim          = kt_dim * 2;
    tile_descriptor.f.z_dim          = FACE_R_DIM;
    for (uint i = 0; i < TILE_DESC_SIZE; i++)
    {
        cfg[THCON_SEC1_REG0_TileDescriptor_ADDR32 + i] = tile_descriptor.val[i];
    }

    TTI_SETADCXX(p_setadc::UNP_A, unpB_num_faces * unpB_face_r_dim * FACE_C_DIM - 1, 0x0);
    TTI_SETADCXX(p_setadc::UNP_B, 1 * FACE_C_DIM - 1, 0x0);

    cfg_reg_rmw_tensix<UNP1_ADDR_CTRL_XY_REG_1_Ystride_RMW>(2 * FACE_C_DIM);

    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(0);

    _llk_unpack_AB_matmul_tilize_A_mop_config();
}

template <std::uint32_t kernel_broadcast_a = 0, std::uint32_t kernel_broadcast_b = 0>
inline void _llk_unpack_AB_matmul_tilize_A(const std::uint32_t base_address_a, const std::uint32_t base_address_b, const std::uint32_t tile_index_a, const std::uint32_t tile_index_b, const std::uint32_t tile_size_a, const std::uint32_t tile_size_b, const std::uint32_t unpA_face_r_dim, const std::uint32_t unpB_face_r_dim, const std::uint32_t ct_dim, const std::uint32_t rt_dim, const std::uint32_t kt_dim, const std::uint32_t reuse_a_hint = 0) {
    volatile uint *cfg = get_cfg_pointer();
    TTI_SETADCXY(0b011, 0, 0, 0, 0, 0b1010);
    TTI_SETADCZW(0b011, 0, 0, 0, 0, 0b1111);

    const bool reuse_a            = reuse_a_hint == 0 ? ct_dim >= rt_dim : reuse_a_hint == 1 ? true : false;
    const std::uint32_t t_dim     = reuse_a ? rt_dim : ct_dim;
    const std::uint32_t reuse_dim = reuse_a ? ct_dim : rt_dim;

    // Calculate tile addresses
    std::uint32_t offset_address_a = (tile_index_a * TILE_C_DIM * 2) >> 4;
    std::uint32_t offset_address_b = tile_size_b * tile_index_b;
    std::uint32_t address_a        = base_address_a + offset_address_a;
    std::uint32_t address_b        = base_address_b + offset_address_b;

    // Wait for free context
    wait_for_next_context(2);

    // Program unpacker 1 base address
    if (0 == unp_cfg_context)
    {
        cfg[THCON_SEC0_REG3_Base_address_ADDR32] = address_b;
        cfg[THCON_SEC1_REG3_Base_address_ADDR32] = address_a;
    }
    else
    {
        cfg[THCON_SEC0_REG3_Base_cntx1_address_ADDR32] = address_b;
        cfg[THCON_SEC1_REG3_Base_cntx1_address_ADDR32] = address_a;
    }

    semaphore_post(semaphore::UNPACK_SYNC); // Trisc::SEMPOST for context acquire

    // Stall unpacker until pending CFG writes from Trisc have completed
    TTI_STALLWAIT(p_stall::STALL_UNPACK, p_stall::TRISC_CFG);

    for (uint t = 0; t < t_dim; t++)
    {
        if(reuse_a)
        {
            TTI_SETADCZW(p_setadc::UNP_A, 0, 0, 0, 0, 0b0010);
            TTI_UNPACR(SrcA, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
            TTI_INCADCZW(p_setadc::UNP_A, 0, 0, 1, 0);

            TTI_MOP(0, 1, 2);

            if (reuse_dim > 1)
            {
                TTI_UNPACR(SrcA, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                TTI_INCADCZW(p_setadc::UNP_A, 0, 0, 1, 0);
            }

            if ((t + 1) < t_dim)
            {
                TTI_MOP(0, 1, 2);
                t++;
            }

            for (uint i = 2; i < reuse_dim; i++)
            {
                TTI_UNPACR(SrcA, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                TTI_INCADCZW(p_setadc::UNP_A, 0, 0, 1, 0);
            }
        }
        else
        {
            TTI_UNPACR(SrcA, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
            TTI_INCADCZW(p_setadc::UNP_A, 0, 0, 1, 0);

            TTI_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, 0b0010);
            TTI_MOP(0, 1, 2);

            if ((t + 1) < t_dim)
            {
                TTI_UNPACR(SrcA, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                TTI_INCADCZW(p_setadc::UNP_A, 0, 0, 1, 0);
                t++;
            }

            if (reuse_dim > 1)
            {
                TTI_MOP(0, (2 * reuse_dim) - 3, 0xAAAA);
            }
        }
    }

    // T6::SEMGET for context release
    t6_semaphore_get(semaphore::UNPACK_SYNC);

    // Switch unpacker config context
    switch_config_context(unp_cfg_context);
}
