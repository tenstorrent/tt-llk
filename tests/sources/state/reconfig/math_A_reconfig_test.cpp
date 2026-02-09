
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
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

void run_kernel(const volatile struct RuntimeParams *params)
{
    (void)params; // ... unused variable warning ...

    llk_t6_dump::wait();

    llk_t6_dump::wait();
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    (void)params; // ... unused variable warning ...

    _llk_math_hw_configure_<is_fp32_dest_acc_en>(
        /* srca_data_format */ (std::uint32_t)formats.pack_src,
        /* srcb_data_format */ (std::uint32_t)formats.unpack_dst);

    llk_t6_dump::wait();

    _llk_math_hw_configure_<is_fp32_dest_acc_en>(
        /* srca_data_format */ (std::uint32_t)formats.unpack_src,
        /* srcb_data_format */ (std::uint32_t)formats.unpack_dst);

    _llk_math_reconfig_data_format_srca_<is_fp32_dest_acc_en, TO_FROM_INT8>(
        /* srca_data_format */ (std::uint32_t)formats.pack_src);

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
