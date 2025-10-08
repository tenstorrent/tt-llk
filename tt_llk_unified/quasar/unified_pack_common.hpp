// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_defs.h"
#include "llk_pack.h"
#include "unified_operand.hpp"
#include "unified_state.hpp"

inline void _unified_pack_hw_configure_(UnifiedOperand dst, bool is_32bit_dest, [[maybe_unused]] StochRndType rounding_mode)
{
    tdma_descriptor_t tdma_desc_dst;
    tdma_desc_dst.buf_desc.f.l1_addr_16B  = dst.l1_address;
    tdma_desc_dst.buf_desc.f.format       = static_cast<uint8_t>(dst.l1_format);
    tdma_desc_dst.buf_desc.f.lmt_addr_16B = 0;
    tdma_desc_dst.buf_desc.f.x_dim        = dst.face_width;
    tdma_desc_dst.buf_desc.f.y_dim        = dst.face_height;
    tdma_desc_dst.buf_desc.f.z_dim        = dst.num_faces();
    tdma_desc_dst.buf_desc_id             = dst.hint;
    tdma_desc_dst.reg_data_format         = (uint)dst.dest_format;

    _llk_pack_hw_configure_<p_pacr::PACK0>(tdma_desc_dst);
    _unified_state_pack_is_32bit_dest_ = is_32bit_dest;
}

inline void _unified_pack_sync_init_(bool full_sync)
{
    set_up_dest_dvalid_per_thread<dest_dvalid_client::PACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});
    _unified_state_pack_full_sync_ = full_sync;
}

inline void _unified_pack_acquire_dest_()
{
}

inline void _unified_pack_release_dest_()
{
    uint8_t mask = _unified_state_pack_full_sync_ ? 0b10 : 0b00;
    mask |= _unified_state_pack_is_32bit_dest_ ? 0b01 : 0b00;

    switch (mask)
    {
        case 0b00:
            _llk_pack_dest_dvalid_section_done_<ckernel::DstSync::SyncHalf, false>();
            break;
        case 0b01:
            _llk_pack_dest_dvalid_section_done_<ckernel::DstSync::SyncHalf, true>();
            break;
        case 0b10:
            _llk_pack_dest_dvalid_section_done_<ckernel::DstSync::SyncFull, false>();
            break;
        case 0b11:
            _llk_pack_dest_dvalid_section_done_<ckernel::DstSync::SyncFull, true>();
            break;
    }
}
