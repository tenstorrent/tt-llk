// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file llk_math_eltwise_unary_sfpu_params.h
 * @brief Parameterized unary SFPU operations for element-wise mathematical functions
 *
 * @details This module provides template-based infrastructure for executing unary SFPU
 * operations with custom parameters on Tenstorrent Tensix cores. It enables flexible
 * mathematical function execution with hardware-optimized vector processing modes.
 *
 * **Key Features:**
 * - Template-based operation dispatch with perfect forwarding
 * - Multiple vector processing modes (Row, Column, Full RC)
 * - Hardware-aware tile addressing and synchronization
 * - Zero-overhead parameter forwarding to SFPU functions
 * - Optimized pipeline coordination with STALLWAIT mechanisms
 *
 * **Hardware Integration:**
 * - Direct TTI instruction generation for optimal performance
 * - SFPU lane coordination across 32x32 tile layout
 * - Pipeline synchronization with MATH and CFG units
 * - Address modification management for concurrent operations
 *
 * **Performance Characteristics:**
 * - VectorMode::RC: Full tile processing (maximum throughput)
 * - VectorMode::R: Row vector processing (broadcast-optimized)
 * - VectorMode::C: Column vector processing (reduction-optimized)
 *
 * **Usage Examples:**
 * ```cpp
 * // Full tile GELU operation with approximation
 * _llk_math_eltwise_unary_sfpu_params_<true>(
 *     [](float param) { TTI_SFPGELU(param); },
 *     dst_index, VectorMode::RC, gelu_param
 * );
 * 
 * // Row vector exponential with custom base
 * _llk_math_eltwise_unary_sfpu_params_<false>(
 *     [](float base) { TTI_SFPEXP(base); },
 *     dst_index, VectorMode::R, exp_base
 * );
 * ```
 */

#pragma once
#include <utility>

#include "llk_math_eltwise_unary_sfpu.h"
#include "llk_sfpu_types.h"

/**
 * @brief Execute parameterized unary SFPU operation with configurable vector processing mode
 * 
 * @tparam APPROXIMATE Enable approximate mode for faster execution (hardware default: true)
 * @tparam Callable SFPU function type (lambda, function pointer, or functor)
 * @tparam Args Variadic parameter pack for SFPU function arguments
 * 
 * @param sfpu_func SFPU operation callable with perfect forwarding semantics
 * @param dst_index Destination tile index in DEST register file (0-15 for standard configuration)
 * @param vector_mode Vector processing mode controlling execution pattern:
 *                    - VectorMode::RC (0): Full tile processing (4 faces, maximum throughput)
 *                    - VectorMode::R (1): Row vector processing (faces 0+1, broadcast optimization)
 *                    - VectorMode::C (2): Column vector processing (faces 0+2, reduction optimization)
 *                    - Other: Single execution (minimal processing for scalar operations)
 * @param args Variadic arguments forwarded to the SFPU function
 * 
 * @details This function provides the core infrastructure for executing parameterized unary
 * SFPU operations across different vector processing patterns. It manages hardware-level
 * details including tile addressing, pipeline synchronization, and face iteration.
 * 
 * **Pipeline Synchronization Strategy:**
 * - STALLWAIT(STALL_SFPU, MATH) ensures proper ordering with matrix operations
 * - Address modification coordination prevents conflicts with concurrent operations
 * - TTI_SETRWC instructions advance through tile faces with hardware-optimized patterns
 * - STALLWAIT(STALL_CFG, WAIT_SFPU) ensures completion before configuration changes
 * 
 * **Vector Mode Performance Characteristics:**
 * - **VectorMode::RC**: Processes all 4 tile faces (32x32 elements), maximum SFPU utilization
 * - **VectorMode::R**: Processes faces 0+1 (first row of 2x2 face layout), optimized for row broadcasts
 * - **VectorMode::C**: Processes faces 0+2 (first column of 2x2 face layout), optimized for column reductions
 * - **Scalar mode**: Single execution, minimal overhead for element-wise operations
 * 
 * **Hardware Constraints:**
 * - Destination index must be within DEST register file capacity
 * - SFPU function must use TTI_SFP* instructions for hardware execution
 * - Parameters must be compatible with SFPU data types (typically float32)
 * - Face iteration count depends on vector mode (2 or 4 faces)
 * 
 * **Memory and Performance Impact:**
 * - Zero-overhead parameter forwarding using perfect forwarding
 * - Template instantiation per APPROXIMATE mode and callable type
 * - Hardware pipeline coordination minimizes stall cycles
 * - Address modification base management ensures clean operation state
 * 
 * **Usage Patterns:**
 * 
 * ```cpp
 * // Exponential with custom base parameter
 * _llk_math_eltwise_unary_sfpu_params_<true>(
 *     [](float base) { 
 *         TTI_SFPEXEXP(base);  // Hardware exponential with base
 *     }, 
 *     dst_index, VectorMode::RC, 2.71828f
 * );
 * 
 * // GELU activation with approximation coefficient  
 * _llk_math_eltwise_unary_sfpu_params_<true>(
 *     [](float coeff) { 
 *         TTI_SFPGELU(coeff);  // GELU with custom coefficient
 *     }, 
 *     dst_index, VectorMode::RC, 1.702f
 * );
 * 
 * // Power function with precise mode
 * _llk_math_eltwise_unary_sfpu_params_<false>(
 *     [](float exponent) { 
 *         TTI_SFPPOW(exponent);  // Precise power operation
 *     }, 
 *     dst_index, VectorMode::RC, 2.0f
 * );
 * 
 * // Row vector normalization with scaling factor
 * _llk_math_eltwise_unary_sfpu_params_<true>(
 *     [](float scale) { 
 *         TTI_SFPMUL(scale);   // Multiply by normalization factor
 *     }, 
 *     dst_index, VectorMode::R, norm_factor
 * );
 * ```
 * 
 * @warning Ensure SFPU function uses TTI_SFP* instructions compatible with hardware
 * @warning Destination index must be valid for current DEST register file configuration
 * @warning Vector mode must match expected data layout for correct results
 * 
 * @see llk_math_eltwise_unary_sfpu.h for non-parameterized unary operations
 * @see llk_sfpu_types.h for VectorMode enumeration and SFPU type definitions
 */
template <bool APPROXIMATE, typename Callable, typename... Args>
inline void _llk_math_eltwise_unary_sfpu_params_(Callable&& sfpu_func, uint dst_index, int vector_mode = (int)VectorMode::RC, Args&&... args)
{
    // **HARDWARE SETUP**: Configure destination tile addressing for 32x32 tile layout
    math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(dst_index);
    math::set_addr_mod_base();  // Initialize address modification base for this operation

    // **PIPELINE SYNCHRONIZATION**: Ensure SFPU is ready and MATH unit coordination
    TTI_STALLWAIT(p_stall::STALL_SFPU, p_stall::MATH);
    VectorMode mode = static_cast<VectorMode>(vector_mode);

    if (mode == VectorMode::R)
    {
        // **ROW VECTOR PROCESSING**: Process faces 0+1 (top row of 2x2 face layout)
        // Optimized for row broadcast operations where same operation applies across columns
#pragma GCC unroll 0  // Prevent loop unrolling for instruction cache efficiency
        for (int face = 0; face < 2; face++)
        {
            // Execute SFPU function with perfect parameter forwarding
            std::forward<Callable>(sfpu_func)(std::forward<Args>(args)...);
            
            // **FACE ADVANCEMENT**: Move through 8 rows (half of 16-row face) twice per face
            // Each SETRWC advances by 8 rows in the DEST register file
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
        }
        
        // **SKIP REMAINING FACES**: Advance through faces 2+3 without processing
        // Maintains proper addressing state for subsequent operations
        TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
        TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
        TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
        TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
    }
    else if (mode == VectorMode::C)
    {
        // **COLUMN VECTOR PROCESSING**: Process faces 0+2 (left column of 2x2 face layout)
        // Optimized for column reduction operations where operation applies across rows
#pragma GCC unroll 0  // Prevent loop unrolling for instruction cache efficiency
        for (int face = 0; face < 2; face++)
        {
            // Execute SFPU function with perfect parameter forwarding
            std::forward<Callable>(sfpu_func)(std::forward<Args>(args)...);
            
            // **COMPLETE FACE PROCESSING**: Process all 4 blocks of 4 rows each (16 rows total)
            // Column mode requires complete face processing for proper data layout
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
        }
    }
    else if (mode == VectorMode::RC)
    {
        // **FULL TILE PROCESSING**: Process all 4 faces for complete 32x32 tile coverage
        // Maximum SFPU utilization - processes entire tile with optimal throughput
#pragma GCC unroll 0  // Prevent loop unrolling for instruction cache efficiency
        for (int face = 0; face < 4; face++)
        {
            // Execute SFPU function with perfect parameter forwarding
            std::forward<Callable>(sfpu_func)(std::forward<Args>(args)...);
            
            // **OPTIMIZED FACE ADVANCEMENT**: 2 SETRWC instructions per face (8 rows each)
            // Efficient traversal through 16 rows per face (2 blocks of 8 rows)
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
        }
    }
    else
    {
        // **SCALAR MODE**: Single execution without face iteration
        // Used for operations that don't require vector processing patterns
        std::forward<Callable>(sfpu_func)(std::forward<Args>(args)...);
    }
    
    // **HARDWARE CLEANUP**: Clear destination addressing state
    math::clear_dst_reg_addr();

    // **PIPELINE SYNCHRONIZATION**: Ensure SFPU completion before configuration changes
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::WAIT_SFPU);
    math::clear_addr_mod_base();  // Clean address modification state for next operation
}
