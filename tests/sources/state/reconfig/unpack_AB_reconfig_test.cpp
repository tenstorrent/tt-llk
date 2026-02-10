
// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"
#include "tensix_dump.h"

// Globals
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    (void)params; // ... unused variable warning ...

    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        /* unpA_src_format */ SRC_FORMAT_A_NEXT,
        /* unpB_src_format */ SRC_FORMAT_B_NEXT,
        /* unpA_dst_format */ DST_FORMAT_A_NEXT,
        /* unpB_dst_format */ DST_FORMAT_B_NEXT,
        /* unpA_face_r_dim */ params->FACE_R_DIM_A_NEXT,
        /* unpB_face_r_dim */ params->FACE_R_DIM_B_NEXT,
        /* unpA_num_faces */ params->NUM_FACES_A_NEXT,
        /* unpB_num_faces */ params->NUM_FACES_B_NEXT,
        /* unpA_tile_size */ params->TILE_SIZE_A_NEXT,
        /* unpB_tile_size */ params->TILE_SIZE_B_NEXT);

    llk_t6_dump::wait();

    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        /* unpA_src_format */ SRC_FORMAT_A,
        /* unpB_src_format */ SRC_FORMAT_B,
        /* unpA_dst_format */ DST_FORMAT_A,
        /* unpB_dst_format */ DST_FORMAT_B,
        /* unpA_face_r_dim */ params->FACE_R_DIM_A,
        /* unpB_face_r_dim */ params->FACE_R_DIM_B,
        /* unpA_num_faces */ params->NUM_FACES_A,
        /* unpB_num_faces */ params->NUM_FACES_B,
        /* unpA_tile_size */ params->TILE_SIZE_A,
        /* unpB_tile_size */ params->TILE_SIZE_B);

    _llk_unpack_reconfig_data_format_srca_impl_<is_fp32_dest_acc_en>(
        /* unpack_src_format */ SRC_FORMAT_A_NEXT,
        /* unpack_dst_format */ DST_FORMAT_A_NEXT,
        /* tile_size */ params->TILE_SIZE_A_NEXT,
        /* unpack_face_r_dim */ params->FACE_R_DIM_A_NEXT,
        /* unpack_num_faces */ params->NUM_FACES_A_NEXT);

    _llk_unpack_reconfig_data_format_srcb_impl_<is_fp32_dest_acc_en>(
        /* unpack_src_format */ SRC_FORMAT_B_NEXT,
        /* unpack_dst_format */ DST_FORMAT_B_NEXT,
        /* tile_size */ params->TILE_SIZE_B_NEXT,
        /* unpack_face_r_dim */ params->FACE_R_DIM_B_NEXT,
        /* unpack_num_faces */ params->NUM_FACES_B_NEXT);

    llk_t6_dump::wait();
}

#endif

#ifdef LLK_TRISC_MATH

void run_kernel(const volatile struct RuntimeParams *params)
{
    (void)params; // ... unused variable warning ...

    llk_t6_dump::wait();

    llk_t6_dump::wait();
}

#endif

#ifdef LLK_TRISC_PACK

void run_kernel(const volatile struct RuntimeParams *params)
{
    (void)params; // ... unused variable warning ...

    llk_t6_dump::wait();

    llk_t6_dump::wait();
}

#endif
