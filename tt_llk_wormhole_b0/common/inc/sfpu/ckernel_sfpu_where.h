// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0


#pragma once

#include "sfpi.h"

namespace ckernel
{
namespace sfpu{

template <int ITERATIONS>
inline void _calculate_where_(int dst_index,sfpi::vFloat true_value, sfpi::vFloat false_value){


    for(int i = 0; i < ITERATIONS; i++){
        sfpi::vFloat cond = sfpi::dst_reg[0];
        v_if (cond != 0.0f)
        {
            sfpi::dst_reg[0] =  true_value;
        }
        v_else
        {
            sfpi::dst_reg[0] =  false_value;
        }
        v_endif;
        sfpi::dst_reg++;
    }

}


}
}