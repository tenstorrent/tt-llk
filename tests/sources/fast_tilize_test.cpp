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

#if defined(LLK_PROFILER)
uint32_t loop_factor = 1024;
#else
uint32_t loop_factor = 1;
#endif

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_common.h"
#include "llk_unpack_tilize.h"

void run_kernel()
{
    {
        ZONE_SCOPED("INIT")
        _llk_unpack_fast_tilize_hw_configure_<is_fp32_dest_acc_en>(UNPACK_A_IN, UNPACK_A_OUT);
        _llk_unpack_fast_tilize_init_(UNPACK_A_OUT, BLOCK_CT_DIM);
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        for (uint32_t loop = 0; loop < loop_factor; loop++)
        {
            for (uint32_t i = 0; i < BLOCK_RT_DIM; i++)
            {
                uint32_t read_offset = i * BLOCK_CT_DIM * TILE_R_DIM;

                uint32_t packed_tiles    = 0;
                uint32_t remaining_tiles = BLOCK_CT_DIM;
                uint32_t dest_size       = is_fp32_dest_acc_en ? 4 : 8;
                uint32_t unit_dim        = BLOCK_CT_DIM == 1 ? 1 : 2;
                uint32_t num_units       = dest_size / unit_dim;

                while (packed_tiles < BLOCK_CT_DIM)
                {
                    uint32_t tile_index = read_offset + packed_tiles;
                    if (remaining_tiles > 2 * dest_size)
                    {
                        _llk_unpack_fast_tilize_block_(L1_ADDRESS(buffer_A[0]), tile_index, UNPACK_A_IN, unit_dim, num_units, BLOCK_CT_DIM);
                        packed_tiles += dest_size;
                        remaining_tiles -= dest_size;
                    }
                    else if (remaining_tiles > dest_size)
                    {
                        uint32_t even_remainder = remaining_tiles / 2 + ((remaining_tiles / 2) % 2);
                        num_units               = even_remainder / unit_dim;
                        _llk_unpack_fast_tilize_block_(L1_ADDRESS(buffer_A[0]), tile_index, UNPACK_A_IN, unit_dim, num_units, BLOCK_CT_DIM);
                        packed_tiles += even_remainder;
                        remaining_tiles -= even_remainder;
                    }
                    else
                    {
                        if (remaining_tiles % 2 == 0 || unit_dim == 1)
                        {
                            num_units = remaining_tiles / unit_dim;
                            _llk_unpack_fast_tilize_block_(L1_ADDRESS(buffer_A[0]), tile_index, UNPACK_A_IN, unit_dim, num_units, BLOCK_CT_DIM);
                        }
                        else if (remaining_tiles == 3)
                        {
                            _llk_unpack_fast_tilize_block_(L1_ADDRESS(buffer_A[0]), tile_index, UNPACK_A_IN, 3, 1, BLOCK_CT_DIM);
                        }
                        else
                        {
                            num_units = (remaining_tiles - 3) / unit_dim;
                            _llk_unpack_fast_tilize_block_(L1_ADDRESS(buffer_A[0]), tile_index, UNPACK_A_IN, unit_dim, num_units, BLOCK_CT_DIM);
                            _llk_unpack_fast_tilize_block_(L1_ADDRESS(buffer_A[0]), tile_index + remaining_tiles - 3, UNPACK_A_IN, 3, 1, BLOCK_CT_DIM);
                        }
                        packed_tiles += remaining_tiles;
                        remaining_tiles = 0;
                    }
                }
            }
        }
        PROFILER_SYNC();
    }

    _llk_unpack_fast_tilize_uninit_<is_fp32_dest_acc_en>();
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_unary_datacopy.h"

void run_kernel()
{
    {
        ZONE_SCOPED("INIT")
        _llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
        _llk_math_hw_configure_(MATH_FORMAT, MATH_FORMAT);
        _llk_math_fast_tilize_init_(MATH_FORMAT, BLOCK_CT_DIM == 1 ? 1 : 2);
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        for (uint32_t loop = 0; loop < loop_factor; loop++)
        {
            for (uint32_t i = 0; i < BLOCK_RT_DIM; i++)
            {
                uint32_t packed_tiles    = 0;
                uint32_t remaining_tiles = BLOCK_CT_DIM;
                uint32_t dest_size       = is_fp32_dest_acc_en ? 4 : 8;
                uint32_t unit_dim        = BLOCK_CT_DIM == 1 ? 1 : 2;
                uint32_t num_units       = dest_size / unit_dim;

                while (packed_tiles < BLOCK_CT_DIM)
                {
                    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();

                    if (remaining_tiles > 2 * dest_size)
                    {
                        _llk_math_fast_tilize_block_(0, MATH_FORMAT, unit_dim, num_units);
                        packed_tiles += dest_size;
                        remaining_tiles -= dest_size;
                    }
                    else if (remaining_tiles > dest_size)
                    {
                        uint32_t even_remainder = remaining_tiles / 2 + ((remaining_tiles / 2) % 2);
                        num_units               = even_remainder / unit_dim;
                        _llk_math_fast_tilize_block_(0, MATH_FORMAT, unit_dim, num_units);
                        packed_tiles += even_remainder;
                        remaining_tiles -= even_remainder;
                    }
                    else
                    {
                        if (remaining_tiles % 2 == 0 || unit_dim == 1)
                        {
                            num_units = remaining_tiles / unit_dim;
                            _llk_math_fast_tilize_block_(0, MATH_FORMAT, unit_dim, num_units);
                        }
                        else if (remaining_tiles == 3)
                        {
                            _llk_math_fast_tilize_block_(0, MATH_FORMAT, 3, 1);
                        }
                        else
                        {
                            num_units = (remaining_tiles - 3) / unit_dim;
                            _llk_math_fast_tilize_block_(0, MATH_FORMAT, unit_dim, num_units);
                            _llk_math_fast_tilize_block_(remaining_tiles - 3, MATH_FORMAT, 3, 1);
                        }
                        packed_tiles += remaining_tiles;
                        remaining_tiles = 0;
                    }

                    _llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                }
            }
        }
        PROFILER_SYNC();
    }

    _llk_math_fast_tilize_uninit_<is_fp32_dest_acc_en>(MATH_FORMAT);
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel()
{
    uint32_t use_32bit_dest = UNPACK_A_OUT == static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Tf32);
    {
        ZONE_SCOPED("INIT")
        _llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor, false>();
        _llk_pack_fast_tilize_hw_configure_<is_fp32_dest_acc_en>(PACK_IN, PACK_OUT);
        _llk_pack_fast_tilize_init_<DstSync::SyncHalf>(use_32bit_dest, PACK_OUT, BLOCK_CT_DIM == 1 ? 1 : 2);
        PROFILER_SYNC();
    }
    {
        ZONE_SCOPED("TILE_LOOP")
        for (uint32_t loop = 0; loop < loop_factor; loop++)
        {
            for (uint32_t i = 0; i < BLOCK_RT_DIM; i++)
            {
                uint32_t write_offset = i * BLOCK_CT_DIM;

                uint32_t packed_tiles    = 0;
                uint32_t remaining_tiles = BLOCK_CT_DIM;
                uint32_t dest_size       = is_fp32_dest_acc_en ? 4 : 8;
                uint32_t unit_dim        = BLOCK_CT_DIM == 1 ? 1 : 2;
                uint32_t num_units       = dest_size / unit_dim;

                while (packed_tiles < BLOCK_CT_DIM)
                {
                    _llk_packer_wait_for_math_done_();

                    uint32_t tile_index = write_offset + packed_tiles;
                    if (remaining_tiles > 2 * dest_size)
                    {
                        _llk_pack_fast_tilize_block_(0, L1_ADDRESS(buffer_Res[tile_index]), unit_dim, num_units);
                        packed_tiles += dest_size;
                        remaining_tiles -= dest_size;
                    }
                    else if (remaining_tiles > dest_size)
                    {
                        uint32_t even_remainder = remaining_tiles / 2 + ((remaining_tiles / 2) % 2);
                        num_units               = even_remainder / unit_dim;
                        _llk_pack_fast_tilize_block_(0, L1_ADDRESS(buffer_Res[tile_index]), unit_dim, num_units);
                        packed_tiles += even_remainder;
                        remaining_tiles -= even_remainder;
                    }
                    else
                    {
                        if (remaining_tiles % 2 == 0 || unit_dim == 1)
                        {
                            num_units = remaining_tiles / unit_dim;
                            _llk_pack_fast_tilize_block_(0, L1_ADDRESS(buffer_Res[tile_index]), unit_dim, num_units);
                        }
                        else if (remaining_tiles == 3)
                        {
                            _llk_pack_fast_tilize_block_(0, L1_ADDRESS(buffer_Res[tile_index]), 3, 1);
                        }
                        else
                        {
                            num_units = (remaining_tiles - 3) / unit_dim;
                            _llk_pack_fast_tilize_block_(0, L1_ADDRESS(buffer_Res[tile_index]), unit_dim, num_units);
                            _llk_pack_fast_tilize_block_(remaining_tiles - 3, L1_ADDRESS(buffer_Res[tile_index + remaining_tiles - 3]), 3, 1);
                        }
                        packed_tiles += remaining_tiles;
                        remaining_tiles = 0;
                    }

                    _llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();
                }
            }
        }
        PROFILER_SYNC();
    }

    _llk_pack_fast_tilize_uninit_<DstSync::SyncHalf, is_fp32_dest_acc_en>(PACK_OUT);
}

#endif
