// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

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

struct p_setibrwc {
    constexpr static std::uint32_tSET_BIAS = 0x0;
    constexpr static std::uint32_tINC_BIAS = 0x1;
    constexpr static std::uint32_tCR_NONE  = 0x0;
    constexpr static std::uint32_tCR_BIAS  = 0x1;
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

    constexpr static std::uint32_tUNP_POP           = 0x0;
    constexpr static std::uint32_tUNP_CLRSRC        = 0x1;
    constexpr static std::uint32_tUNP_NOP           = 0x2;
    constexpr static std::uint32_tUNP_POP_STREAM    = 0x3;
    constexpr static std::uint32_tUNP_CLRSRC_ZERO   = 0x0;
    constexpr static std::uint32_tUNP_CLRSRC_NEGINF = 0x1;
    constexpr static std::uint32_tUNP_CLRSRC_ONE    = 0x2;
    constexpr static std::uint32_tUNP_CLRSRC_IMM    = 0x3;

    constexpr static std::uint32_tUNP_CLRSRC_RESET_ALL_BANKS = 0x1;
    constexpr static std::uint32_tUNP_CLRSRC_ONE_FP16A       = 0x0;
    constexpr static std::uint32_tUNP_CLRSRC_ONE_FP16B       = 0x1;
    constexpr static std::uint32_tUNP_CLRSRC_ONE_TF32        = 0x1;
    constexpr static std::uint32_tUNP_CLRSRC_ONE_INT8        = 0x2;
};

// TODO: RT Review this struct, bits do not match for UNPACR_NOP
struct p_unpacr_nop {
    constexpr static std::uint32_tUNP_POP = 0b000;
    constexpr static std::uint32_tCLR_SRC = 0b01;
    constexpr static std::uint32_tUNP_NOP = 0b010;

    constexpr static std::uint32_tUNP_ZEROSRC   = 0b001;
    constexpr static std::uint32_tUNP_NEGINFSRC = 0b101;

    constexpr static std::uint32_tSET_DVALID = 0x1;

    constexpr static std::uint32_tUNP_ZEROSRC_RESET_ALL_BANKS    = 0b1001; // default is clear current bank
    constexpr static std::uint32_tUNP_ZEROSRC_STALL_RESET_WR_RDY = 0b10001;
    constexpr static std::uint32_tUNP_ZEROSRC_SET_DVALID         = 0b1000001;

    constexpr static std::uint32_tUNP0 = 0x0;
    constexpr static std::uint32_tUNP1 = 0x1;

    constexpr static std::uint32_tCLR_SRC_0      = 0b00;
    constexpr static std::uint32_tCLR_SRC_NEGINF = 0b01;
    constexpr static std::uint32_tCLR_SRC_1      = 0b10;
    constexpr static std::uint32_tCLR_SRC_IMM    = 0b11;
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

    constexpr static std::uint32_tDST_ACCESS_NORMAL_MODE  = 0b0;
    constexpr static std::uint32_tDST_ACCESS_STRIDED_MODE = 0b1;

    constexpr static std::uint32_tNO_ROW_PAD_ZERO                          = 0b000;
    constexpr static std::uint32_tROW_PAD_ZERO_ALL_PACR                    = 0b001;
    constexpr static std::uint32_tROW_PAD_ZERO_ALL_PACR_16DATUM_ALGN       = 0b101;
    constexpr static std::uint32_tROW_PAD_ZERO_NO_CONCAT_PACR              = 0b010;
    constexpr static std::uint32_tROW_PAD_ZERO_NO_CONCAT_PACR_16DATUM_ALGN = 0b110;
    constexpr static std::uint32_tROW_PAD_ZERO_LAST_PACR                   = 0b011;
    constexpr static std::uint32_tROW_PAD_ZERO_LAST_PACR_16DATUM_ALGN      = 0b111;

    constexpr static std::uint32_tCFG_CTXT_0 = 0b00;
    constexpr static std::uint32_tCFG_CTXT_1 = 0b01;
    constexpr static std::uint32_tCFG_CTXT_2 = 0b10;
    constexpr static std::uint32_tCFG_CTXT_3 = 0b11;

    constexpr static std::uint32_tADDR_CNT_CTXT_0 = 0b00;
    constexpr static std::uint32_tADDR_CNT_CTXT_1 = 0b01;
    constexpr static std::uint32_tADDR_CNT_CTXT_2 = 0b10;

    constexpr static std::uint32_tALL_INTF_ACTIVE          = 0b0000;
    constexpr static std::uint32_tALL_INTF_ACTIVE_ONES     = 0b1111;
    constexpr static std::uint32_tSINGLE_INTF_ACTIVE       = 0b0001;
    constexpr static std::uint32_tTWO_INTFS_ACTIVE         = 0b0011;
    constexpr static std::uint32_tTHREE_INTFS_ACTIVE       = 0b0111;
    constexpr static std::uint32_t_0th_AND_2nd_INTF_ACTIVE = 0b0101;
    constexpr static std::uint32_t_1st_AND_3rd_INTF_ACTIVE = 0b1010;

    constexpr static std::uint32_tZERO_WRITE    = 0b1;
    constexpr static std::uint32_tNO_ZERO_WRITE = 0b0;

    constexpr static std::uint32_tNO_CTXT_CTRL               = 0b00;
    constexpr static std::uint32_tRTL_FLOPS_CTXT_SEL         = 0b01;
    constexpr static std::uint32_tRTL_FLOPS_CTXT_RST_AND_NOP = 0b10;
    constexpr static std::uint32_tRTL_FLOPS_CTXT_SEL_NO_RST  = 0b11;
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
    constexpr static std::uint32_tMATH_HALO_ROWS = 0x0;
    constexpr static std::uint32_tMOV_1_ROW      = 0x0;
    constexpr static std::uint32_tMOV_8_ROWS     = 0x2;
};

struct p_movd2a {
    constexpr static std::uint32_tMOV_1_ROW  = 0x0;
    constexpr static std::uint32_tMOV_4_ROWS = 0x2;
};

struct p_movb2d {
    constexpr static std::uint32_tSRC_ZERO_OFFSET          = 0x0;
    constexpr static std::uint32_tSRC_ROW16_OFFSET         = 0x10;
    constexpr static std::uint32_tMOV_1_ROW                = 0x0;
    constexpr static std::uint32_tMOV_1_ROW_D0_BRCST       = 0x1;
    constexpr static std::uint32_tMOV_8_ROW_BRCST          = 0x2;
    constexpr static std::uint32_tMOV_8_ROW_BRCST_D0_BRCST = 0x3;
    constexpr static std::uint32_tMOV_4_ROWS               = 0x4;
    constexpr static std::uint32_tMOV_4_ROWS_D0_BRCST      = 0x5;
};

struct p_movd2b {
    constexpr static std::uint32_tSRC_ZERO_OFFSET  = 0x0;
    constexpr static std::uint32_tSRC_ROW16_OFFSET = 0x10;
    constexpr static std::uint32_tMOV_1_ROW        = 0x0;
    constexpr static std::uint32_tMOV_4_ROWS       = 0x2;
};

struct p_movb2a {
    constexpr static std::uint32_tSRCA_ZERO_OFFSET  = 0x0;
    constexpr static std::uint32_tSRCB_ZERO_OFFSET  = 0x0;
    constexpr static std::uint32_tSRCB_ROW16_OFFSET = 0x10;
    constexpr static std::uint32_tMOV_1_ROW         = 0x0;
    constexpr static std::uint32_tMOV_4_ROWS        = 0x2;
};

struct p_stall {
    // What to stall on
    constexpr static std::uint32_tNONE    = 0x0;
    constexpr static std::uint32_tTHCON   = 0x1;
    constexpr static std::uint32_tUNPACK0 = 0x2;
    constexpr static std::uint32_tUNPACK1 = 0x4;
    constexpr static std::uint32_tUNPACK  = UNPACK0 | UNPACK1; // Added to satisfy the LLK code
    constexpr static std::uint32_tPACK0   = 0x8;
    constexpr static std::uint32_tPACK    = PACK0;
    constexpr static std::uint32_tMATH    = 0x10;
    // constexpr static std::uint32_tSEM_ZERO   = 0x20;
    // constexpr static std::uint32_tSEM_MAX    = 0x40;
    constexpr static std::uint32_tSRCA_CLR  = 0x20;
    constexpr static std::uint32_tSRCB_CLR  = 0x40;
    constexpr static std::uint32_tSRCA_VLD  = 0x80;
    constexpr static std::uint32_tSRCB_VLD  = 0x100;
    constexpr static std::uint32_tXMOV      = 0x200;
    constexpr static std::uint32_tTRISC_CFG = 0x400;
    constexpr static std::uint32_tSFPU1     = 0x800;
    constexpr static std::uint32_tWAIT_SFPU = 0x800;
    constexpr static std::uint32_tCFGEXU    = 0x1000;

    constexpr static std::uint32_tALL_THREAD_RES = THCON | UNPACK0 | UNPACK1 | PACK0 | PACK | MATH | XMOV;

    // What to stall
    constexpr static std::uint32_tSTALL_TDMA   = 0x1;
    constexpr static std::uint32_tSTALL_SYNC   = 0x2;
    constexpr static std::uint32_tSTALL_PACK   = 0x4;
    constexpr static std::uint32_tSTALL_UNPACK = 0x8;
    //    constexpr static std::uint32_tSTALL_XSEARCH = 0x10;
    constexpr static std::uint32_tSTALL_XMOV   = 0x10;
    constexpr static std::uint32_tSTALL_THCON  = 0x20;
    constexpr static std::uint32_tSTALL_MATH   = 0x40;
    constexpr static std::uint32_tSTALL_CFG    = 0x80;
    constexpr static std::uint32_tSTALL_SFPU   = 0x100;
    constexpr static std::uint32_tSTALL_THREAD = 0x1ff;

    constexpr static std::uint32_tSTALL_ON_ZERO = 0x1;
    constexpr static std::uint32_tSTALL_ON_MAX  = 0x2;

    constexpr static std::uint32_tSEMAPHORE_0    = 0x1;
    constexpr static std::uint32_tSEMAPHORE_1    = 0x2;
    constexpr static std::uint32_tSEMAPHORE_2    = 0x4;
    constexpr static std::uint32_tSEMAPHORE_3    = 0x8;
    constexpr static std::uint32_tSEMAPHORE_4    = 0x10;
    constexpr static std::uint32_tSEMAPHORE_5    = 0x20;
    constexpr static std::uint32_tSEMAPHORE_6    = 0x40;
    constexpr static std::uint32_tSEMAPHORE_7    = 0x80;
    constexpr static std::uint32_tSEMAPHORE_BIAS = SEMAPHORE_4;
};

struct p_zeroacc {
    constexpr static std::uint32_tCLR_SPECIFIC = 0b000;
    constexpr static std::uint32_tCLR_16       = 0b001;
    constexpr static std::uint32_tCLR_HALF     = 0b010;
    constexpr static std::uint32_tCLR_ALL      = 0b011;
    constexpr static std::uint32_tCLR_HALF_32B = 0b110;
    constexpr static std::uint32_tCLR_ALL_32B  = 0b111;
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

    constexpr static std::uint32_tRESERVED0    = 0x0;
    constexpr static std::uint32_tRESERVED1    = 0x1;
    constexpr static std::uint32_tRIGHT_AWAY0  = 0x2;
    constexpr static std::uint32_tLEFT_TOWARD0 = 0x3;
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
    constexpr static std::uint32_tINDEX_DIS = 0x0;
    constexpr static std::uint32_tINDEX_EN  = 0x1;
};

struct p_elwise {
    constexpr static std::uint32_tSRCB_NO_BCAST  = 0x0;
    constexpr static std::uint32_tDEST_ACCUM_EN  = 0x1;
    constexpr static std::uint32_tDEST_ACCUM_DIS = 0x0;
    constexpr static std::uint32_tSRCB_BCAST_COL = 0x1;
    constexpr static std::uint32_tSRCB_BCAST_ROW = 0x2;
    constexpr static std::uint32_tSRCB_BCAST_ALL = 0x3;

    constexpr static std::uint32_tCLR_A  = 0x1;
    constexpr static std::uint32_tCLR_B  = 0x2;
    constexpr static std::uint32_tCLR_AB = 0x3;
};

struct p_sfpu {
    // SFPU registers
    constexpr static std::uint32_tLREG0 = 0;
    constexpr static std::uint32_tLREG1 = 1;
    constexpr static std::uint32_tLREG2 = 2;
    constexpr static std::uint32_tLREG3 = 3;
    constexpr static std::uint32_tLREG4 = 4;
    constexpr static std::uint32_tLREG5 = 5;
    constexpr static std::uint32_tLREG6 = 6;
    constexpr static std::uint32_tLREG7 = 7;

    // HW provided constants
    constexpr static std::uint32_tLCONST_0_8373 = 8;
    constexpr static std::uint32_tLCONST_0      = 9;
    constexpr static std::uint32_tLCONST_1      = 10;

    // Programmable constants
    constexpr static std::uint32_tLREG11      = 11;
    constexpr static std::uint32_tLREG12      = 12;
    constexpr static std::uint32_tLREG13      = 13;
    constexpr static std::uint32_tLREG14      = 14;
    constexpr static std::uint32_tLCONST_neg1 = 11;

    constexpr static std::uint32_tLTILEID = 15;

    constexpr static std::uint32_tkCONST_1_FP16B  = 0x3F80;
    constexpr static std::uint32_tkCONST_1_FP16A  = 0x3C00;
    constexpr static std::uint32_tkCONST_0        = 0x0000;
    constexpr static std::uint32_tkCONST_Exp_8Bit = 0;
    constexpr static std::uint32_tkCONST_Exp_5Bit = 1;
};

struct p_sfpswap {
    // SFPSWAP instruction modes
    constexpr static std::uint32_tUNCONDITIONALLY = 0;
    constexpr static std::uint32_tALL_ROWS_MAX    = 1;
    constexpr static std::uint32_tROWS_01_MAX     = 2;
    constexpr static std::uint32_tROWS_02_MAX     = 3;
    constexpr static std::uint32_tROWS_03_MAX     = 4;
    constexpr static std::uint32_tROW_0_MAX       = 5;
    constexpr static std::uint32_tROW_1_MAX       = 6;
    constexpr static std::uint32_tROW_2_MAX       = 5;
    constexpr static std::uint32_tROW_3_MAX       = 6;
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
