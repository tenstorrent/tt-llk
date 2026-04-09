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

// DEST tile layout (64 raw rows per tile):
//   Face 0: dest rows  0-15  (tile columns 0-15)
//   Face 1: dest rows 16-31  (tile columns 16-31)
//   Face 2: dest rows 32-47  (tile columns 0-15)
//   Face 3: dest rows 48-63  (tile columns 16-31)
//
// SFPLOAD at address A loads 4 face-rows into one LREG.  Even columns
// at offset +0, odd columns at offset +2.  Each column-slice of the
// LREG receives one scalar per face-row (4 scalars total).
//
// Row broadcast (BCAST_ROW):
//   Tile row 0 spans face-0 row-0 (cols 0-15) and face-1 row-0 (cols 16-31).
//   That single row is replicated to every row of the tile.
//   Implementation: SFPLOAD row-0 into all 4 LREGs, SFPTRANSP to isolate
//   the row-0 scalar into every row-slot, then SFPSTORE the replicated
//   value across all row-groups of every face in the broadcast tile.
//   After replication the straightforward sfpi::dst_reg loop works.
//
// Column broadcast (BCAST_COL):
//   Each row of the broadcast face is read and applied element-wise against
//   the corresponding data row.  The bcast value advances row-by-row together
//   with the data tile (dst_reg++ in the loop).

// ── helpers for BCAST_ROW ──────────────────────────────────────────────

constexpr std::uint32_t DEST_TILE_SIZE   = 64; // raw DEST rows per tile
constexpr std::uint32_t FACE_DEST_ROWS   = 64; // raw DEST rows per face
constexpr std::uint32_t ROWS_PER_SFPLOAD = 4;  // face-rows loaded per SFPLOAD

inline void _replicate_row0_single_variant_(const std::uint32_t src_addr, const std::uint32_t dst_addr)
{
    TT_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::FP32, ADDR_MOD_3, src_addr * DEST_TILE_SIZE);
    TT_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::FP32, ADDR_MOD_3, src_addr * DEST_TILE_SIZE + 4);
    TT_SFPLOAD(p_sfpu::LREG2, InstrModLoadStore::FP32, ADDR_MOD_3, src_addr * DEST_TILE_SIZE + 8);
    TT_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::FP32, ADDR_MOD_3, src_addr * DEST_TILE_SIZE + 12);

    TTI_SFPTRANSP(0, 0, 0, 0);

    TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP32, ADDR_MOD_3, dst_addr * DEST_TILE_SIZE);
    TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP32, ADDR_MOD_3, dst_addr * DEST_TILE_SIZE + 4);
    TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP32, ADDR_MOD_3, dst_addr * DEST_TILE_SIZE + 8);
    TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP32, ADDR_MOD_3, dst_addr * DEST_TILE_SIZE + 12);
    // TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP32, ADDR_MOD_3, dst_addr * DEST_TILE_SIZE + 8);
    // TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP32, ADDR_MOD_3, dst_addr * DEST_TILE_SIZE + 10);
    // TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP32, ADDR_MOD_3, dst_addr * DEST_TILE_SIZE + 12);
    // TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP32, ADDR_MOD_3, dst_addr * DEST_TILE_SIZE + 14);
}

inline void _prepare_bcast_row_tile_(const std::uint32_t src_tile_base, const std::uint32_t dst_tile_base)
{
    _replicate_row0_single_variant_(src_tile_base, dst_tile_base);
}

// Processes a full tile (4 faces, 32 rows) by calling _calculate_sfpu_binary_bcast_
// in 8-row chunks and advancing the dest read/write counter between faces.
template <bool APPROXIMATION_MODE, BinaryOp BINOP, SfpuBcastDim BCAST_DIM>
inline void _calculate_sfpu_binary_bcast_full_tile_(const std::uint32_t dst_index_data, const std::uint32_t dst_index_bcast, const std::uint32_t dst_index_out)
{
    constexpr std::uint32_t NUM_FACES = 4;

    if constexpr (BCAST_DIM == SfpuBcastDim::BCAST_ROW)
    {
        const std::uint32_t max_idx     = (dst_index_data > dst_index_bcast) ? dst_index_data : dst_index_bcast;
        const std::uint32_t scratch_idx = max_idx + 1;

        _prepare_bcast_row_tile_(dst_index_bcast, scratch_idx);
        TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);

        if constexpr (BINOP == BinaryOp::ADD)
        {
            for (int d = 0; d < 32; d++)
            {
                sfpi::vFloat data      = sfpi::dst_reg[dst_index_data * DEST_TILE_SIZE];
                sfpi::vFloat bcast_val = sfpi::dst_reg[dst_index_bcast * DEST_TILE_SIZE];
                sfpi::vFloat result;
                result                                        = data + bcast_val;
                sfpi::dst_reg[dst_index_out * DEST_TILE_SIZE] = result;
                sfpi::dst_reg++;
            }
        }
    }

    for (std::uint32_t face = 0; face < NUM_FACES; face++)
    {
        // _calculate_sfpu_binary_bcast_<APPROXIMATION_MODE, BINOP, BCAST_DIM, ITERATIONS_PER_FACE>(dst_index_data, bcast_idx, dst_index_out);

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
    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_3);

    addr_mod_t {
        .srca = {.incr = 0},
        .srcb = {.incr = 0},
        .dest = {.incr = 0},
    }
        .set(ADDR_MOD_7);
}

} // namespace sfpu
} // namespace ckernel
