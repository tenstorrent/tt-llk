// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_globals.h"
#include "ckernel_include.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "cmath_common.h"
#include "llk_math_common.h"

using namespace ckernel;

// local function declarations
inline void reduce_configure_addrmod();

template <ReduceDim dim, int num_fidelity_phases>
inline void reduce_configure_mop();

template <
    PoolType type,
    ReduceDim dim,
    bool is_fp32_dest_acc_en,
    int MATH_FIDELITY_DESC         = 0,
    bool is_int_fpu_en             = false,
    bool enforce_fp32_accumulation = false>
inline void _llk_math_reduce_(const uint dst_index, bool narrow_tile = false, const uint num_faces = 4)
{
    constexpr int MATH_FIDELITY_PHASES = get_math_num_fidelity_phases(MATH_FIDELITY_DESC);
    constexpr bool HIGH_FIDELITY       = MATH_FIDELITY_PHASES > 0;

    math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(dst_index);
    if constexpr (dim == ReduceDim::REDUCE_ROW)
    {
        // Transpose for each face in src A done at unpacker, and pool
        if constexpr (type == PoolType::MAX)
        {
            TTI_GMPOOL(p_setrwc::CLR_AB, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0);
        }
        else if constexpr (HIGH_FIDELITY)
        {
            ckernel_template::run();
            TTI_CLEARDVALID(p_setrwc::CLR_AB, 0);
        }
        else
        {
            TTI_GAPOOL(p_setrwc::CLR_AB, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0);
        }

        if constexpr (type == PoolType::MAX)
        {
            TTI_GMPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0);
        }
        else if constexpr (HIGH_FIDELITY)
        {
            ckernel_template::run();
        }
        else
        {
            TTI_GAPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0);
        }

        if constexpr (enforce_fp32_accumulation)
        {
            // Move back to B and transpose in 2 parts, first hi16 bits then lo16 bits
            constexpr int dest_32b_hi = 0;
            constexpr int dest_32b_lo = 1;

            // move hi16 bits D2B
            // we avoid clobbering weights in src B by moving to rows 16 - 31
            TTI_MOVD2B(dest_32b_hi, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);
            // note: transpose on src B on works on rows 16 - 31
            TTI_TRNSPSRCB;
            // move row D2B again for cases of reducing across multiple tiles
            TTI_MOVD2B(dest_32b_hi, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);

            // move hi16 bits B2D
            TTI_MOVB2D(dest_32b_hi, p_movb2d::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 0);
            TTI_MOVB2D(dest_32b_hi, p_movb2d::SRC_ROW16_OFFSET + 4, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 4);
            TTI_MOVB2D(dest_32b_hi, p_movb2d::SRC_ROW16_OFFSET + 8, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 8);
            TTI_MOVB2D(dest_32b_hi, p_movb2d::SRC_ROW16_OFFSET + 12, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 12);

            // move lo16 bits D2B
            TTI_MOVD2B(dest_32b_lo, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);
            // transpose face
            TTI_TRNSPSRCB;
            // move row again for cases of reducing multiple tiles
            TTI_MOVD2B(dest_32b_lo, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);

            // move lo16 bits B2D
            TTI_MOVB2D(dest_32b_lo, p_movb2d::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 0);
            TTI_MOVB2D(dest_32b_lo, p_movb2d::SRC_ROW16_OFFSET + 4, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 4);
            TTI_MOVB2D(dest_32b_lo, p_movb2d::SRC_ROW16_OFFSET + 8, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 8);
            TTI_MOVB2D(dest_32b_lo, p_movb2d::SRC_ROW16_OFFSET + 12, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 12);
        }
        else
        {
            // Datums stored in int32 dest cannot be moved to SrcB which is configured for int8 inputs
            // Cast int32 datums to int8 using SFPU instructions (load int32, store int8) before moving data to srcB
            // Besides SFPU instructions to do cast we also need to set chicken bit FP16A_FORCE_Enable to force dest
            // view to be fp16a as int8 datums are stored in src registers as fp16a
            if constexpr (is_int_fpu_en)
            {
                TTI_STALLWAIT(p_stall::STALL_SFPU, p_stall::MATH);
                TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::INT32, ADDR_MOD_0, 0 /*DEST offset*/);
                TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::INT8, ADDR_MOD_0, 0 /*DEST offset*/);
                TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::INT32, ADDR_MOD_0, 2 /*DEST offset*/);
                TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::INT8, ADDR_MOD_0, 2 /*DEST offset*/);
                TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::WAIT_SFPU);
                TTI_SETC16(FP16A_FORCE_Enable_ADDR32, 0x1);
            }

            // Move back to B and transpose
            // we avoid clobbering weights in src B by moving to rows 16 - 31
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 0, 0, 0, p_setrwc::SET_AB);
            /*
            if constexpr (is_fp32_dest_acc_en) {
                if (0 == (((uint)unpack_dst_format[0]>>2)&0x1)) { // fp32 to fp16_a conversion
                    TTI_STALLWAIT(p_stall::STALL_SFPU, p_stall::MATH);
                    TTI_SFPLOAD(0, 0, 3, 0);
                    TTI_SFP_STOCH_RND(0,0,0,0,0,8);
                    TTI_SFPSTORE(0,1,3,0);
                }
            }
            */
            TTI_MOVD2B(0, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);
            // Note: transpose on src B on works on rows 16 - 31
            TTI_TRNSPSRCB;
            TTI_MOVD2B(0, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);
            if constexpr (is_int_fpu_en)
            {
                TTI_SETC16(FP16A_FORCE_Enable_ADDR32, 0x0);
            }

            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_B, 0, 8, 0, p_setrwc::SET_B);
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_B, 0, 8, 0, p_setrwc::SET_B);
            TTI_ZEROSRC(0, 1, 0, 1); // Clear src A
            TTI_ELWADD(0, 0, p_elwise::SRCB_NO_BCAST, ADDR_MOD_2, 0);
            TTI_ELWADD(0, 0, p_elwise::SRCB_NO_BCAST, ADDR_MOD_2, 0);
        }

        if (num_faces == 2 && !narrow_tile)
        {
            TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_BD);
        }
        else
        {
            // Increment dest by 32 or 16 if narrow tile for the next accumulation
            if (!narrow_tile)
            {
                TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
                TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
            }
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
            TTI_SETRWC(p_setrwc::CLR_AB, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_BD);

            /////////////////////
            // Second Tile Row //
            /////////////////////

            // Transpose at unpacker and pool
            if constexpr (type == PoolType::MAX)
            {
                TTI_GMPOOL(p_setrwc::CLR_AB, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0);
            }
            else if constexpr (HIGH_FIDELITY)
            {
                ckernel_template::run();
                TTI_CLEARDVALID(p_setrwc::CLR_AB, 0);
            }
            else
            {
                TTI_GAPOOL(p_setrwc::CLR_AB, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0);
            }

            if constexpr (type == PoolType::MAX)
            {
                TTI_GMPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0);
            }
            else if constexpr (HIGH_FIDELITY)
            {
                ckernel_template::run();
            }
            else
            {
                TTI_GAPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0);
            }

            if constexpr (enforce_fp32_accumulation)
            {
                // Move back to B and transpose in 2 parts, first hi16 bits then lo16 bits
                constexpr int dest_32b_hi = 0;
                constexpr int dest_32b_lo = 1;

                // move hi16 bits D2B
                // we avoid clobbering weights in src B by moving to rows 16 - 31
                TTI_MOVD2B(dest_32b_hi, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);
                // note: transpose on src B on works on rows 16 - 31
                TTI_TRNSPSRCB;
                // move row D2B again for cases of reducing across multiple tiles
                TTI_MOVD2B(dest_32b_hi, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);

                // move hi16 bits B2D
                TTI_MOVB2D(dest_32b_hi, p_movb2d::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 0);
                TTI_MOVB2D(dest_32b_hi, p_movb2d::SRC_ROW16_OFFSET + 4, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 4);
                TTI_MOVB2D(dest_32b_hi, p_movb2d::SRC_ROW16_OFFSET + 8, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 8);
                TTI_MOVB2D(dest_32b_hi, p_movb2d::SRC_ROW16_OFFSET + 12, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 12);

                // move lo16 bits D2B
                TTI_MOVD2B(dest_32b_lo, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);
                // transpose face
                TTI_TRNSPSRCB;
                // move row again for cases of reducing multiple tiles
                TTI_MOVD2B(dest_32b_lo, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);

                // move lo16 bits B2D
                TTI_MOVB2D(dest_32b_lo, p_movb2d::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 0);
                TTI_MOVB2D(dest_32b_lo, p_movb2d::SRC_ROW16_OFFSET + 4, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 4);
                TTI_MOVB2D(dest_32b_lo, p_movb2d::SRC_ROW16_OFFSET + 8, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 8);
                TTI_MOVB2D(dest_32b_lo, p_movb2d::SRC_ROW16_OFFSET + 12, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 12);
            }
            else
            {
                // Datums stored in int32 dest cannot be moved to SrcB which is configured for int8 inputs
                // Cast int32 datums to int8 using SFPU instructions (load int32, store int8) before moving data to srcB
                // Besides SFPU instructions to do cast we also need to set chicken bit FP16A_FORCE_Enable to force dest
                // view to be fp16a as int8 datums are stored in src registers as fp16a
                if constexpr (is_int_fpu_en)
                {
                    TTI_STALLWAIT(p_stall::STALL_SFPU, p_stall::MATH);
                    TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::INT32, ADDR_MOD_0, 0 /*DEST offset*/);
                    TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::INT8, ADDR_MOD_0, 0 /*DEST offset*/);
                    TTI_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::INT32, ADDR_MOD_0, 2 /*DEST offset*/);
                    TTI_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::INT8, ADDR_MOD_0, 2 /*DEST offset*/);
                    TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::WAIT_SFPU);
                    TTI_SETC16(FP16A_FORCE_Enable_ADDR32, 0x1);
                }

                // Move back to B and transpose
                TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 0, 0, 0, p_setrwc::SET_AB);
                /*
                if constexpr (is_fp32_dest_acc_en) {
                    if (0 == (((uint)unpack_dst_format[0]>>2)&0x1)) { // fp32 to fp16_a conversion
                        TTI_STALLWAIT(p_stall::STALL_SFPU, p_stall::MATH);
                        TTI_SFPLOAD(0, 0, 3, 0);
                        TTI_SFP_STOCH_RND(0,0,0,0,0,8);
                        TTI_SFPSTORE(0,1,3,0);
                    }
                }
                */
                TTI_MOVD2B(0, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);
                // Note: transpose on src B on works on rows 16 - 31
                TTI_TRNSPSRCB;
                TTI_MOVD2B(0, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);
                if constexpr (is_int_fpu_en)
                {
                    TTI_SETC16(FP16A_FORCE_Enable_ADDR32, 0x0);
                }

                TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_B, 0, 8, 0, p_setrwc::SET_B);
                TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_B, 0, 8, 0, p_setrwc::SET_B);
                TTI_ZEROSRC(0, 1, 0, 1); // Clear src A
                TTI_ELWADD(0, 0, p_elwise::SRCB_NO_BCAST, ADDR_MOD_2, 0);
                TTI_ELWADD(0, 0, p_elwise::SRCB_NO_BCAST, ADDR_MOD_2, 0);
            }

            // Reset counters to 0 for next accumulation
            TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_BD);
        }
    }
    else if constexpr (dim == ReduceDim::REDUCE_COL)
    {
        const uint num_row_tiles = narrow_tile ? 2 : ((num_faces > 1) ? num_faces / 2 : 1);
        for (uint row_tile = 0; row_tile < num_row_tiles; row_tile++)
        {
            // Just pool
            if constexpr (type == PoolType::MAX)
            {
                TTI_GMPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0);
            }
            else
            {
                if constexpr (HIGH_FIDELITY)
                {
                    ckernel_template::run();
                }
                else
                {
                    TTI_GAPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0);
                }
            }
            if ((!narrow_tile) && (num_faces > 1))
            {
                TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
                TTI_SETRWC(p_setrwc::CLR_AB, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);

                if constexpr (type == PoolType::MAX)
                {
                    TTI_GMPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0);
                }
                else
                {
                    if constexpr (HIGH_FIDELITY)
                    {
                        ckernel_template::run();
                    }
                    else
                    {
                        TTI_GAPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0);
                    }
                }
            }
            // Reset Dest Counter
            TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_AD);
        }
    }
    else if constexpr (dim == ReduceDim::REDUCE_SCALAR)
    {
        for (int tile = 0; tile < 3; tile++)
        {
            // Wait and pool
            if constexpr (type == PoolType::MAX)
            {
                TTI_GMPOOL(p_setrwc::CLR_AB, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 4);
            }
            else
            {
                if constexpr (HIGH_FIDELITY)
                {
                    ckernel_template::run();
                    TTI_CLEARDVALID(p_setrwc::CLR_AB, 0);
                }
                else
                {
                    TTI_GAPOOL(p_setrwc::CLR_AB, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 4);
                }
            }
        }
        // Wait and pool
        if constexpr (type == PoolType::MAX)
        {
            TTI_GMPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 4);
        }
        else
        {
            if constexpr (HIGH_FIDELITY)
            {
                ckernel_template::run();
            }
            else
            {
                TTI_GAPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 4);
            }
        }

        // Need row in dest as column in src A
        TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 0, 0, 0, p_setrwc::SET_AB);
        // copy over from dest to B and do transpose
        // use rows 16 - 31 in src B as scratch
        TTI_MOVD2B(0, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 4);
        TTI_GATESRCRST(0b1, 0b1);
        TTI_TRNSPSRCB;
        // gate math instructions until src B has been updated
        TTI_GATESRCRST(0b1, 0b1);
        // copy over all 16 rows from B to A
        TTI_MOVB2A(p_movb2a::SRCA_ZERO_OFFSET + 0, ADDR_MOD_0, p_movb2a::MOV_4_ROWS, p_movb2a::SRCB_ROW16_OFFSET + 0);
        TTI_MOVB2A(p_movb2a::SRCA_ZERO_OFFSET + 4, ADDR_MOD_0, p_movb2a::MOV_4_ROWS, p_movb2a::SRCB_ROW16_OFFSET + 4);
        TTI_MOVB2A(p_movb2a::SRCA_ZERO_OFFSET + 8, ADDR_MOD_0, p_movb2a::MOV_4_ROWS, p_movb2a::SRCB_ROW16_OFFSET + 8);
        TTI_MOVB2A(p_movb2a::SRCA_ZERO_OFFSET + 12, ADDR_MOD_0, p_movb2a::MOV_4_ROWS, p_movb2a::SRCB_ROW16_OFFSET + 12);
        // gate math instructions until src A has been updated by MOV instructions
        TTI_GATESRCRST(0b1, 0b1);
        // zero out scratch in dest
        TTI_ZEROACC(p_zeroacc::CLR_SPECIFIC, ADDR_MOD_0, 4);

        if constexpr (type == PoolType::MAX)
        {
            TTI_GMPOOL(p_setrwc::CLR_AB, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0);
        }
        else
        {
            if constexpr (HIGH_FIDELITY)
            {
                for (int i = 0; i < MATH_FIDELITY_PHASES - 1; i++)
                {
                    TTI_GAPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_3, p_gpool::INDEX_DIS, 0);
                }
            }
            TTI_GAPOOL(p_setrwc::CLR_AB, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0);
        }
    }
}

template <PoolType type, int MATH_FIDELITY_DESC>
inline void reduce_configure_addrmod()
{
    constexpr int NUM_FIDELITY_PHASES = get_math_num_fidelity_phases(MATH_FIDELITY_DESC);
    constexpr int FIDELITY_INCREMENT  = get_math_fidelity_increment(MATH_FIDELITY_DESC);
    constexpr bool HIGH_FIDELITY      = NUM_FIDELITY_PHASES > 0;

    addr_mod_t {.srca = {.incr = 0}, .srcb = {.incr = 0}, .dest = {.incr = 0}, .fidelity = {.incr = 0, .clr = 1}}.set(ADDR_MOD_0);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 1},
        .dest = {.incr = 1},
    }
        .set(ADDR_MOD_1);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 8},
        .dest = {.incr = 8},
    }
        .set(ADDR_MOD_2);

    if constexpr (HIGH_FIDELITY)
    {
        addr_mod_t {.srca = {.incr = 0}, .srcb = {.incr = 0}, .dest = {.incr = 0}, .fidelity = {.incr = FIDELITY_INCREMENT}}.set(ADDR_MOD_3);
    }
}

template <ReduceDim dim, int num_fidelity_phases>
inline void reduce_configure_mop()
{
    if constexpr (dim == ReduceDim::REDUCE_SCALAR)
    {
        ckernel_template tmp(1, num_fidelity_phases, TT_OP_GAPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_3, p_gpool::INDEX_DIS, 4));
        tmp.set_last_inner_loop_instr(TT_OP_GAPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 4));
        tmp.set_last_outer_loop_instr(TT_OP_GAPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 4));
        tmp.program();
    }
    else
    {
        ckernel_template tmp(1, num_fidelity_phases, TT_OP_GAPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_3, p_gpool::INDEX_DIS, 0));
        tmp.set_last_inner_loop_instr(TT_OP_GAPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0));
        tmp.set_last_outer_loop_instr(TT_OP_GAPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_0, p_gpool::INDEX_DIS, 0));
        tmp.program();
    }
}

/**
 * Configures address modifiers for specialized reduce_max_row operations.
 *
 * This function works with the following assumptions:
 * - Scaler values are 1.0 and are contained inside F0 of the scaler tile
 * - The scaler doesn't change for the duration of the whole block/tile operation
 * - Operand and scaler data format is bfloat16_b
 * - Operand tile size is 32x32
 * - Can work on both 16-bit or 32-bit DEST register modes based on is_fp32_dest_acc_en flag
 * - Does only MAX pool on ROW dimension
 *
 * This function should NOT be used as a substitute for native reduce LLK configuration.
 * Use the standard reduce configuration functions for general-purpose reduction operations.
 */
inline void reduce_max_row_configure_addrmod()
{
    // ADDR_MOD_0: Default mode used with pool operations (GMPOOL/GAPOOL)
    // No auto-increment on any counter - keeps all address pointers at their current positions
    // Used for initial pooling operations where we want explicit control over counter advancement
    addr_mod_t {
        .srca     = {.incr = 0, .clr = 0, .cr = 0},
        .srcb     = {.incr = 0, .clr = 0, .cr = 0},
        .dest     = {.incr = 0, .clr = 0, .cr = 0},
        .fidelity = {.incr = 0, .clr = 1}}
        .set(ADDR_MOD_0);

    // ADDR_MOD_1: Face-to-face advancement mode used with GMPOOL operations
    // Increments SrcA by 16 rows to advance to the next face (F0->F1, F2->F3)
    // Clears SrcB counter to prepare for subsequent MOVD2B operations that write transposed results
    // This allows processing pairs of faces (F0+F1, then F2+F3) efficiently
    addr_mod_t {
        .srca = {.incr = 16, .clr = 0, .cr = 1},
        .srcb = {.incr = 0, .clr = 1, .cr = 0},
        .dest = {.incr = 0, .clr = 0, .cr = 0},
    }
        .set(ADDR_MOD_1);

    // ADDR_MOD_2: Sequential 4-row write mode used with MOVB2D operations
    // Increments DEST by 4 rows after each operation (writes to rows 0, 4, 8, 12 within a face)
    // This distributes the transposed 1x16 reduction result across the first face (rows 0-15)
    // by writing 4 rows to every 4th row, matching the tile face layout
    addr_mod_t {
        .srca = {.incr = 0, .clr = 0, .cr = 0},
        .srcb = {.incr = 0, .clr = 0, .cr = 0},
        .dest = {.incr = 4, .clr = 0, .cr = 1},
    }
        .set(ADDR_MOD_2);

    // ADDR_MOD_3: Face-boundary jump mode used with final MOVB2D in each face
    // Increments DEST by 20 rows, jumping from row 12 to row 32 (= 12 + 4 + 16)
    // This transitions from the last row of the current face to the first row of the next face
    // Example: After writing rows 0,4,8,12 in F0, this jumps to row 32 to start F2
    addr_mod_t {
        .srca = {.incr = 0, .clr = 0, .cr = 0},
        .srcb = {.incr = 0, .clr = 0, .cr = 0},
        .dest = {.incr = 20, .clr = 0, .cr = 1},
    }
        .set(ADDR_MOD_3);
}

/**
 * Performs specialized reduce_max_row operation on a single tile.
 *
 * This function works with the following assumptions:
 * - Scaler values are 1.0 and are contained inside F0 of the scaler tile
 * - The scaler doesn't change for the duration of the whole tile operation
 * - Operand and scaler data format is bfloat16_b
 * - Operand tile size is 32x32
 * - Can work on both 16-bit or 32-bit DEST register modes based on is_fp32_dest_acc_en flag
 * - Does only MAX pool on ROW dimension
 *
 * This function should NOT be used as a substitute for the native _llk_math_reduce_ LLK.
 * Use the standard _llk_math_reduce_<PoolType::MAX, ReduceDim::REDUCE_ROW>() for general-purpose reduction.
 */
inline void _llk_math_reduce_max_row_(const uint dst_index)
{
    math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(dst_index);

    // The unpacker has already transposed each face in SrcA so we can pool them
    // Pool the first 16x16 face (F0). This reduces it to a 1x16 row stored in DEST row 0.
    // ADDR_MOD_1 advances the SrcA counter by 16 to point to the next face (F1)
    TTI_GMPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_1, p_gpool::INDEX_DIS, 0);
    // Pool the second face (F1). This operation accumulates with the previous result in DEST row 0.
    // Now DEST row 0 contains the combined results from both F0 and F1 (still 16 values total).
    // ADDR_MOD_1 also clears the SrcB counter to prepare for the upcoming MOVD2B operation.
    TTI_GMPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_1, p_gpool::INDEX_DIS, 0);

    // Now we need to transpose the pooled results and write them back to DEST properly
    // Move DEST row 0 to SrcB at offset 16 (so it goes to SrcB rows 16-31)
    TTI_MOVD2B(0, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);
    // Transpose the data in SrcB (note: transpose only works on rows 16-31)
    TTI_TRNSPSRCB;
    // Move the row again to keep the data available for multi-tile reduction operations
    TTI_MOVD2B(0, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);

    // Copy the transposed results from SrcB back to DEST in 4-row chunks
    // ADDR_MOD_2 auto-increments the DEST counter by 4 each time, so we write to rows 0, 4, 8, 12
    // This explains why we always specify dest index 0 - it's actually writing to 0, 4, 8, 12
    // For the last chunk, ADDR_MOD_3 increments by 20 instead of 4, jumping from row 12 to row 32
    // This positions us perfectly to start writing F2 and F3 results in the bottom half of the tile
    TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET, ADDR_MOD_2, p_movb2d::MOV_4_ROWS, 0);
    TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET + 4, ADDR_MOD_2, p_movb2d::MOV_4_ROWS, 0);
    TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET + 8, ADDR_MOD_2, p_movb2d::MOV_4_ROWS, 0);
    TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET + 12, ADDR_MOD_3, p_movb2d::MOV_4_ROWS, 0);

    // Process the bottom two faces (F2 and F3) using the same approach

    // Pool face F2. Result goes to DEST row 32.
    TTI_GMPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_1, p_gpool::INDEX_DIS, 0);
    // Pool face F3. This accumulates with F2 in DEST row 32.
    // ADDR_MOD_1 clears the SrcB counter again for the next MOVD2B.
    TTI_GMPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_1, p_gpool::INDEX_DIS, 0);

    // Transpose and write back F2+F3 results the same way we did for F0+F1
    // Move DEST row 32 to SrcB at offset 16
    TTI_MOVD2B(0, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);
    // Transpose the data in SrcB rows 16-31
    TTI_TRNSPSRCB;
    // Move again for multi-tile reduction support
    TTI_MOVD2B(0, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);

    // Copy transposed F2+F3 results to DEST rows 16-31 in 4-row chunks
    // ADDR_MOD_0 doesn't auto-increment, so we manually specify each destination offset
    TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 0);
    TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET + 4, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 4);
    TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET + 8, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 8);
    TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET + 12, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 12);

    // Clear all counters so everything is ready for the next tile
    TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_ABD);
}

template <PoolType type, ReduceDim dim, bool is_fp32_dest_acc_en, int MATH_FIDELITY_DESC = 0, bool enforce_fp32_accumulation = false>
inline void _llk_math_reduce_init_([[maybe_unused]] const std::uint32_t within_face_16x16_transpose = 0)
{ // within_face_16x16_transpose used for unpack, ignored by math

    constexpr int MATH_FIDELITY_PHASES = get_math_num_fidelity_phases(MATH_FIDELITY_DESC);
    constexpr bool HIGH_FIDELITY       = MATH_FIDELITY_PHASES > 0;

    reduce_configure_addrmod<type, MATH_FIDELITY_DESC>();
    if constexpr (HIGH_FIDELITY)
    {
        reduce_configure_mop<dim, MATH_FIDELITY_PHASES>();
    }

    if constexpr (enforce_fp32_accumulation)
    {
        static_assert(is_fp32_dest_acc_en, "FP32 Dest must be enabled for FP32 accumulation");
        // MOVB2D/D2B depends on SrcA ALU Format - Hi/Lo16 does not work with Tf32 (only on WH)
        // This is needed because FP32 data from L1 that is unpacked to Src registers is reduced to Tf32
        cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_RMW>((uint)DataFormat::Float32);
    }
    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);

    math::reset_counters(p_setrwc::SET_ABD_F);
}

/**
 * Initializes specialized reduce_max_row operation for single tile processing.
 *
 * This function works with the following assumptions:
 * - Scaler values are 1.0 and are contained inside F0 of the scaler tile
 * - The scaler doesn't change for the duration of the whole tile operation
 * - Operand and scaler data format is bfloat16_b
 * - Operand tile size is 32x32
 * - Can work on both 16-bit or 32-bit DEST register modes based on is_fp32_dest_acc_en flag
 * - Does only MAX pool on ROW dimension
 *
 * This function should NOT be used as a substitute for the native _llk_math_reduce_init_ LLK.
 * Use the standard _llk_math_reduce_init_<PoolType::MAX, ReduceDim::REDUCE_ROW>() for general-purpose reduction.
 */
template <bool is_fp32_dest_acc_en = false>
inline void _llk_math_reduce_max_row_init_()
{
    reduce_max_row_configure_addrmod();

    if constexpr (is_fp32_dest_acc_en)
    {
        cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_RMW>((uint)DataFormat::Float32);
    }

    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);

    math::reset_counters(p_setrwc::SET_ABD_F);
}

/**
 * Configures MOP (Macro Operation) for block-based reduce_max_row operations.
 *
 * This function works with the following assumptions:
 * - Scaler values are 1.0 and are contained inside F0 of the scaler tile
 * - The scaler doesn't change for the duration of the whole block operation
 * - Operand and scaler data format is bfloat16_b
 * - Operand tile size is 32x32
 * - Can work on both 16-bit or 32-bit DEST register modes based on is_fp32_dest_acc_en flag
 * - Does only MAX pool on ROW dimension
 *
 * This function should NOT be used as a substitute for native reduce LLK MOP configuration.
 * Use the standard reduce MOP configuration with _llk_math_reduce_init_ for general-purpose reduction.
 */
template <uint32_t block_ct_dim, bool is_fp32_dest_acc_en = false>
inline void _llk_math_reduce_block_max_row_mop_config_()
{
    // Constraint on the outerloop and innerloop dim
    static_assert(block_ct_dim < 128, "block_ct_dim must be less than 128");

    // See _llk_math_reduce_max_row_ for a full algorithm explanation
    // Put the following 15 instructions in a REPLAY buffer
    lltt::record(0, 15);

    // Two GMPOOLs to pool F0 and F1 (or F2 and F3) together
    TTI_GMPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_1, p_gpool::INDEX_DIS, 0);
    TTI_GMPOOL(p_setrwc::CLR_NONE, p_gpool::DIM_16X16, ADDR_MOD_1, p_gpool::INDEX_DIS, 0);

    if constexpr (is_fp32_dest_acc_en)
    {
        // FP32 destination mode, need to move high and low 16 bits to SrcB and transpose separately
        constexpr int dest_32b_hi = 0;
        constexpr int dest_32b_lo = 1;

        // The following instructions are repeated for F0&F1 reduced and F2&F3 reduced
        // Move high 16 bits from DEST row 0 to SrcB rows 16 - 31 and transpose
        TTI_MOVD2B(dest_32b_hi, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);
        TTI_TRNSPSRCB;

        // Move high 16 bits back to Dest
        TTI_MOVB2D(dest_32b_hi, p_movb2d::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 0);
        TTI_MOVB2D(dest_32b_hi, p_movb2d::SRC_ROW16_OFFSET + 4, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 4);
        TTI_MOVB2D(dest_32b_hi, p_movb2d::SRC_ROW16_OFFSET + 8, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 8);
        TTI_MOVB2D(dest_32b_hi, p_movb2d::SRC_ROW16_OFFSET + 12, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 12);

        // Move low 16 bits to SrcB rows 16 - 31 and transpose
        TTI_MOVD2B(dest_32b_lo, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);
        TTI_TRNSPSRCB;

        // Move low 16 bits from SrcB rows 16 - 31 to DEST rows 0, 4, 8, 12
        // ADDR_MOD_2 increments CR_D and Dest counter val by 4, so that's why DEST location is '0', not '0, 4, 8, 12'.
        TTI_MOVB2D(dest_32b_lo, p_movb2d::SRC_ROW16_OFFSET, ADDR_MOD_2, p_movb2d::MOV_4_ROWS, 0);
        TTI_MOVB2D(dest_32b_lo, p_movb2d::SRC_ROW16_OFFSET + 4, ADDR_MOD_2, p_movb2d::MOV_4_ROWS, 0);
        TTI_MOVB2D(dest_32b_lo, p_movb2d::SRC_ROW16_OFFSET + 8, ADDR_MOD_2, p_movb2d::MOV_4_ROWS, 0);
        // ADDR_MOD_3 increments CR_D and Dest counter val by 20, to point to F2.
        TTI_MOVB2D(dest_32b_lo, p_movb2d::SRC_ROW16_OFFSET + 12, ADDR_MOD_3, p_movb2d::MOV_4_ROWS, 0);
        // Clear B valid bits at the end and all address counters
        TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_ABD);
    }
    else
    {
        // The following instructions are going to transpose the whole tile, unlike the FP32 mode.

        // Move row 0 from DEST to SrcB with offset of 16 rows and transpose
        TTI_MOVD2B(0, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);
        TTI_TRNSPSRCB;
        // Move row 0 from SrcB to DEST in 4-row chunks
        // ADDR_MOD_2 increments CR_D and Dest counter val by 4, so that's why DEST location is '0', not '0, 4, 8, 12'.
        TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET, ADDR_MOD_2, p_movb2d::MOV_4_ROWS, 0);
        TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET + 4, ADDR_MOD_2, p_movb2d::MOV_4_ROWS, 0);
        TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET + 8, ADDR_MOD_2, p_movb2d::MOV_4_ROWS, 0);
        // ADDR_MOD_3 increments CR_D and Dest counter val by 20, to point to F2.
        TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET + 12, ADDR_MOD_3, p_movb2d::MOV_4_ROWS, 0);

        // Move row 32 (F2R0) from DEST to SrcB with offset of 16 rows and transpose
        TTI_MOVD2B(0, p_movd2b::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movd2b::MOV_1_ROW, 0);
        TTI_TRNSPSRCB;
        // Move row 32 from SrcB to DEST in 4-row chunks
        TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 0);
        TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET + 4, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 4);
        TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET + 8, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 8);
        TTI_MOVB2D(0, p_movb2d::SRC_ROW16_OFFSET + 12, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 12);

        // Clear B valid bits at the end and all address counters
        TTI_SETRWC(p_setrwc::CLR_B, 0, 0, 0, 0, p_setrwc::SET_ABD);
    }

    static constexpr uint outer_loop = block_ct_dim;
    static constexpr uint inner_loop = 4;

    // Reduce F0 and F1 column-wise, doesn't change DEST counter
    static constexpr uint start_op = TT_OP_REPLAY(0, 2, 0, 0);
    // Increment DEST counter by 32 to point to F2 (DEST row 32)
    static constexpr uint inner_loop_op = TT_OP_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
    // Reduce F2 and F3 column-wise, doesn't change DEST counter, but clears A valid bits at the end
    static constexpr uint end_op_1 = TT_OP_REPLAY(0, 2, 0, 0);
    // Clear A valid bit to get another operand tile (scaler face stays the same)
    static constexpr uint end_op_2 = TT_OP_SETRWC(p_setrwc::CLR_A, 0, 0, 0, 0, p_setrwc::SET_ABD);

    ckernel::ckernel_template mop_template(outer_loop, inner_loop, inner_loop_op);
    mop_template.set_start_op(start_op);
    mop_template.set_end_ops(end_op_1, end_op_2);
    mop_template.program();
}

/**
 * Initializes block-based reduce_max_row operation for processing multiple tiles.
 *
 * This function should NOT be used as a substitute for the native _llk_math_reduce_init_ LLK.
 * Use the standard _llk_math_reduce_init_<PoolType::MAX, ReduceDim::REDUCE_ROW>() with multiple
 * _llk_math_reduce_() calls in a loop for general-purpose block reduction.
 */
template <uint32_t block_ct_dim, bool is_fp32_dest_acc_en = false>
inline void _llk_math_reduce_block_max_row_init_()
{
    reduce_max_row_configure_addrmod();

    if constexpr (is_fp32_dest_acc_en)
    {
        // MOVB2D/D2B depends on SrcA ALU Format - Hi/Lo16 does not work with Tf32 (only on WH)
        // This is needed because FP32 data from L1 that is unpacked to Src registers is reduced to Tf32
        cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_RMW>((uint)DataFormat::Float32);
    }

    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);

    math::reset_counters(p_setrwc::SET_ABD_F);

    _llk_math_reduce_block_max_row_mop_config_<block_ct_dim, is_fp32_dest_acc_en>();
}

/**
 * Performs block-based reduce_max_row operation across multiple tiles in the width dimension.
 *
 * This function works with the following assumptions:
 * - Scaler values are 1.0 and are contained inside F0 of the scaler tile
 * - The scaler doesn't change for the duration of the whole block operation
 * - Operand and scaler data format is bfloat16_b
 * - Operand tile size is 32x32
 * - Can work on both 16-bit or 32-bit DEST register modes based on is_fp32_dest_acc_en flag
 * - Does only MAX pool on ROW dimension
 *
 * This function should NOT be used as a substitute for the native _llk_math_reduce_ LLK.
 * Use the standard _llk_math_reduce_<PoolType::MAX, ReduceDim::REDUCE_ROW>() in a loop
 * for general-purpose block reduction across multiple tiles.
 */
template <uint32_t block_ct_dim, bool is_fp32_dest_acc_en = false>
inline void _llk_math_reduce_block_max_row_(const uint dst_index)
{
    math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(dst_index);

    if constexpr (is_fp32_dest_acc_en)
    {
        // Run the MOP, performing a column reduce across all 4 faces
        ckernel::ckernel_template::run();
        // Replay the 12 instructions to transpose the reduced F0&F1 results
        lltt::replay(2, 12);
        // Replay the 13 instructions to transpose the reduced F2&F3 results
        // 13th instruction clears B valid bit to release SrcB bank and clears all address counters
        lltt::replay(2, 13);
    }
    else
    {
        // Run the MOP, performing a column reduce across all 4 faces
        ckernel::ckernel_template::run();
        // Replay the 13 instructions to transpose the reduced results
        lltt::replay(2, 13);
    }
}
