// SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel.h"
#include "ckernel_defs.h"
#include "noc_nonblocking_api.h"

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

void _welfords_(uint N, uint endN, uint last_run) {
    vFloat past_mean = dst_reg[32];
    vFloat past_var = dst_reg[64];
    vFloat mean = 0;
    vFloat var = 0;

    for(uint32_t i = 0; i < 2; i++){
        for(uint32_t j = 0; j < 4; j++){
            vFloat row0 = dst_reg[(i*16) + (2*j)];
            vFloat row1 = dst_reg[(i*16) + (2*j) + 1];
            vFloat row2 = dst_reg[(i*16) + (2*j) + 8];
            vFloat row3 = dst_reg[(i*16) + (2*j) + 9];
            //Transposes all of them 0-3 to 0-3 and also does 4-7 to 4-7 Need to load in from dst after we do this each time
            //
            //
            __builtin_rvtt_sfptransp(row0.get(), row1.get(), row2.get(), row3.get());
            //pull from dest here
            past_mean = dst_reg[32];
            past_var = dst_reg[64];
            WELFORDS(row0, past_mean, mean, past_var, var, N);
            WELFORDS(row1, past_mean, mean, past_var, var, N);
            WELFORDS(row2, past_mean, mean, past_var, var, N);
            WELFORDS(row3, past_mean, mean, past_var, var, N);
            //write to dst here
            dst_reg[32] = past_mean;
            dst_reg[64] = past_var;
        }
    }
    dst_reg[32] = past_mean;
    dst_reg[64] = past_var;
    return;
}

}
}
