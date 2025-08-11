// SPDX-FileCopyrightText: © 2025 Tenstorrent Inc.
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

//Notes we use macros since the code crashes when we use inline functions.
#define WELFORDS(row, past_mean, mean, past_var, var, N) { \
                mean = row; \
                mean -= past_mean; \
                float inv_n_plus_1 = 1.0/((float)N+1.0); \
                var = inv_n_plus_1; \
                mean *= var; \
                mean += past_mean; \
                past_mean = -past_mean; \
                past_mean += row; \
                mean = -mean; \
                mean += row; \
                past_mean *= mean; \
                past_mean -= past_var; \
                past_mean *= var; \
                past_mean += past_var; \
                past_var = past_mean; \
                past_mean = -mean; \
                N++; \
            }
sfpi_inline void transpose (vFloat &f0,  vFloat &f1, vFloat &f2, vFloat &f3, vFloat &f4, vFloat &f5, vFloat &f6, vFloat &f7) {
   asm volatile ("sfptransp" : "+Q0"(f1.get()), "+Q1"(f1.get()), "+Q2"(f2.get()), "+Q3"(f3.get()),
                "+Q4"(f4.get()), "+Q5"(f5.get()), "+Q6"(f6.get()), "+Q7"(f7.get()));
}

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
    uint32_t bits = std::bit_cast<uint32_t>(inv_n_plus_1);
    uint16_t high16 = static_cast<uint16_t>(bits >> 16);
    uint16_t low16  = static
    TT_SFPLOADI(p_sfpu::LREG7, 8, high16);

    /*mean_{N+1}temp = 1 * (InputLREG + (-mean))*/
    TTI_SFPADD(p_sfpu::LREG10/*LREG10 = <1>*/, InputLREG, p_sfpu::LREG4, p_sfpu::LREG6,0);
    //Next cycle cannot read from LREG6 See tt-isa-documentation

    /*mean_{N} = -1 * ((-1) *mean_{N})*/
    TTI_SFPMULI(negative_one_BF16, p_sfpu::LREG4, 0);
    //Next cycle cannot read from LREG4 See tt-isa-documentation

    /*var_{N+1}temp = 1/(N+1) usage loads low 16 bits*/
    TT_SFPLOADI(p_sfpu::LREG7, 10, low16);

    /*mean_{N+1} = ((mean_{N+1} = (InputLREG-mean) * (1/N+1)) + mean_{N}*/
    TTI_SFPMAD(p_sfpu::LREG6, p_sfpu::LREG7, p_sfpu::LREG4, p_sfpu::LREG6, 0);
    //Next cycle cannot read from LREG6 See tt-isa-documentation

    //mean calculation end
    //
    //var calculation start


    //var_{N+1} = var_{N} + ...
    //...(1/N+1) * (((x_{N+1} - mean_{N}) * (x_{N+1} - mean_{N+1})) - var_{N})

    /*mean_{N} = -1 * ((-1) *mean_{N})*/
    TTI_SFPMULI(negative_one_BF16, p_sfpu::LREG4, 0);
    //Next cycle cannot read from LREG4 See tt-isa-documentation

    /*mean_{N+1} = -1 * ((-1) *mean_{N+1})*/
    TTI_SFPMULI(negative_one_BF16, p_sfpu::LREG6, 0);
    //Next cycle cannot read from LREG6 See tt-isa-documentation

    /*mean_{N}temp = 1 * (InputLREG + (-mean_{N}))*/
    TTI_SFPADD(p_sfpu::LREG10/*LREG10 = <1>*/, InputLREG, p_sfpu::LREG4, p_sfpu::LREG4,0);
    //Next cycle cannot read from LREG4 See tt-isa-documentation


    /*inputLREG temp = 1 * (InputLREG + (-mean_{N}))*/
    TTI_SFPADD(p_sfpu::LREG10/*LREG10 = <1>*/, InputLREG, p_sfpu::LREG6, InputLREG,0);
    //Next cycle cannot read from InputLREG See tt-isa-documentation

    //Calls NOP Macro since InputLREG is read next
    TTI_NOP;

    /*InputLREG temp= (mean_{N} = (InputLREG-mean_{N}) * InputLREG =(InputLREG - mean_{N+1})) + var_{N}*/
    TTI_SFPMAD(p_sfpu::LREG4, InputLREG, p_sfpu::LREG5, InputLREG, 0);
    //Next cycle cannot read from InputLREG See tt-isa-documentation
    TTI_NOP;

    /*var_{N+1} = (InputLREG = (see above) * var_{N+1} = (1/N+1)) + var_{N} = ()*/
    TTI_SFPMAD(InputLREG, p_sfpu::LREG7, p_sfpu::LREG5, p_sfpu::LREG5, 0);

    //var calculation end
    //Now past_mean (LREG4) is population
    //Now past_var (LREG5) is population
}
void _welfords_(uint32_t N, uint32_t endN, uint32_t last_run) {

    // dst0 = input
    // dst1 = past_mean
    // dst2 = past_var

    /*past_mean = dst1*/TTI_SFPLOAD(p_sfpu::LREG4, 0, ADDR_MOD_3, 64);
    /*past_var = dst2*/TTI_SFPLOAD(p_sfpu::LREG5, 0, ADDR_MOD_3, 128);

    //potentially not needed? Helped in SFPI Variation
    /*mean = 0*/TTI_SFPLOADI(p_sfpu::LREG6, , 0)
    /*var = 0*/TTI_SFPLOADI(p_sfpu::LREG7, , 0)

    //total 172 cycles
    //4 * 8 for rows calls
    //8 transpose
    //16 past_pulls
    //
    //14 * 8 Calc
    //
    //16 past writes

    for(uint32_t i = 0; i < 1; i++){
        for(uint32_t j = 0; j < 1; j++){

            /*row1*/TT_SFPLOAD(p_sfpu::LREG0, 0, ADDR_MOD_3, (i*32) + (4*j) );
            /*row2*/TT_SFPLOAD(p_sfpu::LREG1, 0, ADDR_MOD_3,(i*32) + (4*j) + 2 );
            /*row3*/TT_SFPLOAD(p_sfpu::LREG2, 0, ADDR_MOD_3, (i*32) + (4*j) + 16 );
            /*row4*/TT_SFPLOAD(p_sfpu::LREG3, 0, ADDR_MOD_3, (i*32) + (4*j) + 18 );

            /*transposes raw mixed data to logical rows*/
            TTI_SFPTRANSP(0, 0, 0, 0);
            //LOAD was success has been tested for L0 and L1

            Welfords_Math(p_sfpu::LREG0, N);
            N++;
            Welfords_Math(p_sfpu::LREG1, N);
            N++;
            Welfords_Math(p_sfpu::LREG2, N);
            N++;
            Welfords_Math(p_sfpu::LREG3, N);
            N++;
        }
    }
    // dst_reg[32] = past_mean;
    // dst_reg[64] = past_var;
    TT_SFPSTORE(p_sfpu::LREG4, 0, ADDR_MOD_3, 64);
    TT_SFPSTORE(p_sfpu::LREG5, 0, ADDR_MOD_3, 128);
    return;
}

}
}
