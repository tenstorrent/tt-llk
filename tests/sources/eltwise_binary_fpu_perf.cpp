// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "ckernel_defs.h"
#include "llk_defs.h"
#include "params.h"
#include "perf.h"
#include "profiler.h"
#include "tensor_shape.h"

// Globals
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

using namespace ckernel;

static constexpr std::uint32_t MAX_TILES_DEST = is_fp32_dest_acc_en ? 4 : 8;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
    const std::uint8_t face_r_dim           = static_cast<std::uint8_t>(params->TEST_FACE_R_DIM);
    const std::uint8_t face_c_dim           = static_cast<std::uint8_t>(params->TEST_FACE_C_DIM);
    const std::uint8_t num_faces_r_dim      = static_cast<std::uint8_t>(params->num_faces_r_dim_A);
    const std::uint8_t num_faces_c_dim      = static_cast<std::uint8_t>(params->num_faces_c_dim_A);
    const ckernel::TensorShape tensor_shape = {face_r_dim, face_c_dim, num_faces_r_dim, num_faces_c_dim};
    const std::uint32_t num_faces           = tensor_shape.total_num_faces();

    {
        ZONE_SCOPED("INIT")
        _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
            formats.unpack_A_src,
            formats.unpack_B_src,
            formats.unpack_A_dst,
            formats.unpack_B_dst,
            tensor_shape.face_r_dim,
            tensor_shape.face_r_dim,
            num_faces,
            num_faces,
            TILE_SIZE_UNPACK_A,
            TILE_SIZE_UNPACK_B);
        _llk_unpack_AB_init_<>(tensor_shape);
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE)
        {
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            return _perf_unpack_loop_set_valid<true, true>(params->TILE_CNT * num_faces);
        }
        else
        {
            for (std::uint32_t tile = 0; tile < params->TILE_CNT; tile++)
            {
                _llk_unpack_AB_<>(PERF_ADDRESS(PERF_INPUT_A, tile), PERF_ADDRESS(PERF_INPUT_B, tile));
            }
        }
        PROFILER_SYNC();
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
    const std::uint8_t face_r_dim           = static_cast<std::uint8_t>(params->TEST_FACE_R_DIM);
    const std::uint8_t face_c_dim           = static_cast<std::uint8_t>(params->TEST_FACE_C_DIM);
    const std::uint8_t num_faces_r_dim      = static_cast<std::uint8_t>(params->num_faces_r_dim_A);
    const std::uint8_t num_faces_c_dim      = static_cast<std::uint8_t>(params->num_faces_c_dim_A);
    const ckernel::TensorShape tensor_shape = {face_r_dim, face_c_dim, num_faces_r_dim, num_faces_c_dim};
    const std::uint32_t num_faces           = tensor_shape.total_num_faces();

    {
        ZONE_SCOPED("INIT")
        _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
        _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);
        _llk_math_eltwise_binary_init_<ELTWISE_BINARY_OP, BroadcastType::NONE>(tensor_shape, 0 /* acc_to_dest */);
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE)
        {
            return;
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE || PERF_RUN_TYPE == PerfRunType::L1_CONGESTION)
        {
            return _perf_math_loop_clear_valid<true, true>(params->TILE_CNT * num_faces);
        }
        else if constexpr (PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            for (std::uint32_t block_start = 0; block_start < params->TILE_CNT; block_start += MAX_TILES_DEST)
            {
                std::uint32_t block_tiles = std::min(params->TILE_CNT - block_start, MAX_TILES_DEST);

                for (std::uint32_t block_tile = 0; block_tile < block_tiles; block_tile++)
                {
                    _llk_math_eltwise_binary_<ELTWISE_BINARY_OP, BroadcastType::NONE, DstSync::SyncHalf, is_fp32_dest_acc_en>(tensor_shape, block_tile, false);
                }
            }
        }
        else
        {
            for (std::uint32_t block_start = 0; block_start < params->TILE_CNT; block_start += MAX_TILES_DEST)
            {
                std::uint32_t block_tiles = std::min(params->TILE_CNT - block_start, MAX_TILES_DEST);

                _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
                for (std::uint32_t block_tile = 0; block_tile < block_tiles; block_tile++)
                {
                    _llk_math_eltwise_binary_<ELTWISE_BINARY_OP, BroadcastType::NONE, DstSync::SyncHalf, is_fp32_dest_acc_en>(tensor_shape, block_tile, false);
                }
                _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            }
        }
        PROFILER_SYNC();
    }
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
    const std::uint8_t face_r_dim           = static_cast<std::uint8_t>(params->TEST_FACE_R_DIM);
    const std::uint8_t face_c_dim           = static_cast<std::uint8_t>(params->TEST_FACE_C_DIM);
    const std::uint8_t num_faces_r_dim      = static_cast<std::uint8_t>(params->num_faces_r_dim_A);
    const std::uint8_t num_faces_c_dim      = static_cast<std::uint8_t>(params->num_faces_c_dim_A);
    const ckernel::TensorShape tensor_shape = {face_r_dim, face_c_dim, num_faces_r_dim, num_faces_c_dim};

    const std::uint32_t tile_size = tensor_shape.total_tensor_size();
    const std::uint32_t num_faces = tensor_shape.total_num_faces();
    const bool partial_face       = tensor_shape.face_r_dim < FACE_R_DIM;
    const bool narrow_tile        = (tensor_shape.num_faces_c_dim == 1);

    {
        ZONE_SCOPED("INIT")
#ifdef ARCH_BLACKHOLE
        _llk_pack_hw_configure_<is_fp32_dest_acc_en, false /* untilize */, false /* tilize */>(
            formats.pack_src,
            formats.pack_dst,
            tile_size,
            tensor_shape.face_r_dim,
            tensor_shape.total_col_dim(),
            num_faces,
            partial_face,
            false /* narrow_tile */);
#else
        _llk_pack_hw_configure_<is_fp32_dest_acc_en, false /* untilize */>(
            formats.pack_src, formats.pack_dst, tile_size, tensor_shape.face_r_dim, num_faces, partial_face, narrow_tile);
#endif

#ifdef ARCH_BLACKHOLE
        _llk_pack_init_<false /* untilize */, false /* zero_output */>(formats.pack_dst, tensor_shape.face_r_dim, tensor_shape.total_col_dim(), num_faces);
#else
        _llk_pack_init_<false /* untilize */, false /* zero_output */>(formats.pack_dst, tensor_shape.face_r_dim, num_faces, partial_face, narrow_tile);
#endif

#ifdef ARCH_BLACKHOLE
        _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
#else
        _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, false /* untilize */>(tensor_shape.face_r_dim, narrow_tile);
#endif
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        if constexpr (PERF_RUN_TYPE == PerfRunType::UNPACK_ISOLATE || PERF_RUN_TYPE == PerfRunType::MATH_ISOLATE)
        {
            return;
        }
        if constexpr (PERF_RUN_TYPE == PerfRunType::PACK_ISOLATE || PERF_RUN_TYPE == PerfRunType::L1_CONGESTION)
        {
            for (std::uint32_t block_start = 0; block_start < params->TILE_CNT; block_start += MAX_TILES_DEST)
            {
                std::uint32_t block_tiles = std::min(params->TILE_CNT - block_start, MAX_TILES_DEST);

                for (std::uint32_t block_tile = 0; block_tile < block_tiles; block_tile++)
                {
                    _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en>(block_tile, PERF_ADDRESS(PERF_OUTPUT, block_start + block_tile));
                }
            }
        }
        else
        {
            for (std::uint32_t block_start = 0; block_start < params->TILE_CNT; block_start += MAX_TILES_DEST)
            {
                std::uint32_t block_tiles = std::min(params->TILE_CNT - block_start, MAX_TILES_DEST);

                _llk_packer_wait_for_math_done_();
                for (std::uint32_t block_tile = 0; block_tile < block_tiles; block_tile++)
                {
                    _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en>(block_tile, PERF_ADDRESS(PERF_OUTPUT, block_start + block_tile));
                }
                _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            }
        }
        PROFILER_SYNC();
    }
}

#endif
