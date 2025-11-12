// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
// AUTO-GENERATED CONFIGURATION HEADER. DO NOT EDIT MANUALLY!

#pragma once

#include <array>
#include <type_traits>

#include "llk_defs.h"
#include "llk_sfpu_types.h"
#include "operand.h"
#include "perf.h"
#include "tensix_types.h"

// Basic configuration
constexpr std::uint32_t TILE_SIZE_CNT       = 0x1000;
constexpr int LOOP_FACTOR                   = 1;
constexpr bool UNPACKING_TO_DEST            = false;
constexpr bool UNPACK_TRANSPOSE_FACES       = false;
constexpr bool UNPACK_TRANSPOSE_WITHIN_FACE = false;
constexpr int THROTTLE_LEVEL                = 0;
constexpr bool MATH_TRANSPOSE_FACES         = false;
constexpr auto STOCHASTIC_RND               = ckernel::StochRndType::None;
constexpr std::uint32_t L1_to_L1_ITERATIONS = 1;
constexpr std::uint32_t MATH_FIDELITY       = 0;
constexpr bool APPROX_MODE                  = false;
constexpr bool PARTIAL_FACE_A               = false;
constexpr bool PARTIAL_FACE_B               = false;
constexpr bool PARTIAL_FACE                 = false;
constexpr bool PARTIAL_FACE_PACK            = false;
constexpr bool PARTIAL_FACE_MATH            = false;
constexpr bool IMPLIED_MATH_FORMAT          = false;
constexpr int num_faces                     = 4;
constexpr int num_faces_A                   = 4;
constexpr int num_faces_B                   = 4;
constexpr int in0_tile_r_dim                = 32;
constexpr int in0_tile_c_dim                = 32;
constexpr int in1_tile_r_dim                = 32;
constexpr int in1_tile_c_dim                = 32;
constexpr int TEST_FACE_R_DIM               = 16;
constexpr int TEST_FACE_C_DIM               = 16;
constexpr std::uint32_t TILE_SIZE_PACK      = 128;
constexpr std::uint32_t TILE_SIZE_UNPACK_A  = 128;
constexpr std::uint32_t TILE_SIZE_UNPACK_B  = 128;
constexpr std::uint32_t TILE_SIZE           = 1024;
constexpr auto dest_sync                    = ckernel::DstSync::SyncHalf;
constexpr int DST_INDEX                     = 0;
constexpr bool tilize_en                    = false;
constexpr int SRCA_REUSE_COUNT              = 4;
// Data formats inferred by Python inference model
constexpr bool dest_acc_en_input = false;
// Format data for single L1-to-L1 iteration
constexpr auto UNPACK_A_IN  = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
constexpr auto UNPACK_A_OUT = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
constexpr auto MATH_FORMAT  = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
constexpr auto PACK_IN      = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
constexpr auto PACK_OUT     = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);

// Multi-tile test configuration
constexpr int TILE_CNT = 1;
constexpr Operand buffer_A(0x1a000, 2048);
constexpr Operand buffer_B(0x1b800, 2048);
constexpr Operand buffer_Res(0x1d000, 2048);
constexpr uint32_t FULL_RT_DIM  = 1;
constexpr uint32_t FULL_CT_DIM  = 1;
constexpr uint32_t BLOCK_CT_DIM = 1;
constexpr uint32_t BLOCK_RT_DIM = 1;
constexpr uint32_t RT_DIM       = 1;
constexpr uint32_t CT_DIM       = 1;
constexpr uint32_t KT_DIM       = 3;
