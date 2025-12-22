#define FUSED_TEST
#include "ckernel.h"
#include "llk_defs.h"
#include "ckernel_debug.h"
#include "ckernel_defs.h"
#include "ckernel_sfpu.h"
#include "tensix_types.h"
#include "operand.h"
#include "data_format_inference.h"

uint32_t unp_cfg_context          = 0;
uint32_t pack_sync_tile_dst_ptr   = 0;
uint32_t math_sync_tile_dst_index = 0;

inline uint32_t L1_ADDRESS(uint32_t buffer_address)
{
#ifdef ARCH_QUASAR
    return buffer_address / 16;
#else
    return (buffer_address / 16) - 1;
#endif
}

#define UNUSED __attribute__((unused))


#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB.h"
#include "llk_unpack_AB_matmul.h"
#include "llk_unpack_common.h"
#include "llk_unpack_tilize.h"

void run_kernel()
{
    // Operation 0: UnpackerTilizeA
    UNUSED const Operand buffer_A0(0x1a000, 2048);
    UNUSED const Operand buffer_B0(0x1c000, 2048);
    UNUSED const uint32_t unpack_a_src_format0 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    UNUSED const uint32_t unpack_a_dst_format0 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    UNUSED const uint32_t unpack_b_src_format0 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    UNUSED const uint32_t unpack_b_dst_format0 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    _llk_unpack_tilize_hw_configure_<true, StochRndType::None>(
        unpack_a_src_format0, unpack_a_dst_format0, 16, 0, 4
    );
    _llk_unpack_tilize_init_(unpack_a_src_format0, unpack_a_dst_format0, 2, 16, false);
    for (uint32_t i = 0; i < 2; i++)
    {
        for (uint32_t j = 0; j < 2; j++)
        {
            _llk_unpack_tilize_(L1_ADDRESS(buffer_A0[i * 2]), j, unpack_a_src_format0);
        }
    }
    _llk_unpack_tilize_uninit_(unpack_a_dst_format0, 4, 16);
    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
    t6_semaphore_get<>(semaphore::PACK_DONE);

    // Operation 1: UnpackerAB
    UNUSED const Operand buffer_A1(0x1e000, 2048);
    UNUSED const Operand buffer_B1(0x1c000, 2048);
    UNUSED const uint32_t unpack_a_src_format1 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    UNUSED const uint32_t unpack_a_dst_format1 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    UNUSED const uint32_t unpack_b_src_format1 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    UNUSED const uint32_t unpack_b_dst_format1 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    _llk_unpack_reconfig_data_format_srca_impl_<true, false>(
        unpack_a_src_format1, unpack_a_dst_format1, 128
    );
    _llk_unpack_reconfig_data_format_srcb_impl_<true, false>(
        unpack_b_src_format1, unpack_b_dst_format1, 128
    );
    _llk_unpack_AB_init_<BroadcastType::NONE>(16, 4, false, 0);
    for (int i = 0; i < 4; i++)
    {
        _llk_unpack_AB_<>(L1_ADDRESS(buffer_A1[i]), L1_ADDRESS(buffer_B1[i]));
    }

    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
    t6_semaphore_get<>(semaphore::PACK_DONE);

    // Operation 2: MatmulUnpacker
    UNUSED const Operand buffer_A2(0x20000, 2048);
    UNUSED const Operand buffer_B2(0x22000, 2048);
    UNUSED const uint32_t unpack_a_src_format2 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    UNUSED const uint32_t unpack_a_dst_format2 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    UNUSED const uint32_t unpack_b_src_format2 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    UNUSED const uint32_t unpack_b_dst_format2 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    _llk_unpack_reconfig_data_format_srca_impl_<true, false>(
        unpack_a_src_format2, unpack_a_dst_format2, 128
    );
    _llk_unpack_reconfig_data_format_srcb_impl_<true, false>(
        unpack_b_src_format2, unpack_b_dst_format2, 128
    );
    _llk_unpack_AB_matmul_init_<>(0, 2, 2, 2, 16, 16);
    for (uint32_t j = 0; j < 2; j++)
    {
        _llk_unpack_AB_matmul_<>(
            L1_ADDRESS(buffer_A2[0]), L1_ADDRESS(buffer_B2[0]),
            j, j * 2, 128, 128, false, false, 2, 2, 2
        );
    }

    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
    t6_semaphore_get<>(semaphore::PACK_DONE);

    // Operation 3: MatmulUnpacker
    UNUSED const Operand buffer_A3(0x24000, 4096);
    UNUSED const Operand buffer_B3(0x28000, 4096);
    UNUSED const uint32_t unpack_a_src_format3 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float32);
    UNUSED const uint32_t unpack_a_dst_format3 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Tf32);
    UNUSED const uint32_t unpack_b_src_format3 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float32);
    UNUSED const uint32_t unpack_b_dst_format3 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Tf32);
    _llk_unpack_reconfig_data_format_srca_impl_<true, false>(
        unpack_a_src_format3, unpack_a_dst_format3, 256
    );
    _llk_unpack_reconfig_data_format_srcb_impl_<true, false>(
        unpack_b_src_format3, unpack_b_dst_format3, 256
    );
    _llk_unpack_AB_matmul_init_<>(0, 2, 2, 2, 16, 16);
    for (uint32_t j = 0; j < 2; j++)
    {
        _llk_unpack_AB_matmul_<>(
            L1_ADDRESS(buffer_A3[0]), L1_ADDRESS(buffer_B3[0]),
            j, j * 2, 256, 256, false, false, 2, 2, 2
        );
    }

}

#endif

#ifdef LLK_TRISC_MATH

#include "ckernel_defs.h"
#include "ckernel_sfpu.h"
#include "ckernel_sfpu_binary.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "llk_math_eltwise_binary_sfpu.h"
#include "llk_math_eltwise_unary_datacopy.h"
#include "llk_math_eltwise_unary_sfpu.h"
#include "llk_math_matmul.h"
#include "sfpu_operations.h"

void run_kernel()
{
    // Operation 0: Math Setup
    const uint32_t math_format0 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    const DstSync dest_sync0 = DstSync::SyncHalf;
    _llk_math_hw_configure_<false, false>(math_format0, math_format0);
    _llk_math_pack_sync_init_<dest_sync0, true>();

    // Operation 0: Datacopy FPU
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, true, BroadcastType::NONE, True, true>(
        4, math_format0
    );
    _llk_math_wait_for_dest_available_<dest_sync0>();
    for (int i = 0; i < 4; ++i)
    {
        _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, dest_sync0, true, BroadcastType::NONE, false>(
            0 + i, math_format0, math_format0, 4);
    }

    // Operation 0: Unary exponential SFPU
    _llk_math_eltwise_unary_sfpu_init_<SfpuType::exponential>();
    _llk_math_eltwise_unary_sfpu_start_<dest_sync0>(0);
    test_utils::call_sfpu_operation<false, true, 128>(SfpuType::exponential, math_format0);
    _llk_math_eltwise_unary_sfpu_done_();

    // Operation 0: Unary celu SFPU
    _llk_math_eltwise_unary_sfpu_init_<SfpuType::celu>();
    _llk_math_eltwise_unary_sfpu_start_<dest_sync0>(0);
    test_utils::call_sfpu_operation<false, true, 128>(SfpuType::celu, math_format0);
    _llk_math_eltwise_unary_sfpu_done_();

    // Operation 0: Binary ADD SFPU
    _llk_math_eltwise_binary_sfpu_init_<SfpuType::add1>();
    _llk_math_eltwise_binary_sfpu_start_<dest_sync0>(0);
    test_utils::call_binary_sfpu_operation<false, ckernel::BinaryOp::ADD, 32, math_format0>(0, 1, 1);
    _llk_math_eltwise_binary_sfpu_done_();

    _llk_math_dest_section_done_<dest_sync0, true>();

    // Operation 1: Math Setup
    const uint32_t math_format1 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    const DstSync dest_sync1 = DstSync::SyncFull;
    _llk_math_reconfig_data_format_<true, false>(math_format1, math_format1);
    _llk_math_pack_sync_init_<dest_sync1, true>();

    // Operation 1: Eltwise ELWADD FPU
    _llk_math_eltwise_binary_init_<ckernel::EltwiseBinaryType::ELWADD, BroadcastType::NONE, 0>(4, 0);
    _llk_math_wait_for_dest_available_<dest_sync1>();
    for (int i = 0; i < 4; i++)
    {
        _llk_math_eltwise_binary_<ELWADD, BroadcastType::NONE, dest_sync1,
            true, 0, EltwiseBinaryReuseDestType::NONE>(4, i, false);
    }

    // Operation 1: Unary neg SFPU
    _llk_math_eltwise_unary_sfpu_init_<SfpuType::neg>();
    _llk_math_eltwise_unary_sfpu_start_<dest_sync1>(0);
    test_utils::call_sfpu_operation<false, true, 128>(SfpuType::neg, math_format1);
    _llk_math_eltwise_unary_sfpu_done_();

    _llk_math_dest_section_done_<dest_sync1, true>();

    // Operation 2: Math Setup
    const uint32_t math_format2 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    const DstSync dest_sync2 = DstSync::SyncHalf;
    _llk_math_reconfig_data_format_<true, false>(math_format2, math_format2);
    _llk_math_pack_sync_init_<dest_sync2, true>();

    // Operation 2: Matmul FPU
    _llk_math_matmul_init_<0, DstTileFaceLayout::RowMajor>(
        TILE_R_DIM, TILE_C_DIM, TILE_R_DIM, TILE_C_DIM, false, 0, 2, 2
    );
    _llk_math_wait_for_dest_available_<dest_sync2>();
    for (uint32_t j = 0; j < 2; j++)
    {
        _llk_math_matmul_<0, DstTileFaceLayout::RowMajor>(0, 2, 2);
    }

    _llk_math_dest_section_done_<dest_sync2, true>();

    // Operation 3: Math Setup
    const uint32_t math_format3 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Tf32);
    const DstSync dest_sync3 = DstSync::SyncHalf;
    _llk_math_reconfig_data_format_<true, false>(math_format3, math_format3);
    _llk_math_pack_sync_init_<dest_sync3, true>();

    // Operation 3: Matmul FPU
    _llk_math_matmul_init_<0, DstTileFaceLayout::RowMajor>(
        TILE_R_DIM, TILE_C_DIM, TILE_R_DIM, TILE_C_DIM, false, 0, 2, 2
    );
    _llk_math_wait_for_dest_available_<dest_sync3>();
    for (uint32_t j = 0; j < 2; j++)
    {
        _llk_math_matmul_<0, DstTileFaceLayout::RowMajor>(0, 2, 2);
    }

    // Operation 3: Unary neg SFPU
    _llk_math_eltwise_unary_sfpu_init_<SfpuType::neg>();
    _llk_math_eltwise_unary_sfpu_start_<dest_sync3>(0);
    test_utils::call_sfpu_operation<false, true, 128>(SfpuType::neg, math_format3);
    _llk_math_eltwise_unary_sfpu_done_();

    // Operation 3: Unary sqrt SFPU
    _llk_math_eltwise_unary_sfpu_init_<SfpuType::sqrt>();
    _llk_math_eltwise_unary_sfpu_start_<dest_sync3>(0);
    test_utils::call_sfpu_operation<false, true, 128>(SfpuType::sqrt, math_format3);
    _llk_math_eltwise_unary_sfpu_done_();

    // Operation 3: Binary ADD SFPU
    _llk_math_eltwise_binary_sfpu_init_<SfpuType::add1>();
    _llk_math_eltwise_binary_sfpu_start_<dest_sync3>(0);
    test_utils::call_binary_sfpu_operation<false, ckernel::BinaryOp::ADD, 128, math_format3>(0, 0, 0);
    _llk_math_eltwise_binary_sfpu_done_();

    _llk_math_dest_section_done_<dest_sync3, true>();

}

#endif

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"

void run_kernel()
{
    // Operation 0: Packer
    const Operand buffer_Res0(0x1e000, 2048);
    const uint32_t pack_in_format0 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    const uint32_t pack_out_format0 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    _llk_pack_hw_configure_<true, false, True>(
        pack_in_format0, pack_out_format0, 128
    );
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false, True>(
        pack_out_format0
    );
    _llk_pack_dest_init_<DstSync::SyncHalf, true, DstTileFaceLayout::RowMajor>();
    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < 4; i++)
    {
        _llk_pack_<DstSync::SyncHalf, true, false>(i, L1_ADDRESS(buffer_Res0[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, true>();
    t6_semaphore_post<>(semaphore::PACK_DONE);

    // Operation 1: Packer
    const Operand buffer_Res1(0x20000, 2048);
    const uint32_t pack_in_format1 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    const uint32_t pack_out_format1 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b);
    _llk_pack_reconfig_data_format_<true, false, DstTileFaceLayout::RowMajor, false>(
        pack_in_format1, pack_out_format1, 128
    );
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false, False>(
        pack_out_format1
    );
    _llk_pack_dest_init_<DstSync::SyncHalf, true, DstTileFaceLayout::RowMajor>();
    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < 4; i++)
    {
        _llk_pack_<DstSync::SyncHalf, true, false>(i, L1_ADDRESS(buffer_Res1[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, true>();
    t6_semaphore_post<>(semaphore::PACK_DONE);

    // Operation 2: Packer
    const Operand buffer_Res2(0x24000, 4096);
    const uint32_t pack_in_format2 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float32);
    const uint32_t pack_out_format2 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float32);
    _llk_pack_reconfig_data_format_<true, false, DstTileFaceLayout::RowMajor, false>(
        pack_in_format2, pack_out_format2, 256
    );
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false, False>(
        pack_out_format2
    );
    _llk_pack_dest_init_<DstSync::SyncHalf, true, DstTileFaceLayout::RowMajor>();
    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < 4; i++)
    {
        _llk_pack_<DstSync::SyncHalf, true, false>(i, L1_ADDRESS(buffer_Res2[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, true>();
    t6_semaphore_post<>(semaphore::PACK_DONE);

    // Operation 3: Packer
    const Operand buffer_Res3(0x2c000, 4096);
    const uint32_t pack_in_format3 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float32);
    const uint32_t pack_out_format3 = static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float32);
    _llk_pack_reconfig_data_format_<true, false, DstTileFaceLayout::RowMajor, false>(
        pack_in_format3, pack_out_format3, 256
    );
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false, False>(
        pack_out_format3
    );
    _llk_pack_dest_init_<DstSync::SyncHalf, true, DstTileFaceLayout::RowMajor>();
    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < 4; i++)
    {
        _llk_pack_<DstSync::SyncHalf, true, false>(i, L1_ADDRESS(buffer_Res3[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, true>();
}

#endif
