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

//calculates one iteration of welfords
template<uint InputLREG>
sfpi_inline void Welfords_Math(uint32_t N){
    //mean calculation start
    //mean_{N_+1} = mean_{N} + ((1/N+1) * (x_{N+1} - mean_{N}))

    constexpr uint16_t negative_one_BF16 =0xBf80;

    /*var_{N+1}temp = 1/(N+1)*/
    float inv_n_plus_1 = 1.0/((float)N+1.0);
    uint32_t bits = __builtin_bit_cast(std::uint32_t, inv_n_plus_1);
    uint16_t high16_bits = static_cast<uint16_t>(bits >> 16);
    uint16_t low16_bits  = static_cast<uint16_t>(bits & 0xFFFF);
    /*var_{N+1}temp = 1/(N+1) usage loads high 16 bits*/
    TT_SFPLOADI(p_sfpu::LREG7, 8, high16_bits);

    /*mean_{N+1}temp = 1 * (InputLREG + (-mean))*/
    TTI_SFPMAD(p_sfpu::LREG11, p_sfpu::LREG4, InputLREG, p_sfpu::LREG6, 0);
    //Next cycle cannot read from LREG6 See tt-isa-documentation

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

template <uint32_t I, uint32_t J>
sfpi_inline void Welfords_Load_Data() {
    constexpr uint32_t offset1 = (I*32) + (4*J);
    constexpr uint32_t offset2 = offset1 + 2;
    constexpr uint32_t offset3 = offset1 + 16;
    constexpr uint32_t offset4 = offset1 + 18;
            /*row1*/TTI_SFPLOAD(p_sfpu::LREG0, 0, ADDR_MOD_3, offset1);
            /*row2*/TTI_SFPLOAD(p_sfpu::LREG1, 0, ADDR_MOD_3, offset2);
            /*row3*/TTI_SFPLOAD(p_sfpu::LREG2, 0, ADDR_MOD_3, offset3);
            /*row4*/TTI_SFPLOAD(p_sfpu::LREG3, 0, ADDR_MOD_3, offset4);
            /*transposes raw mixed data to logical rows*/
            TTI_SFPTRANSP(0, 0, 0, 0);
            /*past_mean = dst1*/TTI_SFPLOAD(p_sfpu::LREG4, 0, ADDR_MOD_3, 64);
            /*past_var = dst2*/TTI_SFPLOAD(p_sfpu::LREG5, 0, ADDR_MOD_3, 128);
            //wiping LREG 6 and 7 since they may be filled with garbage data
            TTI_SFPLOADI(p_sfpu::LREG6, 0, 0);
            TTI_SFPLOADI(p_sfpu::LREG7, 0, 0);
}

sfpi_inline void Welfords_Load_Initial_Data() {
    constexpr uint32_t offset1 = 0;
    constexpr uint32_t offset2 = offset1 + 2;
    constexpr uint32_t offset3 = offset1 + 16;
    constexpr uint32_t offset4 = offset1 + 18;
            /*row1*/TTI_SFPLOAD(p_sfpu::LREG0, 0, ADDR_MOD_3, offset1);
            /*row2*/TTI_SFPLOAD(p_sfpu::LREG1, 0, ADDR_MOD_3, offset2);
            /*row3*/TTI_SFPLOAD(p_sfpu::LREG2, 0, ADDR_MOD_3, offset3);
            /*row4*/TTI_SFPLOAD(p_sfpu::LREG3, 0, ADDR_MOD_3, offset4);
            /*transposes raw mixed data to logical rows*/
            TTI_SFPTRANSP(0, 0, 0, 0);
                //Needed since LREGS can maintain state between calls/maybe kernels? So setting them to zero is needed
                TTI_SFPLOADI(p_sfpu::LREG4, 0, 0);
                TTI_SFPLOADI(p_sfpu::LREG5, 0, 0);
            //wiping LREG 6 and 7 since they may be filled with garbage data
            TTI_SFPLOADI(p_sfpu::LREG6, 0, 0);
            TTI_SFPLOADI(p_sfpu::LREG7, 0, 0);
}


// Macro to allow returns to exit main function

#define WELFORDS_LOOP_ITERATION(N, endN) \
            Welfords_Math<p_sfpu::LREG0>(N); \
            N++; \
            if(N == endN) { return; }\
            Welfords_Math<p_sfpu::LREG1>(N); \
            N++; \
            if(N == endN) { return; }\
            Welfords_Math<p_sfpu::LREG2>(N); \
            N++; \
            if(N == endN) { return; }\
            Welfords_Math<p_sfpu::LREG3>(N); \
            N++; \
            TTI_SFPSTORE(p_sfpu::LREG4, 0, ADDR_MOD_3, 64); \
            TTI_SFPSTORE(p_sfpu::LREG5, 0, ADDR_MOD_3, 128);

sfpi_inline void Welfords_Main(uint32_t &N, uint32_t &endN, uint32_t &last_run) {
    // I, J, LOAD_PREVIOUS, N, endN. N can only be zero in first iteration
    if (N == 0) {
        Welfords_Load_Initial_Data();
        WELFORDS_LOOP_ITERATION(N, endN)
    } else {
        Welfords_Load_Data<0,0>();
        WELFORDS_LOOP_ITERATION(N, endN)
    }
    Welfords_Load_Data<0,1>();
    WELFORDS_LOOP_ITERATION(N, endN)
    Welfords_Load_Data<0,2>();
    WELFORDS_LOOP_ITERATION(N, endN)
    Welfords_Load_Data<0,3>();
    WELFORDS_LOOP_ITERATION(N, endN)
    Welfords_Load_Data<1,0>();
    WELFORDS_LOOP_ITERATION(N, endN)
    Welfords_Load_Data<1,1>();
    WELFORDS_LOOP_ITERATION(N, endN)
    Welfords_Load_Data<1,2>();
    WELFORDS_LOOP_ITERATION(N, endN)
    Welfords_Load_Data<1,3>();
    WELFORDS_LOOP_ITERATION(N, endN)
    return;
}
#undef WELFORDS_LOOP_ITERATION

void _welfords_(uint32_t N, uint32_t endN, uint32_t last_run){
    Welfords_Main(N, endN, last_run);
    if(N == endN){
        // This subroutine allows us to save the row of mean vals to the dstreg 1 and row of variance vals to dstreg 2
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
} // namespace sfpu
} // namespace ckernel
