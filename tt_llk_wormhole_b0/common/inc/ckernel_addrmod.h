// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_addrmod.h
 * @brief Address generation and modification for Wormhole B0 Tensix data access patterns
 *
 * @details This header provides sophisticated address generation capabilities optimized
 * for the Wormhole B0 Tensix architecture's complex tensor data access patterns. The
 * address generation system supports multi-dimensional addressing with stride control,
 * automatic increment patterns, and carriage return functionality for efficient
 * tensor traversal and data movement operations.
 *
 * **Wormhole B0 Address Generation Architecture:**
 * The Tensix engine features advanced address counters for efficient tensor operations:
 * - **X, Y, Z Counters**: Multi-dimensional address generation for tensor traversal
 * - **Stride Control**: Configurable stride values for non-contiguous data access
 * - **Carriage Return (CR)**: Automatic return to previous positions for reuse patterns
 * - **Base Address + Offset**: Flexible addressing with programmable base and calculated offsets
 *
 * **Address Counter System:**
 * Each thread and unpacker engine has separate counter instances:
 * - **Thread-Specific Contexts**: Independent counters per thread (Unpack, Math, Pack)
 * - **Dual Channel Design**: Channel 0 (source addresses), Channel 1 (destination addresses)
 * - **Per-Unpacker Contexts**: Separate contexts for Unpacker 0 and Unpacker 1
 * - **Dynamic Configuration**: Runtime programmable via WRCFG and address modification
 *
 * **Address Modification Methods:**
 * 1. **SETADC Instructions**: Direct counter value setting (SETADCXY, SETADCZW)
 * 2. **INCADC Instructions**: Counter increment by specified amounts (INCADCXY, INCADCZW)
 * 3. **ADDR_MOD Side Effects**: Automatic increment via unpacker instruction fields
 * 4. **REG2FLOP Backdoor**: GPR-based counter setting (debug/special use only)
 *
 * **Carriage Return (CR) Functionality:**
 * Advanced counter management for complex access patterns:
 * - **Position Counter**: Current address calculation position
 * - **CR Counter**: Stored position for return operations
 * - **Return Operations**: ADDRCRXY/ADDRCRZW instructions for position restoration
 * - **Reuse Patterns**: Efficient handling of matrix multiplication and convolution patterns
 *
 * **Performance Optimization:**
 * - **Hardware Acceleration**: Counter updates integrated with data movement operations
 * - **Parallel Operation**: Address generation doesn't stall computation pipeline
 * - **Pattern Recognition**: Optimized for common AI workload access patterns
 * - **Memory Bank Optimization**: Address patterns optimized for L1 bank interleaving
 */

#pragma once

// MT: This should be dissolved and moved to the appropriate place
#include "cfg_defines.h"
#include "ckernel_ops.h"

namespace ckernel
{

/**
 * @brief Address modification mode 0 - no automatic increment
 * @details Disables automatic address counter increment, maintaining
 * current counter values for repeated access to the same location.
 */
constexpr uint8_t ADDR_MOD_0 = 0;

/**
 * @brief Address modification mode 1 - increment by 1
 * @details Enables automatic address counter increment by 1 after
 * each data access operation, suitable for sequential data traversal.
 */
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

/**
 * @brief Address modification structure for Wormhole B0 Tensix address generation
 * @details Provides comprehensive address modification structures for controlling
 * the sophisticated X,Y,Z counter system and carriage return functionality in
 * the Wormhole B0 Tensix engine. These structures enable efficient tensor
 * traversal patterns optimized for AI workload access patterns.
 */
struct addr_mod_t
{
    /**
     * @brief Source register (SRCA/SRCB) address modification parameters
     * @details Controls address generation for SRCA and SRCB register files
     * (64×16 datums each) with 4-bit increment resolution and carriage return
     * functionality. Optimized for operand access patterns in matrix operations.
     * 
     * **Wormhole B0 Source Register Addressing:**
     * - **4-bit Increment**: Fine-grained addressing control (0-15 increment range)
     * - **Carriage Return**: Enables efficient matrix operand reuse patterns
     * - **Clear Function**: Reset addressing state for fresh computation sequences
     * - **Register File Integration**: Direct hardware integration with SRCA/SRCB
     */
    struct addr_mod_src_t
    {
        /**
         * @brief Address increment value (4-bit range: 0-15)
         * @details Controls how much the address counter advances after each access.
         * Optimized for common tensor access patterns and register file organization.
         */
        uint8_t incr = 0;
        
        /**
         * @brief Clear address counter flag
         * @details When set, resets the address counter to its base value,
         * enabling fresh addressing sequences for new computation phases.
         */
        uint8_t clr  = 0;
        
        /**
         * @brief Carriage return operation flag  
         * @details When set, performs carriage return operation to restore
         * previous address position, essential for matrix multiplication
         * operand reuse patterns in Wormhole B0's 2048-multiplier architecture.
         */
        uint8_t cr   = 0;

        /**
         * @brief Generate hardware-compatible address modification value
         * @return Packed address modification value for Tensix instruction encoding
         * @details Combines increment, carriage return, and clear flags into
         * hardware-compatible format for Tensix address generation instructions.
         */
        constexpr uint8_t val() const
        {
            return (incr & SRC_INCR_MASK) | ((cr & 0x1) << 6) | ((clr & 0x1) << 7);
        }
    };

    /**
     * @brief Destination register (DEST) address modification parameters
     * @details Controls address generation for the DEST register file with
     * 8-bit increment resolution and extended carriage return functionality.
     * Supports both 16-bit mode (1024×16 datums) and 32-bit mode (512×16 datums).
     * 
     * **Wormhole B0 DEST Register Addressing:**
     * - **8-bit Increment**: Extended addressing range for large DEST register file
     * - **Signed Increment**: Supports bidirectional addressing patterns
     * - **Carriage Return**: Advanced CR functionality with copy-to-CR option
     * - **Double Buffering**: Coordinates with dual-bank DEST register architecture
     */
    struct addr_mod_dest_t
    {
        /**
         * @brief Address increment value (8-bit signed range: -128 to +127)
         * @details Controls DEST register addressing advancement with support
         * for both forward and backward addressing patterns. Enables complex
         * tensor access patterns for accumulation and recirculation operations.
         */
        int16_t incr    = 0;
        
        /**
         * @brief Clear DEST address counter flag
         * @details Resets DEST register addressing to base position for
         * fresh computation sequences and double-buffering coordination.
         */
        uint8_t clr     = 0;
        
        /**
         * @brief Carriage return operation flag
         * @details Performs carriage return to restore previous DEST addressing
         * position, supporting complex accumulation patterns and iterative algorithms.
         */
        uint8_t cr      = 0;
        
        /**
         * @brief Copy current position to carriage return flag
         * @details Advanced carriage return functionality that captures current
         * address position for later restoration, enabling sophisticated
         * addressing patterns for matrix operations and convolution algorithms.
         */
        uint8_t c_to_cr = 0;

        /**
         * @brief Generate hardware-compatible DEST address modification value
         * @return Packed 16-bit address modification value for Tensix instructions
         * @details Combines all DEST addressing parameters into hardware format
         * compatible with Wormhole B0 DEST register address generation logic.
         */
        constexpr uint16_t val() const
        {
            return (incr & DEST_INCR_MASK) | ((cr & 0x1) << 10) | ((clr & 0x1) << 11) | ((c_to_cr & 0x1) << 12);
        }
    };

    /**
     * @brief Math fidelity phase address modification parameters
     * @details Controls address generation for the Wormhole B0 fidelity phase
     * system that enables precision vs. performance trade-offs in the 2048
     * hardware multipliers. Supports 4 fidelity phases for full precision.
     * 
     * **Wormhole B0 Fidelity Phase System:**
     * - **4 Fidelity Phases**: Phases 0-3 for complete precision coverage
     * - **Precision Control**: Trade-off between accuracy and performance
     * - **2-bit Increment**: Phase advancement control (0-3)
     * - **Clear Function**: Reset fidelity phase sequencing
     */
    struct addr_mod_fidelity_t
    {
        /**
         * @brief Fidelity phase increment value (2-bit range: 0-3)
         * @details Controls advancement through the 4 fidelity phases,
         * enabling precise control over precision vs. performance trade-offs
         * in the Wormhole B0 2048-multiplier mathematical engine.
         */
        uint8_t incr = 0;
        
        /**
         * @brief Clear fidelity phase counter flag
         * @details Resets fidelity phase sequencing to initial state,
         * typically used at the beginning of new mathematical operations
         * requiring specific precision characteristics.
         */
        uint8_t clr  = 0;

        /**
         * @brief Generate hardware-compatible fidelity address modification value
         * @return Packed fidelity modification value for Tensix instruction encoding
         * @details Combines fidelity increment and clear flags for hardware
         * compatibility with Wormhole B0 fidelity phase control system.
         */
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

} // namespace ckernel
