// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CKERNEL_HELPER_H
#define CKERNEL_HELPER_H

namespace ckernel{
	volatile std::uint32_t tt_reg_ptr *pc_buf_base = reinterpret_cast<volatile std::uint32_t *>(PC_BUF_BASE);
	volatile std::uint32_t tt_reg_ptr *instrn_buffer = reinterpret_cast<volatile std::uint32_t *>(INSTRN_BUF_BASE);
	volatile std::uint32_t tt_reg_ptr *regfile = reinterpret_cast<volatile std::uint32_t *>(REGFILE_BASE);
	volatile std::uint32_t tt_l1_ptr * trisc_l1_mailbox = reinterpret_cast<volatile std::uint32_t tt_l1_ptr *>(0x1d000);
	volatile std::uint32_t tt_reg_ptr *mailbox_base[4] = {
    reinterpret_cast<volatile std::uint32_t tt_reg_ptr *>(TENSIX_MAILBOX0_BASE), reinterpret_cast<volatile std::uint32_t tt_reg_ptr *>(TENSIX_MAILBOX1_BASE),
    reinterpret_cast<volatile std::uint32_t tt_reg_ptr *>(TENSIX_MAILBOX2_BASE), reinterpret_cast<volatile std::uint32_t tt_reg_ptr *>(TENSIX_MAILBOX3_BASE)};

	std::uint32_t cfg_state_id __attribute__((section(".bss"))) = 0;  // Flip between 0 and 1 to keep state between kernel calls
	std::uint32_t dest_offset_id __attribute__((section(".bss"))) = 0; // Flip between 0 and 1 to keep dest pointer between kernel calls
}

using namespace ckernel;


#endif
