// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

// BH full fast-tilize: unpack + math + pack → standard tilized output.
// ct_dim must be divisible by 4, unit_dim=4 always.

#include <cstdint>

#include "ckernel.h"
#include "llk_defs.h"
#include "params.h"
#include "profiler.h"

std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_common.h"
#include "llk_unpack_tilize.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
    const std::uint32_t BLOCK_CT_DIM = params.BLOCK_CT_DIM;
    const std::uint32_t BLOCK_RT_DIM = params.BLOCK_RT_DIM;
    const std::uint32_t LOOP_FACTOR  = params.LOOP_FACTOR;
    const Operand& buffer_A          = params.buffer_A;
    constexpr std::uint32_t unit_dim = 4;

    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats.unpack_A_src, formats.unpack_B_src, formats.unpack_A_dst, formats.unpack_B_dst, FACE_R_DIM, FACE_R_DIM, 4, 4);
    _llk_unpack_fast_tilize_init_(formats.unpack_A_dst, BLOCK_CT_DIM);

    for (std::uint32_t loop = 0; loop < LOOP_FACTOR; loop++)
    {
        for (std::uint32_t i = 0; i < BLOCK_RT_DIM; i++)
        {
            std::uint32_t read_offset = i * BLOCK_CT_DIM * TILE_R_DIM;
            _llk_unpack_fast_tilize_block_(L1_ADDRESS(buffer_A[0]), read_offset, formats.unpack_A_src, unit_dim, BLOCK_CT_DIM / unit_dim, BLOCK_CT_DIM, 4);
        }
    }

    _llk_unpack_fast_tilize_uninit_<is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
    const std::uint32_t BLOCK_CT_DIM = params.BLOCK_CT_DIM;
    const std::uint32_t BLOCK_RT_DIM = params.BLOCK_RT_DIM;
    const std::uint32_t LOOP_FACTOR  = params.LOOP_FACTOR;
    constexpr std::uint32_t unit_dim = 4;

    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats.math, formats.math);
    _llk_math_fast_tilize_init_(formats.math, unit_dim);

    for (std::uint32_t loop = 0; loop < LOOP_FACTOR; loop++)
    {
        std::uint32_t num_units = BLOCK_RT_DIM * (BLOCK_CT_DIM / unit_dim);
        _llk_math_fast_tilize_block_(0, formats.math, unit_dim, num_units, 4);
    }

    _llk_math_fast_tilize_uninit_<is_fp32_dest_acc_en>(formats.math);
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
    const std::uint32_t BLOCK_CT_DIM = params.BLOCK_CT_DIM;
    const std::uint32_t BLOCK_RT_DIM = params.BLOCK_RT_DIM;
    const std::uint32_t LOOP_FACTOR  = params.LOOP_FACTOR;
    const Operand& buffer_Res        = params.buffer_Res;
    constexpr std::uint32_t unit_dim = 4;

    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_pack_hw_configure_<is_fp32_dest_acc_en>(formats.pack_src, formats.pack_dst, SCALE_DATUM_SIZE(formats.pack_dst, TILE_C_DIM * TILE_R_DIM));
    _llk_pack_fast_tilize_init_<DstSync::SyncHalf>(0, formats.pack_dst, unit_dim, 4);

    for (std::uint32_t loop = 0; loop < LOOP_FACTOR; loop++)
    {
        for (std::uint32_t i = 0; i < BLOCK_RT_DIM; i++)
        {
            std::uint32_t num_units = BLOCK_CT_DIM / unit_dim;
            for (std::uint32_t u = 0; u < num_units; u++)
            {
                _llk_packer_wait_for_math_done_();
                std::uint32_t tile_offset = i * BLOCK_CT_DIM + u * unit_dim;
                _llk_pack_fast_tilize_block_(0, L1_ADDRESS(buffer_Res[tile_offset]), unit_dim, 1, 4);
                _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
            }
        }
    }

    _llk_pack_fast_tilize_uninit_<DstSync::SyncHalf, is_fp32_dest_acc_en>(formats.pack_dst, FACE_R_DIM, 4);
}

#endif
