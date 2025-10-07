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
inline void eltwise_binary_reuse_dest_as_src(uint32_t idst = 0)
{
    if(idst == 0)
    {
        if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCA)
        {
            // sanity
            TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_ABD_F);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 0);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 1);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 1,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 2);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 17,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 3);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 2,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 4);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 18,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 5);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 3,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 6);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 19,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 7);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 8);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 9);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 5,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 10);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 21,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 11);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 6,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 12);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 22,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 13);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 7,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 14);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 23,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 15);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 16);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 17);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 9,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 18);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 25,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 19);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 10,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 20);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 26,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 21);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 11,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 22);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 27,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 23);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 24);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 25);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 13,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 26);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 29,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 27);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 14,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 28);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 30,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 29);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 15,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 30);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 31,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 31);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 32);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 33);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 33,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 34);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 49,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 35);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 34,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 36);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 50,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 37);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 35,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 38);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 51,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 39);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 40);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 41);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 37,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 42);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 53,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 43);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 38,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 44);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 54,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 45);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 39,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 46);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 55,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 47);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 48);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 49);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 41,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 50);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 57,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 51);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 42,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 52);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 58,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 53);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 43,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 54);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 59,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 55);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 56);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 57);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 45,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 58);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 61,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 59);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 46,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 60);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 62,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 61);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 47,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 62);
            TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 63,  ADDR_MOD_1, p_movd2a::MOV_1_ROW, 63);
        }
        else if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCB)
        {
            // sanity, also doesn't matter if we move a face or a tile to srcB since we never increment its counter
            TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_ABD_F);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 0,  ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 0);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 4,  ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 4);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 8,  ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 8);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 12, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 12);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 16, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 16);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 20, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 20);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 24, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 24);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 28, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 28);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 32, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 32);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 36, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 36);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 40, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 40);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 44, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 44);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 48, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 48);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 52, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 52);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 56, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 56);
            TTI_MOVD2B(0, p_movb2d::SRC_ZERO_OFFSET + 60, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 60);
            TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_ABD_F);
        }
    }
    else
    {
        // brute forced, temporary solution
        if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCA)
        {
            // Move specific tile from dest to srcA based on idst (tile index)
            // Each tile is 64 rows, but we only move the first face (16 rows) of each tile
            // Use switch statement to make tile_offset compile-time constant instead of 
            // providing dest index as a runtime parameter (which would be less efficient)
            switch (idst) {
                case 0:
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 0);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 1);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 1,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 2);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 17,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 3);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 2,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 4);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 18,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 5);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 3,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 6);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 19,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 7);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 8);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 9);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 5,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 10);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 21,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 11);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 6,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 12);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 22,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 13);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 7,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 14);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 23,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 15);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 16);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 17);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 9,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 18);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 25,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 19);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 10,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 20);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 26,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 21);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 11,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 22);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 27,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 23);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 24);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 25);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 13,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 26);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 29,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 27);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 14,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 28);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 30,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 29);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 15,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 30);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 31,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 31);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 32);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 33);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 33,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 34);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 49,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 35);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 34,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 36);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 50,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 37);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 35,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 38);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 51,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 39);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 40);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 41);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 37,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 42);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 53,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 43);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 38,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 44);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 54,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 45);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 39,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 46);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 55,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 47);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 48);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 49);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 41,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 50);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 57,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 51);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 42,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 52);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 58,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 53);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 43,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 54);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 59,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 55);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 56);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 57);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 45,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 58);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 61,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 59);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 46,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 60);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 62,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 61);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 47,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 62);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 63,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 63);
                    break;
                case 1:
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 64);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 65);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 1,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 66);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 17,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 67);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 2,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 68);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 18,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 69);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 3,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 70);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 19,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 71);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 72);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 73);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 5,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 74);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 21,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 75);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 6,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 76);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 22,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 77);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 7,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 78);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 23,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 79);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 80);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 81);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 9,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 82);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 25,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 83);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 10,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 84);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 26,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 85);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 11,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 86);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 27,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 87);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 88);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 89);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 13,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 90);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 29,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 91);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 14,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 92);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 30,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 93);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 15,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 94);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 31,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 95);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 96);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 97);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 33,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 98);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 49,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 99);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 34,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 100);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 50,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 101);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 35,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 102);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 51,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 103);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 104);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 105);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 37,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 106);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 53,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 107);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 38,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 108);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 54,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 109);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 39,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 110);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 55,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 111);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 112);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 113);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 41,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 114);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 57,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 115);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 42,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 116);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 58,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 117);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 43,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 118);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 59,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 119);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 120);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 121);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 45,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 122);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 61,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 123);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 46,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 124);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 62,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 125);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 47,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 126);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 63,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 127);
                    break;
                case 2:
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 128);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 129);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 1,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 130);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 17,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 131);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 2,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 132);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 18,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 133);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 3,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 134);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 19,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 135);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 136);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 137);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 5,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 138);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 21,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 139);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 6,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 140);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 22,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 141);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 7,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 142);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 23,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 143);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 144);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 145);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 9,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 146);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 25,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 147);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 10,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 148);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 26,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 149);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 11,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 150);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 27,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 151);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 152);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 153);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 13,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 154);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 29,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 155);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 14,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 156);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 30,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 157);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 15,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 158);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 31,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 159);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 160);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 161);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 33,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 162);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 49,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 163);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 34,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 164);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 50,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 165);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 35,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 166);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 51,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 167);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 168);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 169);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 37,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 170);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 53,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 171);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 38,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 172);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 54,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 173);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 39,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 174);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 55,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 175);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 176);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 177);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 41,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 178);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 57,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 179);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 42,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 180);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 58,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 181);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 43,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 182);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 59,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 183);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 184);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 185);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 45,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 186);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 61,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 187);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 46,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 188);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 62,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 189);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 47,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 190);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 63,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 191);
                    break;
                case 3:
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 192);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 193);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 1,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 194);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 17,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 195);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 2,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 196);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 18,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 197);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 3,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 198);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 19,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 199);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 200);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 201);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 5,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 202);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 21,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 203);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 6,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 204);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 22,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 205);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 7,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 206);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 23,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 207);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 208);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 209);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 9,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 210);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 25,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 211);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 10,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 212);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 26,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 213);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 11,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 214);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 27,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 215);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 216);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 217);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 13,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 218);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 29,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 219);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 14,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 220);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 30,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 221);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 15,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 222);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 31,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 223);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 224);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 225);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 33,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 226);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 49,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 227);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 34,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 228);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 50,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 229);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 35,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 230);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 51,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 231);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 232);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 233);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 37,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 234);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 53,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 235);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 38,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 236);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 54,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 237);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 39,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 238);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 55,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 239);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 240);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 241);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 41,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 242);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 57,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 243);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 42,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 244);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 58,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 245);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 43,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 246);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 59,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 247);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 248);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 249);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 45,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 250);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 61,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 251);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 46,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 252);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 62,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 253);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 47,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 254);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 63,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 255);
                    break;
                case 4:
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 256);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 257);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 1,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 258);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 17,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 259);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 2,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 260);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 18,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 261);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 3,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 262);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 19,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 263);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 264);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 265);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 5,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 266);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 21,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 267);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 6,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 268);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 22,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 269);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 7,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 270);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 23,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 271);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 272);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 273);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 9,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 274);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 25,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 275);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 10,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 276);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 26,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 277);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 11,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 278);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 27,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 279);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 280);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 281);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 13,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 282);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 29,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 283);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 14,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 284);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 30,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 285);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 15,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 286);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 31,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 287);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 288);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 289);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 33,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 290);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 49,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 291);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 34,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 292);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 50,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 293);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 35,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 294);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 51,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 295);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 296);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 297);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 37,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 298);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 53,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 299);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 38,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 300);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 54,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 301);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 39,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 302);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 55,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 303);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 304);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 305);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 41,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 306);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 57,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 307);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 42,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 308);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 58,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 309);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 43,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 310);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 59,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 311);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 312);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 313);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 45,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 314);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 61,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 315);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 46,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 316);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 62,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 317);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 47,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 318);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 63,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 319);
                    break;
                case 5:
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 320);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 321);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 1,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 322);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 17,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 323);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 2,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 324);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 18,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 325);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 3,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 326);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 19,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 327);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 328);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 329);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 5,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 330);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 21,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 331);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 6,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 332);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 22,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 333);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 7,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 334);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 23,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 335);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 336);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 337);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 9,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 338);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 25,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 339);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 10,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 340);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 26,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 341);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 11,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 342);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 27,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 343);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 344);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 345);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 13,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 346);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 29,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 347);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 14,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 348);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 30,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 349);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 15,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 350);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 31,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 351);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 352);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 353);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 33,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 354);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 49,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 355);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 34,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 356);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 50,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 357);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 35,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 358);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 51,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 359);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 360);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 361);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 37,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 362);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 53,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 363);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 38,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 364);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 54,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 365);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 39,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 366);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 55,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 367);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 368);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 369);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 41,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 370);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 57,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 371);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 42,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 372);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 58,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 373);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 43,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 374);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 59,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 375);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 376);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 377);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 45,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 378);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 61,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 379);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 46,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 380);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 62,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 381);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 47,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 382);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 63,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 383);
                    break;
                case 6:
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 384);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 385);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 1,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 386);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 17,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 387);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 2,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 388);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 18,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 389);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 3,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 390);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 19,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 391);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 392);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 393);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 5,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 394);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 21,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 395);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 6,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 396);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 22,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 397);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 7,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 398);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 23,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 399);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 400);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 401);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 9,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 402);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 25,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 403);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 10,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 404);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 26,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 405);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 11,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 406);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 27,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 407);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 408);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 409);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 13,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 410);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 29,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 411);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 14,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 412);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 30,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 413);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 15,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 414);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 31,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 415);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 416);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 417);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 33,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 418);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 49,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 419);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 34,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 420);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 50,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 421);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 35,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 422);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 51,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 423);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 424);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 425);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 37,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 426);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 53,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 427);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 38,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 428);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 54,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 429);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 39,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 430);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 55,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 431);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 432);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 433);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 41,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 434);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 57,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 435);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 42,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 436);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 58,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 437);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 43,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 438);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 59,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 439);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 440);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 441);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 45,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 442);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 61,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 443);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 46,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 444);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 62,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 445);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 47,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 446);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 63,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 447);
                    break;
                case 7:
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 0,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 448);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 16,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 449);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 1,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 450);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 17,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 451);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 2,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 452);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 18,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 453);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 3,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 454);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 19,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 455);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 4,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 456);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 20,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 457);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 5,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 458);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 21,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 459);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 6,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 460);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 22,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 461);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 7,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 462);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 23,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 463);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 8,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 464);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 24,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 465);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 9,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 466);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 25,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 467);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 10,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 468);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 26,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 469);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 11,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 470);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 27,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 471);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 12,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 472);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 28,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 473);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 13,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 474);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 29,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 475);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 14,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 476);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 30,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 477);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 15,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 478);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 31,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 479);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 32,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 480);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 48,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 481);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 33,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 482);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 49,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 483);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 34,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 484);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 50,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 485);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 35,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 486);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 51,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 487);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 36,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 488);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 52,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 489);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 37,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 490);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 53,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 491);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 38,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 492);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 54,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 493);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 39,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 494);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 55,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 495);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 40,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 496);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 56,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 497);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 41,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 498);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 57,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 499);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 42,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 500);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 58,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 501);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 43,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 502);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 59,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 503);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 44,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 504);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 60,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 505);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 45,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 506);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 61,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 507);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 46,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 508);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 62,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 509);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 47,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 510);
                    TTI_MOVD2A(0, p_mova2d::MATH_HALO_ROWS + 63,  ADDR_MOD_0, p_movd2a::MOV_1_ROW, 511);
                    break;
                default:
                    // Should never reach here if idst is 0-7, which is used for fused ops
                    break;
            }
        }
    }
}

template <EltwiseBinaryReuseDestType binary_reuse_dest = EltwiseBinaryReuseDestType::NONE>
inline void eltwise_binary_reuse_tile_as_src()
{
    if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCA)
    {
        move_d2a_tile_as_src(ADDR_MOD_1);
    }
    else if constexpr (binary_reuse_dest == EltwiseBinaryReuseDestType::DEST_TO_SRCB)
    {
        move_d2b_tile_as_src(ADDR_MOD_1);
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
