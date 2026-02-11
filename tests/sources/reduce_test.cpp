// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"
#include "params.h"
#include "tensor_shape.h"

// Globals
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

constexpr std::uint32_t within_face_16x16_transpose = (REDUCE_DIM == ckernel::ReduceDim::REDUCE_ROW) ? 1 : 0;
constexpr bool row_pool                             = (REDUCE_DIM == ckernel::ReduceDim::REDUCE_ROW);

// Default 32x32 tile shape for reduce operations
constexpr ckernel::TensorShape default_tensor_shape = {
    ckernel::MAX_FACE_R_DIM, ckernel::MAX_FACE_C_DIM, ckernel::MAX_NUM_FACES_R_DIM, ckernel::MAX_NUM_FACES_C_DIM};

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    // Get tile size from Operand for BFP format support
    const std::uint32_t tile_size_A = buffer_A.get_tile_size();
    const std::uint32_t tile_size_B = buffer_B.get_tile_size();

    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_src,
        formats.unpack_src,
        formats.unpack_dst,
        formats.unpack_dst,
        default_tensor_shape.face_r_dim,
        default_tensor_shape.face_r_dim,
        default_tensor_shape.total_num_faces(),
        default_tensor_shape.total_num_faces(),
        tile_size_A,
        tile_size_B);

    // For reduce, if reduce dimension is row, we need to transpose within the face
    // Transpose of faces should always be false
    // Calling _llk_unpack_AB_init_ performs both transpose within the face and transpose of faces, because it uses the same argument for both
    // The following four lines are equivalent to calling _llk_unpack_AB_init_, but separates the two types of transpose
    cfg_reg_rmw_tensix<THCON_SEC0_REG2_Haloize_mode_RMW>(within_face_16x16_transpose);
    config_unpacker_x_end<p_setadc::UNP_AB>(default_tensor_shape.face_r_dim);
    _llk_unpack_AB_mop_config_<BroadcastType::NONE>(false /* transpose_of_faces */, default_tensor_shape);

    _llk_unpack_AB_<>(L1_ADDRESS(buffer_A[0]), L1_ADDRESS(buffer_B[0]));
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_reduce.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    const std::uint32_t math_fid         = 4;
    const bool is_int_fpu_en             = false;
    const bool enforce_fp32_accumulation = false;
    _llk_math_pack_sync_init_<DstSync::SyncFull, is_fp32_dest_acc_en>();
    _llk_math_wait_for_dest_available_<DstSync::SyncFull>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);
    _llk_math_reduce_init_<POOL_TYPE, REDUCE_DIM, is_fp32_dest_acc_en, math_fid, enforce_fp32_accumulation>();
    _llk_math_reduce_<POOL_TYPE, REDUCE_DIM, is_fp32_dest_acc_en, math_fid, is_int_fpu_en, enforce_fp32_accumulation>(0);
    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    const std::uint32_t tile_size = default_tensor_shape.total_tensor_size();
    const std::uint32_t num_faces = default_tensor_shape.total_num_faces();
    const bool partial_face       = default_tensor_shape.face_r_dim < FACE_R_DIM;
    const bool narrow_tile        = (default_tensor_shape.num_faces_c_dim == 1);

    _llk_pack_init_<false, false>(formats.pack_dst, default_tensor_shape.face_r_dim, num_faces, partial_face, narrow_tile);

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(
        formats.pack_src,
        formats.pack_dst,
        tile_size,
        default_tensor_shape.face_r_dim,
        default_tensor_shape.total_col_dim(),
        num_faces,
        partial_face,
        narrow_tile);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(
        formats.pack_src, formats.pack_dst, tile_size, default_tensor_shape.face_r_dim, num_faces, partial_face, narrow_tile);
#endif

    _llk_pack_reduce_mask_config_<false, REDUCE_DIM>();

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
#else
    _llk_pack_dest_init_<DstSync::SyncFull, is_fp32_dest_acc_en, false>(default_tensor_shape.face_r_dim, narrow_tile);
#endif

    _llk_packer_wait_for_math_done_();
    _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>(0, L1_ADDRESS(buffer_Res[0]));
    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();

    _llk_pack_reduce_mask_clear_();
}

#endif
