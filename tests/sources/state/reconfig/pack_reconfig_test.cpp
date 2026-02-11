
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

void run_kernel(const volatile struct RuntimeParams *params)
{
    (void)params; // ... unused variable warning ...

    llk_t6_dump::wait();

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

#include "llk_pack.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(
        /* pack_src_format */ P_SRC_FORMAT_NEXT,
        /* pack_dst_format */ P_DST_FORMAT_NEXT,
        /* tile_size */ params->P_TILE_SIZE_NEXT,
        /* face_r_dim */ params->P_FACE_R_DIM_NEXT,
#ifdef ARCH_BLACKHOLE
        /* tile_c_dim */ params->P_TILE_C_DIM_NEXT,
#endif
        /* num_faces */ params->P_NUM_FACES_NEXT,
        /* partial_face */ params->P_PARTIAL_FACE_NEXT,
        /* narrow_tile */ params->P_NARROW_TILE_NEXT,
        /* relu_config */ false);

    llk_t6_dump::wait();

    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(
        /* pack_src_format */ P_SRC_FORMAT,
        /* pack_dst_format */ P_DST_FORMAT,
        /* tile_size */ params->P_TILE_SIZE,
        /* face_r_dim */ params->P_FACE_R_DIM,
#ifdef ARCH_BLACKHOLE
        /* tile_c_dim */ params->P_TILE_C_DIM,
#endif
        /* num_faces */ params->P_NUM_FACES,
        /* partial_face */ params->P_PARTIAL_FACE,
        /* narrow_tile */ params->P_NARROW_TILE,
        /* relu_config */ 0);

    // template <bool is_fp32_dest_acc_en, bool is_tile_dim_reconfig_en = false>
    _llk_pack_reconfig_data_format_<is_fp32_dest_acc_en, true>(
        /* pack_src_format */ P_SRC_FORMAT_NEXT,
        /* pack_dst_format */ P_DST_FORMAT_NEXT,
        /* tile_size */ params->P_TILE_SIZE_NEXT,
        /* face_r_dim */ params->P_FACE_R_DIM_NEXT,
#ifdef ARCH_BLACKHOLE
        /* tile_c_dim */ params->P_TILE_C_DIM_NEXT,
#endif
        /* num_faces */ params->P_NUM_FACES_NEXT,
        /* partial_face */ params->P_PARTIAL_FACE_NEXT,
        /* narrow_tile */ params->P_NARROW_TILE_NEXT);

    llk_t6_dump::wait();
}

#endif
