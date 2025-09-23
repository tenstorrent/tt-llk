// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

// MT: This should be dissolved and moved to the appropriate place
#include "cfg_defines.h"
#include "ckernel_ops.h"

namespace ckernel
{

constexpr uint8_t ADDR_MOD_0 = 0;
constexpr uint8_t ADDR_MOD_1 = 1;
constexpr uint8_t ADDR_MOD_2 = 2;
constexpr uint8_t ADDR_MOD_3 = 3;
constexpr uint8_t ADDR_MOD_4 = 4;
constexpr uint8_t ADDR_MOD_5 = 5;
constexpr uint8_t ADDR_MOD_6 = 6;
constexpr uint8_t ADDR_MOD_7 = 7;

constexpr uint32_t SRC_INCR_MASK  = 0x3F;
constexpr uint32_t DEST_INCR_MASK = 0x3FF;

// FIXME: These should be updated from cfg_defines.h

struct addr_mod_t
{
    // CLR, CR, INCR(4 bits)
    struct addr_mod_src_t
    {
        uint8_t incr = 0;
        uint8_t clr  = 0;
        uint8_t cr   = 0;

        constexpr uint8_t val() const
        {
            return (incr & SRC_INCR_MASK) | ((cr & 0x1) << 6) | ((clr & 0x1) << 7);
        }
    };

    // CLR, CR, INCR(8 bits)
    struct addr_mod_dest_t
    {
        int16_t incr    = 0;
        uint8_t clr     = 0;
        uint8_t cr      = 0;
        uint8_t c_to_cr = 0;

        constexpr uint16_t val() const
        {
            return (incr & DEST_INCR_MASK) | ((cr & 0x1) << 10) | ((clr & 0x1) << 11) | ((c_to_cr & 0x1) << 12);
        }
    };

    // CLR, INCT (2 bits)
    struct addr_mod_fidelity_t
    {
        uint8_t incr = 0;
        uint8_t clr  = 0;

        constexpr uint16_t val() const
        {
            return (incr & 0x3) | ((clr & 0x1) << 2);
        }
    };

    // CLR, INCT (4 bits)
    struct addr_mod_bias_t
    {
        uint8_t incr = 0;
        uint8_t clr  = 0;

        constexpr uint16_t val() const
        {
            return (incr & 0xF) | ((clr & 0x1) << 4);
        }
    };

    // Set defaults so that we can skip unchanged in initialization list
    addr_mod_src_t srca          = {};
    addr_mod_src_t srcb          = {};
    addr_mod_dest_t dest         = {};
    addr_mod_fidelity_t fidelity = {};
    addr_mod_bias_t bias         = {};
    addr_mod_src_t pack_ysrc     = {};
    addr_mod_src_t pack_ydst     = {};

    // SrcA/B register is combination of A and B values
    constexpr uint16_t src_val() const
    {
        return srca.val() | (srcb.val() << 8);
    }

    constexpr uint16_t pack_val() const
    {
        return pack_ysrc.val() | (pack_ydst.val() << 6);
    }

    // List of addresses of src/dest registers
    constexpr static uint32_t addr_mod_src_reg_addr[] = {
        ADDR_MOD_AB_SEC0_SrcAIncr_ADDR32,
        ADDR_MOD_AB_SEC1_SrcAIncr_ADDR32,
        ADDR_MOD_AB_SEC2_SrcAIncr_ADDR32,
        ADDR_MOD_AB_SEC3_SrcAIncr_ADDR32,
        ADDR_MOD_AB_SEC4_SrcAIncr_ADDR32,
        ADDR_MOD_AB_SEC5_SrcAIncr_ADDR32,
        ADDR_MOD_AB_SEC6_SrcAIncr_ADDR32,
        ADDR_MOD_AB_SEC7_SrcAIncr_ADDR32};

    constexpr static uint32_t addr_mod_dest_reg_addr[] = {
        ADDR_MOD_DST_SEC0_DestIncr_ADDR32,
        ADDR_MOD_DST_SEC1_DestIncr_ADDR32,
        ADDR_MOD_DST_SEC2_DestIncr_ADDR32,
        ADDR_MOD_DST_SEC3_DestIncr_ADDR32,
        ADDR_MOD_DST_SEC4_DestIncr_ADDR32,
        ADDR_MOD_DST_SEC5_DestIncr_ADDR32,
        ADDR_MOD_DST_SEC6_DestIncr_ADDR32,
        ADDR_MOD_DST_SEC7_DestIncr_ADDR32};

    constexpr static uint32_t addr_mod_bias_reg_addr[] = {
        ADDR_MOD_BIAS_SEC0_BiasIncr_ADDR32,
        ADDR_MOD_BIAS_SEC1_BiasIncr_ADDR32,
        ADDR_MOD_BIAS_SEC2_BiasIncr_ADDR32,
        ADDR_MOD_BIAS_SEC3_BiasIncr_ADDR32,
        ADDR_MOD_BIAS_SEC4_BiasIncr_ADDR32,
        ADDR_MOD_BIAS_SEC5_BiasIncr_ADDR32,
        ADDR_MOD_BIAS_SEC6_BiasIncr_ADDR32,
        ADDR_MOD_BIAS_SEC7_BiasIncr_ADDR32};

    constexpr static uint32_t addr_mod_pack_reg_addr[] = {
        ADDR_MOD_PACK_SEC0_YsrcIncr_ADDR32, ADDR_MOD_PACK_SEC1_YsrcIncr_ADDR32, ADDR_MOD_PACK_SEC2_YsrcIncr_ADDR32, ADDR_MOD_PACK_SEC3_YsrcIncr_ADDR32};

    // Program source and dest registers
    __attribute__((always_inline)) inline void set(const uint8_t mod_index) const
    {
        // KCM - This gets around issue: error: impossible constraint in 'asm'
        // TTI_SETC16(addr_mod_src_reg_addr[mod_index], src_val());
        TTI_SETC16(addr_mod_src_reg_addr[mod_index], srca.val() | (srcb.val() << 8));
        TTI_SETC16(addr_mod_dest_reg_addr[mod_index], dest.val() | (fidelity.val() << 13));
        TTI_SETC16(addr_mod_bias_reg_addr[mod_index], bias.val());
    }
};

// Fluent builder for addr_mod_t - provides clear, explicit parameter setting
class addr_mod_builder
{
private:
    addr_mod_t mod {};

public:
    constexpr addr_mod_builder& srca_incr(uint8_t val)
    {
        mod.srca.incr = val;
        return *this;
    }

    constexpr addr_mod_builder& srca_clr(uint8_t val)
    {
        mod.srca.clr = val;
        return *this;
    }

    constexpr addr_mod_builder& srca_cr(uint8_t val)
    {
        mod.srca.cr = val;
        return *this;
    }

    constexpr addr_mod_builder& srcb_incr(uint8_t val)
    {
        mod.srcb.incr = val;
        return *this;
    }

    constexpr addr_mod_builder& srcb_clr(uint8_t val)
    {
        mod.srcb.clr = val;
        return *this;
    }

    constexpr addr_mod_builder& srcb_cr(uint8_t val)
    {
        mod.srcb.cr = val;
        return *this;
    }

    constexpr addr_mod_builder& dest_incr(int16_t val)
    {
        mod.dest.incr = val;
        return *this;
    }

    constexpr addr_mod_builder& dest_clr(uint8_t val)
    {
        mod.dest.clr = val;
        return *this;
    }

    constexpr addr_mod_builder& dest_cr(uint8_t val)
    {
        mod.dest.cr = val;
        return *this;
    }

    constexpr addr_mod_builder& dest_c_to_cr(uint8_t val)
    {
        mod.dest.c_to_cr = val;
        return *this;
    }

    constexpr addr_mod_builder& fidelity_incr(uint8_t val)
    {
        mod.fidelity.incr = val;
        return *this;
    }

    constexpr addr_mod_builder& fidelity_clr(uint8_t val)
    {
        mod.fidelity.clr = val;
        return *this;
    }

    constexpr addr_mod_builder& bias_incr(uint8_t val)
    {
        mod.bias.incr = val;
        return *this;
    }

    constexpr addr_mod_builder& bias_clr(uint8_t val)
    {
        mod.bias.clr = val;
        return *this;
    }

    constexpr addr_mod_builder& pack_ysrc_incr(uint8_t val)
    {
        mod.pack_ysrc.incr = val;
        return *this;
    }

    constexpr addr_mod_builder& pack_ysrc_clr(uint8_t val)
    {
        mod.pack_ysrc.clr = val;
        return *this;
    }

    constexpr addr_mod_builder& pack_ysrc_cr(uint8_t val)
    {
        mod.pack_ysrc.cr = val;
        return *this;
    }

    constexpr addr_mod_builder& pack_ydst_incr(uint8_t val)
    {
        mod.pack_ydst.incr = val;
        return *this;
    }

    constexpr addr_mod_builder& pack_ydst_clr(uint8_t val)
    {
        mod.pack_ydst.clr = val;
        return *this;
    }

    constexpr addr_mod_builder& pack_ydst_cr(uint8_t val)
    {
        mod.pack_ydst.cr = val;
        return *this;
    }

    constexpr addr_mod_t build() const
    {
        return mod;
    }

    static constexpr addr_mod_builder create()
    {
        return addr_mod_builder {};
    }
};

struct addr_mod_pack_t
{
    // CLR, CR, INCR(4 bits)
    struct addr_mod_vals_t
    {
        uint8_t incr = 0;
        uint8_t clr  = 0;
        uint8_t cr   = 0;

        constexpr uint8_t val() const
        {
            return (incr & 0xF) | ((cr & 0x1) << 4) | ((clr & 0x1) << 5);
        }
    };

    struct addr_mod_reduced_t
    {
        uint8_t incr = 0;
        uint8_t clr  = 0;

        constexpr uint8_t val() const
        {
            return (incr & 0x1) | ((clr & 0x1) << 1);
        }
    };

    // Set defaults so that we can skip unchanged in initialization list
    addr_mod_vals_t y_src    = {};
    addr_mod_vals_t y_dst    = {};
    addr_mod_reduced_t z_src = {};
    addr_mod_reduced_t z_dst = {};

    __attribute__((always_inline)) inline constexpr uint16_t pack_val() const
    {
        return y_src.val() | (y_dst.val() << 6) | (z_src.val() << 12) | (z_dst.val() << 14);
    }

    // List of addresses of src/dest registers
    constexpr static uint32_t addr_mod_pack_reg_addr[] = {
        ADDR_MOD_PACK_SEC0_YsrcIncr_ADDR32, ADDR_MOD_PACK_SEC1_YsrcIncr_ADDR32, ADDR_MOD_PACK_SEC2_YsrcIncr_ADDR32, ADDR_MOD_PACK_SEC3_YsrcIncr_ADDR32};

    // Program source and dest registers
    __attribute__((always_inline)) inline void set(const uint8_t mod_index) const
    {
        TTI_SETC16(addr_mod_pack_reg_addr[mod_index], pack_val());
    }
};

class addr_mod_pack_builder
{
private:
    addr_mod_pack_t mod {};

public:
    constexpr addr_mod_pack_builder& y_src_incr(uint8_t val)
    {
        mod.y_src.incr = val;
        return *this;
    }

    constexpr addr_mod_pack_builder& y_src_clr(uint8_t val)
    {
        mod.y_src.clr = val;
        return *this;
    }

    constexpr addr_mod_pack_builder& y_src_cr(uint8_t val)
    {
        mod.y_src.cr = val;
        return *this;
    }

    constexpr addr_mod_pack_builder& y_dst_incr(uint8_t val)
    {
        mod.y_dst.incr = val;
        return *this;
    }

    constexpr addr_mod_pack_builder& y_dst_clr(uint8_t val)
    {
        mod.y_dst.clr = val;
        return *this;
    }

    constexpr addr_mod_pack_builder& y_dst_cr(uint8_t val)
    {
        mod.y_dst.cr = val;
        return *this;
    }

    constexpr addr_mod_pack_builder& z_src_incr(uint8_t val)
    {
        mod.z_src.incr = val;
        return *this;
    }

    constexpr addr_mod_pack_builder& z_src_clr(uint8_t val)
    {
        mod.z_src.clr = val;
        return *this;
    }

    constexpr addr_mod_pack_builder& z_dst_incr(uint8_t val)
    {
        mod.z_dst.incr = val;
        return *this;
    }

    constexpr addr_mod_pack_builder& z_dst_clr(uint8_t val)
    {
        mod.z_dst.clr = val;
        return *this;
    }

    constexpr addr_mod_pack_t build() const
    {
        return mod;
    }

    static constexpr addr_mod_pack_builder create()
    {
        return addr_mod_pack_builder {};
    }
};

} // namespace ckernel
