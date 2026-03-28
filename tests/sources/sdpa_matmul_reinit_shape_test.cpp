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

constexpr std::uint32_t MATMUL_CT_DIM           = 4;
constexpr std::uint32_t MATMUL_RT_DIM           = 2;
constexpr std::uint32_t MATMUL_KT_DIM           = 4;
constexpr std::uint32_t MATMUL_OUT_TILES        = MATMUL_CT_DIM * MATMUL_RT_DIM;
constexpr std::uint32_t MATMUL_IN1_STRIDE_TILES = 4;
constexpr std::uint32_t SUB_OUT_TILES           = MATMUL_CT_DIM;

constexpr std::uint32_t MATMUL_A_ADDR    = 0x1A000;
constexpr std::uint32_t MATMUL_B_ADDR    = 0x1E000;
constexpr std::uint32_t SUB_A_ADDR       = 0x26000;
constexpr std::uint32_t SUB_B_ADDR       = 0x28000;
constexpr std::uint32_t MATMUL_OUT0_ADDR = 0x28800;
constexpr std::uint32_t SUB_OUT_ADDR     = 0x2C800;
constexpr std::uint32_t MATMUL_OUT1_ADDR = 0x2E800;

} // namespace

#ifdef LLK_TRISC_UNPACK

#include "experimental/llk_unpack_AB_sub_bcast_col_custom.h"
#include "llk_unpack_AB.h"
#include "llk_unpack_AB_matmul.h"
#include "llk_unpack_common.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
    const Operand matmul_a(MATMUL_A_ADDR, MATMUL_KT_DIM * MATMUL_RT_DIM * TILE_SIZE_BYTES);
    const Operand matmul_b(MATMUL_B_ADDR, MATMUL_KT_DIM * MATMUL_CT_DIM * TILE_SIZE_BYTES);
    const Operand sub_a(SUB_A_ADDR, MATMUL_CT_DIM * TILE_SIZE_BYTES);
    const Operand sub_b(SUB_B_ADDR, TILE_SIZE_BYTES);
    const std::uint32_t format = ckernel::to_underlying(DataFormat::Float16_b);
    _llk_unpack_hw_configure_<false, false>(format, format, format, format, 16, 16, 4, 4, TILE_SIZE, TILE_SIZE);

    _llk_unpack_AB_matmul_init_<>(false, MATMUL_CT_DIM, MATMUL_RT_DIM, MATMUL_KT_DIM, 16, 16, 4, 4, false, false);
    for (std::uint32_t kt = 0; kt < MATMUL_KT_DIM; ++kt)
    {
        _llk_unpack_AB_matmul_<>(
            L1_ADDRESS(matmul_a[0]),
            L1_ADDRESS(matmul_b[0]),
            kt,
            kt * MATMUL_IN1_STRIDE_TILES,
            TILE_SIZE,
            TILE_SIZE,
            false,
            false,
            MATMUL_CT_DIM,
            MATMUL_RT_DIM,
            MATMUL_KT_DIM);
    }

    _llk_unpack_reconfig_data_format_srca_impl_<false, false>(format, format, TILE_SIZE);
    _llk_unpack_reconfig_data_format_srcb_impl_<false, false>(format, format, TILE_SIZE);
    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
    t6_semaphore_get<>(semaphore::PACK_DONE);

    _llk_unpack_AB_sub_bcast_col_init_custom_<BroadcastType::COL>();
    _llk_unpack_AB_sub_bcast_col_custom_<BroadcastType::COL>(L1_ADDRESS(sub_a[0]), L1_ADDRESS(sub_b[0]), MATMUL_CT_DIM);

    _llk_unpack_reconfig_data_format_srca_impl_<false, false>(format, format, TILE_SIZE);
    _llk_unpack_reconfig_data_format_srcb_impl_<false, false>(format, format, TILE_SIZE);
    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
    t6_semaphore_get<>(semaphore::PACK_DONE);

    _llk_unpack_AB_matmul_init_<>(false, MATMUL_CT_DIM, MATMUL_RT_DIM, MATMUL_KT_DIM, 16, 16, 4, 4, false, false);
    for (std::uint32_t kt = 0; kt < MATMUL_KT_DIM; ++kt)
    {
        _llk_unpack_AB_matmul_<>(
            L1_ADDRESS(matmul_a[0]),
            L1_ADDRESS(matmul_b[0]),
            kt,
            kt * MATMUL_IN1_STRIDE_TILES,
            TILE_SIZE,
            TILE_SIZE,
            false,
            false,
            MATMUL_CT_DIM,
            MATMUL_RT_DIM,
            MATMUL_KT_DIM);
    }
}

#endif

#ifdef LLK_TRISC_MATH

#include "experimental/llk_math_eltwise_binary_custom.h"
#include "experimental/llk_math_matmul_custom_no_mop.h"
#include "llk_math_common.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
    const std::uint32_t format         = ckernel::to_underlying(DataFormat::Float16_b);
    constexpr DstSync matmul_dest_sync = DstSync::SyncFull;
    constexpr DstSync sub_dest_sync    = DstSync::SyncHalf;

    _llk_math_matmul_init_no_mop_<MATH_FIDELITY, 0>(TILE_R_DIM, TILE_C_DIM, TILE_R_DIM, TILE_C_DIM, false, 0, MATMUL_CT_DIM, MATMUL_RT_DIM);
    _llk_math_pack_sync_init_<matmul_dest_sync, false>();
    _llk_math_hw_configure_<false>(format, format);

    _llk_math_wait_for_dest_available_<matmul_dest_sync>();
    for (std::uint32_t kt = 0; kt < MATMUL_KT_DIM; ++kt)
    {
        _llk_math_matmul_no_mop_<MATH_FIDELITY, 0>(0, MATMUL_CT_DIM, MATMUL_RT_DIM);
    }
    _llk_math_dest_section_done_<matmul_dest_sync, false>();

    _llk_math_reconfig_data_format_<false, false>(format, format);
    _llk_math_pack_sync_init_<sub_dest_sync, false>();
    _llk_math_wait_for_dest_available_<sub_dest_sync>();
    _llk_math_eltwise_binary_init_custom_<ELWSUB, BroadcastType::COL, MATH_FIDELITY>(4, 0);
    math::set_dst_write_addr<DstTileShape::Tile32x32, UnpackDestination::SrcRegs>(0);
    _llk_math_eltwise_binary_bcast_reuse_custom_(MATMUL_CT_DIM);
    math::clear_dst_reg_addr();
    _llk_math_dest_section_done_<sub_dest_sync, false>();

    _llk_math_reconfig_data_format_<false, false>(format, format);
    _llk_math_pack_sync_init_<matmul_dest_sync, false>();
    _llk_math_wait_for_dest_available_<matmul_dest_sync>();
    matmul_configure_addrmod_reinit<MATH_FIDELITY, 0>(false);
    for (std::uint32_t kt = 0; kt < MATMUL_KT_DIM; ++kt)
    {
        _llk_math_matmul_no_mop_<MATH_FIDELITY, 0>(0, MATMUL_CT_DIM, MATMUL_RT_DIM);
    }
    _llk_math_dest_section_done_<matmul_dest_sync, false>();
}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel(RUNTIME_PARAMETERS params)
{
    const Operand matmul_out0(MATMUL_OUT0_ADDR, TILE_SIZE_BYTES);
    const Operand sub_out(SUB_OUT_ADDR, TILE_SIZE_BYTES);
    const Operand matmul_out1(MATMUL_OUT1_ADDR, TILE_SIZE_BYTES);
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
    for (std::uint32_t tile = 0; tile < MATMUL_OUT_TILES; ++tile)
    {
        _llk_pack_<DstSync::SyncFull, false, false>(tile, L1_ADDRESS(matmul_out0[tile]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncFull, false>();
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
    t6_semaphore_post<>(semaphore::PACK_DONE);

    _llk_pack_reconfig_data_format_<false, false>(format, format, TILE_SIZE);
#ifdef ARCH_BLACKHOLE
    _llk_pack_init_<false, false, false>(format);
    _llk_pack_dest_init_<DstSync::SyncFull, false>();
#else
    _llk_pack_init_<false, false>(format);
    _llk_pack_dest_init_<DstSync::SyncFull, false, false>();
#endif
    _llk_packer_wait_for_math_done_();
    for (std::uint32_t tile = 0; tile < MATMUL_OUT_TILES; ++tile)
    {
        _llk_pack_<DstSync::SyncFull, false, false>(tile, L1_ADDRESS(matmul_out1[tile]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncFull, false>();
}

#endif
