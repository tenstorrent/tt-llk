// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_sfpu_converter.h"
#include "ckernel_sfpu_exp.h"
#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel::sfpu
{

/**
 * @brief Compute element-wise ELU (Exponential Linear Unit) activation function using SFPU hardware
 *
 * @details Performs element-wise ELU activation computation on FP32 data using the
 * Tensix Vector Unit's 32-lane SIMD engine. Implements ELU(x) = x for x ≥ 0 and
 * α(e^x - 1) for x < 0, providing smooth, differentiable activation with exponential
 * behavior for negative inputs.
 *
 * **Mathematical Operation:**
 * For each lane i in 32-lane SIMD:
 * ```
 * output[i] = ELU(input[i]) = {
 *     input[i]           if input[i] ≥ 0
 *     α(e^input[i] - 1)  if input[i] < 0
 * }
 * 
 * Where α (alpha) is the slope parameter for negative inputs.
 * 
 * Properties:
 * - ELU(0) = 0
 * - ELU'(0) = 1 (continuous derivative)
 * - ELU(x) → -α as x → -∞
 * - Smooth transition at x = 0
 * - Reduces vanishing gradient problem
 * ```
 *
 * **Algorithm Implementation:**
 * ```
 * 1. Check if input is negative
 * 2. For x ≥ 0: output = x (identity function)
 * 3. For x < 0: 
 *    a. Compute exponential using piecewise approximation
 *    b. Subtract 1 from exponential result
 *    c. Multiply by slope parameter α
 * ```
 *
 * **Exponential Computation:**
 * - Uses `_calculate_exponential_piecewise_` for high-accuracy e^x computation
 * - Configurable approximation mode for speed/accuracy trade-off
 * - No scaling or positive check skipping for ELU requirements
 * - Base scale factor set to 1.0 for standard exponential
 *
 * **Hardware Context:**
 * - Executes on Tensix Vector Unit (SFPU) backend
 * - Uses SFPU conditional execution and exponential computation
 * - Processes 32 values simultaneously across SIMD lanes
 * - Template-based iteration count for compile-time optimization
 * - Requires exponential function initialization before use
 *
 * **Performance Characteristics:**
 * - Latency: ~6-8 cycles per SIMD operation (conditional + exponential + arithmetic)
 * - Throughput: 1 instruction per cycle
 * - Peak Rate: ~4-5 GOps/sec @ 1GHz (limited by exponential computation)
 * - Memory Pattern: Single-input, single-output with in-place result storage
 * - Performance depends on input distribution (positive vs negative)
 *
 * **Common Use Cases:**
 * - Neural network activation function with smooth negative behavior
 * - Alternative to ReLU with better gradient properties
 * - Convolutional neural networks for image processing
 * - Deep learning models requiring smooth activations
 * - Medical imaging and signal processing applications
 * - Recurrent neural networks with gradient stability requirements
 * - Autoencoders and generative models
 *
 * **Activation Function Comparison:**
 * - **vs ReLU**: Smooth for negative inputs, reduces dead neuron problem
 * - **vs Leaky ReLU**: Exponential saturation vs linear negative slope
 * - **vs GELU**: Simpler computation, different mathematical basis
 * - **vs Swish/SiLU**: Different negative behavior, no self-gating
 *
 * **SFPU Microcode:**
 * ```cpp
 * sfpi::vFloat s = Converter::as_float(slope);   // Convert slope parameter
 * 
 * #pragma GCC unroll 0
 * for (each_iteration) {
 *     sfpi::vFloat v = sfpi::dst_reg[0];         // Load input (32 lanes)
 *     
 *     v_if (v < 0.0f) {                          // Handle negative inputs
 *         sfpi::vFloat v_exp = _calculate_exponential_piecewise_<...>(v, ...);
 *         v = s * (v_exp - 1.0f);                // α(e^x - 1)
 *     }
 *     v_endif;                                   // Positive inputs unchanged
 *     
 *     sfpi::dst_reg[0] = v;                      // Store result
 *     sfpi::dst_reg++;                           // Advance to next register
 * }
 * ```
 *
 * @tparam APPROXIMATION_MODE If true, uses faster exponential approximation.
 *                            If false, uses higher precision exponential computation.
 * @tparam ITERATIONS Template parameter defining fixed loop iteration count.
 *
 * @param slope Alpha parameter (α) controlling negative slope behavior
 *
 * @note Input values must be in FP32 format in SFPU destination registers
 * @note Must call `_init_elu_()` before first use to initialize exponential function
 * @note Operation is performed in-place, overwriting input data with ELU result
 * @note Function processes ITERATIONS worth of 32-element vectors
 * @note No runtime iteration parameter - uses template ITERATIONS value
 * @note Slope parameter typically set to 1.0 for standard ELU
 * @note Positive inputs pass through unchanged (identity function)
 * @note Exponential computation may be computationally expensive for very negative inputs
 */
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_elu_(uint slope)
{
    const bool SCALE_EN                  = false; // Elu does not use scale.
    const bool SKIP_POSITIVE_CHECK       = false; // Elu does not skip positive check.
    const uint16_t exp_base_scale_factor = p_sfpu::kCONST_1_FP16B;  // Base scale factor (1.0)

    sfpi::vFloat s = Converter::as_float(slope);   // Convert slope parameter to float
#pragma GCC unroll 0
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat v = sfpi::dst_reg[0];          // Load input (32-lane vector)

        v_if (v < 0.0f)                             // Process negative inputs only
        {
            sfpi::vFloat v_exp = _calculate_exponential_piecewise_<APPROXIMATION_MODE, SCALE_EN, SKIP_POSITIVE_CHECK>(v, exp_base_scale_factor);  // Compute e^x
            v                  = s * (v_exp - 1.0f); // Apply ELU formula: α(e^x - 1)
        }
        v_endif;                                    // Positive inputs remain unchanged

        sfpi::dst_reg[0] = v;                       // Store ELU result

        sfpi::dst_reg++;                            // Advance to next 32-element vector
    }
}

/**
 * @brief Initialize exponential function for ELU computation
 *
 * @details Configures the SFPU exponential function components required for
 * ELU activation computation. Must be called before using `_calculate_elu_()`
 * to ensure proper exponential function initialization.
 *
 * **Initialization Configuration:**
 * - Base scale factor: 1.0 (standard exponential, no scaling)
 * - Fast approximation: Disabled (ELU requires good exponential accuracy)
 * - Configures lookup tables and constants for exponential computation
 *
 * **Exponential Setup:**
 * - Uses standard exponential base (e ≈ 2.71828)
 * - No fast approximation mode (prioritizes accuracy over speed)
 * - Proper initialization for piecewise exponential computation
 * - Same initialization supports both approximation modes
 *
 * **Hardware Context:**
 * - Configures SFPU exponential computation subsystem
 * - Sets up lookup tables and constants for e^x computation
 * - One-time setup cost amortized across multiple ELU calls
 * - Configuration persists until exponential state is overwritten
 *
 * **Performance Characteristics:**
 * - Initialization overhead: Variable (depends on exponential complexity)
 * - Configuration persists across multiple function calls
 * - No runtime overhead once initialized
 * - Optimized for ELU-specific exponential requirements
 *
 * @tparam APPROXIMATION_MODE If true, may enable some exponential optimizations.
 *                            If false, uses full precision exponential setup.
 *
 * @note Must be called before first use of `_calculate_elu_()`
 * @note Configuration persists until exponential state is changed
 * @note Uses standard exponential base and scaling
 * @note Disables fast approximation for better ELU accuracy
 * @note Same initialization supports all ELU slope parameters
 */
template <bool APPROXIMATION_MODE>
inline void _init_elu_()
{
    const uint32_t EXP_BASE_SCALE_FACTOR = 0x3F800000;  // 1.0f in hex representation
    const bool FAST_APPROX               = false; // Elu does not use fast approximation.
    _init_exponential_<APPROXIMATION_MODE, FAST_APPROX, EXP_BASE_SCALE_FACTOR>();  // Initialize exponential function
}

} // namespace ckernel::sfpu
