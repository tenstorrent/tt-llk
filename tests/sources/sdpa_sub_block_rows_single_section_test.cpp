// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>

#include "ckernel.h"
#include "llk_defs.h"
#include "operand.h"
#include "params.h"

std::uint32_t unp_cfg_context          = 0;
std::uint32_t pack_sync_tile_dst_ptr   = 0;
std::uint32_t math_sync_tile_dst_index = 0;

namespace
{

constexpr std::uint32_t TILE_SIZE       = 128;
constexpr std::uint32_t TILE_SIZE_BYTES = TILE_SIZE * 16;

constexpr std::uint32_t CT_DIM    = 4;
constexpr std::uint32_t RT_DIM    = 2;
constexpr std::uint32_t OUT_TILES = CT_DIM * RT_DIM;

constexpr std::uint32_t SUB_A_ADDR   = 0x26000;
constexpr std::uint32_t SUB_B_ADDR   = 0x2A000;
constexpr std::uint32_t SUB_OUT_ADDR = 0x2C000;

} // namespace

#ifdef LLK_TRISC_UNPACK

#include "experimental/llk_unpack_AB_sub_bcast_col_custom.h"
#include "llk_unpack_common.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
    const Operand sub_a(SUB_A_ADDR, OUT_TILES * TILE_SIZE_BYTES);
    const Operand sub_b(SUB_B_ADDR, RT_DIM * TILE_SIZE_BYTES);
    const std::uint32_t format = ckernel::to_underlying(DataFormat::Float16_b);

    _llk_unpack_hw_configure_<false, false>(format, format, format, format, 16, 16, 4, 4, TILE_SIZE, TILE_SIZE);

    for (std::uint32_t row = 0; row < RT_DIM; ++row)
    {
        _llk_unpack_AB_sub_bcast_col_init_custom_<BroadcastType::COL>();
        _llk_unpack_AB_sub_bcast_col_custom_<BroadcastType::COL>(L1_ADDRESS(sub_a[row * CT_DIM]), L1_ADDRESS(sub_b[row]), CT_DIM);
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "experimental/llk_math_eltwise_binary_custom.h"
#include "llk_math_common.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
    const std::uint32_t format = ckernel::to_underlying(DataFormat::Float16_b);

    constexpr DstSync sub_dest_sync = DstSync::SyncFull;
    _llk_math_pack_sync_init_<sub_dest_sync, false>();
    _llk_math_hw_configure_<false>(format, format);

    _llk_math_wait_for_dest_available_<sub_dest_sync>();
    for (std::uint32_t row = 0; row < RT_DIM; ++row)
    {
        _llk_math_eltwise_binary_init_custom_<ELWSUB, BroadcastType::COL, MATH_FIDELITY>(4, 0);
        math::set_dst_write_addr<DstTileShape::Tile32x32, UnpackDestination::SrcRegs>(row * CT_DIM);
        _llk_math_eltwise_binary_bcast_reuse_custom_(CT_DIM);
        math::clear_dst_reg_addr();
    }
    _llk_math_dest_section_done_<sub_dest_sync, false>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
    const Operand sub_out(SUB_OUT_ADDR, OUT_TILES * TILE_SIZE_BYTES);
    const std::uint32_t format = ckernel::to_underlying(DataFormat::Float16_b);

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<false, false, false>(format, format, TILE_SIZE);
    _llk_pack_init_<false, false, false>(format);
    _llk_pack_dest_init_<DstSync::SyncFull, false>();
#else
    _llk_pack_hw_configure_<false, false>(format, format, TILE_SIZE);
    _llk_pack_init_<false, false>(format);
    _llk_pack_dest_init_<DstSync::SyncFull, false, false>();
#endif

    _llk_packer_wait_for_math_done_();
    for (std::uint32_t tile = 0; tile < OUT_TILES; ++tile)
    {
        _llk_pack_<DstSync::SyncFull, false, false>(tile, L1_ADDRESS(sub_out[tile]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncFull, false>();
}

#endif
