
// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "dump.h"
#include "llk_defs.h"

// Globals
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

void run_kernel(const volatile struct RuntimeParams *params)
{
    (void)params; // ... unused variable warning ...

    llk::debug::tensix_dump::request();

    llk::debug::tensix_dump::request();
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    (void)params; // ... unused variable warning ...

    const std::uint32_t prev_a = (std::uint32_t)formats.unpack_A_src;
    const std::uint32_t prev_b = (std::uint32_t)formats.unpack_A_dst;
    const std::uint32_t next_a = (std::uint32_t)formats.pack_src;
    const std::uint32_t next_b = (std::uint32_t)formats.pack_dst;

    _llk_math_hw_configure_<is_fp32_dest_acc_en>(
        /* srca_data_format */ next_a,
        /* srcb_data_format */ next_b);

    llk::debug::tensix_dump::request();

    _llk_math_hw_configure_<is_fp32_dest_acc_en>(
        /* srca_data_format */ prev_a,
        /* srcb_data_format */ prev_b);

    if (prev_a != next_a && prev_b != next_b)
    {
        _llk_math_reconfig_data_format_<is_fp32_dest_acc_en, TO_FROM_INT8>(
            /* srca_data_format */ next_a,
            /* srcb_data_format */ next_b);
    }
    else if (prev_a != next_a)
    {
        _llk_math_reconfig_data_format_srca_<is_fp32_dest_acc_en, TO_FROM_INT8>(
            /* srca_data_format */ next_a);
    }
    else if (prev_b != next_b)
    {
        _llk_math_reconfig_data_format_srcb_<is_fp32_dest_acc_en, TO_FROM_INT8>(
            /* srcb_data_format */ next_b);
    }

    llk::debug::tensix_dump::request();
}

#endif

#ifdef LLK_TRISC_PACK

void run_kernel(const volatile struct RuntimeParams *params)
{
    (void)params; // ... unused variable warning ...

    llk::debug::tensix_dump::request();

    llk::debug::tensix_dump::request();
}

#endif
