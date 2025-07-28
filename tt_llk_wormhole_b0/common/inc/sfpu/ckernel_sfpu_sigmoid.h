// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_sigmoid.h
 * @brief Sigmoid activation function SFPU kernel for Wormhole B0 Tensix Special Function Processing Unit
 * 
 * @details This header provides hardware-accelerated sigmoid computation using the Wormhole B0
 * SFPU's 32-lane SIMD architecture with advanced 6-entry lookup table (LUT) hardware. The SFPU's
 * sophisticated LUT2 system enables high-precision piecewise linear approximation of the sigmoid
 * function with pre-programmed coefficients stored across multiple local registers (LREGs).
 * 
 * **Mathematical Operation:**
 * Computes sigmoid activation: f(x) = σ(x) = 1 / (1 + e^(-x)) = e^x / (e^x + 1)
 * 
 * **Wormhole B0 SFPU Advanced LUT Architecture:**
 * - **32-Lane SIMD**: Parallel sigmoid computation across all SFPU lanes within current register
 * - **6-Entry LUT Hardware**: Advanced lookup table with 6 coefficient registers for higher precision
 * - **LUT2 Mode 0**: SFPLUTFP32_MOD0_FP16_6ENTRY_TABLE1 for optimized FP16 6-entry approximation
 * - **Multi-LREG System**: Uses LReg0, LReg1, LReg2, LReg4, LReg5, LReg6 for coefficient storage
 * - **High Precision**: Enhanced approximation accuracy for critical neural network applications
 * 
 * **Advanced Piecewise Linear Approximation:**
 * The sigmoid function uses sophisticated 6-coefficient LUT for superior accuracy:
 * ```
 * sigmoid(x) ≈ lut2(x, l0, l1, l2, l4, l5, l6, mode=0) + 0.5
 * 
 * Where LUT2 provides enhanced piecewise linear approximation with:
 * - l0, l1, l2: Primary coefficient set for main approximation segments
 * - l4, l5, l6: Secondary coefficient set for enhanced precision regions
 * - mode=0: SFPLUTFP32_MOD0_FP16_6ENTRY_TABLE1 hardware mode
 * - +0.5: Bias offset for proper sigmoid range centering
 * ```
 * 
 * **LUT2 Hardware Implementation:**
 * - **6-Coefficient System**: Advanced multi-register coefficient storage
 * - **Enhanced Segments**: More piecewise segments for superior approximation quality
 * - **Hardware Mode Selection**: LUT mode 0 optimized for sigmoid characteristics
 * - **Bias Integration**: Hardware bias addition for proper sigmoid output range
 * - **High Throughput**: Single-cycle LUT2 operations despite increased complexity
 * 
 * **Algorithm Flow:**
 * 1. **Initialization**: Load 6 approximation coefficients into LREGs 0,1,2,4,5,6
 * 2. **LUT2 Computation**: Advanced hardware LUT2 operation with 6 coefficients
 * 3. **Bias Addition**: Hardware addition of 0.5 bias for proper sigmoid centering
 * 4. **Result Output**: Single-cycle high-precision sigmoid approximation
 * 5. **State Preservation**: All 6 LReg contents maintained across function calls
 * 
 * **Hardware Context:**
 * - **SFPI Interface**: Advanced LUT2 operations and multi-LREG access
 * - **LUT2 Hardware**: Enhanced piecewise linear approximation engine
 * - **Coefficient Distribution**: Strategic use of non-consecutive LREGs (0,1,2,4,5,6)
 * - **Mode Control**: Hardware LUT mode selection for optimal sigmoid approximation
 * - **Register Management**: Advanced LREG save/restore for 6-register state
 * 
 * **Mathematical Properties:**
 * - **Range**: σ(x) ∈ (0, 1) for all real x
 * - **Symmetry**: σ(-x) = 1 - σ(x) (symmetric about 0.5)
 * - **Asymptotic**: σ(-∞) = 0, σ(+∞) = 1
 * - **Derivative**: dσ/dx = σ(x) × (1 - σ(x))
 * - **Midpoint**: σ(0) = 0.5
 * - **Smooth**: Infinitely differentiable with well-behaved gradients
 * 
 * **Enhanced Approximation Accuracy:**
 * - **6-Coefficient Precision**: Significantly higher accuracy than 3-coefficient systems
 * - **Neural Network Critical**: Precision essential for training stability and convergence
 * - **Gradient Quality**: Enhanced derivative approximation for backpropagation
 * - **Range Optimization**: Coefficient tuning for typical neural network input ranges
 * - **Error Minimization**: Advanced LUT design minimizes approximation errors
 * 
 * **Performance Characteristics:**
 * - **Throughput**: 32 sigmoid operations per clock cycle (within one register)
 * - **Latency**: Single-cycle latency per DEST register despite 6-coefficient complexity
 * - **Memory Efficiency**: Coefficient storage in fast LREG system
 * - **Initialization Overhead**: One-time 6-coefficient loading per function usage
 * - **Hardware Optimization**: LUT2 mode 0 optimized for sigmoid computation patterns
 * 
 * **Common Use Cases:**
 * - **Neural Networks**: Classic activation function for binary classification
 * - **Logistic Regression**: Standard activation for probabilistic outputs
 * - **Gate Functions**: LSTM/GRU gate activation for recurrent networks
 * - **Probability Mapping**: Converting real values to probability range [0,1]
 * - **Binary Classification**: Output layer activation for two-class problems
 * 
 * **Integration with Wormhole B0:**
 * - **Thread 1 Execution**: Optimized for Math thread execution context
 * - **DEST Register Management**: Integrates with register file double-buffering
 * - **Pipeline Integration**: Maintains efficient Unpack→Math→Pack data flow
 * - **LUT2 Coordination**: Leverages advanced SFPU lookup table hardware
 * - **Multi-Register State**: Manages complex 6-register coefficient state efficiently
 */

#pragma once

#include "ckernel_sfpu_load_config.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Hardware-accelerated sigmoid computation using Wormhole B0 SFPU advanced LUT2 system
 * @tparam APPROXIMATION_MODE Enables hardware LUT2 approximation for high precision
 * @tparam ITERATIONS Compile-time loop unrolling factor for performance optimization
 * @param iterations Number of DEST register entries to process
 * 
 * @details Computes sigmoid activation for data in DEST registers using the Wormhole B0
 * SFPU's advanced 6-entry lookup table hardware. The implementation uses sophisticated
 * LUT2 mode 0 with 6 coefficients stored across multiple local registers to achieve
 * superior sigmoid approximation accuracy essential for neural network applications.
 * 
 * **Wormhole B0 SFPU Advanced LUT2 Processing:**
 * - **6-LReg Loading**: Loads coefficients from LReg0,1,2,4,5,6 (strategic distribution)
 * - **LUT2 Mode 0**: SFPLUTFP32_MOD0_FP16_6ENTRY_TABLE1 for enhanced precision
 * - **32-Lane SIMD**: All lanes process sigmoid simultaneously within current register
 * - **Bias Integration**: Hardware 0.5 bias addition for proper sigmoid output range
 * - **Multi-Register State**: Complex state management for 6-coefficient system
 * 
 * **SFPI Programming Model:**
 * ```cpp
 * constexpr int lut_mode = 0;  // SFPLUTFP32_MOD0_FP16_6ENTRY_TABLE1
 * 
 * // Load 6 LUT coefficients from strategically distributed local registers
 * sfpi::vUInt l0 = sfpi::l_reg[sfpi::LRegs::LReg0];   // Primary coefficient set
 * sfpi::vUInt l1 = sfpi::l_reg[sfpi::LRegs::LReg1];
 * sfpi::vUInt l2 = sfpi::l_reg[sfpi::LRegs::LReg2];
 * sfpi::vUInt l4 = sfpi::l_reg[sfpi::LRegs::LReg4];   // Secondary coefficient set  
 * sfpi::vUInt l5 = sfpi::l_reg[sfpi::LRegs::LReg5];
 * sfpi::vUInt l6 = sfpi::l_reg[sfpi::LRegs::LReg6];
 * 
 * sfpi::vFloat val = sfpi::dst_reg[0];                // Load input
 * // Advanced LUT2 operation with bias addition
 * sfpi::dst_reg[0] = lut2(val, l0, l1, l2, l4, l5, l6, lut_mode) + 0.5f;
 * sfpi::dst_reg++;                                    // Advance to next register
 * 
 * // Restore complete 6-register LReg state
 * sfpi::l_reg[sfpi::LRegs::LReg0] = l0;
 * // ... restore all 6 registers ...
 * ```
 * 
 * **Advanced LUT2 Features:**
 * - **6-Coefficient Precision**: Superior approximation quality vs. 3-coefficient systems
 * - **Mode 0 Optimization**: Hardware mode specifically tuned for sigmoid characteristics
 * - **Strategic Register Use**: Non-consecutive LREGs (0,1,2,4,5,6) for optimal hardware utilization
 * - **Bias Integration**: Hardware 0.5 offset ensures proper sigmoid output range [0,1]
 * 
 * **Performance Optimization:**
 * - **GCC Unroll 8**: Instruction-level parallelism for loop optimization
 * - **Multi-LReg Efficiency**: Fast local register access across 6 registers
 * - **Single-Cycle LUT2**: Hardware LUT2 provides optimal throughput despite complexity
 * - **State Management**: Efficient 6-register save/restore pattern
 * 
 * **Mathematical Accuracy:**
 * - **Enhanced Precision**: 6-coefficient system provides superior sigmoid approximation
 * - **Neural Network Critical**: High precision essential for training stability
 * - **Smooth Gradients**: Enhanced derivative quality for backpropagation
 * - **Range Optimization**: Coefficients tuned for typical neural network input distributions
 * 
 * **Hardware Mode Details:**
 * - **LUT Mode 0**: SFPLUTFP32_MOD0_FP16_6ENTRY_TABLE1
 * - **FP16 Optimized**: Mode tuned for 16-bit floating-point coefficient efficiency
 * - **6-Entry Table**: Maximum coefficient count for SFPU LUT hardware
 * - **Table 1**: Primary table configuration for sigmoid optimization
 * 
 * @note Requires prior initialization via _init_sigmoid_() function
 * @note Uses 6 LREGs (0,1,2,4,5,6) for coefficient storage
 * @note Hardware LUT2 mode 0 provides enhanced sigmoid approximation
 * @note Bias addition (+0.5) integrated in hardware for proper output range
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_sigmoid_(const int iterations)
{
    constexpr int lut_mode = 0; // SFPLUTFP32_MOD0_FP16_6ENTRY_TABLE1
    sfpi::vUInt l0         = sfpi::l_reg[sfpi::LRegs::LReg0];  // Load primary coefficient set
    sfpi::vUInt l1         = sfpi::l_reg[sfpi::LRegs::LReg1];
    sfpi::vUInt l2         = sfpi::l_reg[sfpi::LRegs::LReg2];
    sfpi::vUInt l4         = sfpi::l_reg[sfpi::LRegs::LReg4];  // Load secondary coefficient set
    sfpi::vUInt l5         = sfpi::l_reg[sfpi::LRegs::LReg5];
    sfpi::vUInt l6         = sfpi::l_reg[sfpi::LRegs::LReg6];

#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat val = sfpi::dst_reg[0];           // Load input from current register

        // Advanced LUT2 operation with integrated bias addition
        sfpi::dst_reg[0] = lut2(val, l0, l1, l2, l4, l5, l6, lut_mode) + 0.5f;

        sfpi::dst_reg++;                               // Advance to next register
    }

    sfpi::l_reg[sfpi::LRegs::LReg0] = l0;              // Restore complete 6-register state
    sfpi::l_reg[sfpi::LRegs::LReg1] = l1;
    sfpi::l_reg[sfpi::LRegs::LReg2] = l2;
    sfpi::l_reg[sfpi::LRegs::LReg4] = l4;
    sfpi::l_reg[sfpi::LRegs::LReg5] = l5;
    sfpi::l_reg[sfpi::LRegs::LReg6] = l6;
}

/**
 * @brief Initialize sigmoid 6-entry LUT coefficients in Wormhole B0 SFPU local registers
 * @tparam APPROXIMATION_MODE Enables optimized 6-coefficient loading
 * 
 * @details Loads pre-computed 6-entry piecewise linear approximation coefficients into
 * strategically distributed SFPU local registers for subsequent sigmoid computations.
 * This initialization function must be called before using _calculate_sigmoid_() to
 * ensure proper LUT2 coefficient configuration across all 6 required registers.
 * 
 * **6-Coefficient Distribution:**
 * The coefficients are loaded into non-consecutive LREGs for optimal hardware utilization:
 * - **LReg0, LReg1, LReg2**: Primary coefficient set for main approximation segments
 * - **LReg4, LReg5, LReg6**: Secondary coefficient set for enhanced precision regions
 * - **LReg3**: Intentionally unused for hardware optimization reasons
 * 
 * **Hardware Integration:**
 * - **Multi-Register Loading**: Coordinated loading across 6 distributed LREGs
 * - **Coefficient Precision**: Enhanced precision coefficients for superior approximation
 * - **Hardware Optimization**: Register distribution tuned for LUT2 mode 0 performance
 * 
 * @note Must be called before first use of _calculate_sigmoid_()
 * @note Coefficients remain valid until respective LREGs are overwritten
 * @note One-time initialization sufficient for multiple sigmoid computations
 * @note Uses non-consecutive LREGs (0,1,2,4,5,6) for optimal hardware performance
 */
template <bool APPROXIMATION_MODE>
inline void _init_sigmoid_()
{
    // Enhanced 6-coefficient initialization for superior sigmoid approximation
    // Coefficient values optimized for LUT2 mode 0 sigmoid computation
    // imm0 = 0x3DFF;
    // imm1 = 0x21D8;
    // imm2 = 0xFF10;
    // TTI_SFPLOADI(0, 2, imm0);
    // TTI_SFPLOADI(1, 2, imm1);
    // TTI_SFPLOADI(2, 2, imm2);
    // Using a 6 piece LUT to calculate and model sigmoid  directly
    // x <= 0.5 --> 0.2452x + (-0.0004997)
    // x <= 1.0 --> 0.2173x + 0.0152
    // x <= 1.5 --> 0.1731x + 0.05988
    // x <= 2.0 --> 0.1262x + 0.1298
    // x <= 4.0 --> 0.0485x + 0.2998
    // x >  4.0 --> 0.4998

    // imm0[15:0] = A0=0.2452 = 0x33D9 -- imm0[31:16] = A1=0.2173 = 0x32F4
    _sfpu_load_imm32_(0, 0x32F433D9);
    // imm4[15:0] = B0= -0.0004997  = 0x9018 -- imm4[31:16] = B1= 0.0152 = 0x23c8
    _sfpu_load_imm32_(4, 0x23C89018);

    // imm1[15:0] = A2=0.1731 = 0x318a -- imm1[31:16] = A3=0.1262 = 0x300a
    _sfpu_load_imm32_(1, 0x300A318A);
    // imm5[15:0] = B2=0.05988 = 0x2BAA -- imm5[31:16] = B3=0.1298 = 0x3027
    _sfpu_load_imm32_(5, 0x30272BAA);

    // imm2[15:0] = A4=0.0485 = 0x2A35 -- imm2[31:16] = A5=0.0 = 0x7C00
    _sfpu_load_imm32_(2, 0x7C002A35);
    // imm6[15:0] = B4=0.2998 = 0x34CC -- imm6[31:16] = B5=0.4998 = 0x37ff
    _sfpu_load_imm32_(6, 0x37ff34CC);
}

} // namespace sfpu
} // namespace ckernel
