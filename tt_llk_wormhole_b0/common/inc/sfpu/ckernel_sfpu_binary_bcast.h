// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "ckernel_addrmod.h"
#include "ckernel_instr_params.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

enum class SfpuBcastDim : std::uint8_t
{
    BCAST_COL = 0,
    BCAST_ROW = 1,
};

// Binary elementwise operation with broadcast from a second DST tile.
//
// Column broadcast (BCAST_COL):
//   The broadcast source tile must have each row's column-0 value replicated
//   across all 16 columns.  The operation then applies element-wise:
//     DST[out][r][c] = op(DST[data][r][c], DST[bcast][r][c])
//   which is equivalent to broadcasting column 0 because every column in the
//   bcast tile holds the same value.
//
// Row broadcast (BCAST_ROW):
//   Row 0 of each face in the broadcast tile is applied to every row of the
//   corresponding face in the data tile.  Because the face iterator resets
//   per face, dst_reg[bcast_offset] at the start of each face always points
//   to face-row-0.

template <bool APPROXIMATION_MODE, BinaryOp BINOP, SfpuBcastDim BCAST_DIM, int ITERATIONS = 8>
inline void _calculate_sfpu_binary_bcast_(const std::uint32_t dst_index_data, const std::uint32_t dst_index_bcast, const std::uint32_t dst_index_out)
{
    constexpr std::uint32_t dst_tile_size_sfpi = 32;

    if constexpr (BCAST_DIM == SfpuBcastDim::BCAST_ROW)
    {
        sfpi::vFloat bcast_row = sfpi::dst_reg[dst_index_bcast * dst_tile_size_sfpi];

        for (int d = 0; d < ITERATIONS; d++)
        {
            sfpi::vFloat data = sfpi::dst_reg[dst_index_data * dst_tile_size_sfpi];
            sfpi::vFloat result;

            if constexpr (BINOP == BinaryOp::ADD)
            {
                result = data + bcast_row;
            }
            else if constexpr (BINOP == BinaryOp::SUB)
            {
                result = data - bcast_row;
            }
            else if constexpr (BINOP == BinaryOp::MUL)
            {
                result = data * bcast_row;
            }

            sfpi::dst_reg[dst_index_out * dst_tile_size_sfpi] = result;
            sfpi::dst_reg++;
        }
    }
    else // BCAST_COL
    {
        // For column broadcast, the bcast tile is expected to have column 0
        // replicated across all columns.  This allows a simple element-wise
        // operation that is semantically equivalent to column broadcast.
        for (int d = 0; d < ITERATIONS; d++)
        {
            sfpi::vFloat data      = sfpi::dst_reg[dst_index_data * dst_tile_size_sfpi];
            sfpi::vFloat bcast_val = sfpi::dst_reg[dst_index_bcast * dst_tile_size_sfpi];

            sfpi::vFloat result;

            if constexpr (BINOP == BinaryOp::ADD)
            {
                result = data + bcast_val;
            }
            else if constexpr (BINOP == BinaryOp::SUB)
            {
                result = data - bcast_val;
            }
            else if constexpr (BINOP == BinaryOp::MUL)
            {
                result = data * bcast_val;
            }

            sfpi::dst_reg[dst_index_out * dst_tile_size_sfpi] = result;
            sfpi::dst_reg++;
        }
    }
}

// Processes a full tile (4 faces, 32 rows) by calling _calculate_sfpu_binary_bcast_
// in 8-row chunks and advancing the dest read/write counter between faces.
template <bool APPROXIMATION_MODE, BinaryOp BINOP, SfpuBcastDim BCAST_DIM>
inline void _calculate_sfpu_binary_bcast_full_tile_(const std::uint32_t dst_index_data, const std::uint32_t dst_index_bcast, const std::uint32_t dst_index_out)
{
    constexpr std::uint32_t ITERATIONS_PER_FACE = 8;
    constexpr std::uint32_t NUM_FACES           = 4;

    for (std::uint32_t face = 0; face < NUM_FACES; face++)
    {
        _calculate_sfpu_binary_bcast_<APPROXIMATION_MODE, BINOP, BCAST_DIM, ITERATIONS_PER_FACE>(dst_index_data, dst_index_bcast, dst_index_out);

        if (face < NUM_FACES - 1)
        {
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
            TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);
        }
    }
}

template <bool APPROXIMATION_MODE, BinaryOp BINOP, SfpuBcastDim BCAST_DIM>
inline void _sfpu_binary_bcast_init_()
{
}

} // namespace sfpu
} // namespace ckernel
