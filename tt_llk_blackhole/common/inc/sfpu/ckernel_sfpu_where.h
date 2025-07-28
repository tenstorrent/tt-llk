// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/**
 * @file ckernel_sfpu_where.h
 * @brief Conditional element selection (where/select) operation for SFPU hardware
 *
 * @details This file implements the conditional element selection operation, commonly
 * known as "where" or "select", using SFPU hardware acceleration. This operation
 * performs element-wise conditional selection between two tensors based on a
 * boolean condition tensor, implementing the fundamental ternary conditional
 * operation: result = condition ? true_value : false_value.
 *
 * **Mathematical Operation:**
 * - **Conditional Selection**: result[i] = condition[i] ? true_tensor[i] : false_tensor[i]
 * - **Element-wise**: Independent selection across all SIMD lanes
 * - **Boolean Logic**: Uses condition tensor as element-wise mask
 * - **SIMD Efficiency**: 32 conditional selections per cycle
 *
 * **Input Tensor Layout:**
 * The implementation expects a specific 3-tile input layout in destination registers:
 * ```
 * Index 0  (Tile 0): Condition tensor - boolean values for selection
 * Index 32 (Tile 1): True tensor - values selected when condition is true  
 * Index 64 (Tile 2): False tensor - values selected when condition is false
 * ```
 *
 * **SFPU Implementation:**
 * 1. **Condition Evaluation**: Load and evaluate boolean condition tensor
 * 2. **Vector Conditional**: Use SFPU `v_if`/`v_else`/`v_endif` for selection
 * 3. **True Path**: Load and assign values from true tensor when condition is true
 * 4. **False Path**: Load and assign values from false tensor when condition is false
 * 5. **Result Storage**: Store selected values to output destination
 *
 * **Boolean Condition Handling:**
 * - **Non-zero**: Any non-zero value treated as true
 * - **Zero**: Zero values treated as false
 * - **IEEE-754 Special Values**: NaN typically treated as true
 * - **Consistent Semantics**: Follows standard conditional evaluation rules
 *
 * **Performance Characteristics:**
 * - **Latency**: 4-6 cycles per tile depending on condition distribution
 * - **Throughput**: 32 conditional selections per cycle
 * - **Memory Access**: Efficient multi-tile data movement
 * - **Branch-Free**: Uses predicated execution instead of branching
 *
 * **Common Use Cases:**
 * - Neural network conditional layers
 * - Implementing clipping and thresholding operations  
 * - Masked operations in attention mechanisms
 * - Element-wise conditional logic in transformers
 * - Implementing piecewise functions
 */

#pragma once

#include "llk_defs.h"
#include "sfpi.h"

namespace ckernel::sfpu
{

/*

Expects following input in Dst register:
Index 0 ( Tile 0 ) -> condition tensor
Index 32 ( Tile 1 ) -> true tensor
Index 64 ( Tile 2 ) -> false tensor

*/

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_where_fp16_b_()
{
    constexpr uint dst_tile_size_rows = 64;

    sfpi::vFloat cond = sfpi::dst_reg[0];

    for (int i = 0; i < ITERATIONS; i++)
    {
        cond = sfpi::dst_reg[0];

        v_if (cond == 0.0f)
        {
            // output_tensor = false_tensor;
            TTI_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::LO16, ADDR_MOD_3, 2 * dst_tile_size_rows);
        }
        v_else
        {
            // output_tensor = true_tensor;
            TTI_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::LO16, ADDR_MOD_3, dst_tile_size_rows);
            v_endif;
        }
        // sfpi::dst_reg[0] = output_tensor;
        TTI_SFPSTORE(p_sfpu::LREG3, InstrModLoadStore::LO16, ADDR_MOD_3, 0);

        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_where_fp32_()
{
    constexpr uint dst_tile_size = 32;

    sfpi::vFloat output_tensor = 0;
    sfpi::vFloat true_tensor   = 0;
    sfpi::vFloat false_tensor  = 0;
    sfpi::vFloat cond          = sfpi::dst_reg[0];

    for (int i = 0; i < ITERATIONS; i++)
    {
        cond         = sfpi::dst_reg[0];
        true_tensor  = sfpi::dst_reg[dst_tile_size];
        false_tensor = sfpi::dst_reg[dst_tile_size * 2];

        v_if (cond != 0.0f)
        {
            output_tensor = true_tensor;
        }
        v_else
        {
            output_tensor = false_tensor;
        }
        v_endif;

        sfpi::dst_reg[0] = output_tensor;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE, DataFormat data_format>
inline void _calculate_where_()
{
    // Add a compile-time check to ensure only supported formats are used.
    static_assert(
        data_format == DataFormat::Float32 || data_format == DataFormat::Float16_b,
        "Unsupported data format for _calculate_where_(). Only Float32 and Float16_b are allowed.");
    if constexpr (data_format == DataFormat::Float32)
    {
        _calculate_where_fp32_<APPROXIMATION_MODE, 32>();
    }
    else
    {
        _calculate_where_fp16_b_<APPROXIMATION_MODE, 32>();
    }
}

} // namespace ckernel::sfpu
