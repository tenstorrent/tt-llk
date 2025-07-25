// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_addrmod.h"
#include "ckernel_ops.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

/**
 * @brief Compute cumulative sum using SFPU with transpose-based algorithm
 *
 * @details Implements cumulative sum computation using a sophisticated algorithm that
 * leverages SFPU transpose operations and replay buffers. The function processes data
 * in structured chunks, applying transpose operations between load and store phases
 * to achieve the cumulative sum effect.
 *
 * **Algorithm Structure:**
 * The implementation processes data in organized sections:
 * - Face 0,1 (rows 0, 4, 8, 12) 
 * - Face 2,3 (rows 0, 4, 8, 12 with +32 offset)
 * 
 * Each section follows the pattern:
 * 1. **Load**: Read data from multiple addresses into LREG registers
 * 2. **Transpose**: Apply SFPTRANSP operation for data reorganization
 * 3. **Replay**: Execute replay buffer (contains cumulative sum logic)
 * 4. **Transpose**: Apply second transpose operation
 * 5. **Store**: Write results back to destination registers
 *
 * **Memory Access Pattern:**
 * ```
 * Section processing addresses:
 * Row 0:  [0, 2, 16, 18]     Row 4:  [4, 6, 20, 22]
 * Row 8:  [8, 10, 24, 26]   Row 12: [12, 14, 28, 30]
 * Face 2,3: Same pattern + 32 offset
 * ```
 *
 * **Transpose Operations:**
 * Uses SFPTRANSP instructions to reorganize data within LReg registers. From the
 * instruction documentation, SFPTRANSP performs matrix transpose operations on
 * columns of LReg data, enabling efficient cumulative computation across lanes.
 *
 * **Replay Buffer Integration:**
 * - `lltt::replay(0, 8)`: Execute replay buffer 0 for first 8 operations
 * - `lltt::replay(8, 8)`: Execute replay buffer starting at offset 8
 * 
 * The replay buffer contains pre-programmed cumulative addition sequence that
 * is applied after the first transpose operation.
 *
 * **Initialization Control:**
 * @param first If true, clears context registers (LREG 4-7) before processing.
 *              This suggests cumsum can be called multiple times with state carry-over.
 *
 * **Context Clearing (when first=true):**
 * ```assembly
 * TTI_SFPMOV(0, 9, 4, 0);  // Clear LREG4
 * TTI_SFPMOV(0, 9, 5, 0);  // Clear LREG5  
 * TTI_SFPMOV(0, 9, 6, 0);  // Clear LREG6
 * TTI_SFPMOV(0, 9, 7, 0);  // Clear LREG7
 * ```
 *
 * **Data Processing Flow Example (Row 0):**
 * ```assembly
 * TTI_SFPLOAD(0, 0, ADDR_MOD_7, 0);    // Load data[0] → LREG0
 * TTI_SFPLOAD(1, 0, ADDR_MOD_7, 2);    // Load data[2] → LREG1
 * TTI_SFPLOAD(2, 0, ADDR_MOD_7, 16);   // Load data[16] → LREG2
 * TTI_SFPLOAD(3, 0, ADDR_MOD_7, 18);   // Load data[18] → LREG3
 * 
 * TTI_SFPTRANSP(0, 0, 0, 0);           // Transpose loaded data
 * lltt::replay(0, 8);                  // Apply cumsum operations
 * TTI_SFPTRANSP(0, 0, 0, 0);           // Transpose back
 * 
 * TTI_SFPSTORE(0, 0, ADDR_MOD_7, 0);   // Store LREG0 → data[0]
 * TTI_SFPSTORE(1, 0, ADDR_MOD_7, 2);   // Store LREG1 → data[2]
 * TTI_SFPSTORE(2, 0, ADDR_MOD_7, 16);  // Store LREG2 → data[16]
 * TTI_SFPSTORE(3, 0, ADDR_MOD_7, 18);  // Store LREG3 → data[18]
 * ```
 *
 * **Hardware Context:**
 * - Uses SFPU load/store units for data movement
 * - Uses SFPU transpose unit for matrix operations
 * - Uses SFPU replay mechanism for efficient instruction sequences
 * - Processes data in 32-lane SIMD fashion across multiple LReg registers
 *
 * **Addressing Strategy:**
 * - ADDR_MOD_7: Optimized addressing mode for tile operations
 * - Structured offset pattern: 0,2,4,6,8,10,12,14 for even columns
 * - Face separation: +16 and +32 offsets for different data faces
 * - Register mapping: Uses LREG 0-7 in alternating 4-register groups
 *
 * @tparam APPROXIMATION_MODE Template parameter (marked unused in implementation)
 * @tparam ITERATIONS Template parameter (marked unused in implementation)
 *
 * @param first Controls initialization behavior:
 *              - true: Clear context registers before processing
 *              - false: Preserve context from previous operations
 *
 * @note Function processes data in-place, overwriting input with cumsum results
 * @note Requires replay buffer to be properly initialized via _cumsum_init_()
 * @note Template parameters are present but not used in current implementation
 * @note Complex addressing pattern suggests specific data layout requirements
 */
template <bool APPROXIMATION_MODE /*unused*/, int ITERATIONS /*unused*/>
inline void _calculate_cumsum_(const bool first)
{
    if (first)
    {
        // Clear context for F0
        TTI_SFPMOV(0, 9, 4, 0);  // Clear LREG4 (context register)
        TTI_SFPMOV(0, 9, 5, 0);  // Clear LREG5 (context register)
        TTI_SFPMOV(0, 9, 6, 0);  // Clear LREG6 (context register)
        TTI_SFPMOV(0, 9, 7, 0);  // Clear LREG7 (context register)
    }

    // F0,1 R0 - Process Face 0,1 Row 0 (addresses 0,2,16,18)
    TTI_SFPLOAD(0, 0, ADDR_MOD_7, 0);      // Load data[0] to LREG0
    TTI_SFPLOAD(1, 0, ADDR_MOD_7, 2);      // Load data[2] to LREG1
    TTI_SFPLOAD(2, 0, ADDR_MOD_7, 0 + 16); // Load data[16] to LREG2
    TTI_SFPLOAD(3, 0, ADDR_MOD_7, 2 + 16); // Load data[18] to LREG3

    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose operation for cumsum preparation
    lltt::replay(0, 8);                    // Execute cumsum replay buffer (operations 0-7)
    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose back to original orientation

    TTI_SFPSTORE(0, 0, ADDR_MOD_7, 0);      // Store LREG0 result to data[0]
    TTI_SFPSTORE(1, 0, ADDR_MOD_7, 2);      // Store LREG1 result to data[2]
    TTI_SFPSTORE(2, 0, ADDR_MOD_7, 0 + 16); // Store LREG2 result to data[16]
    TTI_SFPSTORE(3, 0, ADDR_MOD_7, 2 + 16); // Store LREG3 result to data[18]

    // F0,1 R4 - Process Face 0,1 Row 4 (addresses 4,6,20,22)
    TTI_SFPLOAD(4, 0, ADDR_MOD_7, 4);      // Load data[4] to LREG4
    TTI_SFPLOAD(5, 0, ADDR_MOD_7, 6);      // Load data[6] to LREG5
    TTI_SFPLOAD(6, 0, ADDR_MOD_7, 4 + 16); // Load data[20] to LREG6
    TTI_SFPLOAD(7, 0, ADDR_MOD_7, 6 + 16); // Load data[22] to LREG7

    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose operation
    lltt::replay(8, 8);                    // Execute cumsum replay buffer (operations 8-15)
    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose back

    TTI_SFPSTORE(4, 0, ADDR_MOD_7, 4);      // Store LREG4 result to data[4]
    TTI_SFPSTORE(5, 0, ADDR_MOD_7, 6);      // Store LREG5 result to data[6]
    TTI_SFPSTORE(6, 0, ADDR_MOD_7, 4 + 16); // Store LREG6 result to data[20]
    TTI_SFPSTORE(7, 0, ADDR_MOD_7, 6 + 16); // Store LREG7 result to data[22]

    // F0,1 R8 - Process Face 0,1 Row 8 (addresses 8,10,24,26)
    TTI_SFPLOAD(0, 0, ADDR_MOD_7, 8);      // Load data[8] to LREG0
    TTI_SFPLOAD(1, 0, ADDR_MOD_7, 10);     // Load data[10] to LREG1
    TTI_SFPLOAD(2, 0, ADDR_MOD_7, 8 + 16); // Load data[24] to LREG2
    TTI_SFPLOAD(3, 0, ADDR_MOD_7, 10 + 16); // Load data[26] to LREG3

    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose operation
    lltt::replay(0, 8);                    // Execute cumsum replay buffer (operations 0-7)
    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose back

    TTI_SFPSTORE(0, 0, ADDR_MOD_7, 8);      // Store LREG0 result to data[8]
    TTI_SFPSTORE(1, 0, ADDR_MOD_7, 10);     // Store LREG1 result to data[10]
    TTI_SFPSTORE(2, 0, ADDR_MOD_7, 8 + 16); // Store LREG2 result to data[24]
    TTI_SFPSTORE(3, 0, ADDR_MOD_7, 10 + 16); // Store LREG3 result to data[26]

    // F0,1 R12 - Process Face 0,1 Row 12 (addresses 12,14,28,30)
    TTI_SFPLOAD(4, 0, ADDR_MOD_7, 12);     // Load data[12] to LREG4
    TTI_SFPLOAD(5, 0, ADDR_MOD_7, 14);     // Load data[14] to LREG5
    TTI_SFPLOAD(6, 0, ADDR_MOD_7, 12 + 16); // Load data[28] to LREG6
    TTI_SFPLOAD(7, 0, ADDR_MOD_7, 14 + 16); // Load data[30] to LREG7

    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose operation
    lltt::replay(8, 8);                    // Execute cumsum replay buffer (operations 8-15)
    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose back

    TTI_SFPSTORE(4, 0, ADDR_MOD_7, 12);     // Store LREG4 result to data[12]
    TTI_SFPSTORE(5, 0, ADDR_MOD_7, 14);     // Store LREG5 result to data[14]
    TTI_SFPSTORE(6, 0, ADDR_MOD_7, 12 + 16); // Store LREG6 result to data[28]
    TTI_SFPSTORE(7, 0, ADDR_MOD_7, 14 + 16); // Store LREG7 result to data[30]

    // F2,3 R0 - Process Face 2,3 Row 0 (addresses 32,34,48,50)
    TTI_SFPLOAD(0, 0, ADDR_MOD_7, 0 + 32);     // Load data[32] to LREG0
    TTI_SFPLOAD(1, 0, ADDR_MOD_7, 2 + 32);     // Load data[34] to LREG1
    TTI_SFPLOAD(2, 0, ADDR_MOD_7, 0 + 16 + 32); // Load data[48] to LREG2
    TTI_SFPLOAD(3, 0, ADDR_MOD_7, 2 + 16 + 32); // Load data[50] to LREG3

    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose operation
    lltt::replay(0, 8);                    // Execute cumsum replay buffer (operations 0-7)
    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose back

    TTI_SFPSTORE(0, 0, ADDR_MOD_7, 0 + 32);     // Store LREG0 result to data[32]
    TTI_SFPSTORE(1, 0, ADDR_MOD_7, 2 + 32);     // Store LREG1 result to data[34]
    TTI_SFPSTORE(2, 0, ADDR_MOD_7, 0 + 16 + 32); // Store LREG2 result to data[48]
    TTI_SFPSTORE(3, 0, ADDR_MOD_7, 2 + 16 + 32); // Store LREG3 result to data[50]

    // F2,3 R4 - Process Face 2,3 Row 4 (addresses 36,38,52,54)
    TTI_SFPLOAD(4, 0, ADDR_MOD_7, 4 + 32);     // Load data[36] to LREG4
    TTI_SFPLOAD(5, 0, ADDR_MOD_7, 6 + 32);     // Load data[38] to LREG5
    TTI_SFPLOAD(6, 0, ADDR_MOD_7, 4 + 16 + 32); // Load data[52] to LREG6
    TTI_SFPLOAD(7, 0, ADDR_MOD_7, 6 + 16 + 32); // Load data[54] to LREG7

    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose operation
    lltt::replay(8, 8);                    // Execute cumsum replay buffer (operations 8-15)
    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose back

    TTI_SFPSTORE(4, 0, ADDR_MOD_7, 4 + 32);     // Store LREG4 result to data[36]
    TTI_SFPSTORE(5, 0, ADDR_MOD_7, 6 + 32);     // Store LREG5 result to data[38]
    TTI_SFPSTORE(6, 0, ADDR_MOD_7, 4 + 16 + 32); // Store LREG6 result to data[52]
    TTI_SFPSTORE(7, 0, ADDR_MOD_7, 6 + 16 + 32); // Store LREG7 result to data[54]

    // F2,3 R8 - Process Face 2,3 Row 8 (addresses 40,42,56,58)
    TTI_SFPLOAD(0, 0, ADDR_MOD_7, 8 + 32);     // Load data[40] to LREG0
    TTI_SFPLOAD(1, 0, ADDR_MOD_7, 10 + 32);    // Load data[42] to LREG1
    TTI_SFPLOAD(2, 0, ADDR_MOD_7, 8 + 16 + 32); // Load data[56] to LREG2
    TTI_SFPLOAD(3, 0, ADDR_MOD_7, 10 + 16 + 32); // Load data[58] to LREG3

    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose operation
    lltt::replay(0, 8);                    // Execute cumsum replay buffer (operations 0-7)
    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose back

    TTI_SFPSTORE(0, 0, ADDR_MOD_7, 8 + 32);     // Store LREG0 result to data[40]
    TTI_SFPSTORE(1, 0, ADDR_MOD_7, 10 + 32);    // Store LREG1 result to data[42]
    TTI_SFPSTORE(2, 0, ADDR_MOD_7, 8 + 16 + 32); // Store LREG2 result to data[56]
    TTI_SFPSTORE(3, 0, ADDR_MOD_7, 10 + 16 + 32); // Store LREG3 result to data[58]

    // F2,3 R12 - Process Face 2,3 Row 12 (addresses 44,46,60,62)
    TTI_SFPLOAD(4, 0, ADDR_MOD_7, 12 + 32);    // Load data[44] to LREG4
    TTI_SFPLOAD(5, 0, ADDR_MOD_7, 14 + 32);    // Load data[46] to LREG5
    TTI_SFPLOAD(6, 0, ADDR_MOD_7, 12 + 16 + 32); // Load data[60] to LREG6
    TTI_SFPLOAD(7, 0, ADDR_MOD_7, 14 + 16 + 32); // Load data[62] to LREG7

    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose operation
    lltt::replay(8, 8);                    // Execute cumsum replay buffer (operations 8-15)
    TTI_SFPTRANSP(0, 0, 0, 0);             // Transpose back

    TTI_SFPSTORE(4, 0, ADDR_MOD_7, 12 + 32);    // Store LREG4 result to data[44]
    TTI_SFPSTORE(5, 0, ADDR_MOD_7, 14 + 32);    // Store LREG5 result to data[46]
    TTI_SFPSTORE(6, 0, ADDR_MOD_7, 12 + 16 + 32); // Store LREG6 result to data[60]
    TTI_SFPSTORE(7, 0, ADDR_MOD_7, 14 + 16 + 32); // Store LREG7 result to data[62]
}

/**
 * @brief Initialize replay buffer for cumulative sum operations
 *
 * @details Sets up a replay buffer containing the core cumulative sum algorithm.
 * This buffer is later executed during the cumsum operation via `lltt::replay()`
 * calls. The replay mechanism allows pre-programming a sequence of SFPU operations
 * that can be efficiently executed multiple times.
 *
 * **Replay Buffer Contents:**
 * The buffer contains 16 operations (8 additions + 8 NOPs) that implement
 * cumulative addition across SFPU registers:
 *
 * ```assembly
 * TTI_SFPADD(10, 7, 0, 0, 0);  // Add LREG7 + LREG0 → LREG0
 * TTI_SFPNOP;                  // Pipeline spacing
 * TTI_SFPADD(10, 0, 1, 1, 0);  // Add LREG0 + LREG1 → LREG1  
 * TTI_SFPNOP;                  // Pipeline spacing
 * TTI_SFPADD(10, 1, 2, 2, 0);  // Add LREG1 + LREG2 → LREG2
 * TTI_SFPNOP;                  // Pipeline spacing
 * TTI_SFPADD(10, 2, 3, 3, 0);  // Add LREG2 + LREG3 → LREG3
 * TTI_SFPNOP;                  // Pipeline spacing
 * TTI_SFPADD(10, 3, 4, 4, 0);  // Add LREG3 + LREG4 → LREG4
 * TTI_SFPNOP;                  // Pipeline spacing
 * TTI_SFPADD(10, 4, 5, 5, 0);  // Add LREG4 + LREG5 → LREG5
 * TTI_SFPNOP;                  // Pipeline spacing
 * TTI_SFPADD(10, 5, 6, 6, 0);  // Add LREG5 + LREG6 → LREG6
 * TTI_SFPNOP;                  // Pipeline spacing
 * TTI_SFPADD(10, 6, 7, 7, 0);  // Add LREG6 + LREG7 → LREG7
 * TTI_SFPNOP;                  // Pipeline spacing
 * ```
 *
 * **Cumulative Sum Chain:**
 * The sequence creates a dependency chain where each register accumulates
 * the sum of all previous registers, implementing the cumulative sum:
 * - LREG0 = LREG7 + LREG0  (context from previous operation)
 * - LREG1 = LREG0 + LREG1  (cumulative through LREG1)
 * - LREG2 = LREG1 + LREG2  (cumulative through LREG2)
 * - ... and so on
 *
 * **Pipeline Optimization:**
 * The NOPs between additions provide pipeline spacing to avoid data hazards
 * and ensure proper execution in the SFPU hardware pipeline.
 *
 * **Replay Buffer Usage:**
 * - `load_replay_buf(0, 16, ...)`: Load 16 operations starting at buffer offset 0
 * - Called once during initialization before any cumsum operations
 * - Buffer contents persist and can be replayed multiple times efficiently
 *
 * @tparam APPROXIMATION_MODE Template parameter (marked unused in implementation)
 *
 * @note Must be called once before using _calculate_cumsum_()
 * @note Replay buffer is shared across all cumsum operations
 * @note The lambda function provides the instruction sequence to be buffered
 * @note Buffer provides significant performance improvement over inline operations
 */
template <bool APPROXIMATION_MODE /*unused*/>
inline void _cumsum_init_()
{
    load_replay_buf(
        0,      // Buffer start offset
        16,     // Number of operations to buffer
        []      // Lambda containing the instruction sequence
        {
            TTI_SFPADD(10, 7, 0, 0, 0);  // context + input[0] → output[0]
            TTI_SFPNOP;                  // Pipeline spacing
            TTI_SFPADD(10, 0, 1, 1, 0);  // output[0] + input[1] → output[1]
            TTI_SFPNOP;                  // Pipeline spacing
            TTI_SFPADD(10, 1, 2, 2, 0);  // output[1] + input[2] → output[2]
            TTI_SFPNOP;                  // Pipeline spacing
            TTI_SFPADD(10, 2, 3, 3, 0);  // output[2] + input[3] → output[3]
            TTI_SFPNOP;                  // Pipeline spacing
            TTI_SFPADD(10, 3, 4, 4, 0);  // output[3] + input[4] → output[4]
            TTI_SFPNOP;                  // Pipeline spacing
            TTI_SFPADD(10, 4, 5, 5, 0);  // output[4] + input[5] → output[5]
            TTI_SFPNOP;                  // Pipeline spacing
            TTI_SFPADD(10, 5, 6, 6, 0);  // output[5] + input[6] → output[6]
            TTI_SFPNOP;                  // Pipeline spacing
            TTI_SFPADD(10, 6, 7, 7, 0);  // output[6] + input[7] → output[7]
            TTI_SFPNOP;                  // Pipeline spacing
        });
}

} // namespace sfpu
} // namespace ckernel
