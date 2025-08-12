// SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel.h"
#include "ckernel_defs.h"
#include "noc_nonblocking_api.h"
#include <bit>

#include "sfpi.h"

using namespace sfpi;

namespace ckernel {
namespace sfpu {

//calculates one iteration of welfords
void Welfords_Math(uint InputLREG, uint32_t N){
    //mean calculation start
    //mean_{N_+1} = mean_{N} + ((1/N+1) * (x_{N+1} - mean_{N}))

    constexpr uint16_t negative_one_BF16 =0xBf80;
    /*mean_{N} = -1 * mean_{N}*/
    TTI_SFPMULI(negative_one_BF16, p_sfpu::LREG4, 0);
    //Next cycle cannot read from LREG4 See tt-isa-documentation

    /*var_{N+1}temp = 1/(N+1)*/
    float inv_n_plus_1 = 1.0/((float)N+1.0);
    uint32_t bits = __builtin_bit_cast(std::uint32_t, inv_n_plus_1);
    uint16_t high16_bits = static_cast<uint16_t>(bits >> 16);
    uint16_t low16_bits  = static_cast<uint16_t>(bits & 0xFFFF);
    /*var_{N+1}temp = 1/(N+1) usage loads high 16 bits*/
    TT_SFPLOADI(p_sfpu::LREG7, 8, high16_bits);

    /*mean_{N+1}temp = 1 * (InputLREG + (-mean))*/
    TTI_SFPADD(p_sfpu::LCONST_1/*LREG10 = <1>*/, InputLREG, p_sfpu::LREG4, p_sfpu::LREG6,0);
    //Next cycle cannot read from LREG6 See tt-isa-documentation

    /*mean_{N} = -1 * ((-1) *mean_{N})*/
    TTI_SFPMULI(negative_one_BF16, p_sfpu::LREG4, 0);
    //Next cycle cannot read from LREG4 See tt-isa-documentation

    /*var_{N+1}temp = 1/(N+1) usage loads low 16 bits*/
    TT_SFPLOADI(p_sfpu::LREG7, 10, low16_bits);

    /*mean_{N+1} = ((mean_{N+1} = (InputLREG-mean) * (1/N+1)) + mean_{N}*/
    TTI_SFPMAD(p_sfpu::LREG6, p_sfpu::LREG7, p_sfpu::LREG4, p_sfpu::LREG6, 0);
    //Next cycle cannot read from LREG6 See tt-isa-documentation

    //mean calculation end
    //
    //var calculation start


    //var_{N+1} = var_{N} + ...
    //...(1/N+1) * (((x_{N+1} - mean_{N}) * (x_{N+1} - mean_{N+1})) - var_{N})

    /*mean_{N} = (Input_LREG - mean_{N})*/
    TTI_SFPMAD(p_sfpu::LREG11, p_sfpu::LREG4, InputLREG, p_sfpu::LREG4, 0);
    //Next cycle cannot read from LREG4 See tt-isa-documentation

    /*inputLREG temp = (InputLREG + (-mean_{N+1}))*/
    TTI_SFPMAD(p_sfpu::LREG11/*LREG11 = <-1>*/,p_sfpu::LREG6, InputLREG, InputLREG,0);
    //Next cycle cannot read from InputLREG See tt-isa-documentation

    //var_N = -var_N
    TTI_SFPMULI(negative_one_BF16, p_sfpu::LREG5, 0);
    //Next cycle cannot read from LREG5 See tt-isa-documentation
    TTI_SFPNOP;

    /*InputLREG temp= (mean_{N} = (InputLREG-mean_{N}) * InputLREG =(InputLREG - mean_{N+1})) + var_{N}*/
    TTI_SFPMAD(p_sfpu::LREG4, InputLREG, p_sfpu::LREG5, InputLREG, 0);
    //Next cycle cannot read from InputLREG See tt-isa-documentation

    //var_N = -(-var_N)
    TTI_SFPMULI(negative_one_BF16, p_sfpu::LREG5, 0);
    TTI_SFPNOP;

    //updates the var
    /*var_{N} = (InputLREG = (see above) * (1/N+1)) + var_{N} = ()*/
    TTI_SFPMAD(InputLREG, p_sfpu::LREG7, p_sfpu::LREG5, p_sfpu::LREG5, 0);
    //Next cycle cannot read from LREG5 See tt-isa-documentation

    //keep it simple please change
    /*mean_{N} = -1 * ((-1) *mean_{N+1})*/
    TTI_SFPMUL(p_sfpu::LCONST_1 /*LREG11 = <-1>*/, p_sfpu::LREG6, p_sfpu::LCONST_0, p_sfpu::LREG4, 0);
    //Next cycle cannot read from LREG4 See tt-isa-documentation
    TTI_SFPNOP;

    //var calculation end
    //Now past_mean (LREG4) is population
    //Now past_var (LREG5) is population
}

void Welfords_Main(uint32_t &N, uint32_t &endN, uint32_t &last_run) {

    // dst0 = input
    // dst1 = past_mean
    // dst2 = past_var


    //potentially not needed? Helped in SFPI Variation

    //total 172 cycles
    //4 * 8 for rows calls
    //8 transpose
    //16 past_pulls
    //
    //14 * 8 Calc
    //
    //16 past writes

    for(uint32_t i = 0; i < 2; i++){
        for(uint32_t j = 0; j < 4; j++){

            /*row1*/TT_SFPLOAD(p_sfpu::LREG0, 0, ADDR_MOD_3, (i*32) + (4*j) );
            /*row2*/TT_SFPLOAD(p_sfpu::LREG1, 0, ADDR_MOD_3,(i*32) + (4*j) + 2 );
            /*row3*/TT_SFPLOAD(p_sfpu::LREG2, 0, ADDR_MOD_3, (i*32) + (4*j) + 16 );
            /*row4*/TT_SFPLOAD(p_sfpu::LREG3, 0, ADDR_MOD_3, (i*32) + (4*j) + 18 );

            /*transposes raw mixed data to logical rows*/
            TTI_SFPTRANSP(0, 0, 0, 0);
            if(N == 0){
                //Needed since LREGS can maintain state between calls/maybe kernels? So setting them to zero is needed
                TTI_SFPLOADI(p_sfpu::LREG4, 0, 0);
                TTI_SFPLOADI(p_sfpu::LREG5, 0, 0);
                TTI_SFPLOADI(p_sfpu::LREG6, 0, 0);
                TTI_SFPLOADI(p_sfpu::LREG7, 0, 0);
            }
            else{
            /*past_mean = dst1*/TTI_SFPLOAD(p_sfpu::LREG4, 0, ADDR_MOD_3, 64);
            /*past_var = dst2*/TTI_SFPLOAD(p_sfpu::LREG5, 0, ADDR_MOD_3, 128);
            //wiping LREG 6 and 7 since they may be filled with garbage data
            TTI_SFPLOADI(p_sfpu::LREG6, 0, 0);
            TTI_SFPLOADI(p_sfpu::LREG7, 0, 0);
            }
            //LOAD was success has been tested for L0 and L1

            if(N == endN) return;
            Welfords_Math(p_sfpu::LREG0, N);
            N++;
            if(N == endN) return;
            Welfords_Math(p_sfpu::LREG1, N);
            N++;
            if(N == endN) return;
            Welfords_Math(p_sfpu::LREG2, N);
            N++;
            if(N == endN) return;
            Welfords_Math(p_sfpu::LREG3, N);
            N++;
            //storing since SFPTRANSP will shuffle these LREGS
            TTI_SFPSTORE(p_sfpu::LREG4, 0, ADDR_MOD_3, 64);
            TTI_SFPSTORE(p_sfpu::LREG5, 0, ADDR_MOD_3, 128);
        }
    }
    return;
}
void _welfords_(uint32_t N, uint32_t endN, uint32_t last_run){
    Welfords_Main(N, endN, last_run);
    if(N == endN){
        //This subroutine allows us to save the row of mean vals to the dstreg 1 and row of variance vals to dstreg 2
        TTI_SFPADD(p_sfpu::LCONST_1/*LREG10 = <1>*/, p_sfpu::LCONST_0, p_sfpu::LREG4, p_sfpu::LREG0,0);
        TTI_SFPLOADI(p_sfpu::LREG1, 0, 0);
        TTI_SFPLOADI(p_sfpu::LREG2, 0, 0);
        TTI_SFPLOADI(p_sfpu::LREG3, 0, 0);

        TTI_SFPADD(p_sfpu::LCONST_1/*LREG10 = <1>*/, p_sfpu::LCONST_0, p_sfpu::LREG5, p_sfpu::LREG4,0);
        TTI_SFPLOADI(p_sfpu::LREG5, 0, 0);
        TTI_SFPLOADI(p_sfpu::LREG6, 0, 0);
        TTI_SFPLOADI(p_sfpu::LREG7, 0, 0);

        TTI_SFPTRANSP(0, 0, 0, 0);

        TTI_SFPSTORE(p_sfpu::LREG0, 0, ADDR_MOD_3, 64);
        TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_3, 64 + 2);
        TTI_SFPSTORE(p_sfpu::LREG2, 0, ADDR_MOD_3, 64 + 16);
        TTI_SFPSTORE(p_sfpu::LREG3, 0, ADDR_MOD_3, 64 + 18);

        TTI_SFPSTORE(p_sfpu::LREG4, 0, ADDR_MOD_3, 128);
        TTI_SFPSTORE(p_sfpu::LREG5, 0, ADDR_MOD_3, 128 + 2);
        TTI_SFPSTORE(p_sfpu::LREG6, 0, ADDR_MOD_3, 128 + 16);
        TTI_SFPSTORE(p_sfpu::LREG7, 0, ADDR_MOD_3, 128 + 18);
    }
}
}
}
