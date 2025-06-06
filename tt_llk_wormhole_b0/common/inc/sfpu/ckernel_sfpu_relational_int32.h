// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

enum class RelationalOp : uint8_t
{
    EQ = 0,
    NE = 1,
    LT = 2,
    LE = 3,
    GT = 4,
    GE = 5,
};

template <bool APPROXIMATION_MODE, RelationalOp RELATIONAL_OP, int ITERATIONS = 8>
inline void _calculate_sfpu_relational_int32_(const uint dst_offset)
{
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        constexpr uint dst_tile_size = 32;
        sfpi::vInt in0               = sfpi::dst_reg[0];
        sfpi::vInt in1               = sfpi::dst_reg[dst_offset * dst_tile_size];
        sfpi::vInt result            = 0;

        if constexpr (RELATIONAL_OP == RelationalOp::EQ)
        {
            v_if (in0 == in1)
            {
                result = 1;
            }
            v_endif;
        }
        else if constexpr (RELATIONAL_OP == RelationalOp::NE)
        {
            v_if (in0 != in1)
            {
                result = 1;
            }
            v_endif;
        }
        else if constexpr (RELATIONAL_OP == RelationalOp::LT)
        {
            v_if (in0 < in1)
            {
                result = 1;
            }
            v_endif;
        }
        else if constexpr (RELATIONAL_OP == RelationalOp::LE)
        {
            v_if (in0 <= in1)
            {
                result = 1;
            }
            v_endif;
        }
        else if constexpr (RELATIONAL_OP == RelationalOp::GT)
        {
            v_if (in0 > in1)
            {
                result = 1;
            }
            v_endif;
        }
        else if constexpr (RELATIONAL_OP == RelationalOp::GE)
        {
            v_if (in0 >= in1)
            {
                result = 1;
            }
            v_endif;
        }

        sfpi::dst_reg[0] = result;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE /*unused*/, RelationalOp RELATIONAL_OP>
inline void _sfpu_relational_int32_init_()
{
}

} // namespace sfpu
} // namespace ckernel
