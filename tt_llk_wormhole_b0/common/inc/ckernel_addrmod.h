// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_addrmod.h
 * @brief Address Modification Infrastructure for Wormhole B0 Tensix
 *
 * This header provides structured address modification capabilities for Wormhole B0
 * Tensix cores. Address modification controls how memory addresses are incremented,
 * cleared, and managed during data processing operations across different pipeline
 * stages (unpack, math, pack).
 *
 * @author Tenstorrent AI ULC
 * @version 1.0
 * @date 2025
 *
 * # Key Components
 *
 * - **Address Modifier Constants**: Identifiers for different address modifier slots
 * - **Source Address Control**: Increment, clear, and carriage return for source operands
 * - **Destination Address Control**: Advanced addressing with bias and fidelity support
 * - **Pack Address Control**: Specialized addressing for pack pipeline operations
 * - **Register Programming**: Hardware register programming for address modification
 *
 * # Address Modification Architecture
 *
 * ## Address Modifier Slots
 * The hardware provides 8 address modifier slots (ADDR_MOD_0 through ADDR_MOD_7)
 * that can be independently configured for different addressing patterns.
 *
 * ## Address Control Fields
 * Each address modifier controls:
 * - **Increment (INCR)**: How much to increment the address each cycle
 * - **Clear (CLR)**: Whether to clear the address counter
 * - **Carriage Return (CR)**: Whether to perform carriage return operation
 *
 * ## Pipeline-Specific Addressing
 * Different pipeline stages have different addressing requirements:
 * - **Source A/B**: Basic increment/clear/CR control (6-bit increment)
 * - **Destination**: Extended control with bias and fidelity (10-bit increment)
 * - **Pack**: Y/Z dimension control for output addressing
 *
 * # Usage Patterns
 *
 * ## Basic Address Modification
 * ```cpp
 * // Configure address modifier for sequential access
 * addr_mod_t mod = {
 *     .srca = {.incr = 1, .clr = 0, .cr = 0},
 *     .srcb = {.incr = 1, .clr = 0, .cr = 0},
 *     .dest = {.incr = 1, .clr = 0, .cr = 0}
 * };
 * mod.set(ADDR_MOD_0);
 * ```
 *
 * ## Pack Address Modification
 * ```cpp
 * // Configure pack address modifier for 2D addressing
 * addr_mod_pack_t pack_mod = {
 *     .y_src = {.incr = 1, .clr = 0, .cr = 0},
 *     .y_dst = {.incr = 1, .clr = 0, .cr = 0},
 *     .z_src = {.incr = 0, .clr = 0},
 *     .z_dst = {.incr = 0, .clr = 0}
 * };
 * pack_mod.set(0);
 * ```
 *
 * ## Advanced Destination Control
 * ```cpp
 * // Destination with bias and fidelity control
 * addr_mod_t mod = {
 *     .dest = {.incr = 2, .clr = 0, .cr = 1},
 *     .fidelity = {.incr = 1, .clr = 0},
 *     .bias = {.incr = 4, .clr = 0}
 * };
 * ```
 *
 * @note Address modification is fundamental to efficient data movement patterns
 *       and must be carefully coordinated with the instruction sequences.
 *
 * @warning Incorrect address modification can cause data corruption, access
 *          violations, or incorrect computation results.
 *
 * @see ckernel_ops.h for address modification instructions (SETC16, etc.)
 * @see cfg_defines.h for hardware register address definitions
 */

#pragma once

// MT: This should be dissolved and moved to the appropriate place
#include "cfg_defines.h"
#include "ckernel_ops.h"

/**
 * @namespace ckernel
 * @brief Core kernel functionality and hardware abstraction layer
 */
namespace ckernel
{

/**
 * @defgroup AddressModification Address Modification Infrastructure
 * @brief Hardware address modification for efficient data access patterns
 * @{
 */

/**
 * @defgroup AddressModifierSlots Address Modifier Slot Constants
 * @brief Constants for selecting address modifier slots
 * @{
 */

constexpr uint8_t ADDR_MOD_0 = 0; ///< Address modifier slot 0
constexpr uint8_t ADDR_MOD_1 = 1; ///< Address modifier slot 1
constexpr uint8_t ADDR_MOD_2 = 2; ///< Address modifier slot 2
constexpr uint8_t ADDR_MOD_3 = 3; ///< Address modifier slot 3
constexpr uint8_t ADDR_MOD_4 = 4; ///< Address modifier slot 4
constexpr uint8_t ADDR_MOD_5 = 5; ///< Address modifier slot 5
constexpr uint8_t ADDR_MOD_6 = 6; ///< Address modifier slot 6
constexpr uint8_t ADDR_MOD_7 = 7; ///< Address modifier slot 7

/** @} */ // end of AddressModifierSlots group

/**
 * @defgroup AddressModifierMasks Bit Masks for Address Increment Fields
 * @brief Bit masks for extracting increment values from address modifier fields
 * @{
 */

constexpr uint32_t SRC_INCR_MASK  = 0x3F;  ///< Source increment mask (6 bits, max 63)
constexpr uint32_t DEST_INCR_MASK = 0x3FF; ///< Destination increment mask (10 bits, max 1023)

/** @} */ // end of AddressModifierMasks group

/**
 * @struct addr_mod_t
 * @brief Complete address modification configuration for all pipeline stages
 *
 * Provides structured control over address modification for source operands,
 * destination registers, and specialized addressing modes like fidelity and bias.
 * This structure encapsulates all address modification settings and provides
 * methods to program them into hardware registers.
 *
 * # Structure Organization:
 * - **Source Control**: SrcA and SrcB address modification
 * - **Destination Control**: Extended destination addressing with bias/fidelity
 * - **Pack Control**: Pack-specific Y-dimension addressing
 * - **Hardware Interface**: Register programming and value encoding
 *
 * # Address Modification Fields:
 * Each address modifier controls three key aspects:
 * - **Increment**: Amount to add to address each cycle
 * - **Clear**: Whether to reset the address counter
 * - **Carriage Return**: Whether to perform carriage return operation
 */
struct addr_mod_t
{
    /**
     * @struct addr_mod_src_t
     * @brief Source address modification control (SrcA/SrcB)
     *
     * Controls address modification for source operands with basic increment,
     * clear, and carriage return functionality. Limited to 6-bit increment
     * values (0-63) for source addressing.
     */
    struct addr_mod_src_t
    {
        uint8_t incr = 0; ///< Address increment value (0-63, 6 bits)
        uint8_t clr  = 0; ///< Clear address counter flag
        uint8_t cr   = 0; ///< Carriage return flag

        /**
         * @brief Encode address modification fields into hardware format
         * @return 8-bit encoded value for hardware register
         *
         * Bit layout: [CLR][CR][INCR:6]
         */
        constexpr uint8_t val() const
        {
            return (incr & SRC_INCR_MASK) | ((cr & 0x1) << 6) | ((clr & 0x1) << 7);
        }
    };

    /**
     * @struct addr_mod_dest_t
     * @brief Destination address modification control with extended features
     *
     * Advanced address modification for destination registers supporting larger
     * increment values (10-bit) and additional control features for complex
     * addressing patterns.
     */
    struct addr_mod_dest_t
    {
        int16_t incr    = 0; ///< Address increment value (signed, 10 bits)
        uint8_t clr     = 0; ///< Clear address counter flag
        uint8_t cr      = 0; ///< Carriage return flag
        uint8_t c_to_cr = 0; ///< Clear-to-carriage-return flag

        /**
         * @brief Encode destination address fields into hardware format
         * @return 16-bit encoded value for hardware register
         *
         * Bit layout: [Rsvd:3][C_TO_CR][CLR][CR][INCR:10]
         */
        constexpr uint16_t val() const
        {
            return (incr & DEST_INCR_MASK) | ((cr & 0x1) << 10) | ((clr & 0x1) << 11) | ((c_to_cr & 0x1) << 12);
        }
    };

    /**
     * @struct addr_mod_fidelity_t
     * @brief Fidelity address modification for multi-precision operations
     *
     * Controls fidelity-related address modification for operations that
     * require multiple precision levels or phases. Limited to 2-bit increment.
     */
    struct addr_mod_fidelity_t
    {
        uint8_t incr = 0; ///< Fidelity increment value (0-3, 2 bits)
        uint8_t clr  = 0; ///< Clear fidelity counter flag

        /**
         * @brief Encode fidelity fields into hardware format
         * @return 16-bit encoded value for hardware register
         *
         * Bit layout: [Rsvd:13][CLR][INCR:2]
         */
        constexpr uint16_t val() const
        {
            return (incr & 0x3) | ((clr & 0x1) << 2);
        }
    };

    /**
     * @struct addr_mod_bias_t
     * @brief Bias address modification for bias addition operations
     *
     * Controls bias-related address modification for operations that add
     * bias terms. Supports 4-bit increment values for bias addressing.
     */
    struct addr_mod_bias_t
    {
        uint8_t incr = 0; ///< Bias increment value (0-15, 4 bits)
        uint8_t clr  = 0; ///< Clear bias counter flag

        /**
         * @brief Encode bias fields into hardware format
         * @return 16-bit encoded value for hardware register
         *
         * Bit layout: [Rsvd:11][CLR][INCR:4]
         */
        constexpr uint16_t val() const
        {
            return (incr & 0xF) | ((clr & 0x1) << 4);
        }
    };

    // Address Modifier Members
    addr_mod_src_t srca          = {}; ///< Source A address modification settings
    addr_mod_src_t srcb          = {}; ///< Source B address modification settings
    addr_mod_dest_t dest         = {}; ///< Destination address modification settings
    addr_mod_fidelity_t fidelity = {}; ///< Fidelity address modification settings
    addr_mod_bias_t bias         = {}; ///< Bias address modification settings
    addr_mod_src_t pack_ysrc     = {}; ///< Pack Y-source address modification settings
    addr_mod_src_t pack_ydst     = {}; ///< Pack Y-destination address modification settings

    /**
     * @brief Combine SrcA and SrcB values for hardware register
     * @return 16-bit value combining SrcA (low byte) and SrcB (high byte)
     *
     * The hardware source register combines both SrcA and SrcB address
     * modification settings into a single 16-bit register value.
     */
    constexpr uint16_t src_val() const
    {
        return srca.val() | (srcb.val() << 8);
    }

    /**
     * @brief Combine pack Y-source and Y-destination values
     * @return 16-bit value combining pack Y addressing settings
     *
     * Pack operations use specialized Y-dimension addressing that combines
     * source and destination Y-addressing into a single register.
     */
    constexpr uint16_t pack_val() const
    {
        return pack_ysrc.val() | (pack_ydst.val() << 6);
    }

    // Hardware Register Address Arrays

    /**
     * @brief Hardware register addresses for source address modification
     *
     * Array of register addresses for programming source address modification
     * settings across all 8 address modifier slots.
     */
    constexpr static uint32_t addr_mod_src_reg_addr[] = {
        ADDR_MOD_AB_SEC0_SrcAIncr_ADDR32, ///< Slot 0 source address register
        ADDR_MOD_AB_SEC1_SrcAIncr_ADDR32, ///< Slot 1 source address register
        ADDR_MOD_AB_SEC2_SrcAIncr_ADDR32, ///< Slot 2 source address register
        ADDR_MOD_AB_SEC3_SrcAIncr_ADDR32, ///< Slot 3 source address register
        ADDR_MOD_AB_SEC4_SrcAIncr_ADDR32, ///< Slot 4 source address register
        ADDR_MOD_AB_SEC5_SrcAIncr_ADDR32, ///< Slot 5 source address register
        ADDR_MOD_AB_SEC6_SrcAIncr_ADDR32, ///< Slot 6 source address register
        ADDR_MOD_AB_SEC7_SrcAIncr_ADDR32  ///< Slot 7 source address register
    };

    /**
     * @brief Hardware register addresses for destination address modification
     *
     * Array of register addresses for programming destination address modification
     * settings across all 8 address modifier slots.
     */
    constexpr static uint32_t addr_mod_dest_reg_addr[] = {
        ADDR_MOD_DST_SEC0_DestIncr_ADDR32, ///< Slot 0 destination address register
        ADDR_MOD_DST_SEC1_DestIncr_ADDR32, ///< Slot 1 destination address register
        ADDR_MOD_DST_SEC2_DestIncr_ADDR32, ///< Slot 2 destination address register
        ADDR_MOD_DST_SEC3_DestIncr_ADDR32, ///< Slot 3 destination address register
        ADDR_MOD_DST_SEC4_DestIncr_ADDR32, ///< Slot 4 destination address register
        ADDR_MOD_DST_SEC5_DestIncr_ADDR32, ///< Slot 5 destination address register
        ADDR_MOD_DST_SEC6_DestIncr_ADDR32, ///< Slot 6 destination address register
        ADDR_MOD_DST_SEC7_DestIncr_ADDR32  ///< Slot 7 destination address register
    };

    /**
     * @brief Hardware register addresses for bias address modification
     *
     * Array of register addresses for programming bias address modification
     * settings across all 8 address modifier slots.
     */
    constexpr static uint32_t addr_mod_bias_reg_addr[] = {
        ADDR_MOD_BIAS_SEC0_BiasIncr_ADDR32, ///< Slot 0 bias address register
        ADDR_MOD_BIAS_SEC1_BiasIncr_ADDR32, ///< Slot 1 bias address register
        ADDR_MOD_BIAS_SEC2_BiasIncr_ADDR32, ///< Slot 2 bias address register
        ADDR_MOD_BIAS_SEC3_BiasIncr_ADDR32, ///< Slot 3 bias address register
        ADDR_MOD_BIAS_SEC4_BiasIncr_ADDR32, ///< Slot 4 bias address register
        ADDR_MOD_BIAS_SEC5_BiasIncr_ADDR32, ///< Slot 5 bias address register
        ADDR_MOD_BIAS_SEC6_BiasIncr_ADDR32, ///< Slot 6 bias address register
        ADDR_MOD_BIAS_SEC7_BiasIncr_ADDR32  ///< Slot 7 bias address register
    };

    /**
     * @brief Hardware register addresses for pack address modification
     *
     * Array of register addresses for programming pack-specific address
     * modification settings. Pack operations use only 4 slots.
     */
    constexpr static uint32_t addr_mod_pack_reg_addr[] = {
        ADDR_MOD_PACK_SEC0_YsrcIncr_ADDR32, ///< Pack slot 0 Y-source register
        ADDR_MOD_PACK_SEC1_YsrcIncr_ADDR32, ///< Pack slot 1 Y-source register
        ADDR_MOD_PACK_SEC2_YsrcIncr_ADDR32, ///< Pack slot 2 Y-source register
        ADDR_MOD_PACK_SEC3_YsrcIncr_ADDR32  ///< Pack slot 3 Y-source register
    };

    /**
     * @brief Program address modification settings into hardware registers
     *
     * Programs all address modification settings (source, destination, bias,
     * and fidelity) into the specified hardware address modifier slot.
     *
     * @param mod_index Address modifier slot index (0-7)
     *
     * @note This function is always inlined for performance
     * @note The source register programming works around a compiler constraint issue
     */
    __attribute__((always_inline)) inline void set(const uint8_t mod_index) const
    {
        // KCM - This gets around issue: error: impossible constraint in 'asm'
        // TTI_SETC16(addr_mod_src_reg_addr[mod_index], src_val());
        TTI_SETC16(addr_mod_src_reg_addr[mod_index], srca.val() | (srcb.val() << 8));
        TTI_SETC16(addr_mod_dest_reg_addr[mod_index], dest.val() | (fidelity.val() << 13));
        TTI_SETC16(addr_mod_bias_reg_addr[mod_index], bias.val());
    }
};

/**
 * @struct addr_mod_pack_t
 * @brief Specialized address modification for pack operations
 *
 * Provides pack-specific address modification with Y and Z dimension control.
 * Pack operations require more complex addressing patterns to handle 2D output
 * data organization and support different pack modes.
 *
 * # Pack Address Architecture:
 * - **Y Dimension**: Full control with increment, clear, and carriage return
 * - **Z Dimension**: Reduced control (increment and clear only)
 * - **Source/Destination**: Independent Y/Z control for both directions
 *
 * # Address Encoding:
 * The pack address modifier combines Y and Z addressing into a single 16-bit
 * register value with optimized bit allocation for pack requirements.
 */
struct addr_mod_pack_t
{
    /**
     * @struct addr_mod_vals_t
     * @brief Y-dimension address modification with full control
     *
     * Provides complete address modification control for Y-dimension addressing
     * in pack operations. Supports 4-bit increment values and full control flags.
     */
    struct addr_mod_vals_t
    {
        uint8_t incr = 0; ///< Y-dimension increment value (0-15, 4 bits)
        uint8_t clr  = 0; ///< Clear Y-dimension counter flag
        uint8_t cr   = 0; ///< Y-dimension carriage return flag

        /**
         * @brief Encode Y-dimension fields into hardware format
         * @return 8-bit encoded value for Y-dimension control
         *
         * Bit layout: [Rsvd:2][CLR][CR][INCR:4]
         */
        constexpr uint8_t val() const
        {
            return (incr & 0xF) | ((cr & 0x1) << 4) | ((clr & 0x1) << 5);
        }
    };

    /**
     * @struct addr_mod_reduced_t
     * @brief Z-dimension address modification with reduced control
     *
     * Provides simplified address modification for Z-dimension addressing
     * in pack operations. Limited to 1-bit increment and clear control.
     */
    struct addr_mod_reduced_t
    {
        uint8_t incr = 0; ///< Z-dimension increment value (0-1, 1 bit)
        uint8_t clr  = 0; ///< Clear Z-dimension counter flag

        /**
         * @brief Encode Z-dimension fields into hardware format
         * @return 8-bit encoded value for Z-dimension control
         *
         * Bit layout: [Rsvd:6][CLR][INCR:1]
         */
        constexpr uint8_t val() const
        {
            return (incr & 0x1) | ((clr & 0x1) << 1);
        }
    };

    // Pack Address Modifier Members
    addr_mod_vals_t y_src    = {}; ///< Y-source address modification settings
    addr_mod_vals_t y_dst    = {}; ///< Y-destination address modification settings
    addr_mod_reduced_t z_src = {}; ///< Z-source address modification settings
    addr_mod_reduced_t z_dst = {}; ///< Z-destination address modification settings

    /**
     * @brief Combine all pack address modification values
     * @return 16-bit value combining Y-src, Y-dst, Z-src, and Z-dst settings
     *
     * Combines all pack address modification settings into the hardware format
     * required by pack address modification registers.
     *
     * Bit layout: [Z_DST:2][Z_SRC:2][Y_DST:6][Y_SRC:6]
     */
    __attribute__((always_inline)) inline constexpr uint16_t pack_val() const
    {
        return y_src.val() | (y_dst.val() << 6) | (z_src.val() << 12) | (z_dst.val() << 14);
    }

    /**
     * @brief Hardware register addresses for pack address modification
     *
     * Array of register addresses for programming pack-specific address
     * modification settings. Pack uses 4 dedicated address modifier slots.
     */
    constexpr static uint32_t addr_mod_pack_reg_addr[] = {
        ADDR_MOD_PACK_SEC0_YsrcIncr_ADDR32, ///< Pack slot 0 register
        ADDR_MOD_PACK_SEC1_YsrcIncr_ADDR32, ///< Pack slot 1 register
        ADDR_MOD_PACK_SEC2_YsrcIncr_ADDR32, ///< Pack slot 2 register
        ADDR_MOD_PACK_SEC3_YsrcIncr_ADDR32  ///< Pack slot 3 register
    };

    /**
     * @brief Program pack address modification settings into hardware
     *
     * Programs the combined pack address modification settings into the
     * specified pack address modifier slot.
     *
     * @param mod_index Pack address modifier slot index (0-3)
     *
     * @note This function is always inlined for performance
     * @note Pack operations use separate address modifier slots from general operations
     */
    __attribute__((always_inline)) inline void set(const uint8_t mod_index) const
    {
        TTI_SETC16(addr_mod_pack_reg_addr[mod_index], pack_val());
    }
};

/** @} */ // end of AddressModification group

} // namespace ckernel
