// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

#include "llk_san_extended.h"

// Goes in ComputeAPI
// State set only
void llk_san_support_backtrace(std::string function_name);

// Goes in LLK_API
// State set only
void llk_san_support_globals(bool dst_acc_mode, DstSync dst_sync, bool approx, std::int32_t math_fidelity);

// State set only
template <llk_san_operand operand>
void llk_san_support_operand(
    std::int32_t src_fmt,
    std::int32_t dst_fmt,
    std::int32_t num_faces,
    std::int32_t partial_face,
    std::int32_t face_r_dim,
    std::int32_t narrow_tile,
    std::int32_t tile_r_dim,
    std::int32_t tile_c_dim,
    std::int32_t tile_size,
    std::int32_t page_size);

// Goes in LLK_LIB in HWConfigure and HWReconfig
// State set + no hw config within kernel check
template <bool reconfig = false>
void llk_san_unpack_hw_configure(
    bool dst_acc_en,
    std::int32_t src_fmt_A,
    std::int32_t src_fmt_B,
    std::int32_t dst_fmt_A,
    std::int32_t dst_fmt_B,
    std::int32_t face_height_A,
    std::int32_t face_height_B,
    std::int32_t num_faces_A,
    std::int32_t num_faces_B);

// State set + no hw config within kernel check
template <bool reconfig = false>
void llk_san_math_hw_configure(std::int32_t math_fmt_A, std::int32_t math_fmt_B);

// State set + no hw config within kernel check
template <bool reconfig = false>
void llk_san_pack_hw_configure(
    bool dest_acc_en,
    std::int32_t src_fmt,
    std::int32_t dst_fmt,
    std::int32_t face_height,
    std::int32_t tile_width,
    std::int32_t num_faces,
    std::int32_t partial_face,
    std::int32_t narrow_tile);

// Goes in LLK_LIB in Init, Execute and Uninit
// No state set, just check that non x arguments match the stored ones
void llk_san_unpack_operand_check(
    bool dst_acc_en,
    std::int32_t src_fmt_A,
    std::int32_t src_fmt_B,
    std::int32_t dst_fmt_A,
    std::int32_t dst_fmt_B,
    std::int32_t face_height_A,
    std::int32_t face_height_B,
    std::int32_t num_faces_A,
    std::int32_t num_faces_B);

// No state set, just check that non x arguments match the stored ones
void llk_san_math_operand_check(std::int32_t math_fmt_A, std::int32_t math_fmt_B);

// No state set, just check that non x arguments match the stored ones
void llk_san_pack_operand_check(
    bool dest_acc_en,
    std::int32_t src_fmt,
    std::int32_t dst_fmt,
    std::int32_t face_height,
    std::int32_t tile_width,
    std::int32_t num_faces,
    std::int32_t partial_face,
    std::int32_t narrow_tile);

// Goes in LLK_LIB in Init
// Store operation type and push arguments to state stack
template <llk_san_op op, typename... Ts>
void llk_san_init(Ts... args);

// Set must uninit flag for operation
template <llk_san_op op>
void llk_san_must_uninit();

// Goes in LLK_LIB in Execute
// Check operation type and arguments against stored ones
template <llk_san_op op, typename... Ts>
void llk_san_operation(Ts... args);

// Goes in LLK_LIB in Uninit
// Check operation type and clear must uninit flag
template <llk_san_op op>
void llk_san_uninit();
