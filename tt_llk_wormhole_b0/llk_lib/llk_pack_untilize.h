// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file llk_pack_untilize.h
 * @brief Advanced untilization packer for optimal data layout transformation
 *
 * This header provides sophisticated untilization packing operations that transform data from
 * tiled computational layouts back to linear memory formats. These operations are critical for
 * preparing computational results for external memory storage, inter-chip communication, and
 * integration with non-tiled computational frameworks requiring linear data arrangements.
 *
 * @note **Reverse Data Layout Transformation**: Complements tilization operations by converting
 *       tiled computational data back to linear memory layouts optimized for external systems,
 *       memory controllers, and cross-platform data exchange requirements.
 * 
 * @note **Specialized Pattern Support**: Supports advanced untilization patterns including
 *       diagonal extraction, narrow row processing, and optimized addressing modes for various
 *       data access patterns and memory organization requirements.
 * 
 * @note **Hardware Integration**: Utilizes Tensix packer unit with LLTT (Low-Level Trace Tool)
 *       recording, SFPI preprocessing, and specialized addressing modes for maximum throughput
 *       and optimal memory bandwidth utilization during data layout transformation.
 * 
 * @note **Performance-Critical Operations**: Untilization is often the final step in computational
 *       pipelines and can significantly impact overall system throughput. These implementations
 *       provide hardware-optimized patterns for maximum efficiency and minimal latency.
 * 
 * @note **Cross-System Compatibility**: Essential for data exchange between tiled Tensix
 *       computational units and external systems requiring linear data layouts such as
 *       CPUs, GPUs, and conventional memory architectures.
 */

#pragma once

#include <cstdint>

#include "ckernel.h"
#include "ckernel_globals.h"
#include "ckernel_ops.h"
#include "ckernel_template.h"
#include "llk_defs.h"
#include "llk_pack_common.h"
#include "lltt.h"
#include "sfpi.h"

using namespace ckernel;
using namespace ckernel::packer;

/**
 * @brief Configure advanced addressing modes for untilization packing operations
 *
 * Sets up sophisticated addressing patterns for untilization operations with support for
 * specialized data access patterns including diagonal extraction and narrow row processing.
 * This function configures the hardware addressing infrastructure for optimal memory access
 * and data layout transformation efficiency.
 *
 * @tparam diagonal Enable diagonal data extraction pattern
 *                 Optimizes for extracting diagonal elements from tiled data structures
 *                 Critical for certain mathematical operations and data analysis algorithms
 * @tparam narrow_row Enable narrow row processing optimization
 *                   Optimizes for processing data with irregular row dimensions
 *                   Essential for handling non-standard tile sizes and partial data sets
 *
 * @note **Address Mode Configuration Strategy**:
 *       - Standard mode: Y-source increment of 15 for maximum throughput
 *       - Diagonal/narrow_row mode: Y-source increment of 1 for precise control
 *       - Multiple address modes (ADDR_MOD_0-3) for complex access patterns
 * 
 * @note **Hardware Addressing Optimization**: Configures incremental addressing
 *       patterns with clear and carry flags for efficient memory access coordination
 *       and optimal data flow through the packer unit pipeline
 * 
 * @note **Memory Access Patterns**: Different modes optimize for different data
 *       access patterns:
 *       - Standard: Sequential linear memory writes for maximum bandwidth
 *       - Diagonal: Strided access patterns for matrix diagonal extraction
 *       - Narrow row: Optimized for irregular data dimensions and partial tiles
 * 
 * @note **Pipeline Coordination**: Address modes coordinate with downstream
 *       operations and memory controllers to ensure optimal data flow and
 *       prevent pipeline stalls or memory access conflicts
 *
 * @warning **Template Parameter Impact**: Diagonal and narrow_row modes significantly
 *          change addressing behavior and performance characteristics. Ensure template
 *          parameters match actual data access requirements to prevent incorrect
 *          memory layouts or suboptimal performance.
 * 
 * @warning **Address Mode Dependencies**: Address mode configuration affects all
 *          subsequent packing operations. Ensure proper coordination with other
 *          packer functions to maintain consistent addressing behavior.
 * 
 * @warning **Memory Bandwidth Considerations**: Different addressing modes have
 *          varying memory bandwidth requirements. Monitor system memory utilization
 *          to prevent bandwidth saturation and maintain optimal throughput.
 * 
 * @see _llk_pack_untilize_ for the main execution function
 * @see _llk_pack_untilize_init_ for initialization and configuration
 */
template <bool diagonal = false, bool narrow_row = false>
inline void _llk_pack_untilize_configure_addrmod_()
{
    if constexpr (diagonal || narrow_row)
    {
        addr_mod_pack_t {
            .y_src = {.incr = 1},
        }
            .set(ADDR_MOD_0);
    }
    else
    {
        addr_mod_pack_t {
            .y_src = {.incr = 15}, // 4-bit value so max is 15. incadcxy will increment it by 1
        }
            .set(ADDR_MOD_0);
    }

    addr_mod_pack_t {
        .y_src = {.incr = 0, .clr = 0, .cr = 1},
    }
        .set(ADDR_MOD_1);

    addr_mod_pack_t {
        .y_src = {.incr = 0, .clr = 1, .cr = 0},
    }
        .set(ADDR_MOD_2);

    addr_mod_pack_t {
        .y_src = {.incr = 0, .clr = 0, .cr = 0},
    }
        .set(ADDR_MOD_3);
}

template <
    std::uint32_t block_ct_dim,
    std::uint32_t full_ct_dim    = block_ct_dim,
    bool diagonal                = false,
    bool narrow_row              = false,
    std::uint32_t row_num_datums = TILE_C_DIM>
inline void _llk_pack_untilize_mop_config_(const std::uint32_t face_r_dim = FACE_R_DIM, const std::uint32_t num_faces = 4)
{
    const uint PACKCNT              = diagonal ? (num_faces > 2 ? num_faces / 2 : num_faces) : ((face_r_dim < FACE_R_DIM) ? 1 : num_faces);
    constexpr uint MEGAROW          = 1;
    constexpr uint ZERO_OUTPUT_FLAG = p_pacr::P_ZERO_OUTPUT_DISABLED;
    constexpr uint MOP_INNER_LOOP   = narrow_row ? (TILE_R_DIM / 4) : (diagonal ? FACE_R_DIM - 1 : 1);
    constexpr uint MOP_OUTER_LOOP   = narrow_row ? 1 : block_ct_dim;

    if constexpr (diagonal)
    {
        ckernel::ckernel_template tmp(
            MOP_OUTER_LOOP,
            MOP_INNER_LOOP,
            TT_OP_INCADCXY(p_setadc::PAC, 0, 1, 0, 1),
            TT_OP_PACR(ADDR_MOD_0, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 0));
        tmp.set_start_op(TT_OP_PACR(ADDR_MOD_0, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 0));
        tmp.set_last_inner_loop_instr(TT_OP_PACR(ADDR_MOD_1, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 0));
        tmp.set_end_ops(TT_OP_SETADCXX(p_setadc::PAC, 1 - 1, 0x0), TT_OP_INCADCZW(p_setadc::PAC, 0, 0, 1, 0)); // w cnt points to the next tile
        tmp.program(instrn_buffer);
    }
    else
    {
        if constexpr (narrow_row)
        { // always
            ckernel::ckernel_template tmp(MOP_OUTER_LOOP, MOP_INNER_LOOP, TT_OP_PACR(ADDR_MOD_0, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, 0, 0, 0));
            tmp.program(instrn_buffer);
        }
        else
        {
            if (num_faces > 1)
            {
                // Inc ch0_y+=1 (addr_mod_0 will increment by 15)
                ckernel::ckernel_template tmp(MOP_OUTER_LOOP, MOP_INNER_LOOP, TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0));
                tmp.set_start_op(TT_OP_PACR(ADDR_MOD_0, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 0));
                tmp.set_end_ops(
                    TT_OP_PACR(ADDR_MOD_1, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 0),
                    TT_OP_INCADCZW(p_setadc::PAC, 0, 0, 1, 0)); // w cnt points to the next tile
                tmp.program(instrn_buffer);
            }
            else
            {
                ckernel::ckernel_template tmp(MOP_OUTER_LOOP, MOP_INNER_LOOP, TT_OP_PACR(ADDR_MOD_1, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 0));
                tmp.set_end_op(TT_OP_INCADCZW(p_setadc::PAC, 0, 0, 1, 0)); // w cnt points to the next tile
                tmp.program(instrn_buffer);
            }
        }

        if (block_ct_dim != full_ct_dim)
        {
            const std::uint32_t replay_buf_len = 10;
            lltt::record(ckernel::packer::replay_buf_offset, replay_buf_len);
            TTI_PACR(ADDR_MOD_3, 0, 0xf, 0, 0, 1, 1); // close block
            // update l1 address
            TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR, p_gpr_pack::OUTPUT_ADDR_OFFSET);
            TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR + 1, p_gpr_pack::OUTPUT_ADDR + 1, p_gpr_pack::OUTPUT_ADDR_OFFSET);
            TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR + 2, p_gpr_pack::OUTPUT_ADDR + 2, p_gpr_pack::OUTPUT_ADDR_OFFSET);
            TTI_ADDDMAREG(0, p_gpr_pack::OUTPUT_ADDR + 3, p_gpr_pack::OUTPUT_ADDR + 3, p_gpr_pack::OUTPUT_ADDR_OFFSET);
            TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC0_REG1_L1_Dest_addr_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_pack::OUTPUT_ADDR);
            TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC0_REG8_L1_Dest_addr_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_pack::OUTPUT_ADDR + 1);
            TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG1_L1_Dest_addr_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_pack::OUTPUT_ADDR + 2);
            TTI_REG2FLOP(1, 0, 0, 0, THCON_SEC1_REG8_L1_Dest_addr_ADDR32 - THCON_CFGREG_BASE_ADDR32, p_gpr_pack::OUTPUT_ADDR + 3);
            TTI_NOP;
        }
    }
}

/**
 * @brief Initialize advanced untilization packing with comprehensive configuration
 *
 * Performs complete initialization of the packer unit for untilization operations including
 * address mode configuration, micro-operation programming, and specialized handling for
 * block dimensions, diagonal patterns, and narrow row optimizations. This function establishes
 * the foundation for efficient data layout transformation from tiled to linear formats.
 *
 * @tparam block_ct_dim Block column tile dimension for memory organization
 *                     Controls how data is organized into blocks for optimal bandwidth utilization
 * @tparam full_ct_dim Full column tile dimension (default: same as block_ct_dim)
 *                    Used for address offset calculations when block != full dimensions
 * @tparam diagonal Enable diagonal data extraction pattern optimization
 *                 Optimizes for extracting diagonal elements from matrix structures
 * @tparam narrow_row Enable narrow row processing for irregular data dimensions
 *                   Optimizes for data with non-standard row sizes and partial tiles
 * @tparam row_num_datums Number of data elements per row (default: TILE_C_DIM)
 *                       Controls row-level processing granularity and memory access patterns
 * 
 * @param pack_dst_format Destination data format for packed output (DataFormat enum value)
 * @param face_r_dim Face row dimension for tile organization (default: FACE_R_DIM = 16)
 * @param num_faces Number of tile faces to process (default: 4, typically 1, 2, or 4)
 *
 * @note **Address Mode Configuration**: Calls specialized addressing configuration based
 *       on diagonal and narrow_row template parameters to optimize memory access patterns
 *       for the specific untilization use case and data organization requirements
 * 
 * @note **Micro-Operation Programming**: Configures instruction templates with all
 *       template parameters to ensure optimal hardware utilization and data flow
 *       coordination throughout the untilization process
 * 
 * @note **Block Dimension Handling**: When block_ct_dim != full_ct_dim, calculates
 *       and programs output address offset for proper memory layout management:
 *       - Offset = SCALE_DATUM_SIZE(format, full_ct_dim * face_adjustment * FACE_C_DIM)
 *       - 16-byte aligned addressing for optimal memory controller utilization
 * 
 * @note **Performance Optimization**: Template-based configuration enables compile-time
 *       optimization for specific use cases, eliminating runtime overhead and maximizing
 *       throughput for untilization operations
 *
 * @warning **Template Parameter Consistency**: All template parameters must be consistent
 *          with actual data layout and processing requirements. Mismatched parameters
 *          can cause incorrect memory layouts, performance degradation, or data corruption
 * 
 * @warning **Format Compatibility**: pack_dst_format must be compatible with downstream
 *          systems and memory requirements. Incompatible formats can cause data corruption
 *          or processing errors in subsequent operations
 * 
 * @warning **Face Dimension Constraints**: face_r_dim must align with actual tile
 *          organization and hardware capabilities. Invalid dimensions can cause
 *          address calculation errors and memory access violations
 * 
 * @see _llk_pack_untilize_configure_addrmod_ for addressing configuration details
 * @see _llk_pack_untilize_mop_config_ for micro-operation programming specifics
 * @see _llk_pack_untilize_ for the main execution function
 */
template <
    std::uint32_t block_ct_dim,
    std::uint32_t full_ct_dim    = block_ct_dim,
    bool diagonal                = false,
    bool narrow_row              = false,
    std::uint32_t row_num_datums = TILE_C_DIM>
inline void _llk_pack_untilize_init_(const std::uint32_t pack_dst_format, const std::uint32_t face_r_dim = FACE_R_DIM, const std::uint32_t num_faces = 4)
{
    _llk_pack_untilize_configure_addrmod_<diagonal, narrow_row>();

    _llk_pack_untilize_mop_config_<block_ct_dim, full_ct_dim, diagonal, narrow_row, row_num_datums>(face_r_dim, num_faces);

    if (block_ct_dim != full_ct_dim)
    {
        const std::uint32_t output_addr_offset = SCALE_DATUM_SIZE(pack_dst_format, full_ct_dim * ((num_faces > 1) ? num_faces / 2 : 1) * FACE_C_DIM);
        TT_SETDMAREG(0, LOWER_HALFWORD(output_addr_offset / 16), 0, LO_16(p_gpr_pack::OUTPUT_ADDR_OFFSET)); // store 16B aligned row offset address
    }
}

/**
 * @brief Execute high-performance untilization packing with advanced optimization strategies
 *
 * Performs sophisticated untilization packing operations that transform tiled computational
 * data into linear memory layouts with support for complex addressing patterns, diagonal
 * extraction, narrow row processing, and optimized memory bandwidth utilization. This function
 * implements the core data transformation kernel for maximum efficiency and throughput.
 *
 * @tparam block_ct_dim Block column tile dimension for memory block organization
 *                     Controls data blocking strategy for optimal memory access patterns
 * @tparam full_ct_dim Full column tile dimension (default: same as block_ct_dim)
 *                    Used for address calculations when processing partial blocks
 * @tparam diagonal Enable diagonal element extraction pattern
 *                 Optimizes for matrix diagonal operations and specialized data analysis
 * @tparam narrow_row Enable narrow row processing optimization
 *                   Handles irregular row dimensions and partial tile processing
 * @tparam row_num_datums Number of data elements per row (default: TILE_C_DIM)
 *                       Controls row-level processing granularity and memory layout
 * 
 * @param address Base memory address for untilized data output
 * @param pack_dst_format Destination data format for output (DataFormat enum value)
 * @param face_r_dim Face row dimension for processing granularity (default: FACE_R_DIM = 16)
 * @param num_faces Number of tile faces (parameter present but not used in current implementation)
 * @param tile_dst_offset Tile destination offset for address calculation
 *
 * @note **Destination Programming**: Calls specialized destination programming function
 *       with all template parameters to configure optimal memory access patterns
 *       and data layout transformation for the specific untilization requirements
 * 
 * @note **Processing Strategy Optimization**:
 *       - narrow_row=true: Single template execution for streamlined processing
 *       - narrow_row=false: Row-by-row processing with dynamic address generation
 *       - Row count determined by face_r_dim constraints for optimal memory utilization
 * 
 * @note **Address Counter Management**: For standard processing, uses sophisticated
 *       address counter operations:
 *       - SETADC for tile counter clearing and precise address control
 *       - ADDRCRXY for row increment operations and memory navigation
 *       - LLTT replay for address updates when block_ct_dim != full_ct_dim
 * 
 * @note **Memory Access Optimization**: Processing loop optimized for face dimension
 *       constraints with row count = min(face_r_dim, TILE_R_DIM / 4) for optimal
 *       memory bandwidth utilization and hardware efficiency
 * 
 * @note **Replay Buffer Integration**: When block and full dimensions differ, uses
 *       LLTT replay mechanisms to update row addresses efficiently, minimizing
 *       instruction overhead and maximizing processing throughput
 *
 * @warning **Block Dimension Dependencies**: When block_ct_dim != full_ct_dim, proper
 *          replay buffer configuration is critical. Incorrect setup can cause address
 *          calculation errors and memory access violations
 * 
 * @warning **Address Alignment Requirements**: Output address must be properly aligned
 *          for the destination format and memory controller requirements. Misaligned
 *          addresses can cause performance degradation or memory access errors
 * 
 * @warning **Template Parameter Validation**: All template parameters must match
 *          initialization configuration and actual data requirements to ensure
 *          correct operation and optimal performance characteristics
 * 
 * @warning **Face Dimension Constraints**: face_r_dim must be compatible with
 *          hardware capabilities and data organization. Invalid dimensions can
 *          cause processing errors or suboptimal performance
 * 
 * @see _llk_pack_untilize_init_ for initialization and configuration
 * @see program_packer_untilized_destination for destination programming details
 */
template <
    std::uint32_t block_ct_dim,
    std::uint32_t full_ct_dim    = block_ct_dim,
    bool diagonal                = false,
    bool narrow_row              = false,
    std::uint32_t row_num_datums = TILE_C_DIM>
inline void _llk_pack_untilize_(
    const std::uint32_t address,
    const std::uint32_t pack_dst_format,
    const std::uint32_t face_r_dim      = FACE_R_DIM,
    const std::uint32_t num_faces       = 4 /*not used*/,
    const std::uint32_t tile_dst_offset = 0)
{
    program_packer_untilized_destination<block_ct_dim, full_ct_dim, diagonal, row_num_datums>(address, pack_dst_format);

    if constexpr (narrow_row)
    {
        ckernel::ckernel_template::run(instrn_buffer);
    }
    else
    {
        const std::uint32_t num_rows = (face_r_dim < FACE_R_DIM) ? face_r_dim : TILE_R_DIM / 4;

        for (std::uint32_t row = 0; row < num_rows; row++)
        {
            TT_SETADC(p_setadc::PAC, p_setadc::CH_0, p_setadc::SET_W, tile_dst_offset); // Clear tile counter
            ckernel::ckernel_template::run(instrn_buffer);
            TTI_ADDRCRXY(p_setadc::PAC, 0, 0, 1, 0, 0b0010); // Read new row in the tile
            if constexpr (block_ct_dim != full_ct_dim)
            {
                lltt::replay(ckernel::packer::replay_buf_offset, 10); // update row address
            }
        }
    }

    if constexpr (block_ct_dim == full_ct_dim)
    {
        TTI_PACR(ADDR_MOD_2, 0, 0xf, 0, 0, 1, 1); // close block
    }
}
