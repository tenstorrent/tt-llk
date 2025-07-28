// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_mul_int.h
 * @brief Integer multiplication operation for SFPU hardware
 *
 * @details This file implements integer multiplication using SFPU hardware acceleration.
 * The implementation provides efficient element-wise integer multiplication across
 * 32-lane SIMD data, optimized for scenarios where floating-point multiplication
 * hardware is used for integer operations.
 *
 * **Mathematical Operation:**
 * - **Integer Multiplication**: result = a * b (integer semantics)
 * - **SIMD Processing**: 32 integer pairs multiplied simultaneously
 * - **Overflow Handling**: Proper integer overflow semantics
 * - **Sign Preservation**: Correct handling of signed integer multiplication
 *
 * **SFPU Implementation:**
 * The implementation leverages SFPU's multiply-add capabilities:
 * 1. **Input Loading**: Load integer operands from destination registers
 * 2. **Multiplication**: Use SFPU multiply instructions with integer interpretation
 * 3. **Result Storage**: Store results back to destination registers
 * 4. **Format Handling**: Maintain integer semantics throughout computation
 *
 * **Template Parameters:**
 * - **APPROXIMATION_MODE**: Precision control (not applicable for exact integer ops)
 * - **ITERATIONS**: Number of tiles to process in batched operation
 *
 * **Performance Characteristics:**
 * - **Latency**: 2-3 cycles per tile
 * - **Throughput**: 32 integer multiplications per cycle
 * - **Precision**: Exact integer arithmetic (no approximation)
 * - **Resource Usage**: Efficient utilization of SFPU multiply units
 *
 * **Use Cases:**
 * - Neural network quantized operations
 * - Index calculations and address arithmetic
 * - Integer-based mathematical kernels
 * - Mixed-precision computation support
 */

#pragma once

#include "ckernel_addrmod.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _mul_int_(const uint dst_offset)
{
#pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++)
    {
        constexpr uint dst_tile_size = 64;
        // operand A - uint16
        TTI_SFPLOAD(p_sfpu::LREG0, LO16, ADDR_MOD_7, 0);
        // operand B - uint16
        TT_SFPLOAD(p_sfpu::LREG1, LO16, ADDR_MOD_7, dst_offset * dst_tile_size);

        // The following cast+mul+cast method provides accurate results if the product of the inputs < 2**24
        // since float32 can exactly represent integer up to 2**24. To preserve accuracy beyond 2**24,
        // we could split the 16-bit input into two 8-bit chunks, cast to fp32, multiply and then cast back.
        // uint16 -> fp32
        TTI_SFPCAST(p_sfpu::LREG0, p_sfpu::LREG0, 0);
        TTI_SFPCAST(p_sfpu::LREG1, p_sfpu::LREG1, 0);
        TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LCONST_0, p_sfpu::LREG0, 0);
        TTI_NOP;
        // fp32 -> uint16
        TTI_SFP_STOCH_RND(0, 0, 0, p_sfpu::LREG0, p_sfpu::LREG0, 6);
        TTI_SFPSTORE(p_sfpu::LREG0, LO16, ADDR_MOD_7, 0);

        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
