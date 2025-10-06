// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_addrmod.h"
#include "ckernel_instr_params.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{
/**
 * @brief Calculates column-wise sum and/or average of a 32x32 tile, placing output values into the first row.
 *        Uses an optimized approach that processes vertically aligned face pairs (0+2, 1+3) to minimize
 *        load/store operations and eliminate intermediate storage requirements.
 * @tparam AVERAGE If 0, computes column sums. If 32, computes column averages by dividing sums by 32.
 *                 Must be either 0 or 32 - other values are not supported for 32x32 tiles.
 */
template <uint AVERAGE, bool is_fp32_dest_acc_en>
inline void _calculate_sum_tile_columns_(const std::uint32_t dst_reg_format)
{
    // Each face is 16 rows, tile has 4 faces arranged as:
    // Face 0 (rows 0-15)  | Face 1 (rows 0-15)
    // Face 2 (rows 16-31) | Face 3 (rows 16-31)
    constexpr uint face_offset = 16;
    uint replay_length         = 6;

    bool use_float_arithmetic = false;

    uint8_t instr_mod_index;
    if (dst_reg_format == (uint32_t)DataFormat::Int32)
    {
        instr_mod_index = InstrModLoadStore::INT32;
    }
    else if (dst_reg_format == (uint32_t)DataFormat::UInt16)
    {
        instr_mod_index = InstrModLoadStore::LO16;
    }
    else if (dst_reg_format == (uint32_t)DataFormat::UInt32)
    {
        instr_mod_index = InstrModLoadStore::INT32_2S_COMP;
    }
    else
    {
        use_float_arithmetic = true;
        instr_mod_index      = InstrModLoadStore::FP32; // is_fp32_dest_acc_en ? InstrModLoadStore::FP32 : InstrModLoadStore::FP16B;
        replay_length        = 12; // Float arithmetic needs 12 instructions since SFPADD takes 2 cycles, defined in second replay instantiation
    }

    // Optimized approach: Process 4 iterations to handle all column combinations
    // This reduces operations by processing complementary face pairs simultaneously, less load/store operations
    for (int i = 0; i < 4; i++)
    {
        // Iteration mapping - Process vertically aligned faces (0+2, 1+3) to optimize column operations:
        // i=0: even columns, left half  (faces 0 + 2, columns 0,2,4,6,8,10,12,14)
        // i=1: odd columns,  left half  (faces 0 + 2, columns 1,3,5,7,9,11,13,15)
        // i=2: even columns, right half (faces 1 + 3, columns 16,18,20,22,24,26,28,30)
        // i=3: odd columns,  right half (faces 1 + 3, columns 17,19,21,23,25,27,29,31)
        //
        // Key optimization: Process faces 0+2 and 1+3 (vertically aligned) instead of 0+1 and 2+3
        // This allows processing all 32 rows of a column at once (16 from upper face + 16 from lower face)
        // Reduces load/store operations by accumulating all rows into one LREG per column group
        // Final result stored in top row of upper face (first row in dest) - no intermediate storage needed

        const bool is_right_half  = i > 1;        // true for faces 1&3 (right half of tile), false for faces 0&2 (left half of tile)
        const bool is_odd_columns = (i % 2) == 1; // true for odd columns, false for even columns

        // Calculate face addresses
        const uint upper_face_addr = is_right_half ? face_offset : 0;     // Face 0 (addr 0) or Face 1 (addr 16)
        const uint lower_face_addr = upper_face_addr + (face_offset * 2); // Face 2 (addr 32) or Face 3 (addr 48)

        // Calculate column offset (even columns start at 0, odd columns start at 2)
        const uint column_offset = is_odd_columns ? 2 : 0;

        // Load upper face data (Face 0 or Face 1)
        TT_SFPLOAD(p_sfpu::LREG0, instr_mod_index, ADDR_MOD_3, upper_face_addr + column_offset);      // rows 0-3
        TT_SFPLOAD(p_sfpu::LREG1, instr_mod_index, ADDR_MOD_3, upper_face_addr + column_offset + 4);  // rows 4-7
        TT_SFPLOAD(p_sfpu::LREG2, instr_mod_index, ADDR_MOD_3, upper_face_addr + column_offset + 8);  // rows 8-11
        TT_SFPLOAD(p_sfpu::LREG3, instr_mod_index, ADDR_MOD_3, upper_face_addr + column_offset + 12); // rows 12-15

        // Load lower face data (Face 2 or Face 3)
        TT_SFPLOAD(p_sfpu::LREG4, instr_mod_index, ADDR_MOD_3, lower_face_addr + column_offset);      // rows 0-3
        TT_SFPLOAD(p_sfpu::LREG5, instr_mod_index, ADDR_MOD_3, lower_face_addr + column_offset + 4);  // rows 4-7
        TT_SFPLOAD(p_sfpu::LREG6, instr_mod_index, ADDR_MOD_3, lower_face_addr + column_offset + 8);  // rows 8-11
        TT_SFPLOAD(p_sfpu::LREG7, instr_mod_index, ADDR_MOD_3, lower_face_addr + column_offset + 12); // rows 12-15

        // Process column sums for both faces using transpose and replay buffer
        TT_SFPTRANSP(0, 0, 0, 0);       // Transpose: LREG0-3 → lanes 0-3, LREG4-7 → lanes 0-3 (overlapping)
        lltt::replay(0, replay_length); // Column-wise sum within each lreg after transpose
        TT_SFPTRANSP(0, 0, 0, 0);       // Transpose back to original register layout
        lltt::replay(0, replay_length); // Sum column sums within each face after transpose

        // Combine the column sums from upper and lower faces
        if (use_float_arithmetic)
        {
            TT_SFPADD(
                p_sfpu::LREG0, p_sfpu::LCONST_1, p_sfpu::LREG4, p_sfpu::LREG0, 0); // LREG0 = (LREG0 * 1) + LREG4 = upper_face_sums + lower_face_sums (float)
            TTI_SFPNOP;
        }
        else
        {
            TT_SFPIADD(0, p_sfpu::LREG4, p_sfpu::LREG0, 4); // LREG0 = upper_face_sums + lower_face_sums (integer)
        }

        if constexpr (AVERAGE > 0)
        {
            if (use_float_arithmetic)
            {
                // For a 32x32 tile, each column sum represents the sum of exactly 32 values (one per row)
                // Load 1/32 constant (0.03125) into LREG1 for float division
                TT_SFPLOADI(p_sfpu::LREG1, 8, 0x3D00);  // Load 0.03125 as FP16B high part
                TT_SFPLOADI(p_sfpu::LREG1, 10, 0x0000); // Load 0.03125 as FP16B low part
                // Multiply by 1/32 (divide by 32) - works for both float and integer formats
                TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LCONST_0, p_sfpu::LREG0, 0);
                TTI_NOP; // Required after SFPMUL due to 2-cycle latency
            }
            else
            {
                // For integer formats, shift right by 5 bits (divide by 32) using immediate value
                // The -5 & 0xfff ensures the immediate value is properly formatted for the instruction
                // Note: Wormhole B0 only supports logical shift, not arithmetic shift like Blackhole
                TTI_SFPSHFT(-5 & 0xfff, p_sfpu::LREG0, p_sfpu::LREG0, 0b01);
            }
        }

        // Store the final combined column sums
        TT_SFPSTORE(p_sfpu::LREG0, instr_mod_index, ADDR_MOD_3, upper_face_addr + column_offset);
    }

    // After this loop, the column sums are stored at first row in dest reg:
    // Address 0:  even columns, left half  (columns 0,2,4,6,8,10,12,14)
    // Address 2:  odd columns,  left half  (columns 1,3,5,7,9,11,13,15)
    // Address 16: even columns, right half (columns 16,18,20,22,24,26,28,30)
    // Address 18: odd columns,  right half (columns 17,19,21,23,25,27,29,31)
}

inline void _init_sum_tile_columns_(const uint32_t dst_reg_format)
{
    // Initialize SFPU configuration register
    _init_sfpu_config_reg();

    // Program optimized replay buffer for column summation
    // This replay buffer is called twice per iteration:
    // 1st call: After first transpose - operates on transposed data where LREG0-3 and LREG4-7 both map to lanes 0→3
    // 2nd call: After second transpose - operates on data transposed back to original layout, the sum of 4 rows columns stored in lregs, need to sum lregs for
    // each face to get the final column sums
    if (dst_reg_format == (uint32_t)DataFormat::Float32)
    {
        // Program replay buffer
        lltt::record(0, 12);

        // Column summation for upper face data (originally LREG0-3)
        // After transpose: LREG0→lane0, LREG1→lane1, LREG2→lane2, LREG3→lane3 across lregs 0-3
        TTI_SFPADD(p_sfpu::LREG2, p_sfpu::LCONST_1, p_sfpu::LREG3, p_sfpu::LREG2, 0); // LREG2 = (LREG2 * 1) + LREG3 = LREG2 + LREG3 (float)
        TTI_SFPNOP;
        TTI_SFPADD(p_sfpu::LREG1, p_sfpu::LCONST_1, p_sfpu::LREG2, p_sfpu::LREG1, 0); // LREG1 = (LREG1 * 1) + LREG2 = LREG1 + LREG2 (float)
        TTI_SFPNOP;
        TTI_SFPADD(
            p_sfpu::LREG0,
            p_sfpu::LCONST_1,
            p_sfpu::LREG1,
            p_sfpu::LREG0,
            0); // LREG0 = (LREG0 * 1) + LREG1 = LREG0 + LREG1 (upper face column sums, float)
        TTI_SFPNOP;

        // Column summation for lower face data (originally LREG4-7)
        // After transpose: LREG4→lane0, LREG5→lane1, LREG6→lane2, LREG7→lane3 across lregs 4-7
        TTI_SFPADD(p_sfpu::LREG6, p_sfpu::LCONST_1, p_sfpu::LREG7, p_sfpu::LREG6, 0); // LREG6 = (LREG6 * 1) + LREG7 = LREG6 + LREG7 (float)
        TTI_SFPNOP;
        TTI_SFPADD(p_sfpu::LREG5, p_sfpu::LCONST_1, p_sfpu::LREG6, p_sfpu::LREG5, 0); // LREG5 = (LREG5 * 1) + LREG6 = LREG5 + LREG6 (float)
        TTI_SFPNOP;
        TTI_SFPADD(
            p_sfpu::LREG4,
            p_sfpu::LCONST_1,
            p_sfpu::LREG5,
            p_sfpu::LREG4,
            0); // LREG4 = (LREG4 * 1) + LREG5 = LREG4 + LREG5 (lower face column sums, float)
        TTI_SFPNOP;
    }
    else
    {
        // Program replay buffer
        lltt::record(0, 6);

        // Column summation for upper face data (originally LREG0-3)
        // After transpose: LREG0→lane0, LREG1→lane1, LREG2→lane2, LREG3→lane3 across lregs 0-3
        TTI_SFPIADD(0, p_sfpu::LREG3, p_sfpu::LREG2, 4); // LREG2 = LREG2 + LREG3
        TTI_SFPIADD(0, p_sfpu::LREG2, p_sfpu::LREG1, 4); // LREG1 = LREG1 + LREG2
        TTI_SFPIADD(0, p_sfpu::LREG1, p_sfpu::LREG0, 4); // LREG0 = LREG0 + LREG1 (upper face column sums)

        // Column summation for lower face data (originally LREG4-7)
        // After transpose: LREG4→lane0, LREG5→lane1, LREG6→lane2, LREG7→lane3 across lregs 4-7
        TTI_SFPIADD(0, p_sfpu::LREG7, p_sfpu::LREG6, 4); // LREG6 = LREG6 + LREG7
        TTI_SFPIADD(0, p_sfpu::LREG6, p_sfpu::LREG5, 4); // LREG5 = LREG5 + LREG6
        TTI_SFPIADD(0, p_sfpu::LREG5, p_sfpu::LREG4, 4); // LREG4 = LREG4 + LREG5 (lower face column sums)

        // The transpose operation allows both upper and lower face data to be processed
        // simultaneously in the same lane space, then separated back to their original registers
    }
}

} // namespace sfpu
} // namespace ckernel
