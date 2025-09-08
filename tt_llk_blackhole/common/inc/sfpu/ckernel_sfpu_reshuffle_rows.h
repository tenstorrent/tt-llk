
// SPDX-FileCopyrightText: © 2024 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_defs.h"
#include "ckernel.h"
#include "debug/dprint.h"

using namespace sfpi;

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_reshuffle_rows_(const uint idx_addr) // idx_addr = 0 for input
{
    constexpr uint output_tile_offset = 64; // in rows

    // clr DEST tile 1
    // TODO (Radomir): Add optional clear that is more optimal using tile copy
    // for (uint row=0; row < 32; row+=4) {
    //     TT_SFPSTORE(p_sfpu::LCONST_0, 0, ADDR_MOD_3, output_tile_offset + row);
    //     TT_SFPSTORE(p_sfpu::LCONST_0, 0, ADDR_MOD_3, output_tile_offset + row + 2);
    //     TT_SFPSTORE(p_sfpu::LCONST_0, 0, ADDR_MOD_3, output_tile_offset + row + 32);
    //     TT_SFPSTORE(p_sfpu::LCONST_0, 0, ADDR_MOD_3, output_tile_offset + row + 34);
    // }

    volatile tt_l1_ptr uint8_t *idx_ptr = reinterpret_cast<volatile tt_l1_ptr uint8_t*>(idx_addr+(1<<4)); // idx_ptr = 0 + 1<<4 = 16
    static constexpr uint input_lreg[4] = {p_sfpu::LREG0, p_sfpu::LREG1, p_sfpu::LREG2, p_sfpu::LREG3};
    static constexpr uint output_lreg[4] = {p_sfpu::LREG4, p_sfpu::LREG5, p_sfpu::LREG6, p_sfpu::LREG7};

    for (uint row=0; row < 32; row++) {

        uint input_row_addr = (row < 16) ? ((row/4) * 4) : ((row/4) * 4 + 16);

        /*

        For row = 0, 1, 2, 3, input_row_addr = 0
        For row = 4, 5, 6, 7, input_row_addr = 4
        For row = 8, 9, 10, 11, input_row_addr = 8
        For row = 12, 13, 14, 15, input_row_addr = 12     
        For row = 16, 17, 18, 19, input_row_addr = 32
        For row = 20, 21, 22, 23, input_row_addr = 36
        For row = 24, 25, 26, 27, input_row_addr = 40
        For row = 28, 29, 30, 31, input_row_addr = 44

        */

        uint input_row_lreg = input_lreg[row % 4];

        /*
        row 
        0	input_low_lreg = LREG0
        1	input_low_lreg = LREG1
        2	input_low_lreg = LREG2
        3	input_low_lreg = LREG3
        4	input_low_lreg = LREG0
        5	input_low_lreg = LREG1
        6	input_low_lreg = LREG2
        7	input_low_lreg = LREG3
        8	input_low_lreg = LREG0
        9	input_low_lreg = LREG1
        10	input_low_lreg = LREG2
        11	input_low_lreg = LREG3
        12	input_low_lreg = LREG0
        13	input_low_lreg = LREG1
        14	input_low_lreg = LREG2
        15	input_low_lreg = LREG3
        16	input_low_lreg = LREG0
        17	input_low_lreg = LREG1
        18	input_low_lreg = LREG2
        19	input_low_lreg = LREG3
        20	input_low_lreg = LREG0
        21	input_low_lreg = LREG1
        22	input_low_lreg = LREG2
        23	input_low_lreg = LREG3
        24	input_low_lreg = LREG0
        25	input_low_lreg = LREG1
        26	input_low_lreg = LREG2
        27	input_low_lreg = LREG3
        28	input_low_lreg = LREG0
        29	input_low_lreg = LREG1
        30	input_low_lreg = LREG2
        31	input_low_lreg = LREG3
        
        */

        uint dst_row = (uint)idx_ptr[row]; // pointers to indexes 

        if (dst_row >= 32) continue;
        uint output_row_addr = (dst_row < 16) ? ((dst_row/4) * 4) : ((dst_row/4) * 4 + 16);
        uint output_row_lreg = output_lreg[dst_row % 4];

        // load in the input row and output row
        // #define TT_SFPLOAD(lreg_ind, instr_mod0, sfpu_addr_mode, dest_reg_addr) // LOAD FROM DEST

        /*
        ADDRMOD
            bins: [ {name: "bot_half", slice: "0x10000",  interval: ["0x0","0x1"]},
            {name: "top_half", slice: "0x10000",  interval: ["0x2","0x3"]} ]
        */

        // Loading row0 from face 0 and 1 -> row0 tile 0 (input)

        TT_SFPLOAD(p_sfpu::LREG0, 0, ADDR_MOD_3, input_row_addr); // face 0 row 0  part1 to LREG0
        TT_SFPLOAD(p_sfpu::LREG1, 0, ADDR_MOD_3, input_row_addr + 2); // face 0 row 0 part 2 to LREG1
        TT_SFPLOAD(p_sfpu::LREG2, 0, ADDR_MOD_3, input_row_addr + 16); // face 1 row 0 part1 to LREG2
        TT_SFPLOAD(p_sfpu::LREG3, 0, ADDR_MOD_3, input_row_addr + 18); // face 1 row 0 part2 to LREG3

        // Loading row0 from face 0 and 1 -> row0 tile 1 (output) 

        TT_SFPLOAD(p_sfpu::LREG4, 0, ADDR_MOD_3, output_tile_offset + output_row_addr); 
        TT_SFPLOAD(p_sfpu::LREG5, 0, ADDR_MOD_3, output_tile_offset + output_row_addr + 2);
        TT_SFPLOAD(p_sfpu::LREG6, 0, ADDR_MOD_3, output_tile_offset + output_row_addr + 16); 
        TT_SFPLOAD(p_sfpu::LREG7, 0, ADDR_MOD_3, output_tile_offset + output_row_addr + 18);
        TTI_SFPTRANSP(0,0,0,0); // puts desired input row into LREG "input_row_lreg" and output row into "output_row_lreg"
        
        // HELPERS
        //#define TT_SFPADD(lreg_src_a, lreg_src_b, lreg_src_c, lreg_dest, instr_mod1)
        //constexpr static uint LCONST_1 = 10;

        TT_SFPADD(input_row_lreg, p_sfpu::LCONST_1, output_row_lreg, output_row_lreg, 0);
        TTI_SFPNOP;

        // store back the reduce rows to output

        //#define TTI_SFPTRANSP(imm12_math, lreg_c, lreg_dest, instr_mod1
        TTI_SFPTRANSP(0,0,0,0); // puts desired output row back into into LREG4

        //#define TT_SFPSTORE(lreg_ind, instr_mod0, sfpu_addr_mode, dest_reg_addr)
        TT_SFPSTORE(p_sfpu::LREG4, 0, ADDR_MOD_3, output_tile_offset + output_row_addr);
        TT_SFPSTORE(p_sfpu::LREG5, 0, ADDR_MOD_3, output_tile_offset + output_row_addr + 2);
        TT_SFPSTORE(p_sfpu::LREG6, 0, ADDR_MOD_3, output_tile_offset + output_row_addr + 16);
        TT_SFPSTORE(p_sfpu::LREG7, 0, ADDR_MOD_3, output_tile_offset + output_row_addr + 18);
    }
}

} // namespace sfpu
} // namespace ckernel