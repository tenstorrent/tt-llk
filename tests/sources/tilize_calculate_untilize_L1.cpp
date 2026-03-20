// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "ckernel.h"
#include "llk_defs.h"
#include "tensor_shape.h"

// Globals
std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

using namespace ckernel;

// TODO: CLEANUP

constexpr std::uint32_t buffer_A_tilized = 0x30000; // 192KB from start
constexpr std::uint32_t buffer_B_tilized = 0x50000; // 320KB from start

// Translation of these lines:
// const FormatConfig(&formats_array)[2] = params.formats;
// to English:
// Constant reference to an array of 2 FormatConfig objects

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"
#include "llk_unpack_tilize.h"
#include "params.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#if defined(RUNTIME_FORMATS) && !defined(SPEED_OF_LIGHT)
    const FormatConfig(&formats_array)[2] = params.formats;
#endif

#ifdef ARCH_BLACKHOLE
    const std::uint32_t block_ct_dim = 0;
#else
    const std::uint32_t block_ct_dim = params.BLOCK_CT_DIM;
#endif

    const int num_blocks         = params.NUM_BLOCKS;
    const int num_tiles_in_block = params.NUM_TILES_IN_BLOCK;

    int run = 0; // first L1-to-L1 run, we access the first set of formats_array in our array
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats_array[run].unpack_A_src,
        formats_array[run].unpack_B_src,
        formats_array[run].unpack_A_dst,
        formats_array[run].unpack_B_dst,
        FACE_R_DIM,
        FACE_R_DIM,
        4 /* num_faces */,
        4 /* num_faces */);

    _llk_unpack_tilize_init_(formats_array[run].unpack_A_src, formats_array[run].unpack_A_dst, params.BLOCK_CT_DIM, FACE_R_DIM, false);

    std::uint32_t read_offset = 0;

    for (std::uint32_t i = 0; i < params.BLOCK_RT_DIM; i++)
    {
        for (std::uint32_t j = 0; j < params.BLOCK_CT_DIM; j++)
        {
            _llk_unpack_tilize_(
                L1_ADDRESS(params.buffer_A[read_offset]),
                j,
                formats_array[run].unpack_A_src,
                formats_array[run].unpack_A_dst,
                block_ct_dim,
                FACE_R_DIM,
                4,
                false);
        }
        read_offset += params.BLOCK_CT_DIM;
    }

    _llk_unpack_tilize_init_(formats_array[run].unpack_B_src, formats_array[run].unpack_B_dst, params.BLOCK_CT_DIM, FACE_R_DIM, false);

    read_offset = 0;

    for (std::uint32_t i = 0; i < params.BLOCK_RT_DIM; i++)
    {
        for (std::uint32_t j = 0; j < params.BLOCK_CT_DIM; j++)
        {
            _llk_unpack_tilize_(
                L1_ADDRESS(params.buffer_B[read_offset]),
                j,
                formats_array[run].unpack_B_src,
                formats_array[run].unpack_B_dst,
                block_ct_dim,
                FACE_R_DIM,
                4,
                false);
        }
        read_offset += params.BLOCK_CT_DIM;
    }

    /*
    In this test we fuse two LLK pipeline runs, one is to unpack untilized buffers/operands from L1 (39-45) and pack them in tilized format(130-145).
    The next run unpacks these two tilized operands, performs a math compute and pack them out in untilized format.
    Since we have set all three TRISCs to run at the same time, fusing these two runs will cause a race condition where unpacker will immediately read from
    L1 before the packer has completed writing to L1. To prevent the unpacker from prematurely reading from L1 before packer has completed write
    the unpacker needs to wait for packer to finish writing to L1 before it starts reading from L1 for the second iteration of LLK pipeline.

    Synchronization is accomplished between the packer and unpacker operations using the PACK_DONE semaphore.
    The packer first writes data to L1 and signals the unpacker by incrementing the semaphore (PACK_DONE = 1).
    The unpacker waits for the semaphore to be set to 1 before reading the data from L1.
    This ensures that the unpacker does not read premature or incorrect data, preventing data race conditions.
    Once the unpacker starts reading, it decrements the semaphore (PACK_DONE = 0) signalling it has started processing data.
    */

    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(
        semaphore::PACK_DONE); // Unpacker waits on signal when packer will increment semaphore to 1 (waits while semaphore == 0), utilizing SEMWAIT.
    t6_semaphore_get<>(semaphore::PACK_DONE); // It will acquire the semaphore t6_semaphore_get (decrementing the semaphore back to 0) signalling it has begun
                                              // processing data from L1

    run = 1; // second L1-to-L1 run, we access the second set of formats_array in our array
    _llk_unpack_hw_configure_<is_fp32_dest_acc_en>(
        formats_array[run].unpack_A_src,
        formats_array[run].unpack_B_src,
        formats_array[run].unpack_A_dst,
        formats_array[run].unpack_B_dst,
        FACE_R_DIM,
        FACE_R_DIM,
        4 /* num_faces */,
        4 /* num_faces */);
    _llk_unpack_AB_init_<>(DEFAULT_TENSOR_SHAPE);

    const int num_tiles                 = num_blocks * num_tiles_in_block;
    const std::uint32_t tile_size_bytes = SCALE_DATUM_SIZE(formats_array[run].unpack_A_src, TILE_R_DIM * TILE_C_DIM);
    for (int tile = 0; tile < num_tiles; ++tile)
    {
        _llk_unpack_AB_<>(L1_ADDRESS(buffer_A_tilized + tile * tile_size_bytes), L1_ADDRESS(buffer_B_tilized + tile * tile_size_bytes));
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "params.h"

using namespace ckernel;

void run_kernel(RUNTIME_PARAMETERS params)
{
#if defined(RUNTIME_FORMATS) && !defined(SPEED_OF_LIGHT)
    const FormatConfig(&formats_array)[2] = params.formats;
#endif
    const bool is_int_fpu_en = false;
    int run                  = 0; // first L1-to-L1 run, we access the first set of formats_array in our array

    const int num_blocks                   = params.NUM_BLOCKS;
    const std::uint32_t num_tiles_in_block = params.NUM_TILES_IN_BLOCK;

// copy srca to dest
#ifdef ARCH_BLACKHOLE
    const bool TILIZE = true;
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, TILIZE, is_int_fpu_en>(4, formats_array[run].math);
#else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, is_int_fpu_en>(4, formats_array[run].math);
#endif

    _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<is_fp32_dest_acc_en>(formats_array[run].math, formats_array[run].math);

    // copy tilized inputs.
    for (int block = 0; block < num_blocks; ++block)
    {
        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
        for (std::uint32_t tile = 0; tile < num_tiles_in_block; ++tile)
        {
            LLK_ASSERT(
                (tile < get_dest_max_tiles<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileShape::Tile32x32>()),
                "Block tile index exceeds maximum destination tiles");
            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                tile, formats_array[run].math, formats_array[run].math);
        }
        _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }

    for (int block = 0; block < num_blocks; ++block)
    {
        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
        for (std::uint32_t tile = 0; tile < num_tiles_in_block; ++tile)
        {
            LLK_ASSERT(
                (tile < get_dest_max_tiles<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileShape::Tile32x32>()),
                "Block tile index exceeds maximum destination tiles");
            _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(
                tile, formats_array[run].math, formats_array[run].math);
        }
        _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }

    run = 1; // second L1-to-L1 run, we access the second set of formats_array in our array
    _llk_math_eltwise_binary_init_<ELTWISE_BINARY_OP, BroadcastType::NONE>(DEFAULT_TENSOR_SHAPE, 0 /* acc_to_dest */);

    for (int block = 0; block < num_blocks; ++block)
    {
        _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
        for (std::uint32_t tile = 0; tile < num_tiles_in_block; ++tile)
        {
            _llk_math_eltwise_binary_<ELTWISE_BINARY_OP, BroadcastType::NONE, DstSync::SyncHalf, is_fp32_dest_acc_en>(
                DEFAULT_TENSOR_SHAPE, tile, false /* clear_fp32_dst_acc */);
        }
        _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
#if defined(RUNTIME_FORMATS) && !defined(SPEED_OF_LIGHT)
    const FormatConfig(&formats_array)[2] = params.formats;
#endif
    const bool UNTILIZE                    = false;
    int run                                = 0;
    const int num_blocks                   = params.NUM_BLOCKS;
    const std::uint32_t num_tiles_in_block = params.NUM_TILES_IN_BLOCK;

    const std::uint32_t tile_size_bytes = SCALE_DATUM_SIZE(formats_array[run].pack_dst, TILE_R_DIM * TILE_C_DIM);

#ifdef ARCH_BLACKHOLE
    const bool TILIZE = true;
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, UNTILIZE, TILIZE>(formats_array[run].pack_src, formats_array[run].pack_dst, 16 * 16 * 4);
    _llk_pack_init_<UNTILIZE, false, TILIZE>(formats_array[run].pack_dst);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
#else
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, UNTILIZE>(formats_array[run].pack_src, formats_array[run].pack_dst, 16 * 16 * 4);
    _llk_pack_init_<UNTILIZE, false>(formats_array[run].pack_dst);
    _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, UNTILIZE>();
#endif

    for (int block = 0; block < num_blocks; ++block)
    {
        _llk_packer_wait_for_math_done_();
        for (std::uint32_t tile = 0; tile < num_tiles_in_block; ++tile)
        {
            const int tile_index = block * num_tiles_in_block + tile;
            _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, UNTILIZE>(tile, L1_ADDRESS(buffer_A_tilized + tile_index * tile_size_bytes));
        }
        _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }

    for (int block = 0; block < num_blocks; ++block)
    {
        _llk_packer_wait_for_math_done_();
        for (std::uint32_t tile = 0; tile < num_tiles_in_block; ++tile)
        {
            const int tile_index = block * num_tiles_in_block + tile;
            _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, UNTILIZE>(tile, L1_ADDRESS(buffer_B_tilized + tile_index * tile_size_bytes));
        }
        _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }

    t6_semaphore_post<>(semaphore::PACK_DONE); // The packer signals to the unpacker that it has finished writing to L1 by posting (incrementing) the semaphore.
                                               // Now unpacker's wait condition is satisfied, allowing it to begin processing data from L1.
    run = 1;                                   // second L1-to-L1 run, we access the second set of formats_array in our array

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<is_fp32_dest_acc_en, UNTILIZE, !TILIZE>(formats_array[run].pack_src, formats_array[run].pack_dst, 16 * 16 * 4);
    _llk_pack_init_<UNTILIZE, false, !TILIZE>(formats_array[run].pack_dst);
#endif

    for (int block = 0; block < num_blocks; ++block)
    {
        _llk_packer_wait_for_math_done_();
        for (std::uint32_t tile = 0; tile < num_tiles_in_block; ++tile)
        {
            _llk_pack_<DstSync::SyncHalf, is_fp32_dest_acc_en, UNTILIZE>(tile, L1_ADDRESS(params.buffer_Res[block * num_tiles_in_block + tile]));
        }
        _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
    }
}

#endif
