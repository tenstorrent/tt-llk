// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_math_common.h"

using namespace ckernel;

// local function declarations
inline void eltwise_binary_configure_addrmod();

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

template <EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void eltwise_binary_reuse_dest_as_src_tile(uint32_t idst = 0)
{
    if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCA)
    {
        switch (idst) {
            case 0:
                // Necessary fix
                TT_SETC16(DEST_TARGET_REG_CFG_MATH_Offset_ADDR32, 0);
                TT_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 0);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 4);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 8);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 12);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 16);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 20);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 24);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 28);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 32);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 36);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 40);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 44);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 48);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 52);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 56);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 60);
                break;
            case 1:
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 64);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 68);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 72);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 76);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 80);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 84);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 88);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 92);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 96);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 100);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 104);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 108);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 112);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 116);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 120);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 124);
                break;
            case 2:
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 128);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 132);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 136);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 140);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 144);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 148);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 152);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 156);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 160);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 164);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 168);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 172);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 176);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 180);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 184);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 188);
                break;
            case 3:
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 192);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 196);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 200);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 204);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 208);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 212);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 216);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 220);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 224);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 228);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 232);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 236);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 240);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 244);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 248);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 252);
                break;
            case 4:
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 256);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 260);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 264);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 268);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 272);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 276);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 280);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 284);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 288);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 292);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 296);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 300);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 304);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 308);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 312);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 316);
                break;
            case 5:
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 320);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 324);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 328);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 332);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 336);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 340);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 344);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 348);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 352);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 356);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 360);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 364);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 368);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 372);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 376);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 380);
                break;
            case 6:
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 384);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 388);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 392);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 396);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 400);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 404);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 408);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 412);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 416);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 420);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 424);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 428);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 432);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 436);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 440);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 444);
                break;
            case 7:
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 448);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 452);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 456);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 460);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 464);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 468);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 472);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 476);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 480);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 484);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 488);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 492);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 496);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 500);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 504);
                TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60, ADDR_MOD_0, p_movd2a::MOV_4_ROWS, 508);
                break;
            default:
                // Should never reach here if idst is 0-7, which is used for fused ops
                break;
        }
    }
    else if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCB)
    {

        // Explicitly reset D (dest read) and B (srcB write) counters to 0
        TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);
        TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_B);
        
        // Move all 64 rows from dest to srcB (4 rows at a time, 16 instructions total)
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 0, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 0);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 4, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 4);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 8, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 8);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 12, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 12);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 16, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 16);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 20, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 20);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 24, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 24);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 28, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 28);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 32, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 32);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 36, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 36);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 40, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 40);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 44, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 44);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 48, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 48);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 52, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 52);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 56, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 56);
        TTI_MOVD2B(0, p_movd2b::SRC_ZERO_OFFSET + 60, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 60);
    }
}

template <
    EltwiseBinaryType eltwise_binary_type,
    BroadcastType src_b_bcast_type,
    DstSync Dst,
    bool is_fp32_dest_acc_en,
    int NUM_FIDELITY_PHASES                      = 0,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void _llk_math_eltwise_binary_(const std::uint32_t num_faces, uint dst_index, const bool clear_fp32_dst_acc)
{
    constexpr bool high_fidelity     = (NUM_FIDELITY_PHASES > 0);
    constexpr uint32_t ZERO_ACC_MODE = p_zeroacc::CLR_16;

    math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(dst_index);
    
    // CRITICAL FIX: Reset dest RWC counter to 0 after setting base address
    // set_dst_write_addr sets the base, but D counter is separate and may have stale values
    // Without this, writes go to base_addr + stale_D_counter instead of base_addr + 0
    // Using CLR_NONE for synchronization (don't touch valid flags), SET_D to reset only D counter
    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);

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
            constexpr uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? 4 : 1;
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
            constexpr uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? 2 : 1;
            if constexpr (high_fidelity)
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < 2; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                    {
                        // We clear the DEST face-by-face, given the DEST base, tile index and face index
                        int clear_fp32   = is_fp32_dest_acc_en && clear_fp32_dst_acc ? 1 : 0;
                        auto buffer_base = is_fp32_dest_acc_en && clear_fp32_dst_acc ? get_dest_buffer_base_32b() : get_dest_buffer_base_16b();
                        TT_ZEROACC(
                            ZERO_ACC_MODE, clear_fp32, 0, ADDR_MOD_1, (buffer_base + get_dest_index_in_faces(dst_index, (0 + face_num)))); // Clear faces 0 & 1
                    }
                    ckernel_template::run();
                }
            }
            else
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                    {
                        // We clear the DEST face-by-face, given the DEST base, tile index and face index
                        int clear_fp32   = is_fp32_dest_acc_en && clear_fp32_dst_acc ? 1 : 0;
                        auto buffer_base = is_fp32_dest_acc_en && clear_fp32_dst_acc ? get_dest_buffer_base_32b() : get_dest_buffer_base_16b();
                        TT_ZEROACC(
                            ZERO_ACC_MODE, clear_fp32, 0, ADDR_MOD_1, (buffer_base + get_dest_index_in_faces(dst_index, (0 + face_num)))); // Clear faces 0 & 1
                    }
                    ckernel_template::run();
                }
            }
            TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            if (num_faces == 4)
            {
                if constexpr (high_fidelity)
                {
#pragma GCC unroll 0
                    for (std::uint32_t face_num = 0; face_num < 2; face_num++)
                    {
                        eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                        if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                        {
                            // We clear the DEST face-by-face, given the DEST base, tile index and face index
                            int clear_fp32   = is_fp32_dest_acc_en && clear_fp32_dst_acc ? 1 : 0;
                            auto buffer_base = is_fp32_dest_acc_en && clear_fp32_dst_acc ? get_dest_buffer_base_32b() : get_dest_buffer_base_16b();
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                clear_fp32,
                                0,
                                ADDR_MOD_1,
                                (buffer_base + get_dest_index_in_faces(dst_index, (2 + face_num)))); // Clear faces 2 & 3
                        }
                        ckernel_template::run();
                    }
                }
                else
                {
#pragma GCC unroll 0
                    for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
                    {
                        eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                        if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                        {
                            // We clear the DEST face-by-face, given the DEST base, tile index and face index
                            int clear_fp32   = is_fp32_dest_acc_en && clear_fp32_dst_acc ? 1 : 0;
                            auto buffer_base = is_fp32_dest_acc_en && clear_fp32_dst_acc ? get_dest_buffer_base_32b() : get_dest_buffer_base_16b();
                            TT_ZEROACC(
                                ZERO_ACC_MODE,
                                clear_fp32,
                                0,
                                ADDR_MOD_1,
                                (buffer_base + get_dest_index_in_faces(dst_index, (2 + face_num)))); // Clear faces 2 & 3
                        }
                        ckernel_template::run();
                    }
                }
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, 0);
            }
        }
        else
        {
            // Row and no broadcasted behaves similarly
            const uint32_t outerloop = (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE) ? num_faces : 1;
            if constexpr (high_fidelity)
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < num_faces; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                    {
                        // We clear the DEST face-by-face, given the DEST base, tile index and face index
                        int clear_fp32   = is_fp32_dest_acc_en && clear_fp32_dst_acc ? 1 : 0;
                        auto buffer_base = is_fp32_dest_acc_en && clear_fp32_dst_acc ? get_dest_buffer_base_32b() : get_dest_buffer_base_16b();
                        TT_ZEROACC(ZERO_ACC_MODE, clear_fp32, 0, ADDR_MOD_1, (buffer_base + get_dest_index_in_faces(dst_index, face_num)));
                    }
                    ckernel_template::run();
                }
            }
            else
            {
#pragma GCC unroll 0
                for (std::uint32_t face_num = 0; face_num < outerloop; face_num++)
                {
                    eltwise_binary_reuse_dest_as_src<binary_reuse_dest>();
                    if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
                    {
                        // We clear the DEST face-by-face, given the DEST base, tile index and face index
                        int clear_fp32   = is_fp32_dest_acc_en && clear_fp32_dst_acc ? 1 : 0;
                        auto buffer_base = is_fp32_dest_acc_en && clear_fp32_dst_acc ? get_dest_buffer_base_32b() : get_dest_buffer_base_16b();
                        TT_ZEROACC(ZERO_ACC_MODE, clear_fp32, 0, ADDR_MOD_1, (buffer_base + get_dest_index_in_faces(dst_index, face_num)));
                    }
                    ckernel_template::run();
                }
            }
            if constexpr (src_b_bcast_type == BroadcastType::SCALAR)
            {
                TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_D);
            }
        }
    }
    math::clear_dst_reg_addr();
}

template <EltwiseBinaryType eltwise_binary_type, BroadcastType bcast_type, std::uint32_t FIDELITY_INCREMENT>
inline void eltwise_binary_configure_addrmod()
{
    // Use srcA for data movement
    if constexpr ((eltwise_binary_type == ELWADD) || (eltwise_binary_type == ELWSUB) || (eltwise_binary_type == ELWMUL))
    {
        if constexpr (bcast_type == BroadcastType::NONE || bcast_type == BroadcastType::COL)
        {
            addr_mod_t {
                .srca = {.incr = 8},
                .srcb = {.incr = 8},
                .dest = {.incr = 8},
            }
                .set(ADDR_MOD_0);
        }
        else if constexpr (bcast_type == BroadcastType::ROW || bcast_type == BroadcastType::SCALAR)
        {
            addr_mod_t {
                .srca = {.incr = 8},
                .srcb = {.incr = 0},
                .dest = {.incr = 8},
            }
                .set(ADDR_MOD_0);
        }
        addr_mod_t {
            .srca = {.incr = 0},
            .srcb = {.incr = 0},
            .dest = {.incr = 0},
        }
            .set(ADDR_MOD_1);

        addr_mod_t {
            .srca = {.incr = 0, .clr = 1}, .srcb = {.incr = 0, .clr = 1}, .dest = {.incr = 0, .clr = 0, .cr = 1}, .fidelity = {.incr = FIDELITY_INCREMENT}}
            .set(ADDR_MOD_2);

        addr_mod_t {
            .srca     = {.incr = 0, .clr = 1},
            .srcb     = {.incr = 0, .clr = 1},
            .dest     = {.incr = 8, .clr = 0, .cr = 0, .c_to_cr = 1},
            .fidelity = {.incr = 0, .clr = 1}}
            .set(ADDR_MOD_3);
    }
}

template <
    EltwiseBinaryType eltwise_binary_type,
    BroadcastType bcast_type,
    int NUM_FIDELITY_PHASES                      = 0,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void eltwise_binary_configure_mop(const std::uint32_t acc_to_dest = 0, const std::uint32_t num_faces = 4)
{
    constexpr bool high_fidelity = (NUM_FIDELITY_PHASES > 0);
    const uint addr_mod          = ADDR_MOD_0;
    constexpr uint innerloop     = 16 >> 3; // 8 rows per eltwise op at a time.
    uint outerloop               = num_faces;
    auto broadcast_type          = p_elwise::SRCB_NO_BCAST;
    if constexpr (bcast_type == BroadcastType::COL)
    {
        // The mop only runs for 2 outer loops and mop is called twice for col broadcast
        outerloop      = 2;
        broadcast_type = p_elwise::SRCB_BCAST_COL;
    }
    else if constexpr (bcast_type == BroadcastType::ROW)
    {
        broadcast_type = p_elwise::SRCB_BCAST_ROW;
    }
    else if constexpr (bcast_type == BroadcastType::SCALAR)
    {
        broadcast_type = p_elwise::SRCB_BCAST_ALL;
    }

    if constexpr (binary_reuse_dest != EltwiseBinaryReuseDestType::NONE)
    {
        outerloop = 1;
    }

    // Scalar and Col broadcast should not Clear B within a mop.  This is controlled outside of MOP.
    if constexpr (bcast_type == BroadcastType::COL || bcast_type == BroadcastType::SCALAR)
    {
        if constexpr (eltwise_binary_type == ELWADD)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWADD(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_A, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            tmp.program();
        }
        else if constexpr (eltwise_binary_type == ELWSUB)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWSUB(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_A, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            tmp.program();
        }
        else if constexpr (eltwise_binary_type == ELWMUL)
        {
            ckernel_template tmp(high_fidelity ? NUM_FIDELITY_PHASES : outerloop, innerloop, TT_OP_ELWMUL(0, 0, broadcast_type, addr_mod, 0));
            if constexpr (high_fidelity)
            {
                tmp.set_last_inner_loop_instr(TT_OP_ELWMUL(0, 0, broadcast_type, ADDR_MOD_2, 0)); // Incr fidelity last inst of inner loop
                tmp.set_last_outer_loop_instr(TT_OP_ELWMUL(p_setrwc::CLR_A, 0, broadcast_type, ADDR_MOD_3, 0));
            }
            else
            {
                tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_A, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            }
            tmp.program();
        }
    }
    else
    {
        if constexpr (eltwise_binary_type == ELWADD)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWADD(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_AB, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            tmp.program();
        }
        else if constexpr (eltwise_binary_type == ELWSUB)
        {
            ckernel_template tmp(outerloop, innerloop, TT_OP_ELWSUB(0, acc_to_dest, broadcast_type, addr_mod, 0));
            tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_AB, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            tmp.program();
        }
        else if constexpr (eltwise_binary_type == ELWMUL)
        {
            ckernel_template tmp(high_fidelity ? NUM_FIDELITY_PHASES : outerloop, innerloop, TT_OP_ELWMUL(0, 0, broadcast_type, addr_mod, 0));
            if constexpr (high_fidelity)
            {
                tmp.set_last_inner_loop_instr(TT_OP_ELWMUL(0, 0, broadcast_type, ADDR_MOD_2, 0)); // Incr fidelity last inst of inner loop
                tmp.set_last_outer_loop_instr(TT_OP_ELWMUL(p_setrwc::CLR_AB, 0, broadcast_type, ADDR_MOD_3, 0));
            }
            else
            {
                tmp.set_end_op(TT_OP_SETRWC(p_setrwc::CLR_AB, p_setrwc::CR_AB, 0, 0, 0, p_setrwc::SET_AB));
            }
            tmp.program();
        }
    }
}

template <
    EltwiseBinaryType eltwise_binary_type,
    BroadcastType src_b_bcast_type,
    int MATH_FIDELITY_DESC                       = 0,
    EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void _llk_math_eltwise_binary_init_(const std::uint32_t num_faces, [[maybe_unused]] const std::uint32_t transpose, const std::uint32_t acc_to_dest)
{
    constexpr int MATH_FIDELITY_PHASES    = get_math_num_fidelity_phases(MATH_FIDELITY_DESC);
    constexpr int MATH_FIDELITY_INCREMENT = get_math_fidelity_increment(MATH_FIDELITY_DESC);

    eltwise_binary_configure_addrmod<eltwise_binary_type, src_b_bcast_type, MATH_FIDELITY_INCREMENT>();

    if constexpr ((eltwise_binary_type == ELWADD) || (eltwise_binary_type == ELWSUB) || (eltwise_binary_type == ELWMUL))
    {
        eltwise_binary_configure_mop<eltwise_binary_type, src_b_bcast_type, MATH_FIDELITY_PHASES, binary_reuse_dest>(acc_to_dest, num_faces);
    }

    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);

    math::reset_counters(p_setrwc::SET_ABD_F);
}

// Cleanup function for fused eltwise binary operations
inline void _fused_eltwise_binary_uninit_()
{
    TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_BD);
}
