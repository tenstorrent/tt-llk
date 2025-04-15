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
    const std::uint32_t replay_buf_run_len = 2;
    const std::uint32_t replay_buf_half_len = replay_buf_run_len >> 1;

    TT_REPLAY(0, replay_buf_run_len, 0, 1);

    //Unpacks 1x16 row of datums to SrcB
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    // THCON_SEC0_REG3_Base_address_ADDR32 =  THCON_SEC0_REG3_Base_address_ADDR32 +  SCRATCH_SEC0_val_ADDR32
    //TTI_CFGSHIFTMASK(1, 0b011, 32 - 1, 0, 0b11, THCON_SEC0_REG3_Base_address_ADDR32);
    //TTI_RDCFG(p_gpr_unpack::TMP0, THCON_SEC1_REG3_Base_address_ADDR32);
    //TTI_ADDDMAREG(0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP0, p_gpr_unpack::TILE_OFFSET);
    //TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
    //TTI_NOP;

    //Unpacks 1x16 row of datums to SrcA
    TTI_UNPACR(SrcB, 0b01000001/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);

    // THCON_SEC0_REG3_Base_cntx1_address_ADDR32 =  THCON_SEC0_REG3_Base_cntx1_address_ADDR32 +  SCRATCH_SEC0_val_ADDR32
    //TTI_CFGSHIFTMASK(1, 0b011, 32 - 1, 0, 0b11, THCON_SEC0_REG3_Base_cntx1_address_ADDR32);
    //TTI_RDCFG(p_gpr_unpack::TMP0, THCON_SEC1_REG3_Base_cntx1_address_ADDR32);
    //TTI_ADDDMAREG(0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP0, p_gpr_unpack::TILE_OFFSET);
    //TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_cntx1_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
    //TTI_NOP;

    ckernel_unpack_template tmp = ckernel_unpack_template(
        false,  // src B
        false,  // halo - just used for 4 unpacks
        TT_OP_REPLAY(0, replay_buf_half_len, 0, 0), // runs when context is 0
        0,
        0,
        0,
        TT_OP_REPLAY(replay_buf_half_len, replay_buf_half_len, 0, 0), // runs when context is 1
        0,
        0);

    tmp.program(instrn_buffer);
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
    const std::uint32_t c_dim_size = 2 * kt_dim * TILE_C_DIM >> 4;
    const std::uint32_t right_face_jump = ((2 * FACE_C_DIM) - (2 * kt_dim * TILE_C_DIM * (FACE_R_DIM - 1))) >> 4;
    const std::uint32_t down_face_jump = (2 * (kt_dim * TILE_C_DIM - FACE_C_DIM)) >> 4;
    TT_SETDMAREG(0, LOWER_HALFWORD(c_dim_size), 0, LO_16(p_gpr_unpack::TILE_OFFSET));
    TT_SETDMAREG(0, UPPER_HALFWORD(c_dim_size), 0, HI_16(p_gpr_unpack::TILE_OFFSET));
    TT_SETDMAREG(0, LOWER_HALFWORD(right_face_jump), 0, LO_16(p_gpr_unpack::TILE_SIZE_A));
    TT_SETDMAREG(0, UPPER_HALFWORD(right_face_jump), 0, HI_16(p_gpr_unpack::TILE_SIZE_A));
    TT_SETDMAREG(0, LOWER_HALFWORD(down_face_jump), 0, LO_16(p_gpr_unpack::TILE_SIZE_B));
    TT_SETDMAREG(0, UPPER_HALFWORD(down_face_jump), 0, HI_16(p_gpr_unpack::TILE_SIZE_B));
    //TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
    //TTI_WRCFG(p_gpr_unpack::TILE_OFFSET, 0, SCRATCH_SEC0_val_ADDR32);
    //TTI_NOP;

    volatile uint tt_reg_ptr *cfg = get_cfg_pointer();
    unpack_tile_descriptor_u tile_descriptor;
    for (uint i = 0; i < TILE_DESC_SIZE; i++)
    {
        tile_descriptor.val[i] = cfg[THCON_SEC1_REG0_TileDescriptor_ADDR32 + i];
    }
    tile_descriptor.f.x_dim          = 1;
    tile_descriptor.f.y_dim          = c_dim_size * 8;
    tile_descriptor.f.z_dim          = 128;
    for (uint i = 0; i < TILE_DESC_SIZE; i++)
    {
        cfg[THCON_SEC1_REG0_TileDescriptor_ADDR32 + i] = tile_descriptor.val[i];
    }

    TTI_SETADCXX(p_setadc::UNP_A, unpB_num_faces * unpB_face_r_dim * FACE_C_DIM - 1, 0x0);
    TTI_SETADCXX(p_setadc::UNP_B, 1 * FACE_C_DIM - 1, 0x0);
    
    uint unpB_ch1_y_stride = 2 * FACE_C_DIM;
    cfg_reg_rmw_tensix<UNP1_ADDR_CTRL_XY_REG_1_Ystride_RMW>(unpB_ch1_y_stride);
    
    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(0);

    _llk_unpack_AB_matmul_tilize_A_mop_config();
}

inline void _llk_unpack_AB_matmul_tilize_A_unpack_A()
{
    TTI_SETADCXY(p_setadc::UNP_B, 0, 0, 0, 0, 0b1010);
    TTI_SETADCZW(p_setadc::UNP_B, 0, 0, 0, 0, 0b1111);

    if (0 == unp_cfg_context)
    {
        ckernel_unpack_template::run(instrn_buffer, FACE_R_DIM-1, 0);
        TTI_UNPACR(SrcB, 0b01000000/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
        TTI_RDCFG(p_gpr_unpack::TMP0, THCON_SEC1_REG3_Base_address_ADDR32);
        TTI_ADDDMAREG(0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP0, p_gpr_unpack::TILE_SIZE_A);
        TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
        TTI_NOP;
        ckernel_unpack_template::run(instrn_buffer, FACE_R_DIM-1, 0);
        TTI_UNPACR(SrcB, 0b01000000/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
        TTI_RDCFG(p_gpr_unpack::TMP0, THCON_SEC1_REG3_Base_address_ADDR32);
        TTI_ADDDMAREG(0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP0, p_gpr_unpack::TILE_SIZE_B);
        TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
        TTI_NOP;
        ckernel_unpack_template::run(instrn_buffer, FACE_R_DIM-1, 0);
        TTI_UNPACR(SrcB, 0b01000000/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
        TTI_RDCFG(p_gpr_unpack::TMP0, THCON_SEC1_REG3_Base_address_ADDR32);
        TTI_ADDDMAREG(0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP0, p_gpr_unpack::TILE_SIZE_A);
        TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
        TTI_NOP;
        ckernel_unpack_template::run(instrn_buffer, FACE_R_DIM-1, 0);
        TTI_UNPACR(SrcB, 0b01000000/*CH1_Y+=1*/, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    }
    else
    {
        ckernel_unpack_template::run(instrn_buffer, FACE_R_DIM-1, 0xffff);
        TTI_UNPACR(SrcB, 0b01000000/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
        TTI_RDCFG(p_gpr_unpack::TMP0, THCON_SEC1_REG3_Base_cntx1_address_ADDR32);
        TTI_ADDDMAREG(0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP0, p_gpr_unpack::TILE_SIZE_A);
        TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_cntx1_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
        TTI_NOP;
        ckernel_unpack_template::run(instrn_buffer, FACE_R_DIM-1, 0xffff);
        TTI_UNPACR(SrcB, 0b01000000/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
        TTI_RDCFG(p_gpr_unpack::TMP0, THCON_SEC1_REG3_Base_cntx1_address_ADDR32);
        TTI_ADDDMAREG(0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP0, p_gpr_unpack::TILE_SIZE_B);
        TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_cntx1_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
        TTI_NOP;
        ckernel_unpack_template::run(instrn_buffer, FACE_R_DIM-1, 0xffff);
        TTI_UNPACR(SrcB, 0b01000000/*CH1_Y+=1*/, 0, 0, 0, 1, 0, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
        TTI_RDCFG(p_gpr_unpack::TMP0, THCON_SEC1_REG3_Base_cntx1_address_ADDR32);
        TTI_ADDDMAREG(0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP0, p_gpr_unpack::TILE_SIZE_A);
        TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_cntx1_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
        TTI_NOP;
        ckernel_unpack_template::run(instrn_buffer, FACE_R_DIM-1, 0xffff);
        TTI_UNPACR(SrcB, 0b01000000/*CH1_Y+=1*/, 0, 0, 0, 1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
    }
}

template <std::uint32_t kernel_broadcast_a = 0, std::uint32_t kernel_broadcast_b = 0>
inline void _llk_unpack_AB_matmul_tilize_A(const std::uint32_t base_address_a, const std::uint32_t base_address_b, const std::uint32_t tile_index_a, const std::uint32_t tile_index_b, const std::uint32_t tile_size_a, const std::uint32_t tile_size_b, const std::uint32_t unpA_face_r_dim, const std::uint32_t unpB_face_r_dim, const std::uint32_t ct_dim, const std::uint32_t rt_dim, const std::uint32_t kt_dim, const std::uint32_t reuse_a_hint = 0) {
    volatile uint *cfg        = get_cfg_pointer();
    TTI_SETADCXY(0b011, 0, 0, 0, 0, 0b1010);
    TTI_SETADCZW(0b011, 0, 0, 0, 0, 0b1111);

    const bool reuse_a            = reuse_a_hint == 0 ? ct_dim >= rt_dim : reuse_a_hint == 1 ? true : false;
    const std::uint32_t t_dim     = reuse_a ? rt_dim : ct_dim;
    const std::uint32_t reuse_dim = reuse_a ? ct_dim : rt_dim;

    for (uint t = 0; t < t_dim; t++)
    {
        // Calculate tile addresses
        std::uint32_t offset_address_a      = tile_size_a * (reuse_a ? (t * kt_dim) : (0)) + ((tile_index_a * TILE_C_DIM * 2) >> 4);
        std::uint32_t next_offset_address_a = tile_size_a * (reuse_a ? ((t + 1) * kt_dim) : (0)) + ((tile_index_a * TILE_C_DIM * 2) >> 4);
        std::uint32_t offset_address_b      = tile_size_b * (tile_index_b + (reuse_a ? (0) : (t)));
        std::uint32_t next_offset_address_b = tile_size_b * (tile_index_b + (reuse_a ? (0) : (t + 1)));
        std::uint32_t address_a      = base_address_a + offset_address_a;
        std::uint32_t next_address_a = base_address_a + next_offset_address_a;
        std::uint32_t address_b      = base_address_b + offset_address_b;
        std::uint32_t next_address_b = base_address_b + next_offset_address_b;

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

        if(reuse_a)
        {
            _llk_unpack_AB_matmul_tilize_A_unpack_A();
            if ((t + 1) < t_dim)
            {
                // Let's load one more tile into srcB
                TT_SETDMAREG(0, LOWER_HALFWORD(next_address_a), 0, LO_16(p_gpr_unpack::TMP0));
                TT_SETDMAREG(0, UPPER_HALFWORD(next_address_a), 0, HI_16(p_gpr_unpack::TMP0));
                if (0 == unp_cfg_context)
                {
                    TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
                }
                else
                {
                    TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_cntx1_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
                }
                TTI_DMANOP;
                _llk_unpack_AB_matmul_tilize_A_unpack_A();
                t++;
            }
            for (uint i = 0; i < reuse_dim; i++)
            {
                TTI_UNPACR(SrcA, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                TT_SETDMAREG(0, LOWER_HALFWORD(address_b + tile_size_b * (i + 1)), 0, LO_16(p_gpr_unpack::TMP0));
                TT_SETDMAREG(0, UPPER_HALFWORD(address_b + tile_size_b * (i + 1)), 0, HI_16(p_gpr_unpack::TMP0));
                if (0 == unp_cfg_context)
                {
                    TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC0_REG3_Base_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
                }
                else
                {
                    TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC0_REG3_Base_cntx1_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
                }
                TTI_DMANOP;
            }
        }
        else
        {
            TTI_UNPACR(SrcA, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
            if ((t + 1) < t_dim)
            {
                // Let's load one more tile into srcB
                TT_SETDMAREG(0, LOWER_HALFWORD(next_address_b), 0, LO_16(p_gpr_unpack::TMP0));
                TT_SETDMAREG(0, UPPER_HALFWORD(next_address_b), 0, HI_16(p_gpr_unpack::TMP0));
                if (0 == unp_cfg_context)
                {
                    TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC0_REG3_Base_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
                }
                else
                {
                    TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC0_REG3_Base_cntx1_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
                }
                TTI_DMANOP;
                TTI_UNPACR(SrcA, 0, 0, 0, 0, 1 /*Set OvrdThreadId*/, 1 /*Set Dvalid*/, p_unpacr::RAREFYB_DISABLE, 0, 0 /* Set ContextIdInc */, 0, 0, 1);
                t++;
            }
            for (uint i = 0; i < reuse_dim; i++)
            {
                _llk_unpack_AB_matmul_tilize_A_unpack_A();
                TT_SETDMAREG(0, LOWER_HALFWORD(address_a + tile_size_a * kt_dim * (i + 1)), 0, LO_16(p_gpr_unpack::TMP0));
                TT_SETDMAREG(0, UPPER_HALFWORD(address_a + tile_size_a * kt_dim * (i + 1)), 0, HI_16(p_gpr_unpack::TMP0));
                if (0 == unp_cfg_context)
                {
                    TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
                }
                else
                {
                    TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG3_Base_cntx1_address_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_unpack::TMP0);
                }
                TTI_DMANOP;
            }
        }

        // T6::SEMGET for context release
        t6_semaphore_get(semaphore::UNPACK_SYNC);

        // Switch unpacker config context
        switch_config_context(unp_cfg_context);
    }
}
