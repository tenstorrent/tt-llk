
// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

/* This test is a generic elementwise binary test, with no broadcast, no transpose
   It can test different tile dimensions, and also different number of tiles/block of tiles*/
#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"
#include "tensor_shape.h"

// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    // Cache volatile values to local variables first
    const uint8_t face_r_dim                = static_cast<uint8_t>(params->TEST_FACE_R_DIM);
    const uint8_t face_c_dim                = static_cast<uint8_t>(params->TEST_FACE_C_DIM);
    const uint8_t num_faces_r_dim           = static_cast<uint8_t>(params->num_faces_r_dim_A);
    const uint8_t num_faces_c_dim           = static_cast<uint8_t>(params->num_faces_c_dim_A);
    const ckernel::TensorShape tensor_shape = {face_r_dim, face_c_dim, num_faces_r_dim, num_faces_c_dim};
    const uint32_t transpose                = params->UNPACK_TRANSPOSE_FACES;
    const int num_tiles_in_block            = params->NUM_TILES_IN_BLOCK;
    const int num_blocks                    = params->NUM_BLOCKS;

    // Configure hardware for unpacking, no broadcast, no transpose
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src,
        formats.unpack_src,
        formats.unpack_dst,
        formats.unpack_dst,
        tensor_shape.face_r_dim,
        tensor_shape.face_r_dim,
        tensor_shape.total_num_faces(),
        tensor_shape.total_num_faces());

    _llk_unpack_AB_init_<BROADCAST_TYPE>(tensor_shape, transpose);

    for (int i = 0; i < num_tiles_in_block * num_blocks; ++i)
    {
        _llk_unpack_AB_<BROADCAST_TYPE>(L1_ADDRESS(buffer_A[i]), L1_ADDRESS(buffer_B[i]));
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "params.h"

using namespace ckernel;

void run_kernel(const volatile struct RuntimeParams *params)
{
    // Cache volatile values to local variables first
    const uint8_t face_r_dim       = static_cast<uint8_t>(params->TEST_FACE_R_DIM);
    const uint8_t face_c_dim       = static_cast<uint8_t>(params->TEST_FACE_C_DIM);
    const uint8_t num_faces_r_dim  = static_cast<uint8_t>(params->num_faces_r_dim_A);
    const uint8_t num_faces_c_dim  = static_cast<uint8_t>(params->num_faces_c_dim_A);
    const TensorShape tensor_shape = {face_r_dim, face_c_dim, num_faces_r_dim, num_faces_c_dim};
    const int num_tiles_in_block   = params->NUM_TILES_IN_BLOCK;
    const int num_blocks           = params->NUM_BLOCKS;

    // Initialize math for element-wise operation
    _llk_math_pack_sync_init_<dest_sync, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);
    _llk_math_eltwise_binary_init_<ELTWISE_BINARY_OP, BROADCAST_TYPE>(tensor_shape, 0);

    // Perform element-wise operation
    for (int block = 0; block < num_blocks; block++)
    {
        _llk_math_wait_for_dest_available_<dest_sync>();
        for (int tile = 0; tile < num_tiles_in_block; tile++)
        {
            _llk_math_eltwise_binary_<ELTWISE_BINARY_OP, BROADCAST_TYPE, dest_sync, is_fp32_dest_acc_en>(
                tensor_shape, tile /* dst_index */, false /* clear_fp32_dst_acc */);
        }
        _llk_math_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
    }
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    // Cache volatile values to local variables first
    const uint8_t face_r_dim                = static_cast<uint8_t>(params->TEST_FACE_R_DIM);
    const uint8_t face_c_dim                = static_cast<uint8_t>(params->TEST_FACE_C_DIM);
    const uint8_t num_faces_r_dim           = static_cast<uint8_t>(params->num_faces_r_dim_A);
    const uint8_t num_faces_c_dim           = static_cast<uint8_t>(params->num_faces_c_dim_A);
    const ckernel::TensorShape tensor_shape = {face_r_dim, face_c_dim, num_faces_r_dim, num_faces_c_dim};
    const int num_tiles_in_block            = params->NUM_TILES_IN_BLOCK;
    const int num_blocks                    = params->NUM_BLOCKS;

    const uint32_t tile_size = tensor_shape.total_tensor_size();

    const uint32_t num_faces = tensor_shape.total_num_faces();
    const bool partial_face  = tensor_shape.face_r_dim < FACE_R_DIM;
    const bool narrow_tile   = (tensor_shape.num_faces_c_dim == 1);

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false /* untilize */, false /* tilize */>(
        formats.pack_src, formats.pack_dst, tile_size, tensor_shape.face_r_dim, tensor_shape.total_col_dim(), num_faces, partial_face, narrow_tile);
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
    _llk_pack_dest_init_<dest_sync, is_fp32_dest_acc_en>();
#else
    _llk_pack_dest_init_<dest_sync, is_fp32_dest_acc_en, false /* untilize */>(tensor_shape.face_r_dim, narrow_tile);
#endif

    for (int block = 0; block < num_blocks; block++)
    {
        // asm volatile("ebreak");
        _llk_packer_wait_for_math_done_();
        for (int tile = 0; tile < num_tiles_in_block; tile++)
        {
            int res_tile_idx = (block * num_tiles_in_block) + tile;
            _llk_pack_<dest_sync, is_fp32_dest_acc_en, false /* untilize */>(tile, L1_ADDRESS(buffer_Res[res_tile_idx]));
        }
        _llk_pack_dest_section_done_<dest_sync, is_fp32_dest_acc_en>();
    }
}

#endif
