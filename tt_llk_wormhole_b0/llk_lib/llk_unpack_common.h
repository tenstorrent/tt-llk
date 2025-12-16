// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel.h"
#include "ckernel_defs.h"
#include "ckernel_instr_params.h"
#include "ckernel_ops.h"
#include "cunpack_common.h"
#include "llk_assert.h"

using namespace ckernel;
using namespace ckernel::unpacker;

// If `x` is the result of loading from memory, placing `consume_discard(x)` somewhere
// will ensure that code after `consume_discard(x)` doesn't start until the load is complete.
#define consume_discard(x) __asm volatile("andi x0, %0, 0" : : "r"((x)) : "memory")

// This function stores a value to memory, and then immediately reads it back.
// The load result will not be available until the store has completed.
// This will make sure any subsequent instruction will see the store as complete.
static inline __attribute__((always_inline)) uint32_t store_then_load(volatile uint32_t *addr, uint32_t to_store)
{
    uint32_t result;
    __asm volatile("sw %2, %1; lw %0, %1" : "=r"(result) : "m"(*addr), "r"(to_store));
    return result;
}

void _llk_zero_buffer_(const std::uint32_t base_address, const std::uint32_t size)
{
    TT_SETDMAREG(0, 0, 0, LO_16(p_gpr_unpack::OPERAND_OFFSET_ADDR));
    TT_SETDMAREG(0, 0, 0, HI_16(p_gpr_unpack::OPERAND_OFFSET_ADDR));

    TT_SETDMAREG(0, LOWER_HALFWORD(base_address), 0, LO_16(p_gpr_unpack::p_gpr_unpack::OPERAND_BASE_ADDR));
    TT_SETDMAREG(0, UPPER_HALFWORD(base_address), 0, HI_16(p_gpr_unpack::p_gpr_unpack::OPERAND_BASE_ADDR));

    for (std::uint32_t i = 0; i < size; i++)
    {
        TTI_STOREIND(1, 0, p_ind::LD_16B, LO_16(p_gpr_unpack::OPERAND_OFFSET_ADDR), p_ind::INC_16B, p_gpr_unpack::ZERO_0, p_gpr_unpack::OPERAND_BASE_ADDR);
    }
}

template <bool is_fp32_dest_acc_en>
inline void _llk_unpack_hw_configure_(
    const std::uint32_t unpA_src_format,
    const std::uint32_t unpB_src_format,
    const std::uint32_t unpA_dst_format,
    const std::uint32_t unpB_dst_format,
    const std::uint32_t unpA_face_r_dim,
    const std::uint32_t unpB_face_r_dim,
    const std::uint32_t unpA_num_faces,
    const std::uint32_t unpB_num_faces)
{
    LLK_ASSERT(unpA_num_faces == 1 || unpA_num_faces == 2 || unpA_num_faces == 4, "unpA_num_faces must be 1, 2, or 4");
    LLK_ASSERT(unpB_num_faces == 1 || unpB_num_faces == 2 || unpB_num_faces == 4, "unpB_num_faces must be 1, 2, or 4");
    configure_unpack_AB<is_fp32_dest_acc_en, false, false, false>(
        unpA_src_format, unpB_src_format, unpA_dst_format, unpB_dst_format, unpA_face_r_dim, unpB_face_r_dim, 0, unpA_num_faces, unpB_num_faces);
}

template <StochRndType stoch_rnd_mode>
inline void _llk_unpack_configure_stoch_rnd_()
{
    constexpr uint alu_stoch_rnd_mask = ALU_ROUNDING_MODE_Fpu_srnd_en_MASK | ALU_ROUNDING_MODE_Gasket_srnd_en_MASK | ALU_ROUNDING_MODE_Packer_srnd_en_MASK;
    constexpr bool fpu_srnd_en        = (stoch_rnd_mode == StochRndType::All) || (stoch_rnd_mode == StochRndType::Fpu);
    constexpr bool pack_srnd_en       = (stoch_rnd_mode == StochRndType::All) || (stoch_rnd_mode == StochRndType::Pack);
    alu_config_u alu_payload          = {.val = 0};
    alu_payload.f.ALU_ROUNDING_MODE_Fpu_srnd_en    = fpu_srnd_en;
    alu_payload.f.ALU_ROUNDING_MODE_Gasket_srnd_en = pack_srnd_en;
    alu_payload.f.ALU_ROUNDING_MODE_Packer_srnd_en = pack_srnd_en;
    cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcA_ADDR32, 0, alu_stoch_rnd_mask>(alu_payload.val);
}

inline void _llk_unpack_debug_dump_seek_(std::uint8_t offset)
{
    debug_dump_seek(offset);
}

// TODO NC: Clean up as the part of tt-metal#34499
template <bool is_fp32_dest_acc_en, bool to_from_int8 = false>
inline void _llk_unpack_reconfig_data_format_srca_impl_(
    const std::uint32_t unpack_src_format, const std::uint32_t unpack_dst_format, const std::uint32_t tile_size)
{
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::UNPACK0);
    if constexpr (to_from_int8)
    {
        static_assert(is_fp32_dest_acc_en, "Reconfiguring unpack to/from Int8 formats requires FP32 Dest mode enabled");
        cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcAUnsigned_RMW>(((uint)unpack_src_format == (uint)DataFormat::UInt8) ? 1 : 0);
    }
    cfg_reg_rmw_tensix<THCON_SEC0_REG0_TileDescriptor_ADDR32, 0, 0x0f>(unpack_src_format);
    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Out_data_format_RMW>(unpack_dst_format);
    TT_SETDMAREG(0, LOWER_HALFWORD(tile_size), 0, LO_16(p_gpr_unpack::TILE_SIZE_A)); // update gpr which holds tile size A
}

// TODO NC: Clean up as the part of tt-metal#34499
template <bool is_fp32_dest_acc_en, bool to_from_int8 = false>
inline void _llk_unpack_reconfig_data_format_srcb_impl_(
    const std::uint32_t unpack_src_format, const std::uint32_t unpack_dst_format, const std::uint32_t tile_size)
{
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::UNPACK1);
    if constexpr (to_from_int8)
    {
        static_assert(is_fp32_dest_acc_en, "Reconfiguring unpack to/from Int8 formats requires FP32 Dest mode enabled");
        cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG0_SrcBUnsigned_RMW>(((uint)unpack_src_format == (uint)DataFormat::UInt8) ? 1 : 0);
    }
    cfg_reg_rmw_tensix<THCON_SEC1_REG0_TileDescriptor_ADDR32, 0, 0x0f>(unpack_src_format);
    cfg_reg_rmw_tensix<THCON_SEC1_REG2_Out_data_format_RMW>(unpack_dst_format);
    TT_SETDMAREG(0, LOWER_HALFWORD(tile_size), 0, LO_16(p_gpr_unpack::TILE_SIZE_B)); // update gpr which holds tile size B
}

inline void _llk_unpack_dbg_feature_disable_()
{
    reg_write(RISCV_DEBUG_REG_DBG_FEATURE_DISABLE, 1 << 11); // Set debug feature disable bit 11
                                                             // workaround for bug tenstorrent/budabackend#1372
}

inline void _llk_enable_int8_fpu_math_()
{
    enable_int8_fpu_math();
}

inline void _llk_unpack_set_srcb_dummy_valid_()
{
    TTI_STALLWAIT(p_stall::STALL_UNPACK, p_stall::UNPACK | p_stall::SRCA_CLR | p_stall::SRCB_CLR);
    TTI_UNPACR_NOP(SrcB, p_unpacr_nop::UNP_SET_DVALID);
    TTI_UNPACR_NOP(SrcA, p_unpacr_nop::UNP_SET_DVALID);
}
