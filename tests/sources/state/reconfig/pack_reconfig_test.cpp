
// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "dump.h"
#include "llk_defs.h"
#include "params.h"

// Globals
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

void run_kernel(RUNTIME_PARAMETERS params)
{
    (void)params;
    llk::debug::tensix_dump::request();
}

#endif

#ifdef LLK_TRISC_MATH

void run_kernel(RUNTIME_PARAMETERS params)
{
    (void)params;
    llk::debug::tensix_dump::request();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
    const std::uint32_t prev_src = P_SRC_FORMAT;
    const std::uint32_t prev_dst = P_DST_FORMAT;
    const std::uint32_t next_src = P_SRC_FORMAT_NEXT;
    const std::uint32_t next_dst = P_DST_FORMAT_NEXT;

    if (params.CONFIGURE_TEST_RUN_IDX == 0)
    {
        _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(
            /* pack_src_format */ next_src,
            /* pack_dst_format */ next_dst,
            /* tile_size */ static_cast<std::uint32_t>(params.P_TILE_SIZE_NEXT),
            /* face_r_dim */ static_cast<std::uint32_t>(params.P_FACE_R_DIM_NEXT),
#ifdef ARCH_BLACKHOLE
            /* tile_c_dim */ static_cast<std::uint32_t>(params.P_TILE_C_DIM_NEXT),
#endif
            /* num_faces */ static_cast<std::uint32_t>(params.P_NUM_FACES_NEXT),
            /* partial_face */ static_cast<bool>(params.P_PARTIAL_FACE_NEXT),
            /* narrow_tile */ static_cast<bool>(params.P_NARROW_TILE_NEXT),
            /* relu_config */ 0);
    }
    else
    {
        _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(
            /* pack_src_format */ prev_src,
            /* pack_dst_format */ prev_dst,
            /* tile_size */ static_cast<std::uint32_t>(params.P_TILE_SIZE),
            /* face_r_dim */ static_cast<std::uint32_t>(params.P_FACE_R_DIM),
#ifdef ARCH_BLACKHOLE
            /* tile_c_dim */ static_cast<std::uint32_t>(params.P_TILE_C_DIM),
#endif
            /* num_faces */ static_cast<std::uint32_t>(params.P_NUM_FACES),
            /* partial_face */ static_cast<bool>(params.P_PARTIAL_FACE),
            /* narrow_tile */ static_cast<bool>(params.P_NARROW_TILE),
            /* relu_config */ 0);

        if (prev_src != next_src || prev_dst != next_dst)
        {
            _llk_pack_reconfig_data_format_<is_fp32_dest_acc_en, true>(
                /* pack_src_format */ next_src,
                /* pack_dst_format */ next_dst,
                /* tile_size */ static_cast<std::uint32_t>(params.P_TILE_SIZE_NEXT),
                /* face_r_dim */ static_cast<std::uint32_t>(params.P_FACE_R_DIM_NEXT),
#ifdef ARCH_BLACKHOLE
                /* tile_c_dim */ static_cast<std::uint32_t>(params.P_TILE_C_DIM_NEXT),
#endif
                /* num_faces */ static_cast<std::uint32_t>(params.P_NUM_FACES_NEXT),
                /* partial_face */ static_cast<bool>(params.P_PARTIAL_FACE_NEXT),
                /* narrow_tile */ static_cast<bool>(params.P_NARROW_TILE_NEXT));
        }
    }

    llk::debug::tensix_dump::request();
}

#endif
