#include <cstdint>
#include <cstdio>
#include <algorithm>

#include "llk_defs.h"
#include "ckernel.h"

// Globals
uint32_t unp_cfg_context = 0;
uint32_t pack_sync_tile_dst_ptr = 0;
volatile uint32_t tt_l1_ptr l1_buffer[16] __attribute__ ((section (".text#"))) __attribute__ ((aligned (16)));

#ifdef DEST_ACC
const bool is_fp32_dest_acc_en = true;
#else
const bool is_fp32_dest_acc_en = false;
#endif

volatile uint32_t* const buffer_A = reinterpret_cast<volatile uint32_t*>(0x1a000);
volatile uint32_t* const buffer_B = reinterpret_cast<volatile uint32_t*>(0x1b000);

volatile uint32_t* const buffer_A_tilized = reinterpret_cast<volatile uint32_t*>(0x1c000);
volatile uint32_t* const buffer_B_tilized = reinterpret_cast<volatile uint32_t*>(0x1d000);

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_tilize.h"
#include "llk_unpack_A.h"
#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"
#include "params.h"

void run_kernel()
{
    _llk_unpack_tilize_hw_configure_<false,StochRndType::None>(DATA_FORMAT, DATA_FORMAT, FACE_R_DIM, 0, 4);
    _llk_unpack_tilize_init_(DATA_FORMAT, DATA_FORMAT, 1, FACE_R_DIM, false);
    _llk_unpack_tilize_(L1_ADDRESS(buffer_A),0,DATA_FORMAT,1,FACE_R_DIM,4,false);

    _llk_unpack_tilize_hw_configure_<false,StochRndType::None>(DATA_FORMAT, DATA_FORMAT, FACE_R_DIM, 0, 4);
    _llk_unpack_tilize_init_(DATA_FORMAT, DATA_FORMAT, 1, FACE_R_DIM, false);
    _llk_unpack_tilize_(L1_ADDRESS(buffer_B),0,DATA_FORMAT,1,FACE_R_DIM,4,false);

    _llk_unpack_AB_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(DATA_FORMAT, DATA_FORMAT, DATA_FORMAT, DATA_FORMAT);
    _llk_unpack_AB_init_<>();
    _llk_unpack_AB_<>(L1_ADDRESS(buffer_A_tilized), L1_ADDRESS(buffer_B_tilized));
}

#endif


#ifdef LLK_TRISC_MATH

#include "llk_math_eltwise_unary_datacopy.h"
#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "params.h"

using namespace ckernel;

void run_kernel()
{
    const bool is_int_fpu_en = false;

    // copy srca to dest
    #ifdef ARCH_BLACKHOLE
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, BroadcastType::NONE,false, is_fp32_dest_acc_en, is_int_fpu_en>(0, 0, 4, DATA_FORMAT);
    #else
    _llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, BroadcastType::NONE, is_fp32_dest_acc_en, is_int_fpu_en>(0, 0, 4, DATA_FORMAT);
    #endif
    _llk_math_pack_sync_init_<DstSync::SyncFull,is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false,false>(DATA_FORMAT, DATA_FORMAT);
    _llk_math_wait_for_dest_available_<DstSync::SyncFull>();
    _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncFull, BroadcastType::NONE, is_fp32_dest_acc_en, false>(0, DATA_FORMAT, DATA_FORMAT);
    _llk_math_wait_for_dest_available_<DstSync::SyncFull>();
    _llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncFull, BroadcastType::NONE, is_fp32_dest_acc_en, false>(1, DATA_FORMAT, DATA_FORMAT);

    _llk_math_pack_sync_init_<DstSync::SyncFull,is_fp32_dest_acc_en>();
    _llk_math_eltwise_binary_init_<ELTWISE_BINARY_OP, BroadcastType::NONE,MATH_FIDELITY>(4, 0, 0);
    _llk_math_wait_for_dest_available_<DstSync::SyncFull>();
    _llk_math_eltwise_binary_<ELTWISE_BINARY_OP, BroadcastType::NONE,DstSync::SyncFull, MATH_FIDELITY, EltwiseBinaryReuseDestType::NONE, is_fp32_dest_acc_en>(4, 2, false);

    _llk_math_dest_section_done_<DstSync::SyncFull,is_fp32_dest_acc_en>();
}

#endif 

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"


void run_kernel()
{
    volatile uint32_t* const buffer_Dest = reinterpret_cast<volatile uint32_t*>(0x1e000);
    const bool UNTILIZE = false;
    const std::uint32_t ct_dim = 1;
    const std::uint32_t operand_A_dst_index = 0;
    const std::uint32_t operand_B_dst_index = 1;
    const std::uint32_t res_dst_index = 2;

    std::fill(buffer_Dest, buffer_Dest + 16 * 16 * 4, 0xdeadbeef);

    #ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<UNTILIZE, is_fp32_dest_acc_en, false>(DATA_FORMAT, DATA_FORMAT, 16*16*4);
    #else
    _llk_pack_hw_configure_<UNTILIZE, is_fp32_dest_acc_en>(DATA_FORMAT, DATA_FORMAT, 16*16*4);
    #endif

    _llk_pack_init_<UNTILIZE, false, DstTileFaceLayout::RowMajor, false>(DATA_FORMAT);
    
    #ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncFull,DstTileFaceLayout::RowMajor,is_fp32_dest_acc_en>();
    #else
    _llk_pack_dest_init_<DstSync::SyncFull, DstTileFaceLayout::RowMajor, UNTILIZE, is_fp32_dest_acc_en>();
    #endif

    _llk_packer_wait_for_math_done_();
    _llk_pack_<DstSync::SyncFull,UNTILIZE, is_fp32_dest_acc_en>(operand_A_dst_index, L1_ADDRESS(buffer_A_tilized));
    _llk_packer_wait_for_math_done_();
    _llk_pack_<DstSync::SyncFull,UNTILIZE, is_fp32_dest_acc_en>(operand_B_dst_index, L1_ADDRESS(buffer_B_tilized));


    _llk_pack_init_<UNTILIZE, false, DstTileFaceLayout::RowMajor, false>(DATA_FORMAT);  

    #ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncFull,DstTileFaceLayout::RowMajor,is_fp32_dest_acc_en>();
    #else
    _llk_pack_dest_init_<DstSync::SyncFull, DstTileFaceLayout::RowMajor, UNTILIZE, is_fp32_dest_acc_en>();
    #endif

    _llk_pack_untilize_init_<ct_dim>(DATA_FORMAT,FACE_R_DIM,4);
    _llk_packer_wait_for_math_done_();
    _llk_pack_untilize_<ct_dim>(L1_ADDRESS(buffer_Dest), DATA_FORMAT, FACE_R_DIM, 4, res_dst_index);

    _llk_pack_dest_section_done_<DstSync::SyncFull,is_fp32_dest_acc_en>();

}

#endif