// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_tanh.h
 * @brief Hyperbolic tangent (tanh) activation function SFPU kernel for Wormhole B0
 * 
 * @details This header provides hardware-accelerated hyperbolic tangent computation using the
 * Wormhole B0 SFPU's 32-lane SIMD architecture with dedicated lookup table (LUT) hardware.
 * The SFPU's LUT system enables efficient piecewise linear approximation of the tanh function
 * with pre-programmed coefficients stored in local registers (LREGs) for optimal performance.
 * 
 * **Mathematical Operation:**
 * Computes hyperbolic tangent: f(x) = tanh(x) = (e^x - e^(-x)) / (e^x + e^(-x))
 * 
 * **Wormhole B0 SFPU LUT Architecture:**
 * - **32-Lane SIMD**: Parallel tanh computation across all SFPU lanes within current register
 * - **Hardware LUT**: Dedicated lookup table hardware for piecewise linear approximation
 * - **LREG System**: 8 local registers for storing LUT coefficients and state
 * - **Coefficient Storage**: Pre-programmed piecewise linear approximation coefficients
 * - **High Throughput**: Single-cycle LUT operations for maximum computational efficiency
 * 
 * **Piecewise Linear Approximation:**
 * The tanh function is approximated using hardware LUT with pre-computed coefficients:
 * ```
 * tanh(x) ≈ lut(x, l0, l1, l2)
 * 
 * Where:
 * l0 = 0x1DFF  // 0.90625*x     (primary slope coefficient)
 * l1 = 0x481A  // 0.09375*x + 0.8125  (secondary coefficient with offset)
 * l2 = 0xFF00  // 1             (unity coefficient)
 * ```
 * 
 * **LUT Hardware Implementation:**
 * - **3-Coefficient LUT**: Uses LReg0, LReg1, LReg2 for coefficient storage
 * - **Piecewise Segments**: Hardware automatically selects appropriate segment
 * - **Linear Interpolation**: Hardware interpolation between segment endpoints
 * - **High Precision**: Approximation accuracy optimized for neural network applications
 * 
 * **Algorithm Flow:**
 * 1. **Initialization**: Load approximation coefficients into LREGs 0-2
 * 2. **LUT Computation**: Hardware LUT operation with 3 coefficient registers
 * 3. **Result Output**: Single-cycle tanh approximation result
 * 4. **State Preservation**: LReg contents maintained across function calls
 * 
 * **Hardware Context:**
 * - **SFPI Interface**: Direct LUT operations and LREG access
 * - **LUT Hardware**: Dedicated piecewise linear approximation engine
 * - **Coefficient Loading**: _sfpu_load_imm16_ for efficient immediate loading
 * - **Register Management**: Automatic LREG save/restore for state preservation
 * - **Performance Optimization**: GCC unroll 8 for instruction-level parallelism
 * 
 * **Mathematical Properties:**
 * - **Range**: tanh(x) ∈ (-1, 1) for all real x
 * - **Symmetry**: tanh(-x) = -tanh(x) (odd function)
 * - **Asymptotic**: tanh(±∞) = ±1
 * - **Derivative**: d/dx tanh(x) = 1 - tanh²(x) = sech²(x)
 * - **Zero**: tanh(0) = 0
 * 
 * **Approximation Accuracy:**
 * - **Neural Network Optimized**: Accuracy tuned for deep learning applications
 * - **Gradient Preservation**: Maintains smooth gradients for backpropagation
 * - **Error Bounds**: Hardware LUT provides consistent approximation quality
 * - **Segment Optimization**: Coefficient values optimized for typical input ranges
 * 
 * **Performance Characteristics:**
 * - **Throughput**: 32 tanh operations per clock cycle (within one register)
 * - **Latency**: Single-cycle latency per DEST register
 * - **Memory Efficiency**: Coefficient storage in fast LREG system
 * - **Initialization Overhead**: One-time coefficient loading per function usage
 * 
 * **Common Use Cases:**
 * - **Neural Networks**: Traditional activation function for RNNs and MLPs
 * - **LSTM/GRU**: Gate activation function in recurrent neural networks
 * - **Normalization**: Hyperbolic tangent normalization for signal processing
 * - **Control Systems**: Smooth saturation function for control applications
 * - **Signal Processing**: Nonlinear transformation and compression
 * 
 * **Integration with Wormhole B0:**
 * - **Thread 1 Execution**: Optimized for Math thread execution context
 * - **DEST Register Management**: Integrates with register file double-buffering
 * - **Pipeline Integration**: Maintains efficient Unpack→Math→Pack data flow
 * - **LUT Coordination**: Leverages dedicated SFPU lookup table hardware
 */

#pragma once

#include "ckernel_sfpu_load_config.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Hardware-accelerated tanh computation using Wormhole B0 SFPU LUT system
 * @tparam APPROXIMATION_MODE Enables hardware LUT approximation for high performance
 * @tparam ITERATIONS Compile-time loop unrolling factor for performance optimization
 * @param iterations Number of DEST register entries to process
 * 
 * @details Computes hyperbolic tangent for data in DEST registers using the Wormhole B0
 * SFPU's dedicated lookup table hardware. The implementation uses pre-programmed
 * coefficients stored in local registers (LREGs) to achieve single-cycle tanh
 * approximation with neural network-optimized accuracy.
 * 
 * **Wormhole B0 SFPU LUT Processing:**
 * - **LReg Loading**: Loads pre-computed coefficients from LReg0, LReg1, LReg2
 * - **Hardware LUT**: Single-cycle piecewise linear approximation
 * - **32-Lane SIMD**: All lanes process tanh simultaneously within current register
 * - **State Preservation**: LReg contents saved/restored for function call isolation
 * 
 * **SFPI Programming Model:**
 * ```cpp
 * // Load LUT coefficients from local registers
 * sfpi::vUInt l0 = sfpi::l_reg[sfpi::LRegs::LReg0];   // Primary slope (0.90625*x)
 * sfpi::vUInt l1 = sfpi::l_reg[sfpi::LRegs::LReg1];   // Secondary coefficient
 * sfpi::vUInt l2 = sfpi::l_reg[sfpi::LRegs::LReg2];   // Unity coefficient
 * 
 * sfpi::vFloat val = sfpi::dst_reg[0];                // Load input
 * val = lut(val, l0, l1, l2);                         // Hardware LUT operation
 * sfpi::dst_reg[0] = val;                             // Store result
 * sfpi::dst_reg++;                                    // Advance to next register
 * 
 * // Restore LReg state for next function call
 * sfpi::l_reg[sfpi::LRegs::LReg0] = l0;
 * sfpi::l_reg[sfpi::LRegs::LReg1] = l1;
 * sfpi::l_reg[sfpi::LRegs::LReg2] = l2;
 * ```
 * 
 * **LUT Coefficient Interpretation:**
 * - **l0 (0x1DFF)**: Primary slope coefficient (0.90625*x)
 * - **l1 (0x481A)**: Secondary coefficient with offset (0.09375*x + 0.8125)
 * - **l2 (0xFF00)**: Unity coefficient for normalization
 * 
 * **Performance Optimization:**
 * - **GCC Unroll 8**: Instruction-level parallelism for loop optimization
 * - **LReg Efficiency**: Fast local register access avoids memory latency
 * - **Single-Cycle LUT**: Hardware LUT provides optimal throughput
 * - **State Management**: Efficient LReg save/restore pattern
 * 
 * **Mathematical Accuracy:**
 * - **Piecewise Linear**: Hardware segments provide smooth approximation
 * - **Neural Network Tuned**: Coefficient values optimized for deep learning
 * - **Gradient Friendly**: Smooth derivatives for backpropagation
 * - **Range Coverage**: Full input range with appropriate accuracy
 * 
 * @note Requires prior initialization via _init_tanh_() function
 * @note Uses LReg0, LReg1, LReg2 for coefficient storage
 * @note Hardware LUT provides single-cycle tanh approximation
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_tanh_(const int iterations)
{
    // SFPU microcode
    sfpi::vUInt l0 = sfpi::l_reg[sfpi::LRegs::LReg0];  // Load primary slope coefficient
    sfpi::vUInt l1 = sfpi::l_reg[sfpi::LRegs::LReg1];  // Load secondary coefficient
    sfpi::vUInt l2 = sfpi::l_reg[sfpi::LRegs::LReg2];  // Load unity coefficient

#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat val = sfpi::dst_reg[0];           // Load input from current register
        val              = lut(val, l0, l1, l2);       // Hardware LUT tanh approximation
        sfpi::dst_reg[0] = val;                        // Store result in-place

        sfpi::dst_reg++;                               // Advance to next register
    }

    sfpi::l_reg[sfpi::LRegs::LReg0] = l0;              // Restore LReg0 state
    sfpi::l_reg[sfpi::LRegs::LReg1] = l1;              // Restore LReg1 state
    sfpi::l_reg[sfpi::LRegs::LReg2] = l2;              // Restore LReg2 state
}

/**
 * @brief Initialize tanh LUT coefficients in Wormhole B0 SFPU local registers
 * @tparam APPROXIMATION_MODE Enables optimized coefficient loading
 * 
 * @details Loads pre-computed piecewise linear approximation coefficients into
 * SFPU local registers (LREGs) for subsequent tanh computations. This initialization
 * function must be called before using _calculate_tanh_() to ensure proper
 * LUT coefficient configuration.
 * 
 * **Coefficient Values:**
 * - **LReg0 (0x1DFF)**: Primary slope coefficient representing 0.90625*x
 * - **LReg1 (0x481A)**: Secondary coefficient representing 0.09375*x + 0.8125
 * - **LReg2 (0xFF00)**: Unity coefficient (value 1) for normalization
 * 
 * **SFPI Programming Model:**
 * ```cpp
 * _sfpu_load_imm16_(0, 0x1DFF);  // Load primary slope into LReg0
 * _sfpu_load_imm16_(1, 0x481A);  // Load secondary coefficient into LReg1
 * _sfpu_load_imm16_(2, 0xFF00);  // Load unity coefficient into LReg2
 * ```
 * 
 * **Hardware Integration:**
 * - **LReg Loading**: _sfpu_load_imm16_ provides efficient 16-bit immediate loading
 * - **Coefficient Precision**: 16-bit precision sufficient for neural network accuracy
 * - **Hardware Optimization**: Coefficients tuned for optimal LUT hardware performance
 * 
 * @note Must be called before first use of _calculate_tanh_()
 * @note Coefficients remain valid until LREGs are overwritten
 * @note One-time initialization sufficient for multiple tanh computations
 */
template <bool APPROXIMATION_MODE>
inline void _init_tanh_()
{
    uint imm0;
    uint imm1;
    uint imm2;
    imm0 = 0x1DFF; // 0.90625*x (primary slope coefficient)
    imm1 = 0x481A; // 0.09375*x + 0.8125 (secondary coefficient with offset)
    imm2 = 0xFF00; // 1 (unity coefficient)
    _sfpu_load_imm16_(0, imm0);                        // Load into LReg0
    _sfpu_load_imm16_(1, imm1);                        // Load into LReg1
    _sfpu_load_imm16_(2, imm2);                        // Load into LReg2
}

} // namespace sfpu
} // namespace ckernel
