// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_defs.h"
#include "sfpi.h"

namespace ckernel::sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_where_fp16_b_(const uint dst_index_in0, const uint dst_index_in1, const uint dst_index_in2, const uint dst_index_out)
{
    constexpr uint dst_tile_size_rows      = 64;
    constexpr uint dst_tile_size_rows_sfpi = 32;

    sfpi::vFloat cond = sfpi::dst_reg[dst_index_in0 * dst_tile_size_rows_sfpi];

    for (int i = 0; i < ITERATIONS; i++)
    {
        cond = sfpi::dst_reg[dst_index_in0 * dst_tile_size_rows_sfpi];

        v_if (cond == 0.0f)
        {
            // output_tensor = false_tensor;
            TT_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::LO16, ADDR_MOD_3, dst_index_in2 * dst_tile_size_rows);
        }
        v_else
        {
            // output_tensor = true_tensor;
            TT_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::LO16, ADDR_MOD_3, dst_index_in1 * dst_tile_size_rows);
        }
        v_endif;
        // sfpi::dst_reg[dst_index_out * dst_tile_size_rows_sfpi] = output_tensor;
        TT_SFPSTORE(p_sfpu::LREG3, InstrModLoadStore::LO16, ADDR_MOD_3, dst_index_out * dst_tile_size_rows);

        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_where_fp32_(const uint dst_index_in0, const uint dst_index_in1, const uint dst_index_in2, const uint dst_index_out)
{
    constexpr uint dst_tile_size = 32;

    sfpi::vFloat output_tensor = 0;
    sfpi::vFloat true_tensor   = 0;
    sfpi::vFloat false_tensor  = 0;
    sfpi::vFloat cond          = sfpi::dst_reg[dst_index_in0 * dst_tile_size];

    for (int i = 0; i < ITERATIONS; i++)
    {
        cond         = sfpi::dst_reg[dst_index_in0 * dst_tile_size];
        true_tensor  = sfpi::dst_reg[dst_index_in1 * dst_tile_size];
        false_tensor = sfpi::dst_reg[dst_index_in2 * dst_tile_size];

        v_if (cond != 0.0f)
        {
            output_tensor = true_tensor;
        }
        v_else
        {
            output_tensor = false_tensor;
        }
        v_endif;

        sfpi::dst_reg[dst_index_out * dst_tile_size] = output_tensor;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, DataFormat data_format, int ITERATIONS>
inline void _calculate_where_(const uint dst_index_in0, const uint dst_index_in1, const uint dst_index_in2, const uint dst_index_out)
{
    // Add a compile-time check to ensure only supported formats are used.
    static_assert(
        data_format == DataFormat::Float32 || data_format == DataFormat::Float16_b,
        "Unsupported data format for _calculate_where_(). Only Float32 and Float16_b are allowed.");
    if constexpr (data_format == DataFormat::Float32)
    {
        _calculate_where_fp32_<APPROXIMATION_MODE, ITERATIONS>(dst_index_in0, dst_index_in1, dst_index_in2, dst_index_out);
    }
    else
    {
        _calculate_where_fp16_b_<APPROXIMATION_MODE, ITERATIONS>(dst_index_in0, dst_index_in1, dst_index_in2, dst_index_out);
    }
}

} // namespace ckernel::sfpu
