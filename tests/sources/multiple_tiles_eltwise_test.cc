#include <cstdint>
#include <cstdio>
#include "params.h"

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

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB.h"
#include "llk_unpack_common.h"
#include "params.h"

volatile uint32_t* buffer_A[KERN_CNT];
volatile uint32_t* buffer_B[KERN_CNT];

void run_kernel()
{
    for(int i=0; i< KERN_CNT; i++){
        buffer_A[i] = reinterpret_cast<volatile uint32_t*>(0x1a000 + i*TILE_SIZE_CNT);
        buffer_B[i] = reinterpret_cast<volatile uint32_t*>(0x1a000 + TILE_SIZE_CNT*KERN_CNT + i*TILE_SIZE_CNT);
    }

    _llk_unpack_AB_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(DATA_FORMAT, DATA_FORMAT, DATA_FORMAT, DATA_FORMAT);
    _llk_unpack_AB_init_<>();
    
    for(int index = 0; index < KERN_CNT; index++){
        _llk_unpack_AB_<>(reinterpret_cast<std::uint32_t>(buffer_A[index])/16-1, reinterpret_cast<std::uint32_t>(buffer_B[index])/16-1);
    }

}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_eltwise_binary.h"
#include "params.h"

void run_kernel()
{
    _llk_math_pack_sync_init_<DstSync::SyncFull,is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false,false>(DATA_FORMAT,DATA_FORMAT);
    _llk_math_eltwise_binary_init_<ELTWISE_BINARY_OP, BroadcastType::NONE,MATH_FIDELITY>(4, 0, 0);
    
    for(int index = 0; index < KERN_CNT; index++){

        _llk_math_wait_for_dest_available_<DstSync::SyncFull>();
        _llk_math_eltwise_binary_<ELTWISE_BINARY_OP, BroadcastType::NONE,DstSync::SyncFull, MATH_FIDELITY, EltwiseBinaryReuseDestType::NONE, is_fp32_dest_acc_en>(4, 0, false);
        _llk_math_dest_section_done_<DstSync::SyncFull,is_fp32_dest_acc_en>();
    }
}

#endif 

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

volatile uint32_t* buffer_Dest[KERN_CNT];

void run_kernel()
{
    process_addresses(buffer_Dest,KERN_CNT,PACK_ADDRS);

    #ifdef ARCH_BLACKHOLE
    _llk_pack_hw_configure_<false, is_fp32_dest_acc_en, false>(DATA_FORMAT, DATA_FORMAT, 16*16*4);
    #else
    _llk_pack_hw_configure_<false, is_fp32_dest_acc_en>(DATA_FORMAT, DATA_FORMAT, 16*16*4);
    #endif

    _llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(DATA_FORMAT);
    
    #ifdef ARCH_BLACKHOLE
    _llk_pack_dest_init_<DstSync::SyncFull,DstTileFaceLayout::RowMajor,is_fp32_dest_acc_en>();
    #else
    _llk_pack_dest_init_<DstSync::SyncFull, DstTileFaceLayout::RowMajor, false, false>();
    #endif

    for(int index = 0; index < KERN_CNT; index++){
        _llk_packer_wait_for_math_done_();
        _llk_pack_<DstSync::SyncFull,false, is_fp32_dest_acc_en>(0, reinterpret_cast<std::uint32_t>(buffer_Dest[index])/16-1);
        _llk_pack_dest_section_done_<DstSync::SyncFull,is_fp32_dest_acc_en>();
    }

}

#endif