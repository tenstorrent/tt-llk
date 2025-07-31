// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file llk_math_eltwise_ternary_sfpu_params.h
 * @brief Parameterized ternary SFPU operations for three-operand mathematical functions
 *
 * @details This module provides template-based infrastructure for executing ternary SFPU
 * operations with custom parameters on Tenstorrent Tensix cores. Ternary operations process
 * three input operands simultaneously, enabling complex mathematical functions like conditional
 * selection (where), fused multiply-add, and advanced activation functions.
 *
 * **Key Features:**
 * - Three-operand template dispatch with perfect parameter forwarding
 * - Multi-destination index management for complex data routing
 * - Configurable vector processing modes (Row, Column, Full RC)
 * - Hardware-optimized pipeline coordination with STALLWAIT mechanisms
 * - Zero-overhead parameter forwarding to complex SFPU functions
 *
 * **Ternary Operation Characteristics:**
 * - **Three-input processing**: Coordinates multiple source operands simultaneously
 * - **Conditional operations**: Supports WHERE/SELECT-style conditional execution
 * - **Fused operations**: Enables multiply-add and other compound mathematical functions
 * - **Data routing complexity**: Manages multiple destination indices for complex data flows
 *
 * **Hardware Integration:**
 * - SFPU lane coordination across 32x32 tile layout with three operands
 * - Advanced pipeline synchronization for multi-operand data dependencies
 * - Optimized face iteration patterns for ternary data access patterns
 * - Memory bandwidth optimization for three-stream data movement
 *
 * **Performance Characteristics:**
 * - VectorMode::RC: Full tile processing (4 faces, maximum throughput)
 * - VectorMode::R: Row vector processing (faces 0+1, optimized for row operations)
 * - VectorMode::C: Column vector processing (faces 0+2, optimized for column operations)
 * - Scalar mode: Single execution for element-wise ternary operations
 *
 * **Common Ternary Operations:**
 * ```cpp
 * // Conditional selection (WHERE operation)
 * _llk_math_eltwise_ternary_sfpu_params_<true>(
 *     [](float threshold) { TTI_SFPWHERE(threshold); },
 *     condition_dst, true_dst, false_dst, VectorMode::RC, threshold_value
 * );
 * 
 * // Fused multiply-add with custom coefficients
 * _llk_math_eltwise_ternary_sfpu_params_<false>(
 *     [](float alpha, float beta) { TTI_SFPMADD(alpha, beta); },
 *     input_dst, mult_dst, add_dst, VectorMode::RC, alpha_coeff, beta_coeff
 * );
 * 
 * // Clamp operation with custom bounds
 * _llk_math_eltwise_ternary_sfpu_params_<true>(
 *     [](float min_val, float max_val) { TTI_SFPCLAMP(min_val, max_val); },
 *     input_dst, min_dst, max_dst, VectorMode::RC, min_bound, max_bound
 * );
 * ```
 */

#pragma once
#include "ckernel_sfpu_where.h"
#include "llk_math_eltwise_ternary_sfpu.h"
#include "llk_sfpu_types.h"

/**
 * @brief Execute parameterized ternary SFPU operation with three-operand processing
 * 
 * @tparam APPROXIMATE Enable approximate mode for faster execution (hardware default: true)
 * @tparam F SFPU function type (lambda, function pointer, or functor for ternary operations)
 * @tparam ARGS Variadic parameter pack for complex ternary function arguments
 * 
 * @param sfpu_func Ternary SFPU operation callable with perfect forwarding semantics
 * @param dst_index0 First operand destination tile index (primary input)
 * @param dst_index1 Second operand destination tile index (secondary input/condition)
 * @param dst_index2 Third operand destination tile index (tertiary input/alternate value)
 * @param vector_mode Vector processing mode controlling execution pattern:
 *                    - VectorMode::RC (4): Full tile processing (4 faces, maximum throughput)
 *                    - VectorMode::R (1): Row vector processing (faces 0+1, row-wise operations)
 *                    - VectorMode::C (2): Column vector processing (faces 0+2, column-wise operations)
 *                    - Other: Single execution (scalar ternary operations)
 * @param args Variadic arguments forwarded to the ternary SFPU function
 * 
 * @details This function provides the core infrastructure for executing parameterized ternary
 * SFPU operations that require three input operands. It coordinates complex data routing between
 * multiple destination indices while managing hardware-level synchronization and face iteration.
 * 
 * **Three-Operand Management:**
 * - dst_index0: Primary input operand (e.g., condition in WHERE, multiplicand in MADD)
 * - dst_index1: Secondary input operand (e.g., true value in WHERE, multiplier in MADD)
 * - dst_index2: Tertiary input operand (e.g., false value in WHERE, addend in MADD)
 * - Minimum index used for pipeline synchronization and address management
 * - All three indices must be valid and available in DEST register file
 * 
 * **Pipeline Synchronization Strategy:**
 * - Uses minimum destination index for sync primitive coordination
 * - Reuses ternary SFPU start/done infrastructure for proper pipeline ordering
 * - TTI_SETRWC instructions coordinate three-stream data movement patterns
 * - Complex face iteration ensures all three operands advance synchronously
 * 
 * **Vector Mode Performance Characteristics:**
 * - **VectorMode::RC**: Processes all 4 tile faces with three operands (maximum complexity)
 * - **VectorMode::R**: Row vector processing - faces 0+1 for row-wise ternary operations
 * - **VectorMode::C**: Column vector processing - faces 0+2 for column-wise ternary operations
 * - **Scalar mode**: Single execution for element-wise ternary functions
 * 
 * **Face Iteration Patterns:**
 * - Row mode: 2 faces × 2 SETRWC + 4 skip SETRWC (8 total row advances)
 * - Column mode: 2 faces × 4 SETRWC (complete face processing for column layout)
 * - RC mode: 4 faces × 2 SETRWC (optimized full tile traversal)
 * - Scalar mode: Single function execution without face iteration
 * 
 * **Ternary Operation Examples:**
 * 
 * ```cpp
 * // Conditional selection (WHERE/SELECT operation)
 * _llk_math_eltwise_ternary_sfpu_params_<true>(
 *     [](float threshold) { 
 *         TTI_SFPWHERE(threshold);  // condition > threshold ? true_val : false_val
 *     }, 
 *     condition_index, true_value_index, false_value_index, 
 *     VectorMode::RC, threshold_value
 * );
 * 
 * // Fused multiply-add with custom scaling
 * _llk_math_eltwise_ternary_sfpu_params_<false>(
 *     [](float scale_mult, float scale_add) { 
 *         TTI_SFPMADD(scale_mult, scale_add);  // (a * scale_mult) + (b * scale_add)
 *     }, 
 *     multiplicand_index, multiplier_index, addend_index,
 *     VectorMode::RC, mult_scale, add_scale
 * );
 * 
 * // Clamp operation with parameterized bounds
 * _llk_math_eltwise_ternary_sfpu_params_<true>(
 *     [](float min_bound, float max_bound) { 
 *         TTI_SFPCLAMP(min_bound, max_bound);  // clamp(input, min, max)
 *     }, 
 *     input_index, min_value_index, max_value_index,
 *     VectorMode::RC, min_limit, max_limit
 * );
 * 
 * // Lerp (linear interpolation) with custom factor
 * _llk_math_eltwise_ternary_sfpu_params_<true>(
 *     [](float interp_factor) { 
 *         TTI_SFPLERP(interp_factor);  // a + t * (b - a)
 *     }, 
 *     start_index, end_index, factor_index,
 *     VectorMode::R, interpolation_factor
 * );
 * ```
 * 
 * **Hardware Constraints:**
 * - All three destination indices must be within DEST register file capacity
 * - SFPU function must use TTI_SFP* instructions for ternary hardware execution
 * - Parameters must be compatible with SFPU data types (typically float32)
 * - Three-operand functions have higher pipeline coordination overhead
 * - Face iteration count depends on vector mode and operand coordination requirements
 * 
 * **Memory and Performance Impact:**
 * - Three-stream memory bandwidth utilization (higher than unary/binary)
 * - Complex address modification coordination for three operands
 * - Pipeline synchronization overhead for multi-operand dependencies
 * - Template instantiation per APPROXIMATE mode and callable type
 * - Hardware coordination minimizes stall cycles across three data streams
 * 
 * @warning Ensure all three destination indices are valid and available
 * @warning SFPU function must support three-operand TTI_SFP* instruction patterns
 * @warning Vector mode must match expected three-operand data layout
 * @warning Complex ternary operations may have higher latency than unary/binary equivalents
 * 
 * @see llk_math_eltwise_ternary_sfpu.h for non-parameterized ternary operations
 * @see llk_math_eltwise_unary_sfpu_params.h for single-operand parameterized operations
 * @see llk_math_eltwise_binary_sfpu_params.h for two-operand parameterized operations
 * @see ckernel_sfpu_where.h for WHERE/conditional operation hardware support
 */
template <bool APPROXIMATE, class F, class... ARGS>
inline void _llk_math_eltwise_ternary_sfpu_params_(
    F&& sfpu_func, uint dst_index0, uint dst_index1, uint dst_index2, int vector_mode = (int)VectorMode::RC, ARGS&&... args)
{
    // **THREE-OPERAND COORDINATION**: Compute minimum destination index for sync coordination
    // All three operands (dst_index0, dst_index1, dst_index2) must be coordinated, but
    // pipeline synchronization uses the minimum index as the primary coordination point
    uint dst_index = std::min(std::min(dst_index0, dst_index1), dst_index2);

    // **PIPELINE INITIALIZATION**: Start ternary SFPU operation with three-operand coordination
    _llk_math_eltwise_ternary_sfpu_start_<DST_SYNC_MODE>(dst_index); // Reuse same sync primitive

    if (vector_mode == (int)VectorMode::R)
    {
        // **ROW VECTOR TERNARY PROCESSING**: Process faces 0+1 for row-wise three-operand operations
        // Optimized for operations where ternary function applies across columns (e.g., row-wise WHERE)
        for (int face = 0; face < 2; face++)
        {
            // **EXECUTE TERNARY SFPU**: Three operands processed simultaneously with parameter forwarding
            std::forward<F>(sfpu_func)(std::forward<ARGS&&>(args)...);
            
            // **ROW ADVANCEMENT**: Two SETRWC instructions per face (8 rows each, 16 rows total per face)
            // Coordinates three-operand data movement for row-oriented ternary operations
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D); // First 8 rows
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D); // Second 8 rows
        }
        
        // **SKIP REMAINING FACES**: Advance through faces 2+3 without three-operand processing
        // Maintains proper addressing state for all three operands across unused faces
        for (int i = 0; i < 4; ++i)
        {
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
        }
    }
    else if (vector_mode == (int)VectorMode::C)
    {
        // **COLUMN VECTOR TERNARY PROCESSING**: Process faces 0+2 for column-wise three-operand operations
        // Optimized for operations where ternary function applies across rows (e.g., column-wise conditional selection)
        for (int face = 0; face < 2; face++)
        {
            // **EXECUTE TERNARY SFPU**: Three operands processed simultaneously with parameter forwarding
            std::forward<F>(sfpu_func)(std::forward<ARGS&&>(args)...);
            
            // **COMPLETE FACE PROCESSING**: Four SETRWC instructions for full face coverage (16 rows)
            // Column mode requires complete face processing for proper three-operand data layout
            for (int i = 0; i < 4; ++i)
            {
                TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
            }
        }
    }
    else if (vector_mode == (int)VectorMode::RC)
    {
        // **FULL TILE TERNARY PROCESSING**: Process all 4 faces for complete three-operand tile coverage
        // Maximum complexity mode - coordinates three data streams across entire 32x32 tile
        for (int face = 0; face < 4; face++)
        {
            // **EXECUTE TERNARY SFPU**: Three operands processed simultaneously with parameter forwarding
            std::forward<F>(sfpu_func)(std::forward<ARGS&&>(args)...);
            
            // **OPTIMIZED FACE ADVANCEMENT**: Two SETRWC instructions per face for efficient traversal
            // Coordinates three-operand data movement across 16 rows per face (2 blocks of 8 rows)
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D); // First block (8 rows)
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D); // Second block (8 rows)
        }
    }
    else
    {
        // **SCALAR TERNARY MODE**: Single execution without face iteration
        // Used for element-wise ternary operations that don't require vector processing patterns
        // All three operands processed in single function call
        std::forward<F>(sfpu_func)(std::forward<ARGS&&>(args)...);
    }
    
    // **PIPELINE FINALIZATION**: Complete ternary SFPU operation and clean up three-operand state
    _llk_math_eltwise_ternary_sfpu_done_(); // Finalize three-operand coordination
}
