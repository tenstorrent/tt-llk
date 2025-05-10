// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_ops.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

enum class BinaryBitwiseOp : uint8_t
{
    AND = 0,
    OR  = 1,
    XOR = 2,
};

enum class DataType : uint8_t
{
    DEFAULT    = 0,
    FP16A      = 1,
    FP16B      = 2,
    FP32       = 3,
    INT32      = 4,
    INT32_COMP = 5,
    INT8       = 6,
    INT8_COMP  = 7,
    LO16       = 8,
    LO16_ONLY  = 9,
    HI16       = 10,
    HI16_ONLY  = 11,
};

template <bool APPROXIMATION_MODE, BinaryBitwiseOp BITWISE_OP, DataType DTYPE = INT32, int ITERATIONS = 8>
inline void _calculate_sfpu_binary_bitwise_(const uint dst_offset)
{
    constexpr uint8_t dtype = static_cast<uint8_t>(DTYPE);
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        constexpr uint dst_tile_size = 64;

        TTI_SFPLOAD(0, dtype, 3, 0);
        TT_SFPLOAD(1, dtype, 3, dst_offset * dst_tile_size);

        if constexpr (BITWISE_OP == BinaryBitwiseOp::AND)
        {
            TTI_SFPAND(0, 1, 0, 0);
        }
        else if constexpr (BITWISE_OP == BinaryBitwiseOp::OR)
        {
            TTI_SFPOR(0, 1, 0, 0);
        }
        else if constexpr (BITWISE_OP == BinaryBitwiseOp::XOR)
        {
            TTI_SFPXOR(0, 1, 0, 0);
        }

        TTI_SFPSTORE(0, dtype, 3, 0);
        sfpi::dst_reg++;
    }
}

} // namespace sfpu
} // namespace ckernel
