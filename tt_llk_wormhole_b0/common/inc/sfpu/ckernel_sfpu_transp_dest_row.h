// SPDX-FileCopyrightText: © 2023 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace ckernel::sfpu
{

inline void _transpose_row_configure_addrmod()
{
    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_7);
}

inline void _calculate_transpose_row_()
{
    _transpose_row_configure_addrmod();

    for (uint col = 0; col < 32; col++)
    {
        // Calculate address: (0 & ~0x3) = 0 (always row 0)
        // (col & 0x10) = 0 for cols 0-15, 16 for cols 16-31
        uint input_row_addr = (0 & ~0x3) + (col & 0x10);

        // Load the 4-row block containing our target element
        TT_SFPLOAD(p_sfpu::LREG0, 0, ADDR_MOD_3, input_row_addr);      // Face 0/2, even cols
        TT_SFPLOAD(p_sfpu::LREG1, 0, ADDR_MOD_3, input_row_addr + 2);  // Face 0/2, odd cols
        TT_SFPLOAD(p_sfpu::LREG2, 0, ADDR_MOD_3, input_row_addr + 16); // Face 1/3, even cols
        TT_SFPLOAD(p_sfpu::LREG3, 0, ADDR_MOD_3, input_row_addr + 18); // Face 1/3, odd cols

        // Transpose to isolate elements
        TTI_SFPTRANSP(0, 0, 0, 0);

        // CRITICAL: Use correct LREG mapping after transpose
        // After transpose, the element position depends on (col / 4) and (col % 4)
        uint element_pos = col % 4; // Position within group

        // Map to correct LREG based on element position
        uint target_lreg;
        if (element_pos == 0)
        {
            target_lreg = p_sfpu::LREG0;
        }
        else if (element_pos == 1)
        {
            target_lreg = p_sfpu::LREG1;
        }
        else if (element_pos == 2)
        {
            target_lreg = p_sfpu::LREG2;
        }
        else
        {
            target_lreg = p_sfpu::LREG3;
        }

        // Store to correct row in column 0
        TT_SFPSTORE(target_lreg, 0, ADDR_MOD_3, col);
    }
}
} // namespace ckernel::sfpu
