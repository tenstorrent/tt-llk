// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifdef PERF_DUMP
#include "perf_res_decouple.h"
#endif

// Hand-coded parameter encoding for various common instructions
namespace ckernel {

struct p_setrwc {
#ifdef PERF_DUMP

#if SKIP_UNP == 1
    constexpr static std::uint32_tCLR_A  = 0x0;
    constexpr static std::uint32_tCLR_B  = 0x0;
    constexpr static std::uint32_tCLR_AB = 0x0;
#else
    constexpr static std::uint32_tCLR_A  = 0x1;
    constexpr static std::uint32_tCLR_B  = 0x2;
    constexpr static std::uint32_tCLR_AB = 0x3;
#endif

#else
    constexpr static std::uint32_tCLR_A  = 0x1;
    constexpr static std::uint32_tCLR_B  = 0x2;
    constexpr static std::uint32_tCLR_AB = 0x3;
#endif
    constexpr static std::uint32_tCLR_NONE = 0x0;

    constexpr static std::uint32_tSET_A     = 0x1;
    constexpr static std::uint32_tSET_B     = 0x2;
    constexpr static std::uint32_tSET_AB    = 0x3;
    constexpr static std::uint32_tSET_D     = 0x4;
    constexpr static std::uint32_tSET_AD    = 0x5;
    constexpr static std::uint32_tSET_BD    = 0x6;
    constexpr static std::uint32_tSET_ABD   = 0x7;
    constexpr static std::uint32_tSET_F     = 0x8;
    constexpr static std::uint32_tSET_A_F   = 0x9;
    constexpr static std::uint32_tSET_B_F   = 0xa;
    constexpr static std::uint32_tSET_AB_F  = 0xb;
    constexpr static std::uint32_tSET_D_F   = 0xc;
    constexpr static std::uint32_tSET_AD_F  = 0xd;
    constexpr static std::uint32_tSET_BD_F  = 0xe;
    constexpr static std::uint32_tSET_ABD_F = 0xf;

    constexpr static std::uint32_tCR_A         = 0x1;
    constexpr static std::uint32_tCR_B         = 0x2;
    constexpr static std::uint32_tCR_AB        = 0x3;
    constexpr static std::uint32_tCR_D         = 0x4;
    constexpr static std::uint32_tCR_AD        = 0x5;
    constexpr static std::uint32_tCR_BD        = 0x6;
    constexpr static std::uint32_tCR_ABD       = 0x7;
    constexpr static std::uint32_tC_TO_CR_MODE = 0x8;
};

struct p_unpacr {
    constexpr static std::uint32_tRAREFYB_DISABLE = 0x0;
    constexpr static std::uint32_tRAREFYB_ENABLE  = 0x1;

    constexpr static std::uint32_tTILE0_ADDRCNT_CONTEXT = (0); // Address counter context for tile 0
    constexpr static std::uint32_tTILE1_ADDRCNT_CONTEXT = (0); // Address counter context for tile 1
    constexpr static std::uint32_tTILE2_ADDRCNT_CONTEXT = (1); // Address counter context for tile 2
    constexpr static std::uint32_tTILE3_ADDRCNT_CONTEXT = (1); // Address counter context for tile 3
    constexpr static std::uint32_tTILE0_CFG_CONTEXT     = (0); // Config context for tile 0
    constexpr static std::uint32_tTILE1_CFG_CONTEXT     = (0); // Config context for tile 1
    constexpr static std::uint32_tTILE2_CFG_CONTEXT     = (0); // Config context for tile 2
    constexpr static std::uint32_tTILE3_CFG_CONTEXT     = (0); // Config context for tile 3
    constexpr static std::uint32_tAUTO_INC_CONTEXT =
        (1); // Auto increment config context (max value set through unpacker config command)

    constexpr static std::uint32_tUNP_POP             = 0x0;
    constexpr static std::uint32_tUNP_ZEROSRC         = 0x1;
    constexpr static std::uint32_tUNP_NOP             = 0x2;
    constexpr static std::uint32_tUNP_NEGINFSRC       = 0x5;
    constexpr static std::uint32_tUNP_RESET_ALL_BANKS = 0x8;
};

struct p_srcb {
    constexpr static std::uint32_tFORWARD_PASS  = 0x0;
    constexpr static std::uint32_tBACKWARD_PASS = 0x1;
};

struct p_setadc {
    constexpr static std::uint32_tUNP0   = 0b001;
    constexpr static std::uint32_tUNP1   = 0b010;
    constexpr static std::uint32_tUNP_A  = 0b001;
    constexpr static std::uint32_tUNP_B  = 0b010;
    constexpr static std::uint32_tUNP_AB = 0b011;
    constexpr static std::uint32_tPAC    = 0b100;

    constexpr static std::uint32_tSET_X = 0;
    constexpr static std::uint32_tSET_Y = 1;
    constexpr static std::uint32_tSET_Z = 2;
    constexpr static std::uint32_tSET_W = 3;

    constexpr static std::uint32_tCH_0 = 0;
    constexpr static std::uint32_tCH_1 = 1;
};

struct p_pacr {
    constexpr static std::uint32_tP_ZERO_OUTPUT_DISABLED = 0x0;
    constexpr static std::uint32_tP_ZERO_OUTPUT_ENABLED  = 0x1;
};

struct p_ind {
    constexpr static std::uint32_tHIER_REGFILE = 0x0;
    constexpr static std::uint32_tHIER_L1      = 0x1;

    constexpr static std::uint32_tINC_NONE = 0x0;
    constexpr static std::uint32_tINC_2B   = 0x1;
    constexpr static std::uint32_tINC_4B   = 0x2;
    constexpr static std::uint32_tINC_16B  = 0x3;

    constexpr static std::uint32_tLD_16B   = 0;
    constexpr static std::uint32_tLD_32bit = 1;
    constexpr static std::uint32_tLD_16bit = 2;
    constexpr static std::uint32_tLD_8bit  = 3;
};

struct p_mova2d {
    constexpr static std::uint32_tMOV_1_ROW  = 0x0;
    constexpr static std::uint32_tMOV_8_ROWS = 0x2;
};

struct p_movd2a {
    constexpr static std::uint32_tMOV_1_ROW  = 0x0;
    constexpr static std::uint32_tMOV_4_ROWS = 0x2;
};

struct p_movb2d {
    constexpr static std::uint32_tMOV_1_ROW                = 0x0;
    constexpr static std::uint32_tMOV_1_ROW_D0_BRCST       = 0x1;
    constexpr static std::uint32_tMOV_8_ROW_BRCST          = 0x2;
    constexpr static std::uint32_tMOV_8_ROW_BRCST_D0_BRCST = 0x3;
    constexpr static std::uint32_tMOV_4_ROWS               = 0x4;
    constexpr static std::uint32_tMOV_4_ROWS_D0_BRCST      = 0x5;
};

struct p_stall {
    // What to stall on
    constexpr static std::uint32_tNONE    = 0x0;
    constexpr static std::uint32_tTHCON   = 0x1;
    constexpr static std::uint32_tUNPACK0 = 0x2;
    constexpr static std::uint32_tUNPACK1 = 0x4;
    constexpr static std::uint32_tUNPACK  = UNPACK0 | UNPACK1;
    constexpr static std::uint32_tPACK0   = 0x8;
    constexpr static std::uint32_tPACK1   = 0x10;
    constexpr static std::uint32_tPACK2   = 0x20;
    constexpr static std::uint32_tPACK3   = 0x40;
    constexpr static std::uint32_tPACK    = PACK0 | PACK1 | PACK2 | PACK3;
    constexpr static std::uint32_tMATH    = 0x80;
    // constexpr static std::uint32_tSEM_ZERO    = 0x20;
    // constexpr static std::uint32_tSEM_MAX     = 0x40;
    constexpr static std::uint32_tSRCA_CLR  = 0x100;
    constexpr static std::uint32_tSRCB_CLR  = 0x200;
    constexpr static std::uint32_tSRCA_VLD  = 0x400;
    constexpr static std::uint32_tSRCB_VLD  = 0x800;
    constexpr static std::uint32_tXMOV      = 0x1000;
    constexpr static std::uint32_tTRISC_CFG = 0x2000;

    // What to stall
    constexpr static std::uint32_tSTALL_TDMA    = 0x1;
    constexpr static std::uint32_tSTALL_SYNC    = 0x2;
    constexpr static std::uint32_tSTALL_PACK    = 0x4;
    constexpr static std::uint32_tSTALL_UNPACK  = 0x8;
    constexpr static std::uint32_tSTALL_XSEARCH = 0x10;
    constexpr static std::uint32_tSTALL_XMOV    = 0x20;
    constexpr static std::uint32_tSTALL_THCON   = 0x40;
    constexpr static std::uint32_tSTALL_MATH    = 0x80;
    constexpr static std::uint32_tSTALL_CFG     = 0x100;
    constexpr static std::uint32_tSTALL_THREAD  = 0x1ff;

    constexpr static std::uint32_tSTALL_ON_ZERO = 0x1;
    constexpr static std::uint32_tSTALL_ON_MAX  = 0x2;

    constexpr static std::uint32_tSEMAPHORE_0 = 0x1;
    constexpr static std::uint32_tSEMAPHORE_1 = 0x2;
    constexpr static std::uint32_tSEMAPHORE_2 = 0x4;
    constexpr static std::uint32_tSEMAPHORE_3 = 0x8;
    constexpr static std::uint32_tSEMAPHORE_4 = 0x10;
    constexpr static std::uint32_tSEMAPHORE_5 = 0x20;
    constexpr static std::uint32_tSEMAPHORE_6 = 0x40;
    constexpr static std::uint32_tSEMAPHORE_7 = 0x80;
};

struct p_zeroacc {
    constexpr static std::uint32_tCLR_SPECIFIC = 0x0;
    constexpr static std::uint32_tCLR_16       = 0x1;
    constexpr static std::uint32_tCLR_HALF     = 0x2;
    constexpr static std::uint32_tCLR_ALL      = 0x3;
};

struct p_zerosrc {
    constexpr static std::uint32_tCLR_A  = 0x1;
    constexpr static std::uint32_tCLR_B  = 0x2;
    constexpr static std::uint32_tCLR_AB = 0x3;
};

struct p_shiftx {
    constexpr static std::uint32_tSHIFT_1 = 0x0;
    constexpr static std::uint32_tSHIFT_2 = 0x1;
    constexpr static std::uint32_tSHIFT_4 = 0x2;
    constexpr static std::uint32_tSHIFT_8 = 0x3;

    constexpr static std::uint32_tCOMBINED     = 0x0;
    constexpr static std::uint32_tROTATE       = 0x1;
    constexpr static std::uint32_tINDEPENDANT  = 0x2;
    constexpr static std::uint32_tHALOIZE_MODE = 0x3;
};

struct p_cfg {
    constexpr static std::uint32_tWRCFG_128b = 0x1;
    constexpr static std::uint32_tWRCFG_32b  = 0x0;
};

struct p_alu {
    constexpr static std::uint32_tAND = 0x0;
    constexpr static std::uint32_tOR  = 0x1;
    constexpr static std::uint32_tXOR = 0x2;
};

struct p_gpool {
    constexpr static std::uint32_tDIM_1X16  = 0x0;
    constexpr static std::uint32_tDIM_16X16 = 0x1;
};

struct p_elwise {
    constexpr static std::uint32_tSRCB_NO_BCAST  = 0x0;
    constexpr static std::uint32_tSRCB_BCAST_COL = 0x1;
    constexpr static std::uint32_tSRCB_BCAST_ROW = 0x2;
    constexpr static std::uint32_tSRCB_BCAST_ALL = 0x3;
};

struct p_sfpu {
    constexpr static std::uint32_tLREG0              = 0;
    constexpr static std::uint32_tLREG1              = 1;
    constexpr static std::uint32_tLREG2              = 2;
    constexpr static std::uint32_tLREG3              = 3;
    constexpr static std::uint32_tLCONST_0           = 4;
    constexpr static std::uint32_tLCONST_ln2         = 5;
    constexpr static std::uint32_tLCONST_ln2_recip   = 7;
    constexpr static std::uint32_tLCONST_neg_point_5 = 9;
    constexpr static std::uint32_tLCONST_1           = 10;
    constexpr static std::uint32_tLCONST_neg1        = 11;
    constexpr static std::uint32_tkCONST_1_FP16B     = 0x3F80;
    constexpr static std::uint32_tkCONST_1_FP16A     = 0x3C00;
    constexpr static std::uint32_tkCONST_0           = 0x0000;
    constexpr static std::uint32_tkCONST_Exp_8Bit    = 0;
    constexpr static std::uint32_tkCONST_Exp_5Bit    = 1;
};

struct p_exp {
    constexpr static std::uint32_tFRAC_BITS = 3;
    constexpr static std::uint32_tC23_73    = 0x4340; // Based on FRAC_BITS
    // ADJ_EXP = -0x4300 + 0x003F
    //  0x4300 : 0100 0011 0000 0000
    //  0x003F : 0000 0000 0011 1111
    // -0x4300 : 1011 1101 0000 0000
    // ADJ_EXP : 1011 1101 0011 1111 (-0x4300 + 0x003F = 0xBD3F)
    constexpr static std::uint32_tADJ_EXP = 0xBD3F;
};

} // namespace ckernel
