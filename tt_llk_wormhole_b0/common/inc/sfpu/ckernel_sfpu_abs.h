// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_abs.h
 * @brief Absolute value SFPU kernel for Wormhole B0 Tensix Special Function Processing Unit
 * 
 * @details This header provides hardware-accelerated absolute value computation using the
 * Wormhole B0 SFPU (Special Function Processing Unit). The SFPU is a specialized 32-lane
 * SIMD engine designed for efficient special mathematical functions, consisting of 8 
 * instances × 4 lanes each, integrated within the Compute FPU for high-throughput processing.
 * 
 * **Mathematical Operation:**
 * Computes the absolute value: f(x) = |x| = x if x ≥ 0, -x if x < 0
 * 
 * **Wormhole B0 SFPU Architecture:**
 * - **32 Processing Lanes**: 8 SFPU instances × 4 lanes for parallel computation
 * - **DEST Register Integration**: Operates directly on DEST register file data
 * - **FP32 Precision**: Full 32-bit floating-point processing capability
 * - **Conditional Execution**: Hardware condition code support for lane-specific operations
 * - **Pipeline Integration**: Seamless integration with Thread 1 (Math) operations
 * 
 * **IEEE-754 Implementation:**
 * - **Sign Bit Manipulation**: Hardware-accelerated sign bit clearing for positive result
 * - **Special Cases**: Proper handling of ±0, ±∞, and NaN values per IEEE-754 standard
 * - **No Overflow**: Absolute value operation cannot overflow in IEEE-754 representation
 * - **Bit-Level Accuracy**: Exact result with no approximation error
 * 
 * **Hardware Context:**
 * - **SFPI Interface**: Uses SFPI (Special Function Programming Interface) for hardware control
 * - **DEST Register Access**: Direct access to 32-bit DEST register file data
 * - **Lane Parallelism**: Simultaneous processing across all 32 SFPU lanes
 * - **Zero Latency**: Single-cycle execution per datum through SFPU pipeline
 * 
 * **Performance Characteristics:**
 * - **Throughput**: 32 datums processed per clock cycle (within one register)
 * - **Latency**: Single-cycle latency per DEST register
 * - **Memory Efficiency**: In-place operation requiring no additional memory
 * - **Energy Efficient**: Specialized hardware optimized for mathematical functions
 * 
 * **Common Use Cases:**
 * - **Neural Networks**: ReLU-like activation functions and error calculations
 * - **Signal Processing**: Magnitude computation and signal rectification
 * - **Mathematics**: Vector norm calculations and distance metrics
 * - **Computer Vision**: Image processing and feature extraction algorithms
 * 
 * **Integration with Wormhole B0:**
 * - **Thread 1 Execution**: Typically executed within Math thread context
 * - **DEST Register Coordination**: Integrates with Math↔Pack thread synchronization
 * - **Format Support**: Compatible with all DEST register data formats
 * - **Pipeline Efficiency**: Maintains high utilization of 2048 hardware multipliers
 */

#pragma once

#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Hardware-accelerated absolute value computation using Wormhole B0 SFPU
 * @tparam APPROXIMATION_MODE Unused for exact absolute value operation
 * @tparam ITERATIONS Compile-time loop unrolling factor for performance optimization
 * @param iterations Number of DEST register entries to process
 * 
 * @details Computes absolute value |x| for data in DEST registers using the Wormhole B0
 * SFPU's 32-lane SIMD architecture. The operation is mathematically exact with no
 * approximation, leveraging hardware sign bit manipulation for optimal performance.
 * 
 * **Wormhole B0 SFPU Execution:**
 * - **32-Lane SIMD**: All 32 SFPU lanes process datums within current register simultaneously  
 * - **Sequential Registers**: Processes one DEST register at a time, advancing through register file
 * - **Hardware Acceleration**: Specialized sign bit manipulation hardware
 * - **Single-Cycle**: One clock cycle per register through SFPU pipeline
 * 
 * **SFPI Programming Model:**
 * ```cpp
 * sfpi::vFloat v = sfpi::dst_reg[0];    // Load from current DEST register
 * sfpi::dst_reg[0] = sfpi::abs(v);      // Hardware absolute value on current register
 * sfpi::dst_reg++;                      // Advance pointer to next register
 * ```
 * 
 * **Mathematical Properties:**
 * - **Exact Operation**: No approximation or rounding errors
 * - **IEEE-754 Compliant**: Proper handling of all special floating-point cases
 * - **Sign Preservation**: Maintains magnitude while ensuring positive sign
 * - **Range Preserving**: Output range matches input range magnitude
 * 
 * **Performance Optimization:**
 * - **Loop Unrolling**: Template ITERATIONS parameter enables compile-time optimization
 * - **Register Advancement**: Automatic DEST register pointer advancement
 * - **Memory Locality**: Sequential access pattern optimizes cache performance
 * - **Pipeline Utilization**: Maintains high SFPU throughput
 * 
 * **Thread Integration:**
 * - **Thread 1 Context**: Executed within Math thread for computational efficiency
 * - **Synchronization**: Integrates with semaphore-based thread coordination
 * - **Register Management**: Coordinates with DEST register double-buffering
 * - **Pipeline Flow**: Maintains Unpack→Math→Pack data flow efficiency
 * 
 * @note This function operates in-place on DEST registers
 * @note Template parameters enable compile-time optimizations
 * @note Compatible with all floating-point formats supported by DEST registers
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_abs_(const int iterations)
{
    // SFPU microcode
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat v   = sfpi::dst_reg[0];       // Load from DEST register
        sfpi::dst_reg[0] = sfpi::abs(v);           // Hardware absolute value operation
        sfpi::dst_reg++;                           // Advance to next DEST register
    }
}

} // namespace sfpu
} // namespace ckernel
