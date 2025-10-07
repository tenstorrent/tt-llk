// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"
#include "params.h"
#include "unified_operand.hpp"

// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

UnifiedOperand src_a = {
    ckernel::FACE_C_DIM,
    ckernel::FACE_R_DIM,
    2,
    2,
    static_cast<DataFormat>(formats.unpack_src),
    static_cast<DataFormat>(formats.unpack_dst),
    static_cast<DataFormat>(formats.pack_src),
    31,
    KT_DIM,
    RT_DIM,
    L1_ADDRESS(buffer_A[0])};
UnifiedOperand src_b = {
    ckernel::FACE_C_DIM,
    ckernel::FACE_R_DIM,
    2,
    2,
    static_cast<DataFormat>(formats.unpack_src),
    static_cast<DataFormat>(formats.unpack_dst),
    static_cast<DataFormat>(formats.pack_src),
    31,
    CT_DIM,
    KT_DIM,
    L1_ADDRESS(buffer_B[0])};
UnifiedOperand dst = {
    ckernel::FACE_C_DIM,
    ckernel::FACE_R_DIM,
    2,
    2,
    static_cast<DataFormat>(formats.pack_dst),
    static_cast<DataFormat>(formats.unpack_dst),
    static_cast<DataFormat>(formats.pack_src),
    31,
    CT_DIM,
    RT_DIM,
    L1_ADDRESS(buffer_Res[0])};

#ifdef LLK_TRISC_UNPACK

#include "unified_unpack_common.hpp"
#include "unified_unpack_matmul.hpp"

void run_kernel()
{
    _unified_unpack_sync_init_(dest_sync == ckernel::DstSync::SyncFull);

    _unified_unpack_hw_configure_(src_a, src_b, is_fp32_dest_acc_en, STOCHASTIC_RND);

    _unified_unpack_matmul_init_(src_a, src_b);

    for (uint32_t j = 0; j < matmul_kt_dim(src_a, src_b); j++)
    {
        _unified_unpack_matmul_(src_a, src_b, j, j * matmul_ct_dim(src_a, src_b));
    }

    _unified_unpack_matmul_uninit_(src_a, src_b);
}

#endif

#ifdef LLK_TRISC_MATH

#include "unified_math_common.hpp"
#include "unified_math_matmul.hpp"

void run_kernel()
{
    _unified_math_sync_init_(dest_sync == ckernel::DstSync::SyncFull);

    _unified_math_hw_configure_(src_a, src_b, is_fp32_dest_acc_en, STOCHASTIC_RND);

    _unified_math_matmul_init_<THROTTLE_LEVEL>(src_a, src_b, MATH_FIDELITY);

    _unified_math_acquire_dest_();
    for (uint32_t j = 0; j < matmul_kt_dim(src_a, src_b); j++)
    {
        _unified_math_matmul_<THROTTLE_LEVEL>(src_a, src_b, 0);
    }
    _unified_math_release_dest_();

    _unified_math_matmul_uninit_(src_a, src_b);
}

#endif

#ifdef LLK_TRISC_PACK

#include "unified_pack.hpp"
#include "unified_pack_common.hpp"

void run_kernel()
{
    _unified_pack_sync_init_(dest_sync == ckernel::DstSync::SyncFull);

    _unified_pack_hw_configure_(dst, is_fp32_dest_acc_en, STOCHASTIC_RND);

    _unified_pack_init_(dst);

    _unified_pack_acquire_dest_();
    for (int i = 0; i < matmul_ct_dim(src_a, src_b) * matmul_rt_dim(src_a, src_b); i++)
    {
        _unified_pack_(i, dst, i);
    }
    _unified_pack_release_dest_();

    _unified_pack_uninit_(dst);
}

#endif
