// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#define FUSED_TEST
#include <cstdint>

#include "ckernel.h"
#include "ckernel_debug.h"
#include "ckernel_defs.h"
#include "ckernel_sfpu.h"
#include "llk_defs.h"
#include "operand.h"
#include "tensix_types.h"

std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

#define UNUSED __attribute__((unused))

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB.h"
#include "llk_unpack_AB_reduce.h"
#include "llk_unpack_common.h"
#include "llk_unpack_tilize.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
    for (std::uint32_t batch = 0; batch < 33; ++batch)
    {
        constexpr std::uint32_t REPLAY_BUF_LEN = 2;

        load_replay_buf(
            0,
            REPLAY_BUF_LEN,
            []
            {
                TTI_NOP;
                TTI_NOP;
            });
        cfg_reg_rmw_tensix<SCRATCH_SEC0_val_ADDR32, 1, 1>(0);
    }
    TTI_NOP;
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_reduce.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "perf.h"

void run_kernel(const volatile struct RuntimeParams* params)
{
}

#endif
