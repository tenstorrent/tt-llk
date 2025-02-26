// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_ops.h"

namespace ckernel {

constexpr std::uint8_t ADDR_MOD_0 = 0;
constexpr std::uint8_t ADDR_MOD_1 = 1;
constexpr std::uint8_t ADDR_MOD_2 = 2;
constexpr std::uint8_t ADDR_MOD_3 = 3;
constexpr std::uint8_t ADDR_MOD_4 = 4;
constexpr std::uint8_t ADDR_MOD_5 = 5;
constexpr std::uint8_t ADDR_MOD_6 = 6;
constexpr std::uint8_t ADDR_MOD_7 = 7;

constexpr std::uint32_t SRC_INCR_MASK  = 0x3F;
constexpr std::uint32_t DEST_INCR_MASK = 0x3FF;

// TODO: these are defined in tt_metal/hw/inc/<arch>/cfg_defines.h, we need to think of a way to include them

constexpr std::uint32_t __METAL_ADDR_MOD_AB_SEC0_SrcAIncr_ADDR32 = 12;
constexpr std::uint32_t __METAL_ADDR_MOD_AB_SEC1_SrcAIncr_ADDR32 = 13;
constexpr std::uint32_t __METAL_ADDR_MOD_AB_SEC2_SrcAIncr_ADDR32 = 14;
constexpr std::uint32_t __METAL_ADDR_MOD_AB_SEC3_SrcAIncr_ADDR32 = 15;
constexpr std::uint32_t __METAL_ADDR_MOD_AB_SEC4_SrcAIncr_ADDR32 = 16;
constexpr std::uint32_t __METAL_ADDR_MOD_AB_SEC5_SrcAIncr_ADDR32 = 17;
constexpr std::uint32_t __METAL_ADDR_MOD_AB_SEC6_SrcAIncr_ADDR32 = 18;
constexpr std::uint32_t __METAL_ADDR_MOD_AB_SEC7_SrcAIncr_ADDR32 = 19;

constexpr std::uint32_t __METAL_ADDR_MOD_DST_SEC0_DestIncr_ADDR32 = 28;
constexpr std::uint32_t __METAL_ADDR_MOD_DST_SEC1_DestIncr_ADDR32 = 29;
constexpr std::uint32_t __METAL_ADDR_MOD_DST_SEC2_DestIncr_ADDR32 = 30;
constexpr std::uint32_t __METAL_ADDR_MOD_DST_SEC3_DestIncr_ADDR32 = 31;
constexpr std::uint32_t __METAL_ADDR_MOD_DST_SEC4_DestIncr_ADDR32 = 32;
constexpr std::uint32_t __METAL_ADDR_MOD_DST_SEC5_DestIncr_ADDR32 = 33;
constexpr std::uint32_t __METAL_ADDR_MOD_DST_SEC6_DestIncr_ADDR32 = 34;
constexpr std::uint32_t __METAL_ADDR_MOD_DST_SEC7_DestIncr_ADDR32 = 35;

constexpr std::uint32_t __METAL_ADDR_MOD_PACK_SEC0_YsrcIncr_ADDR32 = 37;
constexpr std::uint32_t __METAL_ADDR_MOD_PACK_SEC1_YsrcIncr_ADDR32 = 38;
constexpr std::uint32_t __METAL_ADDR_MOD_PACK_SEC2_YsrcIncr_ADDR32 = 39;
constexpr std::uint32_t __METAL_ADDR_MOD_PACK_SEC3_YsrcIncr_ADDR32 = 40;

constexpr std::uint32_t __METAL_ADDR_MOD_BIAS_SEC0_BiasIncr_ADDR32 = 47;
constexpr std::uint32_t __METAL_ADDR_MOD_BIAS_SEC1_BiasIncr_ADDR32 = 48;
constexpr std::uint32_t __METAL_ADDR_MOD_BIAS_SEC2_BiasIncr_ADDR32 = 49;
constexpr std::uint32_t __METAL_ADDR_MOD_BIAS_SEC3_BiasIncr_ADDR32 = 50;
constexpr std::uint32_t __METAL_ADDR_MOD_BIAS_SEC4_BiasIncr_ADDR32 = 51;
constexpr std::uint32_t __METAL_ADDR_MOD_BIAS_SEC5_BiasIncr_ADDR32 = 52;
constexpr std::uint32_t __METAL_ADDR_MOD_BIAS_SEC6_BiasIncr_ADDR32 = 53;
constexpr std::uint32_t __METAL_ADDR_MOD_BIAS_SEC7_BiasIncr_ADDR32 = 54;

// FIXME: These should be updated from cfg_defines.h

struct addr_mod_t {
    // CLR, CR, INCR(4 bits)
    struct addr_mod_src_t {
        std::uint8_t incr = 0;
        std::uint8_t clr  = 0;
        std::uint8_t cr   = 0;

        constexpr std::uint8_t val() const { return (incr & SRC_INCR_MASK) | ((cr & 0x1) << 6) | ((clr & 0x1) << 7); }
    };

    // CLR, CR, INCR(8 bits)
    struct addr_mod_dest_t {
        std::int16_t incr    = 0;
        std::uint8_t clr     = 0;
        std::uint8_t cr      = 0;
        std::uint8_t c_to_cr = 0;

        constexpr std::uint16_t val() const {
            return (incr & DEST_INCR_MASK) | ((cr & 0x1) << 10) | ((clr & 0x1) << 11) | ((c_to_cr & 0x1) << 12);
        }
    };

    // CLR, INCT (2 bits)
    struct addr_mod_fidelity_t {
        std::uint8_t incr = 0;
        std::uint8_t clr  = 0;

        constexpr std::uint16_t val() const { return (incr & 0x3) | ((clr & 0x1) << 2); }
    };

    // CLR, INCT (4 bits)
    struct addr_mod_bias_t {
        std::uint8_t incr = 0;
        std::uint8_t clr  = 0;

        constexpr std::uint16_t val() const { return (incr & 0xF) | ((clr & 0x1) << 4); }
    };

    // Set defaults so that we can skip unchanged in initialization list
    addr_mod_src_t      srca      = {};
    addr_mod_src_t      srcb      = {};
    addr_mod_dest_t     dest      = {};
    addr_mod_fidelity_t fidelity  = {};
    addr_mod_bias_t     bias      = {};
    addr_mod_src_t      pack_ysrc = {};
    addr_mod_src_t      pack_ydst = {};

    // SrcA/B register is combination of A and B values
    constexpr std::uint16_t src_val() const { return srca.val() | (srcb.val() << 8); }

    constexpr std::uint16_t pack_val() const { return pack_ysrc.val() | (pack_ydst.val() << 6); }

    // List of addresses of src/dest reigsters
    constexpr static std::uint32_t addr_mod_src_reg_addr[] = {
        __METAL_ADDR_MOD_AB_SEC0_SrcAIncr_ADDR32,
        __METAL_ADDR_MOD_AB_SEC1_SrcAIncr_ADDR32,
        __METAL_ADDR_MOD_AB_SEC2_SrcAIncr_ADDR32,
        __METAL_ADDR_MOD_AB_SEC3_SrcAIncr_ADDR32,
        __METAL_ADDR_MOD_AB_SEC4_SrcAIncr_ADDR32,
        __METAL_ADDR_MOD_AB_SEC5_SrcAIncr_ADDR32,
        __METAL_ADDR_MOD_AB_SEC6_SrcAIncr_ADDR32,
        __METAL_ADDR_MOD_AB_SEC7_SrcAIncr_ADDR32};

    constexpr static std::uint32_t addr_mod_dest_reg_addr[] = {
        __METAL_ADDR_MOD_DST_SEC0_DestIncr_ADDR32,
        __METAL_ADDR_MOD_DST_SEC1_DestIncr_ADDR32,
        __METAL_ADDR_MOD_DST_SEC2_DestIncr_ADDR32,
        __METAL_ADDR_MOD_DST_SEC3_DestIncr_ADDR32,
        __METAL_ADDR_MOD_DST_SEC4_DestIncr_ADDR32,
        __METAL_ADDR_MOD_DST_SEC5_DestIncr_ADDR32,
        __METAL_ADDR_MOD_DST_SEC6_DestIncr_ADDR32,
        __METAL_ADDR_MOD_DST_SEC7_DestIncr_ADDR32};

    constexpr static std::uint32_t addr_mod_bias_reg_addr[] = {
        __METAL_ADDR_MOD_BIAS_SEC0_BiasIncr_ADDR32,
        __METAL_ADDR_MOD_BIAS_SEC1_BiasIncr_ADDR32,
        __METAL_ADDR_MOD_BIAS_SEC2_BiasIncr_ADDR32,
        __METAL_ADDR_MOD_BIAS_SEC3_BiasIncr_ADDR32,
        __METAL_ADDR_MOD_BIAS_SEC4_BiasIncr_ADDR32,
        __METAL_ADDR_MOD_BIAS_SEC5_BiasIncr_ADDR32,
        __METAL_ADDR_MOD_BIAS_SEC6_BiasIncr_ADDR32,
        __METAL_ADDR_MOD_BIAS_SEC7_BiasIncr_ADDR32};

    constexpr static std::uint32_t addr_mod_pack_reg_addr[] = {
        __METAL_ADDR_MOD_PACK_SEC0_YsrcIncr_ADDR32,
        __METAL_ADDR_MOD_PACK_SEC1_YsrcIncr_ADDR32,
        __METAL_ADDR_MOD_PACK_SEC2_YsrcIncr_ADDR32,
        __METAL_ADDR_MOD_PACK_SEC3_YsrcIncr_ADDR32};

    // Program source and dest registers
    __attribute__((always_inline)) inline void set(const std::uint8_t mod_index) const {
        // KCM - This gets around issue: error: impossible constraint in 'asm'
        // TTI_SETC16(addr_mod_src_reg_addr[mod_index], src_val());
        TTI_SETC16(addr_mod_src_reg_addr[mod_index], srca.val() | (srcb.val() << 8));
        TTI_SETC16(addr_mod_dest_reg_addr[mod_index], dest.val() | (fidelity.val() << 13));
        TTI_SETC16(addr_mod_bias_reg_addr[mod_index], bias.val());
    }
};

struct addr_mod_pack_t {
    // CLR, CR, INCR(4 bits)
    struct addr_mod_vals_t {
        std::uint8_t incr = 0;
        std::uint8_t clr  = 0;
        std::uint8_t cr   = 0;

        constexpr std::uint8_t val() const { return (incr & 0xF) | ((cr & 0x1) << 4) | ((clr & 0x1) << 5); }
    };

    struct addr_mod_reduced_t {
        std::uint8_t incr = 0;
        std::uint8_t clr  = 0;

        constexpr std::uint8_t val() const { return (incr & 0x1) | ((clr & 0x1) << 1); }
    };

    // Set defaults so that we can skip unchanged in initialization list
    addr_mod_vals_t    y_src = {};
    addr_mod_vals_t    y_dst = {};
    addr_mod_reduced_t z_src = {};
    addr_mod_reduced_t z_dst = {};

    __attribute__((always_inline)) inline constexpr std::uint16_t pack_val() const {
        return y_src.val() | (y_dst.val() << 6) | (z_src.val() << 12) | (z_dst.val() << 14);
    }

    // List of addresses of src/dest reigsters
    constexpr static std::uint32_t addr_mod_pack_reg_addr[] = {
        __METAL_ADDR_MOD_PACK_SEC0_YsrcIncr_ADDR32,
        __METAL_ADDR_MOD_PACK_SEC1_YsrcIncr_ADDR32,
        __METAL_ADDR_MOD_PACK_SEC2_YsrcIncr_ADDR32,
        __METAL_ADDR_MOD_PACK_SEC3_YsrcIncr_ADDR32};

    // Program source and dest registers
    __attribute__((always_inline)) inline void set(const std::uint8_t mod_index) const {
        TTI_SETC16(addr_mod_pack_reg_addr[mod_index], pack_val());
    }
};

} // namespace ckernel
