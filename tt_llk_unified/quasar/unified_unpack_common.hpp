// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "llk_unpack_common.h"
#include "unified_operand.hpp"
#include "unified_state.hpp"

inline void _unified_unpack_hw_configure_(UnifiedOperand src_a, UnifiedOperand src_b, bool is_32bit_dest, [[maybe_unused]] StochRndType rounding_mode)
{
    set_ttsync_enables<TRACK_ALL>(ckernel::unpack::TRISC_ID);

    tdma_descriptor_t tdma_desc_src_a;
    tdma_desc_src_a.buf_desc.f.l1_addr_16B  = src_a.l1_address;
    tdma_desc_src_a.buf_desc.f.format       = static_cast<uint8_t>(src_a.l1_format);
    tdma_desc_src_a.buf_desc.f.lmt_addr_16B = 0;
    tdma_desc_src_a.buf_desc.f.x_dim        = src_a.face_width;
    tdma_desc_src_a.buf_desc.f.y_dim        = src_a.face_height;
    tdma_desc_src_a.buf_desc.f.z_dim        = src_a.num_faces();
    tdma_desc_src_a.buf_desc_id             = src_a.hint;
    tdma_desc_src_a.reg_data_format         = (uint)src_a.src_format;
    tdma_descriptor_t tdma_desc_src_b;
    tdma_desc_src_b.buf_desc.f.l1_addr_16B  = src_b.l1_address;
    tdma_desc_src_b.buf_desc.f.format       = static_cast<uint8_t>(src_b.l1_format);
    tdma_desc_src_b.buf_desc.f.lmt_addr_16B = 0;
    tdma_desc_src_b.buf_desc.f.x_dim        = src_b.face_width;
    tdma_desc_src_b.buf_desc.f.y_dim        = src_b.face_height;
    tdma_desc_src_b.buf_desc.f.z_dim        = src_b.num_faces();
    tdma_desc_src_b.buf_desc_id             = src_b.hint;
    tdma_desc_src_b.reg_data_format         = (uint)src_b.src_format;

    _llk_unpack_hw_configure_<ckernel::p_unpacr::UNP_B>(tdma_desc_src_a);
    _llk_unpack_hw_configure_<ckernel::p_unpacr::UNP_A>(tdma_desc_src_b);

    _unified_state_unpack_is_32bit_dest_ = is_32bit_dest;
}

inline void _unified_unpack_sync_init_(bool full_sync)
{
    set_up_dest_dvalid_per_thread<dest_dvalid_client::UNPACK>({dest_dvalid_client::FPU, dest_dvalid_client::PACK});
    _unified_state_unpack_full_sync_ = full_sync;
}
