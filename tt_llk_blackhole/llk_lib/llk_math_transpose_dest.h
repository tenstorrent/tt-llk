// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
// SPDX-FileCopyrightText: © 2025 Jason Davies <jason@jasondavies.com>
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
template <bool is_32bit>
inline void transpose_dest_configure_addrmod();
template <bool transpose_of_faces, bool is_32bit>
inline void transpose_dest_configure_mop();

// Inline helper: transpose one 16x16 face of 32-bit data in Dst using SrcA format switching.
// Performs two passes (hi16 then lo16) through SrcB[16:31] with TRNSPSRCB.
// On entry Fp32_enabled must be 1 (for Dst32b reads). On exit Fp32_enabled is 1.
// The last MOVB2D uses addr_mod_last (ADDR_MOD_0 to advance dest, or ADDR_MOD_1 to stay).
inline void transpose_dest_face_32b(std::uint32_t addr_mod_last)
{
    // --- Pass 1: hi16 ---
    cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_RMW>(to_underlying(DataFormat::Tf32));

    TTI_MOVD2B(0, 16, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 0);
    TTI_MOVD2B(0, 20, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 4);
    TTI_MOVD2B(0, 24, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 8);
    TTI_MOVD2B(0, 28, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 12);

    TTI_TRNSPSRCB;

    TTI_MOVB2D(0, 16, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 0);
    TTI_MOVB2D(0, 20, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 4);
    TTI_MOVB2D(0, 24, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 8);
    TTI_MOVB2D(0, 28, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 12);

    // --- Pass 2: lo16 ---
    cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_RMW>(to_underlying(DataFormat::Float16_b));

    TTI_MOVD2B(1, 16, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 0);
    TTI_MOVD2B(1, 20, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 4);
    TTI_MOVD2B(1, 24, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 8);
    TTI_MOVD2B(1, 28, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 12);

    TTI_TRNSPSRCB;

    // lo16 writeback: Fp32_enabled=0 + SrcA=Float32 → writes lo16, preserves hi16
    cfg_reg_rmw_tensix<ALU_ACC_CTRL_Fp32_enabled_RMW>(0);
    cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_RMW>(to_underlying(DataFormat::Float32));

    TTI_MOVB2D(1, 16, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 0);
    TTI_MOVB2D(1, 20, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 4);
    TTI_MOVB2D(1, 24, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 8);
    TTI_MOVB2D(1, 28, addr_mod_last, p_movb2d::MOV_4_ROWS, 12);

    // Restore Fp32_enabled=1 for subsequent 32-bit Dst access
    cfg_reg_rmw_tensix<ALU_ACC_CTRL_Fp32_enabled_RMW>(1);
}

// Inline helper: swap 4 rows between Face 1 and Face 2 for 32-bit data.
// face1_row / face2_row are 10-bit DstRow offsets from the current dest counter.
// Uses SrcB[16:19] and SrcB[24:27] for hi16, SrcB[20:23] and SrcB[28:31] for lo16.
// On entry/exit Fp32_enabled is 1.
inline void swap_face_rows_32b(std::uint32_t face1_row, std::uint32_t face2_row)
{
    // Read hi16 from both faces (SrcA=Tf32, Fp32_enabled=1)
    cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_RMW>(to_underlying(DataFormat::Tf32));

    TTI_MOVD2B(0, 16, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, face1_row);
    TTI_MOVD2B(0, 24, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, face2_row);

    // Read lo16 from both faces (SrcA=Float16_b)
    cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_RMW>(to_underlying(DataFormat::Float16_b));

    TTI_MOVD2B(1, 20, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, face1_row);
    TTI_MOVD2B(1, 28, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, face2_row);

    // Write hi16 swapped (SrcA=Tf32): Face2→Face1, Face1→Face2
    cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_RMW>(to_underlying(DataFormat::Tf32));

    TTI_MOVB2D(0, 24, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, face1_row);
    TTI_MOVB2D(0, 16, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, face2_row);

    // Write lo16 swapped (Fp32_enabled=0, SrcA=Float32): preserves hi16
    cfg_reg_rmw_tensix<ALU_ACC_CTRL_Fp32_enabled_RMW>(0);
    cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_RMW>(to_underlying(DataFormat::Float32));

    TTI_MOVB2D(1, 28, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, face1_row);
    TTI_MOVB2D(1, 20, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, face2_row);

    // Restore Fp32_enabled=1
    cfg_reg_rmw_tensix<ALU_ACC_CTRL_Fp32_enabled_RMW>(1);
}

// Notes on these template parameters:
// 1. <transpose_of_faces=false, is_32bit=false>: not supported.
// 2. <transpose_of_faces=false, is_32bit=true>: 4x 16x16 face transpose; can be combined with _llk_unpack_A_ with transpose_of_faces=true.
// 3. <transpose_of_faces=true, is_32bit=false>: the default case (full 32x32 tile transpose, non-32-bit).
// 4. <transpose_of_faces=true, is_32bit=true>: full 32x32 tile transpose for 32-bit.
//
// We may want to revisit these template parameters, and perhaps the
// transpose_dest API generally as it's not currently widely used:
// https://github.com/tenstorrent/tt-llk/issues/290
template <bool is_fp32_dest_acc_en, bool transpose_of_faces = true, bool is_32bit = false>
inline void _llk_math_transpose_dest_(const std::uint32_t dst_index)
{
    math::set_dst_write_addr<DstTileShape::Tile32x32, UnpackDestination::SrcRegs>(dst_index);
    math::reset_counters(p_setrwc::SET_ABD_F);

    // Wait condition SRCA_VLD is required as MOVB2A doesn't automatically wait
    // for SrcA[MatrixUnit.SrcABank].AllowedClient == SrcClient::MatrixUnit.
    // Wait condition SRCB_VLD is required as MOVD2B doesn't automatically wait
    // for SrcB[MatrixUnit.SrcBBank].AllowedClient == SrcClient::MatrixUnit.
    TTI_STALLWAIT(p_stall::STALL_MATH, p_stall::WAIT_SFPU | p_stall::SRCA_VLD | p_stall::SRCB_VLD);

    if constexpr (is_32bit)
    {
        // Disable implied SrcA format inference so manual SrcA format switches take effect.
        // On Blackhole the ALU format is normally inferred; we must disable this for
        // the SrcA format switching approach to control MOVD2B/MOVB2D behavior.
        TTI_SETC16(DISABLE_IMPLIED_SRCA_FMT_Base_ADDR32, 1);

        // Disable zero flag to prevent mantissa flushing when exponent bits are 0.
        cfg_reg_rmw_tensix<ALU_ACC_CTRL_Zero_Flag_disabled_src_RMW>(1);

        // Enable Fp32 for 32-bit Dst access (UseDst32b=true in MOVD2B/MOVB2D).
        cfg_reg_rmw_tensix<ALU_ACC_CTRL_Fp32_enabled_RMW>(1);

        // Within-face 16x16 transpose for each of the 4 faces.
        // Each call advances the dest counter by 16 via ADDR_MOD_0 on the last instruction.
        transpose_dest_face_32b(ADDR_MOD_0);
        transpose_dest_face_32b(ADDR_MOD_0);
        transpose_dest_face_32b(ADDR_MOD_0);
        transpose_dest_face_32b(ADDR_MOD_0);

        if constexpr (transpose_of_faces)
        {
            // Swap Face 1 (Dst rows 16-31) and Face 2 (Dst rows 32-47).
            // After 4 face transposes, dest counter is at +64 from tile base.
            // Use 10-bit negative offsets to address the faces.
            swap_face_rows_32b(0x3ff & (-48 + 0), 0x3ff & (-32 + 0));
            swap_face_rows_32b(0x3ff & (-48 + 4), 0x3ff & (-32 + 4));
            swap_face_rows_32b(0x3ff & (-48 + 8), 0x3ff & (-32 + 8));
            swap_face_rows_32b(0x3ff & (-48 + 12), 0x3ff & (-32 + 12));
        }

        // Restore config state
        TTI_SETC16(DISABLE_IMPLIED_SRCA_FMT_Base_ADDR32, 0);
        if constexpr (!is_fp32_dest_acc_en)
        {
            cfg_reg_rmw_tensix<ALU_ACC_CTRL_Fp32_enabled_RMW>(0);
        }
        cfg_reg_rmw_tensix<ALU_ACC_CTRL_Zero_Flag_disabled_src_RMW>(0);
    }
    else
    {
        ckernel_unpack_template::run(2, 2);
    }

    TTI_SETRWC(p_setrwc::CLR_AB, 0, 0, 0, 0, p_setrwc::SET_ABD);
}

template <bool is_32bit>
inline void transpose_dest_configure_addrmod()
{
    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 16},
    }
        .set(ADDR_MOD_0);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_1);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = is_32bit ? 2 : 0x3ff & -16},
    }
        .set(ADDR_MOD_2);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 32},
    }
        .set(ADDR_MOD_3);
}

template <bool transpose_of_faces, bool is_32bit>
inline void transpose_dest_configure_mop()
{
    if constexpr (!is_32bit)
    {
        load_replay_buf(
            16,
            15,
            []
            {
                // ABCD
                TTI_MOVB2A(0, ADDR_MOD_1, p_movb2a::MOV_4_ROWS, 16);
                TTI_MOVB2A(4, ADDR_MOD_1, p_movb2a::MOV_4_ROWS, 20);
                TTI_MOVB2A(8, ADDR_MOD_1, p_movb2a::MOV_4_ROWS, 24);
                TTI_MOVB2A(12, ADDR_MOD_0, p_movb2a::MOV_4_ROWS, 28); // dst += 16

                // EFGHIJKLM
                TTI_MOVD2B(0, 16, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 0);
                TTI_MOVD2B(0, 20, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 4);
                TTI_MOVD2B(0, 24, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 8);
                TTI_MOVD2B(0, 28, ADDR_MOD_1, p_movd2b::MOV_4_ROWS, 12);
                TTI_TRNSPSRCB;
                TTI_MOVB2D(0, 16, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 0);
                TTI_MOVB2D(0, 20, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 4);
                TTI_MOVB2D(0, 24, ADDR_MOD_1, p_movb2d::MOV_4_ROWS, 8);
                TTI_MOVB2D(0, 28, ADDR_MOD_0, p_movb2d::MOV_4_ROWS, 12); // dst += 16

                // NO
                // Note: the 0x3ff mask ensures the negative offsets are 10 bits.
                TTI_MOVA2D(0, 0, ADDR_MOD_2, p_mova2d::MOV_8_ROWS, 0x3ff & (0 - 32)); // dst -= 16
                TTI_MOVA2D(0, 8, ADDR_MOD_2, p_mova2d::MOV_8_ROWS, 0x3ff & (8 - 16)); // dst -= 16
            });

        // The next 7 instructions expand as follows:
        // Face 0: 4x MOVD2B, TRNSPSRCB, 4x MOVB2D, dst += 16 (EFGHIJKLM)
        // Face 1: 4x MOVD2B, TRNSPSRCB, 4x MOVB2A, dst += 16 (EFGHI, ABCD..)
        // Face 2: 4x MOVD2B (dst -= 16), TRNSPSRCB, 4x MOVB2D, dst += 32 (..EFG, P, IJKL, Q)
        // Face 3: 4x MOVD2B, TRNSPSRCB, 4x MOVB2D, dst += 16 (EFGHIJKLM..)
        // Face 1: 2x MOVA2D (2x dst -= 16) (..NO)

        std::uint32_t EFGHIJKLM   = lltt::replay_insn(20, 9);
        std::uint32_t EFGHI       = lltt::replay_insn(20, 5);
        std::uint32_t ABCDEFG     = lltt::replay_insn(16, 7);
        std::uint32_t P           = TT_OP_MOVD2B(0, 28, ADDR_MOD_2, p_movd2b::MOV_4_ROWS, 12); // dst -= 16
        std::uint32_t IJKL        = lltt::replay_insn(24, 4);
        std::uint32_t Q           = TT_OP_MOVB2D(0, 28, ADDR_MOD_3, p_movb2d::MOV_4_ROWS, 12); // dst += 32
        std::uint32_t EFGHIJKLMNO = lltt::replay_insn(20, 11);

        // The following MOP config simply runs the above 7 instructions in order (when executed with zmask 0b10):
        ckernel_unpack_template tmp(true, true, EFGHIJKLM, EFGHI, ABCDEFG, P, /* skip A */ Q, /* B */ IJKL, /* skip B */ EFGHIJKLMNO);
        tmp.program();
    }
}

template <bool transpose_of_faces = true, bool is_32bit = false>
inline void _llk_math_transpose_dest_init_()
{
    transpose_dest_configure_addrmod<is_32bit>();
    transpose_dest_configure_mop<transpose_of_faces, is_32bit>();

    TTI_SETC16(CLR_DVALID_SrcA_Disable_ADDR32, 0);
}

inline void _llk_math_transpose_dest_uninit_()
{
    // No state to restore - all states are transient or default
}
