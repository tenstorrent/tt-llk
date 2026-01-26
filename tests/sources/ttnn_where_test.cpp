// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>

#include "ckernel.h"
#include "ckernel_debug.h"
#include "llk_defs.h"

// Globals
uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    const int num_tiles_in_block = params->NUM_TILES_IN_BLOCK;
    const int num_blocks         = params->NUM_BLOCKS;

    using DataFormatUT = std::underlying_type_t<DataFormat>;
    auto to_ufmt       = [](DataFormat fmt) constexpr { return static_cast<DataFormatUT>(fmt); };

    uint8_t UNPACK_FMT;
    if (UNPACK_A_IN == to_ufmt(DataFormat::Float32))
    {
        UNPACK_FMT = to_ufmt(DataFormat::Float32);
    }
    else if (UNPACK_A_IN == to_ufmt(DataFormat::Bfp8_b))
    {
        UNPACK_FMT = to_ufmt(DataFormat::Bfp8_b);
    }
    else if (UNPACK_A_IN == to_ufmt(DataFormat::Int32))
    {
        UNPACK_FMT = to_ufmt(DataFormat::Int32);
    }
    else
    {
        UNPACK_FMT = to_ufmt(DataFormat::UInt16);
    }

    _llk_unpack_hw_configure_<is_fp32_dest_acc_en, disable_src_zero_flag>(
        UNPACK_FMT, UNPACK_FMT, UNPACK_FMT, UNPACK_FMT, FACE_R_DIM, FACE_R_DIM, 4 /* num_faces */, 4 /* num_faces */);
    _llk_unpack_A_init_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(0, 0, FACE_R_DIM, 4, UNPACK_FMT, UNPACK_FMT);
    
    for (int i = 0; i < num_tiles_in_block * num_blocks; ++i)
    {
        _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(L1_ADDRESS(buffer_A[i]), UNPACK_FMT, UNPACK_FMT);  // condition
        _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(L1_ADDRESS(buffer_B[i]), UNPACK_FMT, UNPACK_FMT);  // true_value
        _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(L1_ADDRESS(buffer_C[i]), UNPACK_FMT, UNPACK_FMT);  // false_value
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "ckernel_sfpu.h"
#include "ckernel_sfpu_where.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_ternary_sfpu.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "llk_math_eltwise_unary_sfpu.h"
#include "params.h"

using namespace ckernel;

// using namespace sfpu;

void run_kernel(const volatile struct RuntimeParams *params)
{
    const int num_tiles_in_block = params->NUM_TILES_IN_BLOCK;
    const int num_blocks         = params->NUM_BLOCKS;

    using DataFormatUT = std::underlying_type_t<DataFormat>;
    auto to_ufmt       = [](DataFormat fmt) constexpr { return static_cast<DataFormatUT>(fmt); };

    uint8_t MATH_FMT;
    if (UNPACK_A_IN == to_ufmt(DataFormat::Float32))
    {
        MATH_FMT = to_ufmt(DataFormat::Float32);
    }
    else if (UNPACK_A_IN == to_ufmt(DataFormat::Bfp8_b))
    {
        MATH_FMT = to_ufmt(DataFormat::Bfp8_b);
    }
    else if (UNPACK_A_IN == to_ufmt(DataFormat::Int32))
    {
        MATH_FMT = to_ufmt(DataFormat::Int32);
    }
    else
    {
        MATH_FMT = to_ufmt(DataFormat::UInt16);
    }

    // copy srca to dest
#ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false, false>(4, MATH_FMT);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false>(4, MATH_FMT);
#endif
    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(MATH_FMT, MATH_FMT);

    // calculation of sfpu operation on dest
    _llk_math_eltwise_ternary_sfpu_init_<SfpuType::where>();
    ckernel::sfpu::_init_where_<false>();

    constexpr int iterations = 32;

    for (int block = 0; block < num_blocks; block++)
    {
        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
        for (int tile = 0; tile < num_tiles_in_block; tile++)
        {
            int tile_index = (block * num_tiles_in_block) + tile;
            int dst_index = tile * 3;  // Each tile uses 3 dest registers (condition, true, false)

            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                dst_index + 0, MATH_FMT, MATH_FMT); // buffer condition
            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                dst_index + 1, MATH_FMT, MATH_FMT); // buffer true
            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                dst_index + 2, MATH_FMT, MATH_FMT); // buffer false

            _llk_math_eltwise_ternary_sfpu_start_<DstSync::SyncHalf>(dst_index);

            ckernel::sfpu::_calculate_where_<false, static_cast<DataFormat>(UNPACK_A_IN), iterations>(dst_index + 0, dst_index + 1, dst_index + 2, dst_index + 0);

            _llk_math_eltwise_ternary_sfpu_done_();
        }
        _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel(const volatile struct RuntimeParams *params)
{
    const int num_tiles_in_block = params->NUM_TILES_IN_BLOCK;
    const int num_blocks         = params->NUM_BLOCKS;

    using DataFormatUT = std::underlying_type_t<DataFormat>;
    auto to_ufmt       = [](DataFormat fmt) constexpr { return static_cast<DataFormatUT>(fmt); };

    std::uint8_t PACK_FMT;
    if (UNPACK_A_IN == to_ufmt(DataFormat::Float32))
    {
        PACK_FMT = to_ufmt(DataFormat::Float32);
    }
    else if (UNPACK_A_IN == to_ufmt(DataFormat::Bfp8_b))
    {
        PACK_FMT = to_ufmt(DataFormat::Bfp8_b);
    }
    else if (UNPACK_A_IN == to_ufmt(DataFormat::Int32))
    {
        PACK_FMT = to_ufmt(DataFormat::Int32);
    }
    else
    {
        PACK_FMT = to_ufmt(DataFormat::UInt16);
    }

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(PACK_FMT, PACK_FMT, 16 * 16 * 4);
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(PACK_FMT, PACK_FMT, 16 * 16 * 4);
#endif

    _llk_pack_init_<false, false>(PACK_FMT);

#ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
#else
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, false>();
#endif

    for (int block = 0; block < num_blocks; block++)
    {
        _llk_packer_wait_for_math_done_();
        for (int tile = 0; tile < num_tiles_in_block; tile++)
        {
            int res_tile_idx = (block * num_tiles_in_block) + tile;
            int dst_index = tile * 3;  // Each tile uses 3 dest registers (condition, true, false)
            _llk_pack_<DstSync::SyncHalf, false, is_fp32_dest_acc_en>(dst_index, L1_ADDRESS(buffer_Res[res_tile_idx]));
        }
        _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }
}

#endif
