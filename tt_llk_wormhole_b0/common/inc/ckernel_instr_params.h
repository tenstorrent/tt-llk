// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_instr_params.h
 * @brief Instruction Parameter Definitions and Constants for Wormhole B0 Tensix
 *
 * This header provides structured parameter definitions, constants, and helper functions
 * for Tensix instruction generation. It contains hand-coded parameter encodings for
 * common instruction patterns, making instruction generation more readable and
 * type-safe compared to raw numeric constants.
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # Key Components
 *
 * - **Parameter Structures**: Organized constant definitions for instruction fields
 * - **Helper Functions**: Utility functions for parameter calculation and encoding
 * - **Common Instruction Macros**: Pre-configured instruction variants for frequent patterns
 * - **Context Management**: Address counter and configuration context definitions
 *
 * # Usage Pattern
 *
 * Instead of using raw numeric constants:
 * ```cpp
 * // Difficult to understand and error-prone
 * TT_UNPACR(0, 0x3, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1);
 * ```
 *
 * Use structured parameter definitions:
 * ```cpp
 * // Clear, readable, and type-safe
 * TT_UNPACR(p_unpacr::UNP_A, p_setrwc::SET_AB, 0,
 *           p_unpacr::TILE0_CFG_CONTEXT, p_unpacr::TILE0_ADDRCNT_CONTEXT,
 *           1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
 * ```
 *
 * # Organization
 *
 * Parameter structures are organized by instruction family:
 * - `p_setrwc` - Set Read/Write/Clear counter parameters
 * - `p_unpacr` - Unpack operation parameters and contexts
 * - `p_stall` - Stall and synchronization parameters
 * - `p_setadc` - Address counter configuration parameters
 * - And many more for specific instruction types
 *
 * @note These parameters must match the hardware instruction encoding exactly.
 *       Incorrect values can cause hardware malfunctions.
 *
 * @warning This file contains low-level hardware parameter definitions.
 *          Use the high-level LLK API when possible instead of direct instruction generation.
 *
 * @see ckernel_ops.h for instruction generation macros
 * @see ckernel.h for core infrastructure
 * @see ckernel_template.h for MOP programming
 */

#pragma once

// MT: This should be dissolved and moved to the appropriate place
#include "tensix.h"

/**
 * @namespace ckernel
 * @brief Core kernel functionality and hardware abstraction layer
 */
namespace ckernel
{

/**
 * @defgroup InstructionParameters Instruction Parameter Definitions
 * @brief Structured parameter constants for Tensix instruction generation
 * @{
 */

/**
 * @struct p_setrwc
 * @brief Parameters for SETRWC (Set Read/Write/Clear) instruction
 *
 * The SETRWC instruction controls read/write counters and carriage return operations
 * for data movement pipelines. This structure provides readable constants for the
 * various counter and mode configurations.
 *
 * # Usage
 * ```cpp
 * // Reset A and B counters, set D counter
 * TT_SETRWC(p_setrwc::CLR_AB, p_setrwc::CR_NONE, p_setrwc::SET_D,
 *           p_setrwc::SET_NONE, p_setrwc::SET_NONE, bitmask);
 * ```
 */
struct p_setrwc
{
    /**
     * @defgroup ClearParameters Counter Clear Control
     * @brief Parameters for clearing specific counters
     * @{
     */

    constexpr static uint CLR_A    = 0x1; ///< Clear source A counter
    constexpr static uint CLR_B    = 0x2; ///< Clear source B counter
    constexpr static uint CLR_AB   = 0x3; ///< Clear both A and B counters
    constexpr static uint CLR_NONE = 0x0; ///< Do not clear any counters

    /** @} */ // end of ClearParameters group

    /**
     * @defgroup SetParameters Counter Set Control
     * @brief Parameters for setting/enabling specific counters and modes
     * @{
     */

    constexpr static uint SET_A     = 0x1; ///< Set source A counter
    constexpr static uint SET_B     = 0x2; ///< Set source B counter
    constexpr static uint SET_AB    = 0x3; ///< Set both A and B counters
    constexpr static uint SET_D     = 0x4; ///< Set destination counter
    constexpr static uint SET_AD    = 0x5; ///< Set A and destination counters
    constexpr static uint SET_BD    = 0x6; ///< Set B and destination counters
    constexpr static uint SET_ABD   = 0x7; ///< Set A, B, and destination counters
    constexpr static uint SET_F     = 0x8; ///< Set flush mode
    constexpr static uint SET_A_F   = 0x9; ///< Set A counter with flush
    constexpr static uint SET_B_F   = 0xa; ///< Set B counter with flush
    constexpr static uint SET_AB_F  = 0xb; ///< Set A and B counters with flush
    constexpr static uint SET_D_F   = 0xc; ///< Set destination counter with flush
    constexpr static uint SET_AD_F  = 0xd; ///< Set A and destination with flush
    constexpr static uint SET_BD_F  = 0xe; ///< Set B and destination with flush
    constexpr static uint SET_ABD_F = 0xf; ///< Set all counters with flush

    /** @} */ // end of SetParameters group

    /**
     * @defgroup CarriageReturnParameters Carriage Return Control
     * @brief Parameters for carriage return (CR) operations
     * @{
     */

    constexpr static uint CR_A         = 0x1; ///< Carriage return for source A
    constexpr static uint CR_B         = 0x2; ///< Carriage return for source B
    constexpr static uint CR_AB        = 0x3; ///< Carriage return for A and B
    constexpr static uint CR_D         = 0x4; ///< Carriage return for destination
    constexpr static uint CR_AD        = 0x5; ///< Carriage return for A and destination
    constexpr static uint CR_BD        = 0x6; ///< Carriage return for B and destination
    constexpr static uint CR_ABD       = 0x7; ///< Carriage return for all sources
    constexpr static uint C_TO_CR_MODE = 0x8; ///< Convert C counter to CR mode

    /** @} */ // end of CarriageReturnParameters group
};

/**
 * @struct p_setibrwc
 * @brief Parameters for SETIBRWC (Set Increment Bias Read/Write/Clear) instruction
 *
 * Controls bias increment behavior and carriage return settings for specialized
 * addressing modes in convolution and other operations.
 */
struct p_setibrwc
{
    constexpr static uint SET_BIAS = 0x0; ///< Set bias value directly
    constexpr static uint INC_BIAS = 0x1; ///< Increment bias value
    constexpr static uint CR_NONE  = 0x0; ///< No carriage return for bias
    constexpr static uint CR_BIAS  = 0x1; ///< Enable carriage return for bias
};

/**
 * @struct p_unpacr
 * @brief Parameters for UNPACR (Unpack) instruction
 *
 * Defines constants for unpack operation configuration, including rarefy control,
 * tile context management, and address counter contexts. These parameters are
 * critical for proper data flow in the unpack pipeline.
 *
 * # Context Management
 *
 * Tiles are organized into context groups for efficient address counter management:
 * - **Tiles 0,1**: Use address counter context 0
 * - **Tiles 2,3**: Use address counter context 1
 * - **All tiles**: Use configuration context 0 by default
 *
 * # Usage Example
 * ```cpp
 * // Unpack for tile 2 with rarefy disabled
 * TT_UNPACR(p_setadc::UNP_A, 0b01, 0,
 *           p_unpacr::TILE2_CFG_CONTEXT, p_unpacr::TILE2_ADDRCNT_CONTEXT,
 *           1, 1, p_unpacr::RAREFYB_DISABLE, 0, 0, 0, 0, 1);
 * ```
 */
struct p_unpacr
{
    /**
     * @defgroup RarefyControl Rarefy Operation Control
     * @brief Controls rarefy behavior for source B data
     * @{
     */

    constexpr static uint RAREFYB_DISABLE = 0x0; ///< Disable rarefy for source B
    constexpr static uint RAREFYB_ENABLE  = 0x1; ///< Enable rarefy for source B (skip zeros)

    /** @} */ // end of RarefyControl group

    /**
     * @defgroup TileContexts Tile Context Definitions
     * @brief Address counter and configuration contexts for tile processing
     * @{
     */

    constexpr static uint TILE0_ADDRCNT_CONTEXT = (0); ///< Address counter context for tile 0
    constexpr static uint TILE1_ADDRCNT_CONTEXT = (0); ///< Address counter context for tile 1
    constexpr static uint TILE2_ADDRCNT_CONTEXT = (1); ///< Address counter context for tile 2
    constexpr static uint TILE3_ADDRCNT_CONTEXT = (1); ///< Address counter context for tile 3
    constexpr static uint TILE0_CFG_CONTEXT     = (0); ///< Configuration context for tile 0
    constexpr static uint TILE1_CFG_CONTEXT     = (0); ///< Configuration context for tile 1
    constexpr static uint TILE2_CFG_CONTEXT     = (0); ///< Configuration context for tile 2
    constexpr static uint TILE3_CFG_CONTEXT     = (0); ///< Configuration context for tile 3
    constexpr static uint AUTO_INC_CONTEXT      = (1); ///< Auto increment config context (max value set through unpacker config command)

    /** @} */ // end of TileContexts group
};

/**
 * @defgroup CommonInstructionMacros Common Instruction Convenience Macros
 * @brief Pre-configured instruction macros for frequent usage patterns
 * @{
 */

/**
 * @brief Common UNPACR instruction with typical parameter defaults
 *
 * This macro provides a simplified interface for the most common UNPACR usage pattern,
 * setting typical defaults for most parameters while allowing control over the three
 * most frequently varied parameters.
 *
 * @param Unpack_block_selection Which unpacker block to target (A or B)
 * @param AddrMode Address mode for the unpack operation
 * @param SetDatValid Whether to set data valid signal
 *
 * # Default Parameters Set:
 * - CfgContextCntInc: 0 (no increment)
 * - CfgContextId: 0 (default context)
 * - AddrCntContextId: 0 (default address counter context)
 * - OvrdThreadId: 1 (override thread ID)
 * - srcb_bcast: 0 (no broadcast)
 * - ZeroWrite2: 0 (normal write)
 * - AutoIncContextID: 0 (manual context)
 * - RowSearch: 0 (no row search)
 * - SearchCacheFlush: 0 (no cache flush)
 * - Last: 1 (mark as last)
 *
 * @note Use this macro for standard unpack operations to reduce code complexity
 */
#define TTI_UNPACR_COMMON(Unpack_block_selection, AddrMode, SetDatValid) \
    TTI_UNPACR(                                                          \
        Unpack_block_selection,                                          \
        AddrMode,                                                        \
        0 /*CfgContextCntInc*/,                                          \
        0 /*CfgContextId*/,                                              \
        0 /*AddrCntContextId*/,                                          \
        1 /*OvrdThreadId*/,                                              \
        SetDatValid,                                                     \
        0 /*srcb_bcast*/,                                                \
        0 /*ZeroWrite2*/,                                                \
        0 /*AutoIncContextID*/,                                          \
        0 /*RowSearch*/,                                                 \
        0 /*SearchCacheFlush*/,                                          \
        1 /*Last*/)

/**
 * @brief Common UNPACR instruction generation with typical defaults
 *
 * Instruction generation variant of TTI_UNPACR_COMMON. Creates the instruction
 * word but does not execute it immediately.
 *
 * @param Unpack_block_selection Which unpacker block to target
 * @param AddrMode Address mode for the unpack operation
 * @param SetDatValid Whether to set data valid signal
 * @return 32-bit instruction word
 *
 * @see TTI_UNPACR_COMMON for parameter details and usage patterns
 */
#define TT_OP_UNPACR_COMMON(Unpack_block_selection, AddrMode, SetDatValid) \
    TT_OP_UNPACR(                                                          \
        Unpack_block_selection,                                            \
        AddrMode,                                                          \
        0 /*CfgContextCntInc*/,                                            \
        0 /*CfgContextId*/,                                                \
        0 /*AddrCntContextId*/,                                            \
        1 /*OvrdThreadId*/,                                                \
        SetDatValid,                                                       \
        0 /*srcb_bcast*/,                                                  \
        0 /*ZeroWrite2*/,                                                  \
        0 /*AutoIncContextID*/,                                            \
        0 /*RowSearch*/,                                                   \
        0 /*SearchCacheFlush*/,                                            \
        1 /*Last*/)

/** @} */ // end of CommonInstructionMacros group

/**
 * @struct p_unpacr_nop
 * @brief Parameters for UNPACR_NOP (Unpack No-Operation) instruction
 *
 * The UNPACR_NOP instruction provides various special unpack behaviors including
 * data source control, bank management, and data valid signaling without
 * performing actual data unpacking.
 *
 * # Common Usage Patterns
 * ```cpp
 * // Provide zero data without unpacking
 * TTI_UNPACR_NOP(p_unpacr_nop::UNP0, p_unpacr_nop::UNP_ZEROSRC);
 *
 * // Set data valid signal
 * TTI_UNPACR_NOP(p_unpacr_nop::UNP0, p_unpacr_nop::UNP_SET_DVALID);
 * ```
 */
struct p_unpacr_nop
{
    /**
     * @defgroup BasicNOPOperations Basic NOP Operations
     * @brief Standard NOP operation types
     * @{
     */

    constexpr static uint UNP_POP = 0b000; ///< Pop data from unpacker without processing
    constexpr static uint UNP_NOP = 0b010; ///< Standard no-operation, maintain state

    /** @} */ // end of BasicNOPOperations group

    /**
     * @defgroup DataSourceControl Data Source Control
     * @brief Control data source values for unpack operations
     * @{
     */

    constexpr static uint UNP_ZEROSRC   = 0b001; ///< Provide zero values as source data
    constexpr static uint UNP_NEGINFSRC = 0b101; ///< Provide negative infinity as source data

    /** @} */ // end of DataSourceControl group

    /**
     * @defgroup ControlSignals Control and Status Signals
     * @brief Special control signals for unpack coordination
     * @{
     */

    constexpr static uint UNP_SET_DVALID = 0b111; ///< Set data valid signal

    /** @} */ // end of ControlSignals group

    /**
     * @defgroup AdvancedOperations Advanced NOP Operations
     * @brief Complex NOP operations with bank and stall control
     * @{
     */

    constexpr static uint UNP_ZEROSRC_RESET_ALL_BANKS    = 0b1001;    ///< Zero source with all bank reset (default: current bank only)
    constexpr static uint UNP_ZEROSRC_STALL_RESET_WR_RDY = 0b10001;   ///< Zero source with stall reset and write ready
    constexpr static uint UNP_ZEROSRC_SET_DVALID         = 0b1000001; ///< Zero source with data valid signal

    /** @} */ // end of AdvancedOperations group

    /**
     * @defgroup UnpackerSelection Unpacker Block Selection
     * @brief Constants for selecting unpacker blocks
     * @{
     */

    constexpr static uint UNP0 = 0x0; ///< Select unpacker block 0 (usually source A)
    constexpr static uint UNP1 = 0x1; ///< Select unpacker block 1 (usually source B)

    /** @} */ // end of UnpackerSelection group
};

/**
 * @struct p_srcb
 * @brief Parameters for source B operation direction control
 *
 * Controls the direction of source B operations, particularly important
 * for operations that need to process data in different directions
 * such as convolution forward and backward passes.
 */
struct p_srcb
{
    constexpr static uint FORWARD_PASS  = 0x0; ///< Forward pass direction
    constexpr static uint BACKWARD_PASS = 0x1; ///< Backward pass direction
};

/**
 * @defgroup AddressCounterHelpers Address Counter Helper Functions
 * @brief Utility functions for address counter channel configuration
 * @{
 */

/**
 * @brief Configure address counter for channel 0 only
 * @param cnt Counter value
 * @return Encoded value for channel 0
 */
constexpr static uint SETADC_CH0(uint cnt)
{
    return cnt;
}

/**
 * @brief Configure address counter for channel 1 only
 * @param cnt Counter value
 * @return Encoded value for channel 1 (shifted to appropriate bit position)
 */
constexpr static uint SETADC_CH1(uint cnt)
{
    return cnt << 2;
}

/**
 * @brief Configure address counter for both channels 0 and 1
 * @param cnt Counter value
 * @return Encoded value for both channels
 */
constexpr static uint SETADC_CH01(uint cnt)
{
    return cnt << 2 | cnt;
}

/** @} */ // end of AddressCounterHelpers group

/**
 * @struct p_setadc
 * @brief Parameters for SETADC (Set Address Counter) instruction
 *
 * Provides constants for configuring address counters for unpacker and packer
 * blocks. Address counters control data addressing patterns for efficient
 * data movement through the processing pipeline.
 *
 * # Coordinate System
 * The address counter operates in a 4D coordinate system (X, Y, Z, W)
 * where each dimension can be independently controlled and configured.
 */
struct p_setadc
{
    /**
     * @defgroup BlockSelection Block Selection
     * @brief Constants for selecting unpacker and packer blocks
     * @{
     */

    constexpr static uint UNP0   = 0b001; ///< Unpacker block 0 (source A)
    constexpr static uint UNP1   = 0b010; ///< Unpacker block 1 (source B)
    constexpr static uint UNP_A  = 0b001; ///< Unpacker source A (alias for UNP0)
    constexpr static uint UNP_B  = 0b010; ///< Unpacker source B (alias for UNP1)
    constexpr static uint UNP_AB = 0b011; ///< Both unpacker blocks
    constexpr static uint PAC    = 0b100; ///< Packer block

    /** @} */ // end of BlockSelection group

    /**
     * @defgroup DimensionSelection Dimension Selection for SET operations
     * @brief Constants for selecting which dimension to set
     * @{
     */

    constexpr static uint SET_X = 0; ///< Set X dimension
    constexpr static uint SET_Y = 1; ///< Set Y dimension
    constexpr static uint SET_Z = 2; ///< Set Z dimension
    constexpr static uint SET_W = 3; ///< Set W dimension

    /** @} */ // end of DimensionSelection group

    /**
     * @defgroup DimensionMasks Dimension Masks for multi-dimension operations
     * @brief Bitmasks for enabling multiple dimensions simultaneously
     * @{
     */

    constexpr static uint X  = 1; ///< X dimension mask
    constexpr static uint Y  = 2; ///< Y dimension mask
    constexpr static uint XY = 3; ///< X and Y dimensions mask
    constexpr static uint Z  = 1; ///< Z dimension mask
    constexpr static uint W  = 2; ///< W dimension mask
    constexpr static uint ZW = 3; ///< Z and W dimensions mask

    /** @} */ // end of DimensionMasks group

    /**
     * @defgroup ChannelSelection Channel Selection
     * @brief Constants for selecting address counter channels
     * @{
     */

    constexpr static uint CH_0 = 0; ///< Channel 0
    constexpr static uint CH_1 = 1; ///< Channel 1

    /** @} */ // end of ChannelSelection group
};

struct p_pacr
{
    constexpr static uint P_ZERO_OUTPUT_DISABLED = 0x0;
    constexpr static uint P_ZERO_OUTPUT_ENABLED  = 0x1;
};

#define TTI_PACR_COMMON(AddrMode, ZeroWrite, PackSel, Flush, Last) TTI_PACR(AddrMode, ZeroWrite, PackSel, 0 /*OvrdThreadId*/, 0 /*Concat*/, Flush, Last)

#define TT_OP_PACR_COMMON(AddrMode, ZeroWrite, PackSel, Flush, Last) TT_OP_PACR(AddrMode, ZeroWrite, PackSel, 0 /*OvrdThreadId*/, 0 /*Concat*/, Flush, Last)

struct p_ind
{
    constexpr static uint HIER_REGFILE = 0x0;
    constexpr static uint HIER_L1      = 0x1;

    constexpr static uint INC_NONE = 0x0;
    constexpr static uint INC_2B   = 0x1;
    constexpr static uint INC_4B   = 0x2;
    constexpr static uint INC_16B  = 0x3;

    constexpr static uint LD_16B   = 0;
    constexpr static uint LD_32bit = 1;
    constexpr static uint LD_16bit = 2;
    constexpr static uint LD_8bit  = 3;
};

struct p_mov
{
    constexpr static uint DEST_NORM    = 0;
    constexpr static uint DEST_32B_LOW = 1;
};

struct p_mova2d
{
    constexpr static uint MATH_HALO_ROWS = 0x0;
    constexpr static uint MOV_1_ROW      = 0x0;
    constexpr static uint MOV_8_ROWS     = 0x2;
};

struct p_movd2a
{
    constexpr static uint MOV_1_ROW  = 0x0;
    constexpr static uint MOV_4_ROWS = 0x2;
};

struct p_movb2d
{
    constexpr static uint SRC_ZERO_OFFSET          = 0x0;
    constexpr static uint SRC_ROW16_OFFSET         = 0x10;
    constexpr static uint MOV_1_ROW                = 0x0;
    constexpr static uint MOV_1_ROW_D0_BRCST       = 0x1;
    constexpr static uint MOV_8_ROW_BRCST          = 0x2;
    constexpr static uint MOV_8_ROW_BRCST_D0_BRCST = 0x3;
    constexpr static uint MOV_4_ROWS               = 0x4;
    constexpr static uint MOV_4_ROWS_D0_BRCST      = 0x5;
};

struct p_movd2b
{
    constexpr static uint SRC_ZERO_OFFSET  = 0x0;
    constexpr static uint SRC_ROW16_OFFSET = 0x10;
    constexpr static uint MOV_1_ROW        = 0x0;
    constexpr static uint MOV_4_ROWS       = 0x2;
};

struct p_movb2a
{
    constexpr static uint SRCA_ZERO_OFFSET  = 0x0;
    constexpr static uint SRCB_ZERO_OFFSET  = 0x0;
    constexpr static uint SRCB_ROW16_OFFSET = 0x10;
    constexpr static uint MOV_1_ROW         = 0x0;
    constexpr static uint MOV_4_ROWS        = 0x2;
};

struct p_stall
{
    // What to stall on
    constexpr static uint NONE    = 0x0;
    constexpr static uint THCON   = 0x1;
    constexpr static uint UNPACK0 = 0x2;
    constexpr static uint UNPACK1 = 0x4;
    constexpr static uint UNPACK  = UNPACK0 | UNPACK1;
    constexpr static uint PACK0   = 0x8;
    constexpr static uint PACK1   = 0x10;
    constexpr static uint PACK2   = 0x20;
    constexpr static uint PACK3   = 0x40;
    constexpr static uint PACK    = PACK0 | PACK1 | PACK2 | PACK3;
    constexpr static uint MATH    = 0x80;
    // constexpr static uint SEM_ZERO    = 0x20;
    // constexpr static uint SEM_MAX     = 0x40;
    constexpr static uint SRCA_CLR       = 0x100;
    constexpr static uint SRCB_CLR       = 0x200;
    constexpr static uint SRCA_VLD       = 0x400;
    constexpr static uint SRCB_VLD       = 0x800;
    constexpr static uint XMOV           = 0x1000;
    constexpr static uint TRISC_CFG      = 0x2000;
    constexpr static uint SFPU1          = 0x4000;
    constexpr static uint WAIT_SFPU      = 0x4000;
    constexpr static uint ALL_THREAD_RES = THCON | UNPACK | PACK | MATH | XMOV;

    // What to stall
    constexpr static uint STALL_TDMA   = 0x1;
    constexpr static uint STALL_SYNC   = 0x2;
    constexpr static uint STALL_PACK   = 0x4;
    constexpr static uint STALL_UNPACK = 0x8;
    //    constexpr static uint STALL_XSEARCH = 0x10;
    constexpr static uint STALL_XMOV   = 0x10;
    constexpr static uint STALL_THCON  = 0x20;
    constexpr static uint STALL_MATH   = 0x40;
    constexpr static uint STALL_CFG    = 0x80;
    constexpr static uint STALL_SFPU   = 0x100;
    constexpr static uint STALL_THREAD = 0x1ff;

    constexpr static uint STALL_ON_ZERO = 0x1;
    constexpr static uint STALL_ON_MAX  = 0x2;

    constexpr static uint SEMAPHORE_0    = 0x1;
    constexpr static uint SEMAPHORE_1    = 0x2;
    constexpr static uint SEMAPHORE_2    = 0x4;
    constexpr static uint SEMAPHORE_3    = 0x8;
    constexpr static uint SEMAPHORE_4    = 0x10;
    constexpr static uint SEMAPHORE_5    = 0x20;
    constexpr static uint SEMAPHORE_6    = 0x40;
    constexpr static uint SEMAPHORE_7    = 0x80;
    constexpr static uint SEMAPHORE_BIAS = SEMAPHORE_4;
};

struct p_zeroacc
{
    constexpr static uint CLR_SPECIFIC = 0b000;
    constexpr static uint CLR_16       = 0b001;
    constexpr static uint CLR_HALF     = 0b010;
    constexpr static uint CLR_ALL      = 0b011;
    constexpr static uint CLR_HALF_32B = 0b110;
    constexpr static uint CLR_ALL_32B  = 0b111;
};

struct p_zerosrc
{
    constexpr static uint CLR_A  = 0x1;
    constexpr static uint CLR_B  = 0x2;
    constexpr static uint CLR_AB = 0x3;
};

struct p_shiftx
{
    constexpr static uint SHIFT_1 = 0x0;
    constexpr static uint SHIFT_2 = 0x1;
    constexpr static uint SHIFT_4 = 0x2;
    constexpr static uint SHIFT_8 = 0x3;

    constexpr static uint RESERVED0    = 0x0;
    constexpr static uint RESERVED1    = 0x1;
    constexpr static uint RIGHT_AWAY0  = 0x2;
    constexpr static uint LEFT_TOWARD0 = 0x3;
};

struct p_cfg
{
    constexpr static uint WRCFG_128b = 0x1;
    constexpr static uint WRCFG_32b  = 0x0;
};

struct p_alu
{
    constexpr static uint AND = 0x0;
    constexpr static uint OR  = 0x1;
    constexpr static uint XOR = 0x2;
};

struct p_gpool
{
    constexpr static uint DIM_1X16  = 0x0;
    constexpr static uint DIM_16X16 = 0x1;
    constexpr static uint INDEX_DIS = 0x0;
    constexpr static uint INDEX_EN  = 0x1;
};

struct p_elwise
{
    constexpr static uint SRCB_NO_BCAST  = 0x0;
    constexpr static uint DEST_ACCUM_EN  = 0x1;
    constexpr static uint DEST_ACCUM_DIS = 0x0;
    constexpr static uint SRCB_BCAST_COL = 0x1;
    constexpr static uint SRCB_BCAST_ROW = 0x2;
    constexpr static uint SRCB_BCAST_ALL = 0x3;

    constexpr static uint CLR_A  = 0x1;
    constexpr static uint CLR_B  = 0x2;
    constexpr static uint CLR_AB = 0x3;
};

struct p_sfpu
{
    // SFPU registers
    constexpr static uint LREG0 = 0;
    constexpr static uint LREG1 = 1;
    constexpr static uint LREG2 = 2;
    constexpr static uint LREG3 = 3;
    constexpr static uint LREG4 = 4;
    constexpr static uint LREG5 = 5;
    constexpr static uint LREG6 = 6;
    constexpr static uint LREG7 = 7;

    // HW provided constants
    constexpr static uint LCONST_0_8373 = 8;
    constexpr static uint LCONST_0      = 9;
    constexpr static uint LCONST_1      = 10;

    // Programmable constants
    constexpr static uint LREG11      = 11;
    constexpr static uint LREG12      = 12;
    constexpr static uint LREG13      = 13;
    constexpr static uint LREG14      = 14;
    constexpr static uint LCONST_neg1 = 11;

    constexpr static uint LTILEID = 15;

    constexpr static uint kCONST_1_FP16B  = 0x3F80;
    constexpr static uint kCONST_1_FP16A  = 0x3C00;
    constexpr static uint kCONST_0        = 0x0000;
    constexpr static uint kCONST_Exp_8Bit = 0;
    constexpr static uint kCONST_Exp_5Bit = 1;
};

struct p_sfpswap
{
    // SFPSWAP instruction modes
    constexpr static uint UNCONDITIONALLY = 0;
    constexpr static uint ALL_ROWS_MAX    = 1;
    constexpr static uint ROWS_01_MAX     = 2;
    constexpr static uint ROWS_02_MAX     = 3;
    constexpr static uint ROWS_03_MAX     = 4;
    constexpr static uint ROW_0_MAX       = 5;
    constexpr static uint ROW_1_MAX       = 6;
    constexpr static uint ROW_2_MAX       = 5;
    constexpr static uint ROW_3_MAX       = 6;
};

struct p_exp
{
    constexpr static uint FRAC_BITS = 3;
    constexpr static uint C23_73    = 0x4340; // Based on FRAC_BITS
    // ADJ_EXP = -0x4300 + 0x003F
    //  0x4300 : 0100 0011 0000 0000
    //  0x003F : 0000 0000 0011 1111
    // -0x4300 : 1011 1101 0000 0000
    // ADJ_EXP : 1011 1101 0011 1111 (-0x4300 + 0x003F = 0xBD3F)
    constexpr static uint ADJ_EXP = 0xBD3F;
};

struct p_setdmareg
{
    constexpr static uint PAYLOAD_IMMEDIATE   = 0;
    constexpr static uint PAYLOAD_16BIT       = 0;
    constexpr static uint PAYLOAD_32BIT       = 1;
    constexpr static uint PAYLOAD_128BIT      = 2;
    constexpr static uint PAYLOAD_TILE_HEADER = 3;

    constexpr static uint MODE_IMMEDIATE = 0;
    constexpr static uint MODE_SIGNAL    = 1;
};

struct p_mop
{
    constexpr static uint MASK_LOOP   = 0;
    constexpr static uint DOUBLE_LOOP = 1;
};

struct p_adddmareg
{
    constexpr static uint REG_PLUS_REG = 0;
    constexpr static uint REG_PLUS_IMM = 1;
};

/**
 * @brief Calculate FLOP index from address
 *
 * Converts a hardware address to a FLOP index by subtracting the base
 * address for the THCON configuration registers.
 *
 * @param addr Hardware address
 * @return FLOP index for use with REG2FLOP instructions
 */
constexpr static uint REG2FLOP_FLOP_INDEX(uint addr)
{
    return addr - THCON_CFGREG_BASE_ADDR32;
}

/**
 * @struct p_reg2flop
 * @brief Parameters for REG2FLOP (Register to FLOP) instruction
 *
 * Controls the data size for register-to-FLOP transfer operations.
 * FLOP (Floating Point Operations) units require specific data sizes
 * for optimal performance.
 */
struct p_reg2flop
{
    constexpr static uint WRITE_16B = 0; ///< Write 16 bytes (128 bits)
    constexpr static uint WRITE_4B  = 1; ///< Write 4 bytes (32 bits)
    constexpr static uint WRITE_2B  = 2; ///< Write 2 bytes (16 bits)
    constexpr static uint WRITE_1B  = 3; ///< Write 1 byte (8 bits)
};

/**
 * @brief Common REG2FLOP instruction with typical defaults
 *
 * Simplified interface for register-to-FLOP operations with standard defaults.
 * Sets TargetSel, ByteOffset, and ContextId to their most common values.
 *
 * @param SizeSel Size of data to write (use p_reg2flop constants)
 * @param FlopIndex Target FLOP index
 * @param RegIndex Source register index
 */
#define TTI_REG2FLOP_COMMON(SizeSel, FlopIndex, RegIndex) TTI_REG2FLOP(SizeSel, 0 /*TargetSel*/, 0 /*ByteOffset*/, 0 /*ContextId*/, FlopIndex, RegIndex)

/**
 * @brief Common REG2FLOP instruction generation with typical defaults
 *
 * Instruction generation variant of TTI_REG2FLOP_COMMON.
 *
 * @param SizeSel Size of data to write (use p_reg2flop constants)
 * @param FlopIndex Target FLOP index
 * @param RegIndex Source register index
 * @return 32-bit instruction word
 */
#define TT_OP_REG2FLOP_COMMON(SizeSel, FlopIndex, RegIndex) TT_OP_REG2FLOP(SizeSel, 0 /*TargetSel*/, 0 /*ByteOffset*/, 0 /*ContextId*/, FlopIndex, RegIndex)

/** @} */ // end of InstructionParameters group

} // namespace ckernel
