// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "cmath_common.h"
#include "llk_defs.h"
using namespace ckernel;
using namespace ckernel::trisc;
using namespace ckernel::math;

/**
 * @brief Sets up ALU formats for math destination register
 * @tparam implied_math_format: If set to true, will imply math dest format
 * from SrcA reg format
 * @tparam fp32_math_format: Set to true to use math dest in Float32
 * otherwise default behaviour is Float16/Float16_b depending on input
 * format exponent width
 * @tparam int32_math_format: Set to true to use math dest in Int32
 * otherwise default behaviour is Float16/Float16_b depending on input
 * format exponent width
 * @tparam srca_format: Input srcA format, used to set ALU configs if not implied math format
 * values = Dataformat enum, ex: <Float16/Float16_b/Tf32/Int8/Int16/UInt8>
 * @tparam srcb_format: Input srcB format, used to set ALU configs if not implied math format
 * values = Dataformat enum, ex: <Float16/Float16_b/Tf32/Int8/Int16/UInt8>
 */
inline void _llk_math_srcAB_hw_configure_(
    bool implied_math_format, bool fp32_math_format, bool int32_math_format, DataFormat srca_format, DataFormat srcb_format)
{
    // Turn on automatic Tensix-TRISC synchronization
    // RT: This is turned on by default by HW, this should be removed
    set_ttsync_enables<TRACK_ALL>(TRISC_ID);

    // LLK_ASSERT(!(fp32_math_format && int32_math_format), "Cannot have Int32 dest & Float32 dest at the same time");

    // Check valid integer conversions
    // if (int32_math_format)
    // {
    //     static_assert(
    //         (SRCA_FORMAT == DataFormat::Int8 && SRCB_FORMAT == DataFormat::Int8) || (SRCA_FORMAT == DataFormat::Uint8 && SRCB_FORMAT == DataFormat::Uint8) ||
    //             (SRCA_FORMAT == DataFormat::Int16 && SRCB_FORMAT == DataFormat::Int16),
    //         "Cannot have Int32 Destination register + non-integer source formats");
    // }

    // Set implied math dest format mode
    cfg[DISABLE_IMPLIED_SRCA_FMT_SEC0_Base_ADDR32 + TRISC_ID] = !implied_math_format;

    uint8_t srca_format_masked = static_cast<uint8_t>(srca_format) & 0xFF;
    uint8_t srcb_format_masked = static_cast<uint8_t>(srcb_format) & 0xFF;

    alu_config_u alu_config;
    for (uint32_t i = 0; i < NUM_WORDS_ALU_FORMAT; i++)
    {
        alu_config.val[i] = 0;
    }

    if (!implied_math_format)
    {
        // Set ALU SrcA format since it is not implied
        // If input format has exp_width == 5, the math dest set to Float16
        // else input format has exp_width == 8, the math dest set to Float16_b
        alu_config.f.ALU_FORMAT_SPEC_REG_SrcA_val      = srca_format_masked;
        alu_config.f.ALU_FORMAT_SPEC_REG_SrcA_override = 0x1;
        alu_config.f.ALU_FORMAT_SPEC_REG_SrcB_val      = srcb_format_masked;
        alu_config.f.ALU_FORMAT_SPEC_REG_SrcB_override = 0x1;

        // RT: Since SrcA & SrcB need to match exponent widths, can set them the same for now
        // Check with HW team if different mixes between Src registers are allowed
        alu_config.f.ALU_FORMAT_SPEC_REG0_SrcA = srca_format_masked;
        alu_config.f.ALU_FORMAT_SPEC_REG1_SrcB = srcb_format_masked;
    }

    alu_config.f.ALU_ACC_CTRL_Fp32_enabled      = fp32_math_format;
    alu_config.f.ALU_ACC_CTRL_SFPU_Fp32_enabled = fp32_math_format;
    alu_config.f.ALU_ACC_CTRL_INT8_math_enabled = int32_math_format;

    for (uint32_t i = 0; i < NUM_WORDS_ALU_FORMAT; i++)
    {
        cfg[ALU_FORMAT_SPEC_REG_SrcA_val_ADDR32 + i] = alu_config.val[i];
    }
}

/**
 * @brief Sets up ALU formats for math destination register, specifically for upk to dest
 * @tparam EN_IMPLIED_MATH_FORMAT: If set to true, will imply math dest format
 * from SrcA reg format
 * @tparam EN_FP32_MATH_FORMAT: Set to true to use math dest in Float32
 * otherwise default behaviour is Float16/Float16_b depending on input
 * format exponent width
 * @tparam EN_INT32_MATH_FORMAT: Set to true to use math dest in Int32
 * otherwise default behaviour is Float16/Float16_b depending on input
 * format exponent width
 */
template <bool EN_IMPLIED_MATH_FORMAT, bool EN_FP32_MATH_FORMAT, bool EN_INT32_MATH_FORMAT>
inline void _llk_math_upk_to_dest_hw_configure_()
{
    // Set implied math dest format mode
    cfg[DISABLE_IMPLIED_SRCA_FMT_SEC0_Base_ADDR32 + TRISC_ID] = !EN_IMPLIED_MATH_FORMAT;

    alu_config_u alu_config;
    for (uint32_t i = 0; i < NUM_WORDS_ALU_FORMAT; i++)
    {
        alu_config.val[i] = 0;
    }

    // Program DEST fmt
    alu_config.f.ALU_ACC_CTRL_Fp32_enabled      = EN_FP32_MATH_FORMAT;
    alu_config.f.ALU_ACC_CTRL_SFPU_Fp32_enabled = EN_FP32_MATH_FORMAT;
    alu_config.f.ALU_ACC_CTRL_INT8_math_enabled = EN_INT32_MATH_FORMAT;
}

/**
 * @brief Sets the dest dvalid for a FPU/SFPU
 * @tparam SET_DEST_DVALID: which client to set data valid for, values = <p_cleardvalid::FPU/SFPU>
 **/
template <uint8_t SET_DEST_DVALID>
inline void _llk_math_set_dvalid_()
{
    static_assert(SET_DEST_DVALID == p_cleardvalid::FPU || SET_DEST_DVALID == p_cleardvalid::SFPU, "Can only set dest dvalid for FPU and SFPU");

    TTI_STALLWAIT(p_stall::STALL_MATH, 0, 0, p_stall::WAIT_SFPU);
    TTI_CLEARDVALID(0, 0, 0, 0, SET_DEST_DVALID, 0);
}

/**
 * All the following functions are added to enable Math <-> Pack synchronization
 * on destination register due to dest dvalid issue:
 * The following functions should be removed once the above issue is resolved
 */
template <DstSync DST>
inline void _llk_math_pack_sync_init_()
{
    static_assert(DST == DstSync::SyncFull || DST == DstSync::SyncHalf, "Only Dest Sync Half and Full are supported");

    // Wait for previous packs to finish before claiming all dest
    while (semaphore_read(semaphore::MATH_PACK) > 0)
    {
    };

    _reset_dest_bank_id_();
    _set_dest_section_base_<TRISC_ID>(_get_dest_buffer_base_());

    constexpr uint32_t num_sem = (DST == DstSync::SyncFull) ? 1 : 2;
    TTI_SEMINIT(num_sem, 0, 0, p_stall::SEMAPHORE_1);
}

inline void _llk_math_wait_for_dest_available_()
{
    TTI_SEMWAIT(p_stall::STALL_MATH | p_stall::STALL_SFPU | p_stall::STALL_SYNC, p_stall::STALL_ON_MAX, 0, semaphore::t6_sem(semaphore::MATH_PACK));
}

template <DstSync DST>
inline void _llk_math_dest_section_done_()
{
    t6_semaphore_post<p_stall::MATH, p_stall::WAIT_SFPU>(semaphore::MATH_PACK);
    if constexpr (DST == DstSync::SyncHalf)
    {
        _update_dest_bank_id_();
        uint base_addr = _get_dest_buffer_base_();
        TTI_STALLWAIT(p_stall::STALL_CFG, 0, p_stall::MATH, p_stall::WAIT_SFPU);
        _set_dest_section_base_<TRISC_ID>(base_addr);
    }
}
