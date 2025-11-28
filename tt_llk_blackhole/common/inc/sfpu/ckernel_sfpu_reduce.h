// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_addrmod.h"
#include "ckernel_instr_params.h"
#include "lltt.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

// ============================================================================
// Constants for 32x32 Tile Layout
// ============================================================================
// Each face is 16 rows, tile has 4 faces arranged as:
// Face 0 (rows 0-15)  | Face 1 (rows 0-15)
// Face 2 (rows 16-31) | Face 3 (rows 16-31)

constexpr uint NUM_FACES                   = 4;
constexpr uint UPPER_FACE_ADDRS[NUM_FACES] = {0, 0, 16, 16};   // Face 0, 0, 1, 1
constexpr uint LOWER_FACE_ADDRS[NUM_FACES] = {32, 32, 48, 48}; // Face 2, 2, 3, 3
constexpr uint COLUMN_OFFSETS[NUM_FACES]   = {0, 2, 0, 2};     // even, odd, even, odd

constexpr uint ROWS_PER_LOAD = 4;

// Constants for averaging (division by 32)
constexpr uint AVG_SHIFT_AMOUNT = 5;     // 2^5 = 32
constexpr uint AVG_SHIFT_MASK   = 0xfff; // Mask for shift instruction encoding

// FP16B representation of 1/32 = 0.03125
constexpr uint FP16B_ONE_OVER_32_HIGH = 0x3D00;
constexpr uint FP16B_ONE_OVER_32_LOW  = 0x0000;

// Constants for MAX reduction
constexpr uint32_t ROWS_PER_TILE = 64;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Generic reduction operation for a 32x32 tile, placing output values into the first row.
 *        Currently able to calculate column-wise sum and/or average of a 32x32 tile, placing output values into the first row.
 *        Uses an optimized approach that processes vertically aligned face pairs (0+2, 1+3) to minimize
 *        load/store operations and eliminate intermediate storage requirements.
 *        For integer formats with averaging, handles negative numbers properly using condition codes
 *        since Wormhole B0 only supports logical shift (not arithmetic shift).
 * @tparam pool_type The pool/reduction pool_type (SUM, AVG, MAX). Currently only SUM and AVG are supported.
 * @tparam reduce_dim The reduction dimension (REDUCE_ROW, REDUCE_COL, REDUCE_SCALAR). Currently only REDUCE_COL is supported.
 * @tparam INSTRUCTION_MODE The instruction modifier that determines the data type and precision:
 *                          - InstrModLoadStore::INT32: Signed 32-bit integers
 *                          - InstrModLoadStore::INT32_2S_COMP: 32-bit integers in 2's complement (no effect in Wormhole B0)
 *                          - InstrModLoadStore::LO16: Unsigned 16-bit integers (lower 16 bits)
 */
template <InstrModLoadStore INSTRUCTION_MODE>
inline void load_face_data(uint upper_face_addr, uint lower_face_addr, uint column_offset)
{
    // Load upper face data (Face 0 or Face 1) into LREG0-3
    TT_SFPLOAD(p_sfpu::LREG0, INSTRUCTION_MODE, ADDR_MOD_7, upper_face_addr + column_offset);                     // rows 0-3
    TT_SFPLOAD(p_sfpu::LREG1, INSTRUCTION_MODE, ADDR_MOD_7, upper_face_addr + column_offset + ROWS_PER_LOAD);     // rows 4-7
    TT_SFPLOAD(p_sfpu::LREG2, INSTRUCTION_MODE, ADDR_MOD_7, upper_face_addr + column_offset + 2 * ROWS_PER_LOAD); // rows 8-11
    TT_SFPLOAD(p_sfpu::LREG3, INSTRUCTION_MODE, ADDR_MOD_7, upper_face_addr + column_offset + 3 * ROWS_PER_LOAD); // rows 12-15

    // Pre-calculated face addresses and column offsets for each iteration
    // Each face is 16 rows, tile has 4 faces arranged as:
    // Face 0 (rows 0-15)  | Face 1 (rows 0-15)
    // Face 2 (rows 16-31) | Face 3 (rows 16-31)
    //
    // Iteration mapping - Process vertically aligned faces (0+2, 1+3) to optimize column operations:
    // i=0: even columns, left half  (faces 0 + 2, columns 0,2,4,6,8,10,12,14)
    // i=1: odd columns,  left half  (faces 0 + 2, columns 1,3,5,7,9,11,13,15)
    // i=2: even columns, right half (faces 1 + 3, columns 16,18,20,22,24,26,28,30)
    // i=3: odd columns,  right half (faces 1 + 3, columns 17,19,21,23,25,27,29,31)
    constexpr uint UPPER_FACE_ADDRS[4] = {0, 0, 16, 16};   // Face 0, 0, 1, 1
    constexpr uint LOWER_FACE_ADDRS[4] = {32, 32, 48, 48}; // Face 2, 2, 3, 3
    constexpr uint COLUMN_OFFSETS[4]   = {0, 2, 0, 2};     // even, odd, even, odd

    // Optimized approach: Process 4 iterations to handle all column combinations
    // This reduces operations by processing complementary face pairs simultaneously, less load/store operations
    for (int i = 0; i < 4; i++)
    {
        // Key optimization: Process faces 0+2 and 1+3 (vertically aligned) instead of 0+1 and 2+3
        // This allows processing all 32 rows of a column at once (16 from upper face + 16 from lower face)
        // Reduces load/store operations by accumulating all rows into one LREG per column group
        // Final result stored in top row of upper face (first row in dest) - no intermediate storage needed

        const uint UPPER_FACE_ADDR = UPPER_FACE_ADDRS[i];
        const uint LOWER_FACE_ADDR = LOWER_FACE_ADDRS[i];
        const uint COLUMN_OFFSET   = COLUMN_OFFSETS[i];

        // Load upper face data (Face 0 or Face 1)
        TT_SFPLOAD(p_sfpu::LREG0, INSTRUCTION_MODE, ADDR_MOD_3, UPPER_FACE_ADDR + COLUMN_OFFSET);      // rows 0-3
        TT_SFPLOAD(p_sfpu::LREG1, INSTRUCTION_MODE, ADDR_MOD_3, UPPER_FACE_ADDR + COLUMN_OFFSET + 4);  // rows 4-7
        TT_SFPLOAD(p_sfpu::LREG2, INSTRUCTION_MODE, ADDR_MOD_3, UPPER_FACE_ADDR + COLUMN_OFFSET + 8);  // rows 8-11
        TT_SFPLOAD(p_sfpu::LREG3, INSTRUCTION_MODE, ADDR_MOD_3, UPPER_FACE_ADDR + COLUMN_OFFSET + 12); // rows 12-15

        // Load lower face data (Face 2 or Face 3)
        TT_SFPLOAD(p_sfpu::LREG4, INSTRUCTION_MODE, ADDR_MOD_3, LOWER_FACE_ADDR + COLUMN_OFFSET);      // rows 0-3
        TT_SFPLOAD(p_sfpu::LREG5, INSTRUCTION_MODE, ADDR_MOD_3, LOWER_FACE_ADDR + COLUMN_OFFSET + 4);  // rows 4-7
        TT_SFPLOAD(p_sfpu::LREG6, INSTRUCTION_MODE, ADDR_MOD_3, LOWER_FACE_ADDR + COLUMN_OFFSET + 8);  // rows 8-11
        TT_SFPLOAD(p_sfpu::LREG7, INSTRUCTION_MODE, ADDR_MOD_3, LOWER_FACE_ADDR + COLUMN_OFFSET + 12); // rows 12-15

        // Process column sums for both faces using transpose and replay buffer
        TT_SFPTRANSP(0, 0, 0, 0); // Transpose: LREG0-3 → lanes 0-3, LREG4-7 → lanes 0-3 (overlapping)
        lltt::replay(0, 6);       // Column-wise sum within each lreg after transpose
        TT_SFPTRANSP(0, 0, 0, 0); // Transpose back to original register layout
        lltt::replay(0, 6);       // Sum column sums within each face after transpose

        TT_SFPIADD(0, p_sfpu::LREG4, p_sfpu::LREG0, 4); // LREG0 = upper_face_sums + lower_face_sums (integer)

        if constexpr (pool_type == AVG)
        {
            // For integer formats, we need to handle negative numbers properly for division by 32
            // Since Wormhole B0 only supports logical shift (not arithmetic), we need to:
            // 1. Check if the number is negative using condition codes (only for signed formats)
            // 2. If negative, negate it, shift right by 5 bits, then negate back
            // 3. If positive, just shift right by 5 bits

            if constexpr (INSTRUCTION_MODE == InstrModLoadStore::INT32)
            {
                // For signed Int32 format, use absolute value approach for proper division by 32
                // Save original value for sign check
                TTI_SFPMOV(0, p_sfpu::LREG0, p_sfpu::LREG1, 0);

                // Get absolute value of LREG0
                TTI_SFPABS(0, p_sfpu::LREG0, p_sfpu::LREG0, 0);

                // Perform logical right shift by 5 bits (divide by 32)
                TTI_SFPSHFT(-5 & 0xfff, p_sfpu::LREG0, p_sfpu::LREG0, 0b01);

                // Restore sign if original value was negative
                // Check if original value was negative (sign bit set)
                TTI_SFPSETCC(0, p_sfpu::LREG1, 0, 4);               // Set condition code if original sign bit is 0 (positive)
                TTI_SFPCOMPC(0, 0, 0, 0);                           // Invert condition code (now true if original was negative)
                TTI_SFPIADD(0, p_sfpu::LCONST_0, p_sfpu::LREG0, 6); // Negate LREG0 if condition is true

                // Clear condition codes
                TTI_SFPENCC(0, 0, 0, 0);
            }
            else
            {
                // For unsigned formats (UInt32), just use logical shift directly
                // since they can't be negative
                TTI_SFPSHFT(-5 & 0xfff, p_sfpu::LREG0, p_sfpu::LREG0, 0b01);
            }
        }

        // Store the final combined column sums
        TT_SFPSTORE(p_sfpu::LREG0, INSTRUCTION_MODE, ADDR_MOD_3, UPPER_FACE_ADDR + COLUMN_OFFSET);
    }

    // After this loop, the column sums are stored at first row in dest reg:
    // Address 0:  even columns, left half  (columns 0,2,4,6,8,10,12,14)
    // Address 2:  odd columns,  left half  (columns 1,3,5,7,9,11,13,15)
    // Address 16: even columns, right half (columns 16,18,20,22,24,26,28,30)
    // Address 18: odd columns,  right half (columns 17,19,21,23,25,27,29,31)
}

/**
 * @brief Perform column-wise summation using transpose and replay buffer
 *
 * Process column sums for both upper and lower face using transpose and replay buffer.
 * @tparam replay_buffer_length The replay buffer index to use (6 for both int and float on Blackhole)
 */
template <uint32_t replay_buffer_length>
inline void sum_columns()
{
    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose: LREG0-3 → lanes 0-3, LREG4-7 → lanes 0-3 (overlapping)
    lltt::replay(0, replay_buffer_length); // Column-wise sum within each lreg after transpose
    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose back to original register layout
    lltt::replay(0, replay_buffer_length); // Sum column sums within each face after transpose
}

/**
 * @brief Perform integer averaging with proper handling of negative numbers
 * @tparam INSTRUCTION_MODE The instruction mode (determines signed vs unsigned)
 *
 * For integer formats, we need to handle negative numbers properly for division by 32.
 * Since Blackhole only supports logical shift (not arithmetic), we need to:
 * 1. Check if the number is negative using condition codes (only for signed formats)
 * 2. If negative, negate it, shift right by 5 bits, then negate back
 * 3. If positive, just shift right by 5 bits
 */
template <InstrModLoadStore INSTRUCTION_MODE>
inline void perform_int_average()
{
    if constexpr (INSTRUCTION_MODE == InstrModLoadStore::INT32)
    {
        // For signed Int32 format, use absolute value approach for proper division by 32
        TTI_SFPMOV(0, p_sfpu::LREG0, p_sfpu::LREG1, 0);                                      // Save original value for sign check
        TTI_SFPABS(0, p_sfpu::LREG0, p_sfpu::LREG0, 0);                                      // Get absolute value of LREG0
        TTI_SFPSHFT(-AVG_SHIFT_AMOUNT & AVG_SHIFT_MASK, p_sfpu::LREG0, p_sfpu::LREG0, 0b01); // Perform logical right shift by 5 bits (divide by 32)

        const uint UPPER_FACE_ADDR = UPPER_FACE_ADDRS[i];
        const uint LOWER_FACE_ADDR = LOWER_FACE_ADDRS[i];
        const uint COLUMN_OFFSET   = COLUMN_OFFSETS[i];

        // Load upper face data (Face 0 or Face 1)
        TT_SFPLOAD(p_sfpu::LREG0, INSTRUCTION_MODE, ADDR_MOD_3, UPPER_FACE_ADDR + COLUMN_OFFSET);      // rows 0-3
        TT_SFPLOAD(p_sfpu::LREG1, INSTRUCTION_MODE, ADDR_MOD_3, UPPER_FACE_ADDR + COLUMN_OFFSET + 4);  // rows 4-7
        TT_SFPLOAD(p_sfpu::LREG2, INSTRUCTION_MODE, ADDR_MOD_3, UPPER_FACE_ADDR + COLUMN_OFFSET + 8);  // rows 8-11
        TT_SFPLOAD(p_sfpu::LREG3, INSTRUCTION_MODE, ADDR_MOD_3, UPPER_FACE_ADDR + COLUMN_OFFSET + 12); // rows 12-15

        // Load lower face data (Face 2 or Face 3)
        TT_SFPLOAD(p_sfpu::LREG4, INSTRUCTION_MODE, ADDR_MOD_3, LOWER_FACE_ADDR + COLUMN_OFFSET);      // rows 0-3
        TT_SFPLOAD(p_sfpu::LREG5, INSTRUCTION_MODE, ADDR_MOD_3, LOWER_FACE_ADDR + COLUMN_OFFSET + 4);  // rows 4-7
        TT_SFPLOAD(p_sfpu::LREG6, INSTRUCTION_MODE, ADDR_MOD_3, LOWER_FACE_ADDR + COLUMN_OFFSET + 8);  // rows 8-11
        TT_SFPLOAD(p_sfpu::LREG7, INSTRUCTION_MODE, ADDR_MOD_3, LOWER_FACE_ADDR + COLUMN_OFFSET + 12); // rows 12-15

        // Process column sums for both faces using transpose and replay buffer
        TT_SFPTRANSP(0, 0, 0, 0); // Transpose: LREG0-3 → lanes 0-3, LREG4-7 → lanes 0-3 (overlapping)
        lltt::replay(0, 12);      // Column-wise sum within each lreg after transpose
        TT_SFPTRANSP(0, 0, 0, 0); // Transpose back to original register layout
        lltt::replay(0, 12);      // Sum column sums within each face after transpose

        // Combine the column sums from upper and lower faces
        TT_SFPADD(p_sfpu::LREG0, p_sfpu::LCONST_1, p_sfpu::LREG4, p_sfpu::LREG0, 0); // LREG0 = (LREG0 * 1) + LREG4 = upper_face_sums + lower_face_sums (float)
        TTI_SFPNOP;                                                                  // Required for Wormhole

        if constexpr (pool_type == AVG)
        {
            // For a 32x32 tile, each column sum represents the sum of exactly 32 values (one per row)
            // Load 1/32 constant (0.03125) into LREG1 for float division
            TT_SFPLOADI(p_sfpu::LREG1, 8, 0x3D00);  // Load 0.03125 as FP16B high part
            TT_SFPLOADI(p_sfpu::LREG1, 10, 0x0000); // Load 0.03125 as FP16B low part
            // Multiply by 1/32 (divide by 32) - works for both float and integer formats
            TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LCONST_0, p_sfpu::LREG0, 0);
            TTI_NOP; // Required after SFPMUL due to 2-cycle latency
        }
        // Store the final combined column sums
        TT_SFPSTORE(p_sfpu::LREG0, INSTRUCTION_MODE, ADDR_MOD_3, UPPER_FACE_ADDR + COLUMN_OFFSET);
    }
    else
    {
        // For unsigned formats (UInt32), just use logical shift directly since they can't be negative
        TTI_SFPSHFT(-AVG_SHIFT_AMOUNT & AVG_SHIFT_MASK, p_sfpu::LREG0, p_sfpu::LREG0, 0b01);
    }
}

/**
 * @brief Perform floating-point averaging (multiply by 1/32)
 *
 * For a 32x32 tile, each column sum represents the sum of exactly 32 values (one per row).
 * This function divides by 32 by multiplying by the constant 1/32 (0.03125).
 */
inline void perform_float_average()
{
    // Load 1/32 constant (0.03125) into LREG1 for float division
    TTI_SFPLOADI(p_sfpu::LREG1, 8, FP16B_ONE_OVER_32_HIGH); // Load 0.03125 as FP16B high part
    TTI_SFPLOADI(p_sfpu::LREG1, 10, FP16B_ONE_OVER_32_LOW); // Load 0.03125 as FP16B low part

    // Multiply by 1/32 (divide by 32) - works for both float and integer formats
    TTI_SFPMUL(p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LCONST_0, p_sfpu::LREG0, 0);
}

/**
 * @brief Runtime validation helper for supported data formats for reduce sfpu kernel
 */
constexpr bool is_supported_reduce_format(DataFormat format)
{
    return format == DataFormat::Int32 || format == DataFormat::UInt32 || format == DataFormat::Float32 || format == DataFormat::Float16_b ||
           format == DataFormat::UInt16;
}

/**
 * @brief Column-wise sum/average reduction kernel for SFPU reduce SUM and AVG operations.
 *        Computes the sum or average of each column, placing the 32 output values into the first row
 *        of the output tile (row 0 of faces 0 and 1).
 *
 *        Uses a 4-iteration approach that processes vertically aligned face pairs (0+2, 1+3) to optimize
 *        column operations and minimize load/store operations. Each iteration handles 8 columns using
 *        transpose operations and replay buffers for tree reduction.
 *
 *        For AVG mode: Integer formats use arithmetic shift with condition codes to handle negative numbers;
 *        float formats multiply by 1/32 constant.
 *
 * @tparam pool_type The reduction operation, currently supported: (SUM, AVG)
 * @tparam reduce_dim The reduction dimension (currently only REDUCE_COL is supported)
 * @tparam INSTRUCTION_MODE The instruction mode for integer and float formats: INT32, INT32_2S_COMP, LO16, FP32, FP16B
 */
template <PoolType pool_type, ReduceDim reduce_dim, InstrModLoadStore INSTRUCTION_MODE>
inline void calculate_reduce_sum_avg()
{
    static_assert(reduce_dim == REDUCE_COL, "Only column reduction (REDUCE_COL) is currently supported");
    static_assert(is_supported_reduce_format(format), "Unsupported data format. Supported formats: Int32, UInt32, Float32");

    // Determine if integer or float mode at compile time
    constexpr bool is_integer_mode =
        (INSTRUCTION_MODE == InstrModLoadStore::INT32 || INSTRUCTION_MODE == InstrModLoadStore::INT32_2S_COMP || INSTRUCTION_MODE == InstrModLoadStore::LO16);
    constexpr bool is_float_mode = (INSTRUCTION_MODE == InstrModLoadStore::FP32 || INSTRUCTION_MODE == InstrModLoadStore::FP16B);

    static_assert(is_integer_mode || is_float_mode, "INSTRUCTION_MODE must be one of: INT32, INT32_2S_COMP, LO16, FP32, FP16B");

    // Optimized approach: Process 4 iterations to handle all column combinations
    // This reduces operations by processing complementary face pairs simultaneously, less load/store operations
    for (uint i = 0; i < NUM_FACES; i++)
    {
        // _calculate_reduce_max_col_<pool_type, reduce_dim, format>(block_rt_dim);
    }
    else
    {
        if constexpr (format == DataFormat::Int32)
        {
            calculate_reduce_int<pool_type, reduce_dim, InstrModLoadStore::INT32>();
        }
        else if constexpr (format == DataFormat::UInt32)
        {
            calculate_reduce_int<pool_type, reduce_dim, InstrModLoadStore::INT32_2S_COMP>();
        }
        else if constexpr (format == DataFormat::Float32)
        {
            calculate_reduce_float<pool_type, reduce_dim, InstrModLoadStore::FP32>();
        }
    }
}

/**
 * @brief Unified initialization wrapper for SFPU reduce kernel.
 *        Automatically chooses between integer and floating-point initialization based on the data format.
 * @tparam format The data format (DataFormat enum value) that determines which initialization to use:
 *                - Supported integer formats: Int32, UInt32 (uses integer initialization)
 *                - Supported floating-point formats: Float32 (uses floating-point initialization)
 */
template <PoolType pool_type, DataFormat format>
inline void _init_reduce_()
{
    static_assert(is_supported_reduce_format(format), "Unsupported data format. Supported formats: Int32, UInt32, Float32, Float16_b");

        const uint upper_face_addr = UPPER_FACE_ADDRS[i];
        const uint lower_face_addr = LOWER_FACE_ADDRS[i];
        const uint column_offset   = COLUMN_OFFSETS[i];

    // Determine if we're working with integer or float based on DataFormat
    constexpr bool is_integer_mode = (format == DataFormat::Int32 || format == DataFormat::UInt32);
    constexpr bool is_float_mode   = (format == DataFormat::Float32 || format == DataFormat::Float16_b);

    static_assert(is_integer_mode || is_float_mode, "DataFormat must be one of: Int32, UInt32 (integer) or Float32 (float)");

    // Program optimized replay buffer for column summation
    // This replay buffer is called twice per iteration:
    // 1st call: After first transpose - operates on transposed data where LREG0-3 and LREG4-7 both map to lanes 0→3
    // 2nd call: After second transpose - operates on data transposed back to original layout, the sum of 4 rows columns stored in lregs, need to sum lregs for
    // each face to get the final column sums

    if constexpr (pool_type == PoolType::MAX)
    {
        // _init_reduce_max_col_<format>();
    }
    else
    {
        if constexpr (is_integer_mode)
        {
            sum_columns<6>();                                // Integer replay buffer
            TTI_SFPIADD(0, p_sfpu::LREG4, p_sfpu::LREG0, 4); // LREG0 = upper_face_sums + lower_face_sums (int)
        }
        else
        {
            // Program replay buffer for float operations (12 instructions)
            lltt::record(0, 12);

            // Column summation for upper face data (originally LREG0-3) - float version
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

            // Column summation for lower face data (originally LREG4-7) - float version
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

        // Perform averaging if requested (different for int vs float)
        if constexpr (pool_type == AVG)
        {
            if constexpr (is_integer_mode)
            {
                perform_int_average<INSTRUCTION_MODE>();
            }
            else
            {
                perform_float_average();
            }
        }

        // Store the final combined column sums
        TTI_SFPSTORE(p_sfpu::LREG0, INSTRUCTION_MODE, ADDR_MOD_7, upper_face_addr + column_offset);
    }

    // After this loop, the column sums are stored at first row in dest reg:
    // Address 0:  even columns, left half  (columns 0,2,4,6,8,10,12,14)
    // Address 2:  odd columns,  left half  (columns 1,3,5,7,9,11,13,15)
    // Address 16: even columns, right half (columns 16,18,20,22,24,26,28,30)
    // Address 18: odd columns,  right half (columns 17,19,21,23,25,27,29,31)
}

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Unified reduction init kernel wrapper for SFPU reduce kernel.
 *        Determines the instruction mode from format, then dispatches to the appropriate init kernel.
 * @tparam pool_type The reduction operation, currently supported: (SUM, AVG, MAX)
 * @tparam format The data format, currently supported: (Int32, UInt32, UInt16, Float32, Float16_b)
 * @param block_ct_dim Block dimension (used for MAX reduction to specify number of columns, default is 1 for single tile)
 */
template <PoolType pool_type, DataFormat format>
inline void _init_reduce_(uint32_t block_ct_dim = 1)
{
    static_assert(is_supported_reduce_format(format), "Unsupported data format. Supported formats: Int32, UInt32, UInt16, Float32, Float16_b");

    // Determine InstrModLoadStore based on format in dst register
    constexpr InstrModLoadStore INSTRUCTION_MODE = get_instruction_mode<format>();

    // Dispatch to appropriate PoolType init
    if constexpr (pool_type == PoolType::MAX)
    {
        init_reduce_max<INSTRUCTION_MODE>(block_ct_dim);
    }
    else if constexpr (pool_type == PoolType::SUM || pool_type == PoolType::AVG)
    {
        init_reduce_sum_avg<INSTRUCTION_MODE>();
    }
    else
    {
        static_assert(
            pool_type == PoolType::SUM || pool_type == PoolType::AVG || pool_type == PoolType::MAX,
            "Unsupported pool_type. Currently supported: SUM, AVG, MAX");
    }
}

/**
 * @brief Unified reduction kernel wrapper for a 32x32 tile.
 *        Determines the instruction mode from format, then dispatches to the appropriate reduction kernel.
 * @tparam pool_type The reduction operation, currently supported: (SUM, AVG, MAX)
 * @tparam reduce_dim The reduction dimension (currently only REDUCE_COL is supported)
 * @tparam format The data format, currently supported: (Int32, UInt32, UInt16, Float32, Float16_b)
 * @param block_rt_dim Block dimension (used for MAX reduction to specify block height, default is 1 for single tile)
 */
template <PoolType pool_type, ReduceDim reduce_dim, DataFormat format>
inline void _calculate_reduce_(uint32_t block_rt_dim = 1)
{
    static_assert(reduce_dim == REDUCE_COL, "Only column reduction (REDUCE_COL) is currently supported");
    static_assert(is_supported_reduce_format(format), "Unsupported data format. Supported formats: Int32, UInt32, UInt16, Float32, Float16_b");

    // Determine InstrModLoadStore based on format in dst register
    constexpr InstrModLoadStore INSTRUCTION_MODE = get_instruction_mode<format>();

    // Dispatch to appropriate reduction kernel based on PoolType
    if constexpr (pool_type == PoolType::MAX)
    {
        calculate_reduce_max<pool_type, reduce_dim, INSTRUCTION_MODE>(block_rt_dim);
    }
    else if constexpr (pool_type == PoolType::SUM || pool_type == PoolType::AVG)
    {
        calculate_reduce_sum_avg<pool_type, reduce_dim, INSTRUCTION_MODE>();
    }
    else
    {
        static_assert(
            pool_type == PoolType::SUM || pool_type == PoolType::AVG || pool_type == PoolType::MAX,
            "Unsupported pool_type. Currently supported: SUM, AVG, MAX");
    }
}

} // namespace sfpu
} // namespace ckernel
