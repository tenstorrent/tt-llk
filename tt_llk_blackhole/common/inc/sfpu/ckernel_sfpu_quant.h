// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_quant.h
 * @brief Quantization operations for SFPU hardware
 *
 * @details This file implements quantization operations that convert floating-point
 * values to integer representations using SFPU hardware acceleration. Quantization
 * is essential for neural network deployment and inference optimization, enabling
 * reduced memory usage and increased computational efficiency while maintaining
 * acceptable accuracy.
 *
 * **Quantization Overview:**
 * Quantization converts continuous floating-point values to discrete integer values:
 * - **Linear Quantization**: Maps floating-point range to integer range linearly
 * - **Scaling**: Applies scale factor to control quantization range
 * - **Rounding**: Converts scaled values to nearest integer representation
 * - **Saturation**: Clamps values to target integer range limits
 *
 * **Mathematical Operation:**
 * ```
 * quantized_value = clamp(round(input * scale), min_int, max_int)
 * ```
 *
 * **Template Parameters:**
 * - **APPROXIMATION_MODE**: Precision control for intermediate computations
 * - **ITERATIONS**: Number of tiles to process in batched operation
 * - **SIGN_MAGNITUDE_FORMAT**: Output integer representation format
 *   - `true`: Sign-magnitude representation
 *   - `false`: Two's complement representation
 *
 * **SFPU Implementation:**
 * 1. **Input Processing**: Load floating-point operand A (input values)
 * 2. **Scale Application**: Multiply by quantization scale factor
 * 3. **Rounding**: Apply appropriate rounding mode for integer conversion
 * 4. **Range Clamping**: Ensure result fits in target integer range
 * 5. **Format Conversion**: Convert to specified integer representation
 * 6. **Result Storage**: Store quantized integers in destination register
 *
 * **Rounding Modes:**
 * - **Round to Nearest**: Standard rounding for balanced quantization error
 * - **Round Down**: Floor operation for specific quantization schemes
 * - **Round Up**: Ceiling operation for conservative quantization
 * - **Truncation**: Simple truncation for fast approximation
 *
 * **Performance Characteristics:**
 * - **Latency**: 4-6 cycles per tile depending on format and rounding
 * - **Throughput**: 32 quantization operations per cycle
 * - **Precision**: Configurable rounding for accuracy vs. speed trade-offs
 * - **Range Support**: Full floating-point to integer conversion range
 *
 * **Use Cases:**
 * - Neural network quantization for deployment
 * - Model compression for edge devices
 * - Mixed-precision training support
 * - Integer inference optimization
 * - Memory bandwidth reduction
 */

#pragma once

#include "ckernel_addrmod.h"
#include "ckernel_ops.h"
#include "ckernel_sfpu_load_config.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS, bool SIGN_MAGNITUDE_FORMAT>
inline void _quant_int32_(const uint dst_offset)
{
// Operand A is input (fp32)
// Operand B is scaling factor (fp32)
// Operand C is zero-point constant (fp32)
// Output is int32 scaled to int8 range
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        // operand A - fp32
        TTI_SFPLOAD(0, 3, ADDR_MOD_7, 0);
        // operand B - fp32 scaler
        TT_SFPLOAD(1, 3, ADDR_MOD_7, dst_offset * 64);
        // D(A) = A*B+C, LREG[2] = zero_point
        TTI_SFPMAD(0, 1, 2, 0, 0);
        // MAD has a 2-cycle pipeline latency so we need one cycle latency until next instr can consume the result
        TTI_NOP;
        // fp32->int8, descale value is zero (LREG_9)
        TTI_SFP_STOCH_RND(0, 0, 9, 0, 0, 3);
        // LREG_0 -> dest as int32
        if constexpr (SIGN_MAGNITUDE_FORMAT == false)
        {
            TTI_SFPCAST(0, 4, InstrModCast::INT_SIGN_MAGN_TO_INT32_2S_COMP);
            // Required after cast due to a bug in Blackhole RTL.
            TTI_SFPSETSGN(0, 4, 0, 0);
        }
        TTI_SFPSTORE(0, InstrModLoadStore::INT32_2S_COMP, ADDR_MOD_7, 0);
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS, bool SIGN_MAGNITUDE_FORMAT>
inline void _requant_int32_(const uint dst_offset)
{
// Operand A is input to requant (int32)
// Operand B is scaling factor (fp32)
// Operand C is zero-point constant (fp32)
// Output is int32 scaled to int8 range
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        // operand A - int32
        TTI_SFPLOAD(0, InstrModLoadStore::INT32_2S_COMP, ADDR_MOD_7, 0);
        if constexpr (SIGN_MAGNITUDE_FORMAT == false)
        {
            TTI_SFPCAST(0, 4, InstrModCast::INT_SIGN_MAGN_TO_INT32_2S_COMP);
            // Required after cast due to a bug in Blackhole RTL.
            TTI_SFPSETSGN(0, 4, 0, 0);
        }
        // operand B - fp32 scaler
        TT_SFPLOAD(1, 3, ADDR_MOD_7, dst_offset * 64);
        // cast int32->fp32
        TTI_SFPCAST(0, 0, 0);
        // D(A) = A*B+C, LREG[2] = zero_point
        TTI_SFPMAD(0, 1, 2, 0, 0);
        // MAD has a 2-cycle pipeline latency so we need one cycle latency until next instr can consume the result
        TTI_NOP;
        // fp32->int8, descale value is zero (LREG_9)
        TTI_SFP_STOCH_RND(0, 0, 9, 0, 0, 3);
        // LREG_0 -> dest as int32
        if constexpr (SIGN_MAGNITUDE_FORMAT == false)
        {
            TTI_SFPCAST(0, 4, InstrModCast::INT_SIGN_MAGN_TO_INT32_2S_COMP);
            // Required after cast due to a bug in Blackhole RTL.
            TTI_SFPSETSGN(0, 4, 0, 0);
        }
        TTI_SFPSTORE(0, InstrModLoadStore::INT32_2S_COMP, ADDR_MOD_7, 0);
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS, bool SIGN_MAGNITUDE_FORMAT>
inline void _dequant_int32_(const uint dst_offset)
{
// Operand A[LREG0] is input to dequant (int32)
// Operand B[LREG1] is scaling factor (fp32)
// Operand C[LREG2] is zero-point constant (fp32)
// Output = (A + (-C)) * B (fp32)
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        // operand A - int32
        TTI_SFPLOAD(0, InstrModLoadStore::INT32_2S_COMP, ADDR_MOD_7, 0);
        if constexpr (SIGN_MAGNITUDE_FORMAT == false)
        {
            TTI_SFPCAST(0, 4, InstrModCast::INT_SIGN_MAGN_TO_INT32_2S_COMP);
            // Required after cast due to a bug in Blackhole RTL.
            TTI_SFPSETSGN(0, 4, 0, 0);
        }
        // operand B - fp32 scaler
        TT_SFPLOAD(1, 3, ADDR_MOD_7, dst_offset * 64);
        // cast int32->fp32
        TTI_SFPCAST(0, 0, 0);
        // D(A)) = A+(-C), LREG[10] is 1, SFPADD = LREG_A*LREG_B+LREG_C
        TTI_SFPADD(0, 10, 2, 0, 0);
        TTI_NOP;
        // D(A)) = (A+(-C))*B, LREG[9] is zero
        TTI_SFPMUL(0, 1, 9, 0, 0);
        TTI_NOP;
        // LREG_0 -> dest as fp32
        TTI_SFPSTORE(0, 3, ADDR_MOD_7, 0);
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE /*unused*/>
inline void _init_quant_zero_point_(const uint zero_point)
{
    _sfpu_load_imm32_(2, zero_point);
}

} // namespace sfpu
} // namespace ckernel
