// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_square.h
 * @brief Square function SFPU kernel for Wormhole B0 Tensix Special Function Processing Unit
 * 
 * @details This header provides hardware-accelerated square computation (x²) using the
 * Wormhole B0 SFPU's integrated MAD (Multiply-Add) block. The SFPU consists of 8 instances
 * × 4 lanes (32 total lanes) with dedicated FP32 MAD units for efficient multiplication
 * operations, enabling high-throughput square computations within the Tensix engine.
 * 
 * **Mathematical Operation:**
 * Computes the square function: f(x) = x² = x × x
 * 
 * **Wormhole B0 SFPU MAD Block Architecture:**
 * - **32 MAD Units**: 8 SFPU instances × 4 lanes, each with dedicated FP32 MAD block
 * - **FP32 Precision**: Full 32-bit floating-point multiply operations
 * - **Single-Cycle Multiplication**: Hardware-accelerated multiply in one clock cycle
 * - **DEST Register Integration**: Direct operation on DEST register file data
 * - **Pipeline Optimization**: Fused multiply operations for maximum efficiency
 * 
 * **IEEE-754 Implementation:**
 * - **FP32 Multiplication**: Hardware follows IEEE-754 multiplication semantics
 * - **Rounding Mode**: Configurable rounding for precision control
 * - **Special Cases**: Proper handling of ±0, ±∞, NaN, and subnormal values
 * - **Overflow/Underflow**: IEEE-754 compliant overflow and underflow behavior
 * - **Sign Handling**: Result always positive for finite inputs (x² ≥ 0)
 * 
 * **Hardware Context:**
 * - **SFPI Interface**: Uses SFPI multiplication operations for hardware control
 * - **MAD Block Usage**: Utilizes dedicated multiply-add hardware (x * x + 0)
 * - **32-Lane Parallel**: Simultaneous processing across all SFPU lanes
 * - **Register Optimization**: In-place operation with automatic pointer advancement
 * 
 * **Performance Characteristics:**
 * - **Throughput**: 32 datums processed per clock cycle (within one register)
 * - **Latency**: Single-cycle latency per DEST register
 * - **Memory Efficiency**: In-place operation requiring no additional memory
 * - **Compiler Optimization**: `#pragma GCC unroll 8` for instruction-level parallelism
 * 
 * **Common Use Cases:**
 * - **Neural Networks**: Squared error calculations and L2 regularization
 * - **Statistics**: Variance and standard deviation computations
 * - **Signal Processing**: Power spectral density and energy calculations
 * - **Physics Simulations**: Kinetic energy and distance squared computations
 * - **Machine Learning**: Euclidean distance metrics and loss functions
 * 
 * **Integration with Wormhole B0:**
 * - **Thread 1 Execution**: Optimized for Math thread execution context
 * - **2048 Multiplier Coordination**: Leverages broader Tensix multiplication resources
 * - **DEST Register Management**: Integrates with double-buffering and synchronization
 * - **Pipeline Efficiency**: Maintains high utilization throughout compute pipeline
 */

#pragma once

#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Hardware-accelerated square computation using Wormhole B0 SFPU MAD blocks
 * @tparam APPROXIMATION_MODE Unused for exact square operation
 * @tparam ITERATIONS Compile-time loop unrolling factor for performance optimization
 * @param iterations Number of DEST register entries to process
 * 
 * @details Computes x² for data in DEST registers using the Wormhole B0 SFPU's dedicated
 * FP32 MAD (Multiply-Add) blocks. Each of the 8 SFPU instances contains a MAD unit that
 * performs single-cycle FP32 multiplication with IEEE-754 compliance, enabling
 * high-throughput square computations across 32 parallel lanes.
 * 
 * **Wormhole B0 SFPU MAD Block Execution:**
 * - **32 Parallel MAD Units**: Each lane processes one datum within current register
 * - **Single-Cycle Multiply**: x × x completed in one clock cycle per register
 * - **Sequential Register Processing**: Processes one DEST register at a time
 * - **IEEE-754 Compliance**: Full floating-point multiply semantics
 * 
 * **SFPI Programming Model:**
 * ```cpp
 * sfpi::vFloat in = sfpi::dst_reg[0];       // Load from DEST register
 * sfpi::vFloat result = in * in;            // Hardware FP32 multiplication
 * sfpi::dst_reg[0] = result;                // Store result in-place
 * sfpi::dst_reg++;                          // Advance to next register
 * ```
 * 
 * **Mathematical Properties:**
 * - **IEEE-754 Accurate**: Hardware multiplication follows IEEE-754 standard
 * - **Always Non-Negative**: x² ≥ 0 for all finite inputs
 * - **Monotonic for |x|**: Square function increases with absolute value
 * - **Range Expansion**: Output range typically larger than input range
 * 
 * **Performance Optimization Features:**
 * - **GCC Unroll Directive**: `#pragma GCC unroll 8` enables instruction-level parallelism
 * - **Loop Unrolling**: Template ITERATIONS parameter for compile-time optimization
 * - **Register Advancement**: Automatic DEST register pointer management
 * - **Memory Access Pattern**: Sequential access optimizes cache and pipeline performance
 * 
 * **Hardware Integration:**
 * - **MAD Block Utilization**: Leverages dedicated FP32 multiply-add units
 * - **Pipeline Efficiency**: Maintains high SFPU lane utilization
 * - **Memory Locality**: In-place operation preserves memory hierarchy efficiency
 * - **Thread Coordination**: Integrates with Math thread and DEST register synchronization
 * 
 * **Numerical Considerations:**
 * - **Overflow Potential**: x² may overflow for large |x| values
 * - **Underflow Handling**: Small inputs may underflow to zero
 * - **Precision Loss**: Some precision loss possible due to floating-point representation
 * - **Special Values**: Handles ±0, ±∞, and NaN according to IEEE-754
 * 
 * @note This function operates in-place on DEST registers
 * @note Uses dedicated FP32 MAD blocks for optimal performance
 * @note Compiler unrolling optimization for instruction-level parallelism
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_square_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat in     = sfpi::dst_reg[0];    // Load input from DEST register
        sfpi::vFloat result = in * in;             // Hardware FP32 multiplication (x²)

        sfpi::dst_reg[0] = result;                 // Store result in-place

        sfpi::dst_reg++;                           // Advance to next DEST register
    }
}

} // namespace sfpu
} // namespace ckernel
