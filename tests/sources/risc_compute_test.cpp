// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"
#include "params.h"
#include "profiler.h"

// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

void run_kernel()
{
    int32_t* A = (int32_t*)buffer_A[0];
    int32_t* B = (int32_t*)buffer_B[0];
    int32_t* C = (int32_t*)buffer_Res[0];

    for (int i = 0; i < 1024; i++)
    {
        C[i] = A[i] + B[i];
    }
}

#endif

#ifdef LLK_TRISC_MATH

void run_kernel()
{
    int32_t* A = (int32_t*)buffer_A[1];
    int32_t* B = (int32_t*)buffer_B[1];
    int32_t* C = (int32_t*)buffer_Res[1];

    for (int i = 0; i < 1024; i++)
    {
        C[i] = A[i] + B[i];
    }
}

#endif

#ifdef LLK_TRISC_PACK

void run_kernel()
{
    int32_t* A = (int32_t*)buffer_A[2];
    int32_t* B = (int32_t*)buffer_B[2];
    int32_t* C = (int32_t*)buffer_Res[2];

    for (int i = 0; i < 1024; i++)
    {
        C[i] = A[i] + B[i];
    }
}

#endif
