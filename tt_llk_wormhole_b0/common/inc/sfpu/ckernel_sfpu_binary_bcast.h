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

constexpr std::uint32_t DEST_TILE_SIZE   = 32; // raw DEST rows per tile
constexpr std::uint32_t FACE_DEST_ROWS   = 16; // raw DEST rows per face
constexpr std::uint32_t ROWS_PER_SFPLOAD = 4;  // face-rows loaded per SFPLOAD

// SFPU DEST address mapping for SFPLOAD/SFPSTORE:
//   LREG ↔ face/parity is determined by which LREG and which address offset:
//     addr + 0  → face 0/2 even cols (LREG0)
//     addr + 2  → face 0/2 odd  cols (LREG1)
//     addr + 16 → face 1/3 even cols (LREG2)
//     addr + 18 → face 1/3 odd  cols (LREG3)
//
// SFPLOAD at address A loads 4 consecutive face-rows [A, A+1, A+2, A+3]
// into one LREG.  We need row-0 only, which sits in row-slot 0 of the
// 4-row group at address 0.
//
// SFPTRANSP transposes the row-dimension across LREG0-3:
//   before: LREG-j row-k = data[j][k]
//   after:  LREG-j row-k = data[k][j]
//
// Strategy: load the SAME 4-row group (addr 0, containing row-0) from one
// face/parity variant into ALL 4 LREGs.  After SFPTRANSP, LREG0 has
// row-0 replicated across all 4 row-slots.  Then store LREG0 to every
// row-group of that same face/parity variant in the destination.
// Repeat for each of the 4 face/parity variants.

inline void _replicate_row0_single_variant_(const std::uint32_t src_addr, const std::uint32_t dst_addr)
{
    TT_SFPLOAD(p_sfpu::LREG0, InstrModLoadStore::FP32, ADDR_MOD_3, src_addr);
    TT_SFPLOAD(p_sfpu::LREG1, InstrModLoadStore::FP32, ADDR_MOD_3, src_addr);
    TT_SFPLOAD(p_sfpu::LREG2, InstrModLoadStore::FP32, ADDR_MOD_3, src_addr);
    TT_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::FP32, ADDR_MOD_3, src_addr);

    TTI_SFPTRANSP(0, 0, 0, 0);

    for (std::uint32_t rg = 0; rg < FACE_DEST_ROWS; rg += ROWS_PER_SFPLOAD)
    {
        TT_SFPSTORE(p_sfpu::LREG0, InstrModLoadStore::FP32, ADDR_MOD_3, dst_addr + rg);
    }
}

inline void _prepare_bcast_row_tile_(const std::uint32_t src_tile_base, const std::uint32_t dst_tile_base)
{
    // Tile row-0 in row-major space spans columns 0-31.
    // In face layout: face-0 row-0 covers cols 0-15, face-1 row-0 covers cols 16-31.
    //
    // Each face/parity variant of row-0 must be replicated to ALL row-groups
    // of the same variant across all 4 destination faces.
    //
    // Source addresses for the 4 variants of tile row-0:
    //   face-0 even cols: src_tile_base + 0
    //   face-0 odd  cols: src_tile_base + 2
    //   face-1 even cols: src_tile_base + 16
    //   face-1 odd  cols: src_tile_base + 18
    //
    // Destination layout (4 faces):
    //   Face 0: dst_tile_base + 0  .. dst_tile_base + 15   (cols 0-15,  rows 0-15)
    //   Face 1: dst_tile_base + 16 .. dst_tile_base + 31   (cols 16-31, rows 0-15)
    //   Face 2: dst_tile_base + 32 .. dst_tile_base + 47   (cols 0-15,  rows 16-31)
    //   Face 3: dst_tile_base + 48 .. dst_tile_base + 63   (cols 16-31, rows 16-31)
    //
    // face-0 even → dst faces 0,2 even;  face-0 odd → dst faces 0,2 odd
    // face-1 even → dst faces 1,3 even;  face-1 odd → dst faces 1,3 odd

    constexpr std::uint32_t offsets[4] = {0, 2, FACE_DEST_ROWS, FACE_DEST_ROWS + 2};

    for (std::uint32_t v = 0; v < 4; v++)
    {
        const std::uint32_t src_addr = src_tile_base + offsets[v];

        _replicate_row0_single_variant_(src_addr, dst_tile_base + offsets[v]);
        _replicate_row0_single_variant_(src_addr, dst_tile_base + 2 * FACE_DEST_ROWS + offsets[v]);
    }
}

// ── core per-face kernel ───────────────────────────────────────────────

template <bool APPROXIMATION_MODE, BinaryOp BINOP, SfpuBcastDim BCAST_DIM, int ITERATIONS = 8>
inline void _calculate_sfpu_binary_bcast_(const std::uint32_t dst_index_data, const std::uint32_t dst_index_bcast, const std::uint32_t dst_index_out)
{
    constexpr std::uint32_t dst_tile_size_sfpi = 32;

    if constexpr (BCAST_DIM == SfpuBcastDim::BCAST_ROW)
    {
        // After _prepare_bcast_row_tile_, every dst_reg row of the bcast
        // tile already contains the replicated row-0 values.  A simple
        // element-wise loop is sufficient.
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
    else // BCAST_COL
    {
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

    std::uint32_t bcast_idx = dst_index_bcast;

    if constexpr (BCAST_DIM == SfpuBcastDim::BCAST_ROW)
    {
        const std::uint32_t max_idx     = (dst_index_data > dst_index_bcast) ? dst_index_data : dst_index_bcast;
        const std::uint32_t scratch_idx = max_idx + 1;

        _prepare_bcast_row_tile_(dst_index_bcast * DEST_TILE_SIZE, scratch_idx * DEST_TILE_SIZE);
        TTI_SETRWC(p_setrwc::CLR_NONE, 0, 0, 0, 0, p_setrwc::SET_D);

        bcast_idx = scratch_idx;
    }

    for (std::uint32_t face = 0; face < NUM_FACES; face++)
    {
        _calculate_sfpu_binary_bcast_<APPROXIMATION_MODE, BINOP, BCAST_DIM, ITERATIONS_PER_FACE>(dst_index_data, bcast_idx, dst_index_out);

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
        .set(ADDR_MOD_7);
}

} // namespace sfpu
} // namespace ckernel
