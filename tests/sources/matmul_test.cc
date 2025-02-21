#include <cstdint>
#include <cstdio>
#include <algorithm>

#include "llk_defs.h"
#include "ckernel.h"

// Globals
uint32_t unp_cfg_context = 0;
uint32_t pack_sync_tile_dst_ptr = 0;
uint32_t math_sync_tile_dst_index = 0;
//const bool is_fp32_dest_acc_en = true;

volatile uint32_t tt_l1_ptr l1_buffer[16] __attribute__ ((section (".text#"))) __attribute__ ((aligned (16)));

#ifdef DEST_ACC
const bool is_fp32_dest_acc_en = true;
#else
const bool is_fp32_dest_acc_en = false;
#endif

#ifdef LLK_TRISC_UNPACK

#include "llk_unpack_AB_matmul.h"
#include "params.h"

volatile uint32_t* const buffer_A = reinterpret_cast<volatile uint32_t*>(0x1a000);
volatile uint32_t* const buffer_B = reinterpret_cast<volatile uint32_t*>(0x1b000);

void run_kernel()
{
    _llk_unpack_AB_matmul_hw_configure_<is_fp32_dest_acc_en, StochRndType::None>(DATA_FORMAT, DATA_FORMAT, DATA_FORMAT, DATA_FORMAT,16,16,0,4,4,128,128);
    _llk_unpack_AB_matmul_init_<>();
    _llk_unpack_AB_matmul_<>(reinterpret_cast<std::uint32_t>(buffer_A)/16-1, reinterpret_cast<std::uint32_t>(buffer_B)/16-1,0,0,128,128);
}

#endif

#ifdef LLK_TRISC_MATH

#include "llk_math_common.h"
#include "llk_math_matmul.h"
#include "params.h"

void run_kernel()
{
    _llk_math_matmul_init_<MATH_FIDELITY,DstTileFaceLayout::RowMajor>();
    _llk_math_pack_sync_init_<DstSync::SyncFull,is_fp32_dest_acc_en>();
    _llk_math_hw_configure_<false,false>(DATA_FORMAT,DATA_FORMAT);
    _llk_math_wait_for_dest_available_<DstSync::SyncFull>();
    _llk_math_matmul_<MATH_FIDELITY,DstTileFaceLayout::RowMajor>(0);
    _llk_math_dest_section_done_<DstSync::SyncFull,is_fp32_dest_acc_en>();
}

#endif 

#ifdef LLK_TRISC_PACK

#include "llk_pack.h"
#include "llk_pack_common.h"
#include "params.h"

volatile uint32_t* const buffer_Dest = reinterpret_cast<volatile uint32_t*>(0x1c000);
void run_kernel()
{

    std::fill(buffer_Dest, buffer_Dest + 16 * 16 * 4, 0xdeadbeef);

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

    _llk_packer_wait_for_math_done_();
    _llk_pack_<DstSync::SyncFull,false, is_fp32_dest_acc_en>(0, reinterpret_cast<std::uint32_t>(buffer_Dest)/16-1);
    _llk_pack_dest_section_done_<DstSync::SyncFull,is_fp32_dest_acc_en>();
}

#endif