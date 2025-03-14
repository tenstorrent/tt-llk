// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ckernel_ops.h"
#include "noc_nonblocking_api.h"
#include "sfpi.h"
#include "sfpi_fp16.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_dropout_(const int iterations, uint prob, uint scale)
{
    // SFPU microcode

    FWLOG1("calculate_dropout() -- prob:%x", prob);
    FWLOG1("calculate_dropout() -- scale:%x", scale);

    sfpi::vUInt rand = sfpi::l_reg[sfpi::LRegs::LReg3];

#pragma GCC unroll 0
    for (int d = 0; d < iterations; d++)
    {
        ////////////////////////
        // Scale samples
        ///////////////////////
        sfpi::dst_reg[0] = sfpi::dst_reg[0] * sfpi::s2vFloat16b(scale);

        ////////////////////////
        // Drop samples
        ///////////////////////
        v_if (rand < prob)
        {
            sfpi::dst_reg[0] = sfpi::vConst0;
        }
        v_endif;

        ////////////////////////
        // 16-bit PRNG update
        ///////////////////////
        sfpi::vUInt lfsr = sfpi::vConstIntPrgm1;
        sfpi::vUInt tmp  = lfsr & rand;
        rand             = rand >> 1;
        v_if (tmp != 0)
        {
            sfpi::vUInt mask = sfpi::vConstIntPrgm0;
            rand ^= mask;
        }
        v_endif;

        sfpi::dst_reg++;
    }

    sfpi::l_reg[sfpi::LRegs::LReg3] = rand;
}

inline void _init_dropout_seed_(uint16_t p2)
{
    FWLOG1("calculate_dropout() -- input seed:%x", p2);

    uint32_t noc_id_reg = NOC_CMD_BUF_READ_REG(0, 0, NOC_NODE_ID);

    uint16_t my_x = noc_id_reg & NOC_NODE_ID_MASK;
    uint16_t my_y = (noc_id_reg >> NOC_ADDR_NODE_ID_BITS) & NOC_NODE_ID_MASK;

    uint16_t per_tensix_input_seed = p2 ^ (my_x << my_y);

    FWLOG1("calculate_dropout() -- calculated seed:%x", per_tensix_input_seed);

    sfpi::vInt result = sfpi::l_reg[sfpi::LRegs::LReg3];

    sfpi::vInt tmp  = sfpi::vConstTileId << 10;
    sfpi::vInt ptis = per_tensix_input_seed;
    result          = ~(tmp & ptis) & (tmp | ptis);

    sfpi::l_reg[sfpi::LRegs::LReg3] = result;
}

inline void _init_dropout_(const uint seed)
{
    sfpi::vConstIntPrgm0 = 0xb400;
    sfpi::vConstIntPrgm1 = 0x1; // binary 0b1 - used to extract LSB

    _init_dropout_seed_(seed);
}

} // namespace sfpu
} // namespace ckernel
