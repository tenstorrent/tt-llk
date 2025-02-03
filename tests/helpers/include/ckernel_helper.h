#ifndef CKERNEL_HELPER_H
#define CKERNEL_HELPER_H

namespace ckernel{
	volatile uint tt_reg_ptr *pc_buf_base = reinterpret_cast<volatile uint *>(PC_BUF_BASE);
	volatile uint tt_reg_ptr *instrn_buffer = reinterpret_cast<volatile uint *>(INSTRN_BUF_BASE);
	volatile uint tt_reg_ptr *regfile = reinterpret_cast<volatile uint *>(REGFILE_BASE);
	volatile uint tt_l1_ptr * trisc_l1_mailbox = reinterpret_cast<volatile uint tt_l1_ptr *>(0x1d000);
	uint32_t math_sync_tile_dst_index = 0;
	volatile uint tt_reg_ptr *mailbox_base[4] = {
    reinterpret_cast<volatile uint tt_reg_ptr *>(TENSIX_MAILBOX0_BASE), reinterpret_cast<volatile uint tt_reg_ptr *>(TENSIX_MAILBOX1_BASE),
    reinterpret_cast<volatile uint tt_reg_ptr *>(TENSIX_MAILBOX2_BASE), reinterpret_cast<volatile uint tt_reg_ptr *>(TENSIX_MAILBOX3_BASE)};

	volatile uint32_t inst_trace_ptr  __attribute__((section(".init"))) = 0;
	volatile uint32_t inst_trace[1024]  __attribute__((section(".init"))) = {0};

	uint32_t cfg_state_id __attribute__((section(".bss"))) = 0;  // Flip between 0 and 1 to keep state between kernel calls
	uint32_t dest_offset_id __attribute__((section(".bss"))) = 0; // Flip between 0 and 1 to keep dest pointer between kernel calls
}

using namespace ckernel;


#endif
