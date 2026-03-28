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

constexpr std::uint32_t WARMUP_CT_DIM = 1;
constexpr std::uint32_t WARMUP_RT_DIM = 1;
constexpr std::uint32_t WARMUP_KT_DIM = 1;
constexpr std::uint32_t SUB_CT_DIM    = 4;
constexpr std::uint32_t SUB_OUT_TILES = SUB_CT_DIM;

constexpr std::uint32_t WARMUP_A_ADDR   = 0x1A000;
constexpr std::uint32_t WARMUP_B_ADDR   = 0x1A800;
constexpr std::uint32_t WARMUP_OUT_ADDR = 0x1B000;
constexpr std::uint32_t SUB_A_ADDR      = 0x26000;
constexpr std::uint32_t SUB_B_ADDR      = 0x28000;
constexpr std::uint32_t SUB_OUT_ADDR    = 0x2C800;

} // namespace

#ifdef LLK_TRISC_UNPACK

#include "experimental/llk_unpack_AB_sub_bcast_col_custom.h"
#include "llk_unpack_AB_matmul.h"
#include "llk_unpack_common.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
    const Operand warmup_a(WARMUP_A_ADDR, TILE_SIZE_BYTES);
    const Operand warmup_b(WARMUP_B_ADDR, TILE_SIZE_BYTES);
    const Operand sub_a(SUB_A_ADDR, SUB_CT_DIM * TILE_SIZE_BYTES);
    const Operand sub_b(SUB_B_ADDR, TILE_SIZE_BYTES);
    const std::uint32_t format = ckernel::to_underlying(DataFormat::Float16_b);
    _llk_unpack_hw_configure_<false, false>(format, format, format, format, 16, 16, 4, 4, TILE_SIZE, TILE_SIZE);

    // One warmup matmul tile is enough to flip unp_cfg_context away from 0.
    _llk_unpack_AB_matmul_init_<>(false, WARMUP_CT_DIM, WARMUP_RT_DIM, WARMUP_KT_DIM, 16, 16, 4, 4, false, false);
    _llk_unpack_AB_matmul_<>(
        L1_ADDRESS(warmup_a[0]), L1_ADDRESS(warmup_b[0]), 0, 0, TILE_SIZE, TILE_SIZE, false, false, WARMUP_CT_DIM, WARMUP_RT_DIM, WARMUP_KT_DIM);

    _llk_unpack_reconfig_data_format_srca_impl_<false, false>(format, format, TILE_SIZE);
    _llk_unpack_reconfig_data_format_srcb_impl_<false, false>(format, format, TILE_SIZE);
    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
    t6_semaphore_get<>(semaphore::PACK_DONE);

    _llk_unpack_AB_sub_bcast_col_init_custom_<BroadcastType::COL>();
    _llk_unpack_AB_sub_bcast_col_custom_<BroadcastType::COL>(L1_ADDRESS(sub_a[0]), L1_ADDRESS(sub_b[0]), SUB_CT_DIM);
}

#endif

#ifdef LLK_TRISC_MATH

#include "experimental/llk_math_eltwise_binary_custom.h"
#include "experimental/llk_math_matmul_custom_no_mop.h"
#include "llk_math_common.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
    const std::uint32_t format         = ckernel::to_underlying(DataFormat::Float16_b);
    constexpr DstSync warmup_dest_sync = DstSync::SyncHalf;
    constexpr DstSync sub_dest_sync    = DstSync::SyncHalf;

    _llk_math_matmul_init_no_mop_<MATH_FIDELITY, 0>(TILE_R_DIM, TILE_C_DIM, TILE_R_DIM, TILE_C_DIM, false, 0, WARMUP_CT_DIM, WARMUP_RT_DIM);
    _llk_math_pack_sync_init_<warmup_dest_sync, false>();
    _llk_math_hw_configure_<false>(format, format);

    _llk_math_wait_for_dest_available_<warmup_dest_sync>();
    _llk_math_matmul_no_mop_<MATH_FIDELITY, 0>(0, WARMUP_CT_DIM, WARMUP_RT_DIM);
    _llk_math_dest_section_done_<warmup_dest_sync, false>();

    _llk_math_reconfig_data_format_<false, false>(format, format);
    _llk_math_pack_sync_init_<sub_dest_sync, false>();
    _llk_math_wait_for_dest_available_<sub_dest_sync>();
    _llk_math_eltwise_binary_init_custom_<ELWSUB, BroadcastType::COL, MATH_FIDELITY>(4, 0);
    math::set_dst_write_addr<DstTileShape::Tile32x32, UnpackDestination::SrcRegs>(0);
    _llk_math_eltwise_binary_bcast_reuse_custom_(SUB_CT_DIM);
    math::clear_dst_reg_addr();
    _llk_math_dest_section_done_<sub_dest_sync, false>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
    const Operand warmup_out(WARMUP_OUT_ADDR, TILE_SIZE_BYTES);
    const Operand sub_out(SUB_OUT_ADDR, TILE_SIZE_BYTES);
    const std::uint32_t format = ckernel::to_underlying(DataFormat::Float16_b);

#ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<false, false, false>(format, format, TILE_SIZE);
    _llk_pack_init_<false, false, false>(format);
    _llk_pack_dest_init_<DstSync::SyncHalf, false>();
#else
    _llk_pack_hw_configure_<false, false>(format, format, TILE_SIZE);
    _llk_pack_init_<false, false>(format);
    _llk_pack_dest_init_<DstSync::SyncHalf, false, false>();
#endif

    _llk_packer_wait_for_math_done_();
    _llk_pack_<DstSync::SyncHalf, false, false>(0, L1_ADDRESS(warmup_out[0]));
    _llk_pack_dest_section_done_<DstSync::SyncHalf, false>();
    t6_semaphore_post<>(semaphore::PACK_DONE);

    _llk_pack_reconfig_data_format_<false, false>(format, format, TILE_SIZE);
#ifdef ARCH_BLACKHOLE
    _llk_pack_init_<false, false, false>(format);
    _llk_pack_dest_init_<DstSync::SyncHalf, false>();
#else
    _llk_pack_init_<false, false>(format);
    _llk_pack_dest_init_<DstSync::SyncHalf, false, false>();
#endif
    _llk_packer_wait_for_math_done_();
    for (std::uint32_t tile = 0; tile < SUB_OUT_TILES; ++tile)
    {
        _llk_pack_<DstSync::SyncHalf, false, false>(tile, L1_ADDRESS(sub_out[tile]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, false>();
}

#endif
