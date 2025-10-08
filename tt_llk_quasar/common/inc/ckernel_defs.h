// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "tensix.h"

// #include "tensix_types.h"
// #include "ckernel_cmd.h"
// #include "ckernel_enum.h"
// #include "ckernel_ops.h"

namespace ckernel
{

// Default face dimensions
constexpr static uint32_t FACE_R_DIM = 16;
constexpr static uint32_t FACE_C_DIM = 16;

// Default tile dimensions
constexpr static uint32_t TILE_R_DIM = 32;
constexpr static uint32_t TILE_C_DIM = 32;

// Default number of faces
constexpr static uint32_t NUM_FACES = 4;

enum register_space_e
{
    TDMA_REGS     = 0x0,
    LOCAL_REGS    = 0x1,
    ADDR_COUNTERS = 0x2
};

// This struct contains all the information needed to specify a tile
//  shape, for a default 32x32 tile these are the values:
//  num_faces = 4;
//  face_r_dim = 16;
//  face_c_dim = 16;
//  narrow_tile = 0;
struct TileShape
{
    uint32_t num_faces;
    uint32_t face_r_dim;
    uint32_t face_c_dim;
    bool narrow_tile;
};

// For instructions that address lower/upper 16 bits of a register
#define LO_16(REG) (2 * (REG))
#define HI_16(REG) (2 * (REG) + 1)

constexpr static std::uint32_t GET_L1_HEADERLESS_TILE_SIZE(uint format)
{
    switch (format & 0xF)
    {
        case ((uint8_t)DataFormat::Int32):
        case ((uint8_t)DataFormat::Float32):
            return (4096 >> 4);
        case ((uint8_t)DataFormat::Float16):
        case ((uint8_t)DataFormat::Float16_b):
            return (2048 >> 4);
        case ((uint8_t)DataFormat::Int8):
            return (1024 >> 4);
        default:
            return ((1024 >> 4) + (64 >> 4));
    };
}

} // namespace ckernel
