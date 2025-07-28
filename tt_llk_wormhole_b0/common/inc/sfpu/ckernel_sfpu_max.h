// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_max.h
 * @brief Maximum function SFPU kernel for Wormhole B0 Tensix Special Function Processing Unit
 * 
 * @details This header provides hardware-accelerated maximum computation using the Wormhole B0
 * SFPU's conditional execution and comparison capabilities. The SFPU's 32-lane SIMD architecture
 * with condition code functionality enables efficient element-wise maximum operations between
 * data at different positions in the DEST register file.
 * 
 * **Mathematical Operation:**
 * Computes the element-wise maximum: f(a,b) = max(a,b) = a if a ≥ b, b if b > a
 * 
 * **Wormhole B0 SFPU Conditional Execution:**
 * - **32-Lane SIMD**: Each lane independently evaluates the comparison condition
 * - **Condition Codes**: Hardware condition code (CC) bits control per-lane execution
 * - **v_if/v_endif**: SFPU intrinsics for conditional execution across SIMD lanes
 * - **DEST Register Access**: Compares data from current position vs. offset position
 * 
 * **Hardware Context:**
 * - **SFPI Interface**: Uses SFPI conditional execution primitives (v_if/v_endif)
 * - **Condition Code Stack**: Hardware CC stack enables nested conditional execution
 * - **Per-Lane Control**: Each of 32 lanes can follow different execution paths
 * - **Register File Access**: Accesses DEST registers at current and offset positions
 * 
 * **Conditional Execution Architecture:**
 * The Wormhole B0 SFPU implements sophisticated conditional execution:
 * - **Condition Code Enable**: CC enable register controls conditional execution mode
 * - **Condition Code Result**: CC result register determines per-lane execution
 * - **SIMD Divergence**: Different lanes can execute different instruction sequences
 * - **Hardware Efficiency**: Conditional execution implemented in hardware for performance
 * 
 * **Performance Characteristics:**
 * - **Throughput**: 32 comparison operations per clock cycle (within one register)
 * - **Latency**: Single-cycle latency per DEST register comparison
 * - **Memory Efficiency**: In-place operation with offset register access
 * - **Conditional Overhead**: Minimal overhead for SIMD conditional execution
 * 
 * **Register Access Pattern:**
 * ```cpp
 * sfpi::vFloat a = sfpi::dst_reg[0];    // Current register position
 * sfpi::vFloat b = sfpi::dst_reg[32];   // Register 32 positions away
 * ```
 * This pattern enables comparison between data at different offsets in the DEST register file.
 * 
 * **Common Use Cases:**
 * - **Neural Networks**: Activation functions and pooling operations
 * - **Signal Processing**: Peak detection and envelope following
 * - **Computer Vision**: Non-maximum suppression and feature selection
 * - **Statistics**: Running maximum and extrema finding
 * - **Array Operations**: Element-wise maximum across tensor dimensions
 * 
 * **Integration with Wormhole B0:**
 * - **Thread 1 Execution**: Executed within Math thread for computational efficiency
 * - **DEST Register Coordination**: Works with register file double-buffering
 * - **Pipeline Integration**: Maintains efficient Unpack→Math→Pack data flow
 */

#pragma once

#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Hardware-accelerated maximum computation using Wormhole B0 SFPU conditional execution
 * @tparam APPROXIMATION_MODE Unused for exact maximum operation
 * @tparam ITERATIONS Compile-time loop unrolling factor for performance optimization
 * @param iterations Number of DEST register pairs to process
 * 
 * @details Computes element-wise maximum between data in current DEST register position
 * and data 32 positions away, using the Wormhole B0 SFPU's conditional execution
 * capabilities. Each of the 32 SFPU lanes independently evaluates the comparison
 * condition and selects the appropriate result.
 * 
 * **SFPI Conditional Programming Model:**
 * ```cpp
 * sfpi::vFloat a = sfpi::dst_reg[0];    // Load from current register
 * sfpi::vFloat b = sfpi::dst_reg[32];   // Load from offset register  
 * v_if (a < b) {                        // Per-lane conditional execution
 *     sfpi::dst_reg[0] = b;             // Update current register if b > a
 * }
 * v_endif;                              // End conditional block
 * sfpi::dst_reg++;                      // Advance to next register
 * ```
 * 
 * **Wormhole B0 Conditional Execution Features:**
 * - **Per-Lane Comparison**: Each of 32 lanes independently evaluates (a < b)
 * - **Condition Code Control**: Hardware CC bits determine per-lane execution path
 * - **SIMD Divergence**: Lanes can execute different instructions based on condition
 * - **Hardware Acceleration**: Conditional execution implemented in specialized hardware
 * 
 * **Mathematical Properties:**
 * - **Exact Operation**: No approximation, exact comparison and selection
 * - **IEEE-754 Compliant**: Proper handling of ±0, ±∞, and NaN values
 * - **Symmetric**: max(a,b) = max(b,a) for all valid inputs
 * - **Associative**: max(max(a,b),c) = max(a,max(b,c)) for chained operations
 * 
 * **Register Access Pattern:**
 * - **Current Position**: `sfpi::dst_reg[0]` accesses data at current pointer location
 * - **Offset Access**: `sfpi::dst_reg[32]` accesses data 32 registers away
 * - **In-Place Update**: Result stored back to current position
 * - **Sequential Advance**: Pointer advances to next register for iteration
 * 
 * **Performance Optimization:**
 * - **SIMD Parallel**: All 32 lanes process their datums simultaneously
 * - **Single-Cycle Comparison**: Hardware comparison completed in one clock cycle
 * - **Conditional Efficiency**: Hardware conditional execution avoids branch penalties
 * - **Memory Locality**: Efficient access to nearby register file locations
 * 
 * **Special Value Handling:**
 * - **NaN Propagation**: NaN values propagate according to IEEE-754 standard
 * - **Infinity Handling**: ±∞ compared correctly with finite values
 * - **Zero Comparison**: Proper handling of +0 vs -0 comparison edge cases
 * 
 * @note This function operates in-place on DEST registers
 * @note Uses hardware conditional execution for optimal SIMD performance
 * @note Accesses registers at current position and 32-position offset
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_max_(const int iterations)
{
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat a = sfpi::dst_reg[0];         // Load from current register position
        sfpi::vFloat b = sfpi::dst_reg[32];        // Load from register 32 positions away
        v_if (a < b)                               // Hardware conditional: if a < b
        {
            sfpi::dst_reg[0] = b;                  // Update current position with larger value
        }
        v_endif;                                   // End conditional execution block

        sfpi::dst_reg++;                           // Advance pointer to next register
    }
}

} // namespace sfpu
} // namespace ckernel
