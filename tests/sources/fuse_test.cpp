
#define FUSE_TEST
#include <cstdint>

#include <array>
#include <type_traits>

#include <cstddef>
#include <utility>

#include "ckernel.h"
#include "llk_defs.h"

#include "ckernel_debug.h"
#include "ckernel_defs.h"
#include "ckernel_sfpu.h"
#include "tensix_types.h"
#include "operand.h"

#include "llk_sfpu_types.h"

using namespace ckernel;

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

constexpr bool UNPACKING_TO_DEST    = false;
constexpr bool APPROX_MODE          = false;
constexpr bool is_fp32_dest_acc_en  = false;

#include "data_format_inference.h"


#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_A.h"
#include "llk_unpack_AB.h"
#include "llk_unpack_AB_matmul.h"
#include "llk_unpack_common.h"
#include "llk_unpack_tilize.h"

void run_kernel()
{
	// Operation 0: Unpacker A

    Operand buffer_A0(0x1a000, 2048);

    _llk_unpack_A_init_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, false>(
        0, 0, 16, 4, static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b), static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b));
    _llk_unpack_A_hw_configure_<false, StochRndType::None>(static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b), static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b), 16, 0, 4);

    for (int i = 0; i < 4; ++i)
    {
        _llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, false>(
            L1_ADDRESS(buffer_A0[i]), 0, static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b), static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b));
    }

	// Operation 1: Unpacker AB

    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
    t6_semaphore_get<>(semaphore::PACK_DONE);

    Operand buffer_A1(0x22000, 2048);
    Operand buffer_B1(0x1c000, 2048);

    _llk_unpack_AB_hw_configure_<false, StochRndType::None>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)
    );
    _llk_unpack_AB_init_<>();
    for (int i = 0; i < 4; i++)
    {
        _llk_unpack_AB_<>(L1_ADDRESS(buffer_A1[i]), L1_ADDRESS(buffer_B1[i]));
    }

	// Operation 2: Matmul Unpacker AB

    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
    t6_semaphore_get<>(semaphore::PACK_DONE);

    Operand buffer_A2(0x24000, 2048);
    Operand buffer_B2(0x1e000, 2048);
    _llk_unpack_AB_matmul_hw_configure_<false, StochRndType::None>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        16,
        16,
        0,
        4,
        4,
        128,
        128
    );
    _llk_unpack_AB_matmul_init_<>(0, 2, 2, 2, 16, 16);
    for (uint32_t j = 0; j < 2; j++)
    {
        _llk_unpack_AB_matmul_<>(
            L1_ADDRESS(buffer_A2[0]),
            L1_ADDRESS(buffer_B2[0]),
            j,
            j * 2,
            128,
            128,
            16,
            16,
            false,
            false,
            2,
            2,
            2
        );
    }

	// Operation 3: Matmul Unpacker AB

    t6_semaphore_wait_on_zero<p_stall::STALL_SYNC>(semaphore::PACK_DONE);
    t6_semaphore_get<>(semaphore::PACK_DONE);

    Operand buffer_A3(0x26000, 2048);
    Operand buffer_B3(0x20000, 2048);
    _llk_unpack_AB_matmul_hw_configure_<false, StochRndType::None>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        16,
        16,
        0,
        4,
        4,
        128,
        128
    );
    _llk_unpack_AB_matmul_init_<>(0, 2, 2, 2, 16, 16);
    for (uint32_t j = 0; j < 2; j++)
    {
        _llk_unpack_AB_matmul_<>(
            L1_ADDRESS(buffer_A3[0]),
            L1_ADDRESS(buffer_B3[0]),
            j,
            j * 2,
            128,
            128,
            16,
            16,
            false,
            false,
            2,
            2,
            2
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

using namespace ckernel::sfpu;

void run_kernel()
{
	// Operation 0: Datacopy FPU

    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, false, BroadcastType::NONE, false, false>(
        0, 0, 4, static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b));

    _llk_math_pack_sync_init_<DstSync::SyncHalf, false>();
    _llk_math_hw_configure_<false, false>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)
    );
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (int i = 0; i < 4; ++i)
    {
    
        _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, false, BroadcastType::NONE, false>(
            0 + i, static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b), static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b), 4);
        
    }

	// Operation 0: Unary exponential SFPU

    _llk_math_eltwise_unary_sfpu_init_<SfpuType::exponential>();
    _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(0);
    test_utils::call_sfpu_operation<128, false, false>(
        SfpuType::exponential,
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)
    );
    _llk_math_eltwise_unary_sfpu_done_();

	// Operation 0: Unary celu SFPU

    _llk_math_eltwise_unary_sfpu_init_<SfpuType::celu>();
    _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(0);
    test_utils::call_sfpu_operation<128, false, false>(
        SfpuType::celu,
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)
    );
    _llk_math_eltwise_unary_sfpu_done_();

	// Operation 0: Binary ADD SFPU

    _llk_math_eltwise_binary_sfpu_init_<SfpuType::add1>();
    _llk_math_eltwise_binary_sfpu_start_<DstSync::SyncHalf>(0);
    test_utils::call_binary_sfpu_operation<false, ckernel::BinaryOp::ADD, 32, static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)>(0, 1, 1);
    _llk_math_eltwise_binary_sfpu_done_();

    _llk_math_dest_section_done_<DstSync::SyncHalf, false>();

	// Operation 1: Eltwise ELWADD FPU

    _llk_math_pack_sync_init_<DstSync::SyncHalf, false>();
    _llk_math_hw_configure_<false, false>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)
    );
    _llk_math_eltwise_binary_init_<ckernel::EltwiseBinaryType::ELWADD, BroadcastType::NONE, 0>(4, 0, 0);

    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (int i = 0; i < 4; i++)
    {
        _llk_math_eltwise_binary_<
            ELWADD,
            BroadcastType::NONE,
            DstSync::SyncHalf,
            false,
            0,
            EltwiseBinaryReuseDestType::NONE>(4, i, false);
    }

	// Operation 1: Unary neg SFPU

    _llk_math_eltwise_unary_sfpu_init_<SfpuType::neg>();
    _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(0);
    test_utils::call_sfpu_operation<128, false, false>(
        SfpuType::neg,
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)
    );
    _llk_math_eltwise_unary_sfpu_done_();

    _llk_math_dest_section_done_<DstSync::SyncHalf, false>();

	// Operation 2: Matmul FPU

    _llk_math_matmul_init_<0, DstTileFaceLayout::RowMajor>(TILE_R_DIM, TILE_C_DIM, TILE_R_DIM, TILE_C_DIM, false, 0, 2, 2, 2);
    _llk_math_pack_sync_init_<DstSync::SyncHalf, false>();
    _llk_math_hw_configure_<false, false>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)
    );
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (uint32_t j = 0; j < 2; j++)
    {
        _llk_math_matmul_<0, DstTileFaceLayout::RowMajor>(0, 0, 2, 2, 2);
    }

    _llk_math_dest_section_done_<DstSync::SyncHalf, false>();

	// Operation 3: Matmul FPU

    _llk_math_matmul_init_<0, DstTileFaceLayout::RowMajor>(TILE_R_DIM, TILE_C_DIM, TILE_R_DIM, TILE_C_DIM, false, 0, 2, 2, 2);
    _llk_math_pack_sync_init_<DstSync::SyncHalf, false>();
    _llk_math_hw_configure_<false, false>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)
    );
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (uint32_t j = 0; j < 2; j++)
    {
        _llk_math_matmul_<0, DstTileFaceLayout::RowMajor>(0, 0, 2, 2, 2);
    }

	// Operation 3: Unary neg SFPU

    _llk_math_eltwise_unary_sfpu_init_<SfpuType::neg>();
    _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(0);
    test_utils::call_sfpu_operation<128, false, false>(
        SfpuType::neg,
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)
    );
    _llk_math_eltwise_unary_sfpu_done_();

	// Operation 3: Unary sqrt SFPU

    _llk_math_eltwise_unary_sfpu_init_<SfpuType::sqrt>();
    _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(0);
    test_utils::call_sfpu_operation<128, false, false>(
        SfpuType::sqrt,
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)
    );
    _llk_math_eltwise_unary_sfpu_done_();

	// Operation 3: Binary ADD SFPU

    _llk_math_eltwise_binary_sfpu_init_<SfpuType::add1>();
    _llk_math_eltwise_binary_sfpu_start_<DstSync::SyncHalf>(0);
    test_utils::call_binary_sfpu_operation<false, ckernel::BinaryOp::ADD, 128, static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)>(0, 0, 0);
    _llk_math_eltwise_binary_sfpu_done_();

    _llk_math_dest_section_done_<DstSync::SyncHalf, false>();

}
#endif

#ifdef LLK_TRISC_PACK
#include "llk_pack.h"
#include "llk_pack_common.h"
void run_kernel()
{
	// Operation 0: Packer

    Operand buffer_Res0(0x22000, 2048);

    _llk_pack_hw_configure_<false, false, false>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        128
    );
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false, false>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)
    );
    _llk_pack_dest_init_<DstSync::SyncHalf, false, DstTileFaceLayout::RowMajor>();

    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < 4; i++)
    {
        _llk_pack_<DstSync::SyncHalf, false, false>(i, L1_ADDRESS(buffer_Res0[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, false>();

    t6_semaphore_post<>(semaphore::PACK_DONE);

	// Operation 1: Packer

    Operand buffer_Res1(0x24000, 2048);

    _llk_pack_hw_configure_<false, false, false>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        128
    );
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false, false>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)
    );
    _llk_pack_dest_init_<DstSync::SyncHalf, false, DstTileFaceLayout::RowMajor>();

    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < 4; i++)
    {
        _llk_pack_<DstSync::SyncHalf, false, false>(i, L1_ADDRESS(buffer_Res1[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, false>();

    t6_semaphore_post<>(semaphore::PACK_DONE);

	// Operation 2: Packer

    Operand buffer_Res2(0x26000, 2048);

    _llk_pack_hw_configure_<false, false, false>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        128
    );
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false, false>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)
    );
    _llk_pack_dest_init_<DstSync::SyncHalf, false, DstTileFaceLayout::RowMajor>();

    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < 4; i++)
    {
        _llk_pack_<DstSync::SyncHalf, false, false>(i, L1_ADDRESS(buffer_Res2[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, false>();

    t6_semaphore_post<>(semaphore::PACK_DONE);

	// Operation 3: Packer

    Operand buffer_Res3(0x28000, 2048);

    _llk_pack_hw_configure_<false, false, false>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b),
        128
    );
    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false, false>(
        static_cast<std::underlying_type_t<DataFormat>>(DataFormat::Float16_b)
    );
    _llk_pack_dest_init_<DstSync::SyncHalf, false, DstTileFaceLayout::RowMajor>();

    _llk_packer_wait_for_math_done_();
    for (int i = 0; i < 4; i++)
    {
        _llk_pack_<DstSync::SyncHalf, false, false>(i, L1_ADDRESS(buffer_Res3[i]));
    }
    _llk_pack_dest_section_done_<DstSync::SyncHalf, false>();

}
#endif
