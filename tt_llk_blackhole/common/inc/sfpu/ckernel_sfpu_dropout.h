// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_ops.h"
#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Implement dropout regularization using SFPU hardware PRNG and conditional execution
 *
 * @details Performs dropout regularization on neural network data using hardware-accelerated
 * pseudorandom number generation and conditional execution. Dropout randomly sets a fraction
 * of input elements to zero during training to prevent overfitting, while scaling the
 * remaining elements to maintain expected output magnitude.
 *
 * **Mathematical Operation:**
 * For each element in the input tensor:
 * ```
 * random_value = hardware_prng()
 * if (random_value < probability_threshold):
 *     output[i] = 0.0                    # Drop this element
 * else:
 *     output[i] = input[i] * scale       # Keep and scale this element
 * ```
 *
 * **Dropout Algorithm:**
 * 1. **Generate Random Number**: Use SFPU hardware PRNG to generate random uint32_t
 * 2. **Clear Sign Bit**: Ensure positive value for comparison
 * 3. **Compare with Threshold**: Check if random_value < probability_threshold
 * 4. **Conditional Operation**: 
 *    - If dropping: Set output to 0.0
 *    - If keeping: Scale input by scale factor
 *
 * **Parameter Interpretation:**
 * - `probability`: Dropout probability as integer threshold (0 to INT_MAX)
 *   - Higher values = more elements dropped
 *   - Represents the threshold for random comparison
 * - `scale`: Scaling factor as IEEE-754 float32 bit representation
 *   - Typically `1.0 / (1.0 - dropout_rate)` to maintain output magnitude
 *   - Applied to elements that are NOT dropped
 *
 * **Hardware PRNG Usage:**
 * ```assembly
 * TTI_SFPMOV(0, 9, LREG3, 8);     // Generate pseudorandom uint32_t
 * TTI_SFPSETSGN(0, LREG3, LREG3, 1); // Clear sign bit (force positive)
 * ```
 * The instruction `TTI_SFPMOV` with `instr_mod1=8` and `lreg_c=9` triggers
 * the hardware PRNG to generate a pseudorandom number per lane.
 *
 * **Conditional Execution Flow:**
 * ```assembly
 * // Compare: random < probability_threshold
 * TTI_SFPIADD(0, LREG2, LREG3, 10);  // Sets condition flags
 * 
 * // Conditional move: if (condition) output = 0.0
 * TTI_SFPMOV(0, LCONST_0, LREG0, 0); // Move 0.0 to output if condition true
 * 
 * // Enable/disable conditional execution
 * TTI_SFPENCC(0, 0, 0, 0);
 * ```
 *
 * **Parameter Loading Strategy:**
 * Scale and probability parameters are loaded into LREG registers using 16-bit chunks:
 * ```assembly
 * // Load scale factor (float32 as two 16-bit parts)
 * TT_SFPLOADI(LREG1, 10, scale & 0xFFFF);    // Low 16 bits
 * TT_SFPLOADI(LREG1, 8, scale >> 16);        // High 16 bits
 * 
 * // Load probability threshold (int32 as two 16-bit parts)  
 * TT_SFPLOADI(LREG2, 10, probability & 0xFFFF); // Low 16 bits
 * TT_SFPLOADI(LREG2, 8, probability >> 16);     // High 16 bits
 * ```
 *
 * **Processing Loop Structure:**
 * ```assembly
 * for each_iteration:
 *     TTI_SFPLOAD(LREG0, 0, 3, 0);              // Load input data
 *     TTI_SFPMUL(LREG0, LREG1, LCONST_0, LREG0, 0); // Scale input
 *     TTI_SFPMOV(0, 9, LREG3, 8);               // Generate random number
 *     TTI_SFPSETSGN(0, LREG3, LREG3, 1);        // Clear sign bit
 *     TTI_SFPIADD(0, LREG2, LREG3, 10);         // Compare with threshold
 *     TTI_SFPMOV(0, LCONST_0, LREG0, 0);        // Conditionally zero output
 *     TTI_SFPENCC(0, 0, 0, 0);                  // Manage conditional execution
 *     TTI_SFPSTORE(0, 0, 3, 0);                 // Store result
 *     dst_reg++;                                // Advance to next vector
 * ```
 *
 * **Neural Network Context:**
 * Dropout is a regularization technique that:
 * - **Training**: Randomly "drops out" neurons to prevent overfitting
 * - **Inference**: Usually disabled (dropout_rate = 0) or absorbed into weights
 * - **Scaling**: Compensates for dropped neurons to maintain output magnitude
 * - **Stochastic**: Different random pattern each forward pass
 *
 * **Common Usage Patterns:**
 * ```cpp
 * // Training with 50% dropout rate
 * uint32_t prob_threshold = 0x7FFFFFFF;  // 50% of uint32_t range
 * uint32_t scale_bits = 0x40000000;      // 2.0f in IEEE-754 (1/(1-0.5))
 * _calculate_dropout_<false, 8>(tile_size, prob_threshold, scale_bits);
 * 
 * // Training with 20% dropout rate  
 * uint32_t prob_threshold = 0x33333333;  // ~20% of uint32_t range
 * uint32_t scale_bits = 0x3FA66666;      // 1.25f in IEEE-754 (1/(1-0.2))
 * _calculate_dropout_<false, 8>(tile_size, prob_threshold, scale_bits);
 * ```
 *
 * **Hardware Performance:**
 * - **PRNG**: Hardware pseudorandom number generation per lane
 * - **SIMD**: 32 parallel dropout decisions per cycle
 * - **Conditional**: Hardware conditional execution avoids branching overhead
 * - **Throughput**: Processes 32 elements per iteration cycle
 *
 * **Register Usage:**
 * - **LREG0**: Input data, scaled output, final result
 * - **LREG1**: Scale factor (IEEE-754 float32)
 * - **LREG2**: Probability threshold (int32)
 * - **LREG3**: Generated random number
 * - **LCONST_0**: Constant 0.0f for zeroing dropped elements
 *
 * **PRNG Quality Considerations:**
 * The hardware PRNG provides sufficient randomness for dropout but may have
 * limited statistical properties. For critical applications requiring high-quality
 * randomness, consider seeding appropriately and monitoring training stability.
 *
 * @tparam APPROXIMATION_MODE If true, may use faster but less precise operations.
 *                            If false, uses exact floating-point operations.
 * @tparam ITERATIONS Template parameter for loop unrolling optimization.
 *
 * @param iterations Number of 32-element vectors to process (runtime parameter).
 * @param probability Dropout probability threshold as integer (0 to INT_MAX).
 *                   Higher values increase dropout rate.
 * @param scale Scaling factor as IEEE-754 float32 bit representation.
 *              Applied to elements that are NOT dropped.
 *
 * @note Requires PRNG to be initialized via _init_dropout_() before first use
 * @note Function processes data in-place, overwriting input with dropout results
 * @note Random numbers generated independently for each SIMD lane (32 parallel)
 * @note Conditional execution ensures dropped elements are exactly 0.0
 * @note Scale parameter should account for expected dropout rate for proper magnitude
 * @note Pragma unroll 0 disables loop unrolling for code size optimization
 */
// probability should be between 0 - INT_MAX (signed)
// scale should be binary representation of a float32
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_dropout_(const int iterations, uint probability, uint scale)
{
    // SFPU microcode

    // Load scale factor into LREG1 (float32 as two 16-bit chunks)
    TT_SFPLOADI(p_sfpu::LREG1, 10, scale & 0xFFFF);      // Load low 16 bits of scale
    TT_SFPLOADI(p_sfpu::LREG1, 8, scale >> 16);          // Load high 16 bits of scale
    
    // Load probability threshold into LREG2 (int32 as two 16-bit chunks)
    TT_SFPLOADI(p_sfpu::LREG2, 10, probability & 0xFFFF); // Load low 16 bits of probability
    TT_SFPLOADI(p_sfpu::LREG2, 8, probability >> 16);     // Load high 16 bits of probability

#pragma GCC unroll 0  // Disable loop unrolling for code size optimization
    for (int d = 0; d < iterations; d++)
    {
        ////////////////////////
        // Scale input samples
        // Equivalent to: dst_reg[0] = dst_reg[0] * scale_factor
        ///////////////////////
        TTI_SFPLOAD(p_sfpu::LREG0, 0, 3, 0);    // Load input data from destination register
        TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LCONST_0, p_sfpu::LREG0, 0); // Multiply by scale factor

        ////////////////////////
        // Generate pseudorandom number using hardware PRNG
        // TTI_SFPMOV with instr_mod1=8 and lreg_c=9 generates uint32_t pseudorandom number
        // Arguments: (imm12_math, lreg_c, lreg_dest, instr_mod1)
        // Clear sign-bit to ensure positive value for comparison with probability threshold
        ////////////////////////
        TTI_SFPMOV(0, 9, p_sfpu::LREG3, 8);                // Generate random uint32_t per lane
        TTI_SFPSETSGN(0, p_sfpu::LREG3, p_sfpu::LREG3, 1); // Clear sign bit (force positive)

        ////////////////////////
        // Apply dropout decision based on random comparison
        // Conditional logic: if (random_number < probability_threshold) output = 0.0
        // else output = scaled_input (already computed above)
        ///////////////////////
        TTI_SFPIADD(0, p_sfpu::LREG2, p_sfpu::LREG3, 10);  // Compare: set flags based on random < probability
        TTI_SFPMOV(0, p_sfpu::LCONST_0, p_sfpu::LREG0, 0); // Conditionally move 0.0 to output if dropping
        TTI_SFPENCC(0, 0, 0, 0);                           // Enable/disable conditional execution based on flags
        TTI_SFPSTORE(0, 0, 3, 0);                          // Store final result (either 0.0 or scaled_input)

        sfpi::dst_reg++;  // Advance to next 32-element vector
    }
}

/**
 * @brief Initialize hardware PRNG for dropout operations
 *
 * @details Seeds the hardware pseudorandom number generator used by dropout operations.
 * This initialization must be called before using _calculate_dropout_() to ensure
 * proper random number generation. Different seeds produce different dropout patterns,
 * which is important for training reproducibility and stochastic behavior.
 *
 * **PRNG Seeding:**
 * The hardware PRNG maintains internal state that determines the sequence of
 * random numbers generated. Proper seeding ensures:
 * - **Reproducibility**: Same seed produces same dropout pattern
 * - **Stochasticity**: Different seeds produce different patterns
 * - **Quality**: Good seed distribution improves randomness quality
 *
 * **Initialization Sequence:**
 * ```cpp
 * // Initialize PRNG with specific seed
 * _init_dropout_(12345);
 * 
 * // Now dropout operations will use seeded PRNG
 * _calculate_dropout_<false, 8>(iterations, prob, scale);
 * ```
 *
 * **Seed Selection Guidelines:**
 * - **Training**: Use different seed each epoch/batch for variety
 * - **Debugging**: Use fixed seed for reproducible behavior
 * - **Quality**: Avoid simple patterns (0, 1, 2, 3...) 
 * - **Distribution**: Use well-distributed values across uint32_t range
 *
 * **Hardware Context:**
 * Calls the underlying `init_prng_seed()` function which configures the
 * SFPU hardware PRNG state. This affects all subsequent PRNG operations
 * until the next re-seeding.
 *
 * @param seed 32-bit seed value for initializing the PRNG state.
 *             Different seeds produce different random sequences.
 *
 * @note Must be called before first use of _calculate_dropout_()
 * @note Seed affects all PRNG operations, not just dropout
 * @note Consider using time-based or entropy-based seeds for training
 * @note Fixed seeds useful for debugging and reproducible testing
 * @note PRNG state is shared across all SFPU operations using randomness
 */
inline void _init_dropout_(const uint seed)
{
    init_prng_seed(seed);  // Initialize hardware PRNG with provided seed
}

} // namespace sfpu
} // namespace ckernel
